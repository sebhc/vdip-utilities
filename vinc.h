/********************************************************
** vinc.h
**
** template definitions for vinc library, plus useful
** shared values.
**
**      31 January 2022
**      Glenn Roberts
**
********************************************************/
#ifndef EXTERN
#define EXTERN extern
#endif

EXTERN char td_string[15];      /* time/date hex value */
EXTERN char linebuff[128];      /* I/O line buffer */
EXTERN int p_data;
EXTERN int p_stat;

/* FTDI VDIP bits */
#define VTXE    004             /* TXE# when hi ok to write */
#define VRXF    010             /* RXF# when hi data avail  */

/* FTDI VDIP default ports */
#define VDATA   0331
#define VSTAT   0332

/* standard VDIP command prompt */
#define PROMPT  "D:\\>"
#define CFERROR "Command Failed"

/* union used to dissect long (4 bytes) into pieces */
union u_fil {
        long l;         /* long            */
        unsigned i[2];  /* 2 unsigned ints */
        char b[4];      /* same as 4 bytes */
};

/* default max time (sec) to wait for response */
#define MAXWAIT 15


/* templates for stdlib routines */
char *itoa();

/* routines to access Vinculum API */
int str_send();
int str_rdw();
int in_v();
int out_v();
int in_vwait();
int out_vwait();
int vfind_disk();
int vpurge();
int vhandshake();
int vinit();
int vsync();
int vdirf();
int vdird();
int vprompt();
int vropen();
int vwopen();
int vseek();
int vclose();
int vclf();
int vipa();
int vread();
int vwrite();
int vcd();
int vcdroot();
int vcdup();
int vmkd();
