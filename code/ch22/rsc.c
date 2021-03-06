// rsc.c
// Simple windows remote system call mechanism

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int Marshall( unsigned char flags, unsigned size, unsigned char *data, unsigned char *out, unsigned out_len )
{
     out[0] = flags;
     *((unsigned *)(&(out[1]))) = size;
     memcpy( &(out[5]), data, size );

     return size + 5;
}

////////////////////////////
// Parameter Flags /////////
////////////////////////////

// this thing is a pointer to a thing, rather than the thing itself
#define IS_PTR     0x01

// everything is either in, out or in | out
#define IS_IN     0x02 
#define IS_OUT     0x04

// null terminated data
#define IS_SZ     0x08

// null short terminated data (e.g. unicode string)
#define IS_SZZ     0x10


////////////////////////////
// Function Flags //////////
////////////////////////////

// function is __cdecl (default is __stdcall)
#define FN_CDECL     0x01


int AsmDemarshallAndCall( unsigned char *buff, void *loadlib, void *getproc )
{
     // params:
     // ebp: dllname
     // +4     : fnname
     // +8     : num_params
     // +12     : out_param_size
     // +16     : function_flags
     // +20     : params_so_far
     // +24     : loadlibrary
     // +28     : getprocaddress
     // +32     : address of out data buffer

     _asm
     {
     // set up params - this is a little complicated
     // due to the fact we're calling a function with inline asm

          push ebp
          sub esp, 0x100
          mov ebp, esp
          mov ebx, dword ptr[ebp+0x158]; // buff
          mov dword ptr [ebp + 12], 0;
          mov eax, dword ptr [ebp+0x15c];//loadlib
          mov dword ptr[ebp + 24], eax;
          mov eax, dword ptr [ebp+0x160];//getproc
          mov dword ptr[ebp + 28], eax;

          mov dword ptr [ebp], ebx; // ebx = dllname

          sub esp, 0x800;          // give ourselves some data space
          mov dword ptr[ebp + 32], esp;

          jmp start;

          // increment ebx until it points to a '0' byte
skip_string:
          mov al, byte ptr [ebx];
          cmp al, 0;
          jz done_string;
          inc ebx;
          jmp skip_string;

done_string:
          inc ebx;
          ret;

start:
          // so skip the dll name
          call skip_string;

          // store function name
          mov dword ptr[ ebp + 4 ], ebx

          // skip the function name
          call skip_string;
               
          // store parameter count
          mov ecx, dword ptr [ebx]
          mov edx, ecx
          mov dword ptr[ ebp + 8 ], ecx

          // store out param size
          add ebx,4
          mov ecx, dword ptr [ebx]
          mov dword ptr[ ebp + 12 ], ecx

          // store function flags
          add ebx,4
          mov ecx, dword ptr [ebx]
          mov dword ptr[ ebp + 16 ], ecx

          add ebx,4

// in this loop, edx holds the num parameters we have left to do.

next_param:
          cmp edx, 0
          je call_proc
          
          mov cl, byte ptr[ ebx ];     // cl = flags
          inc ebx;

          mov eax, dword ptr[ ebx ];     // eax = size
          add ebx, 4;

          mov ch,cl;
          and cl, 1;                         // is it a pointer?
          jz not_ptr;

          mov cl,ch;

// is it an 'in' or 'inout' pointer?
          and cl, 2;                         
          jnz is_in;
          
                                   // so it's an 'out'
                                   // get current data pointer
          mov ecx, dword ptr [ ebp + 32 ]
          push ecx

// set our data pointer to end of data buffer
          add dword ptr [ ebp + 32 ], eax          
          add ebx, eax
          dec edx
          jmp next_param

is_in:
          push ebx                         

// arg is 'in' or 'inout'
// this implies that the data is contained in the received packet
          add ebx, eax
          dec edx
          jmp next_param


not_ptr:
          mov eax, dword ptr[ ebx ];
          push eax;
          add ebx, 4
          dec edx
          jmp next_param;

call_proc:
          // args are now set up. let's call...
          mov eax, dword ptr[ ebp ];
          push eax;
          mov eax, dword ptr[ ebp + 24 ];
          call eax;
          mov ebx, eax;
          mov eax, dword ptr[ ebp + 4 ];
          push eax;
          push ebx;
          mov eax, dword ptr[ ebp + 28 ];
          call eax; // this is getprocaddress
          call eax; // this is our function call

          // now we tidy up
          add esp, 0x800;
          add esp, 0x100;
          pop ebp
     }

     return 1;
}



int main( int argc, char *argv[] )
{
     unsigned char buff[ 256 ];
     unsigned char *psz;
     DWORD freq = 1234;
     DWORD dur = 1234;
     DWORD show = 0;
     HANDLE hk32;
     void *loadlib, *getproc;
     char *cmd = "cmd /c dir > c:\\foo.txt";

     psz = buff;

     strcpy( psz, "kernel32.dll" );
     psz += strlen( psz ) + 1;

     strcpy( psz, "WinExec" );
     psz += strlen( psz ) + 1;

     *((unsigned *)(psz)) = 2;          // parameter count
     psz += 4;

     *((unsigned *)(psz)) = strlen( cmd ) + 1;     // parameter size
     psz += 4;

     // set fn_flags
     *((unsigned *)(psz)) = 0;
     psz += 4;

     psz += Marshall( IS_IN, sizeof( DWORD ), (unsigned char *)&show, psz, sizeof( buff ) );
     psz += Marshall( IS_PTR | IS_IN, strlen( cmd ) + 1, (unsigned char *)cmd, psz, sizeof( buff ) );

     hk32 = LoadLibrary( "kernel32.dll" );
     loadlib = GetProcAddress( hk32, "LoadLibraryA" );
     getproc = GetProcAddress( hk32, "GetProcAddress" );

     AsmDemarshallAndCall( buff, loadlib, getproc );

     return 0;
}
