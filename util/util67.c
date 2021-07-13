/********************************************************
** util67.c
**
** These are utility routines for interfacing to the
** Heath/Zenith Z-67 mass storage system.  They are 
** designed for use with the Software Toolworks C/80
** v. 3.1 compiler with the optional support for
** floats and longs.  The compiler should be configured
** to produce a Microsoft relocatable module (.REL file)
** file which can (optionally) be stored in a library
** (.LIB file) using the Microsoft LIB-80 Library Manager.
** The Microsoft LINK-80 loader program is then used to
** link this code, along with any other required modules,
** with the main (calling) program.
**
** There are four utility routines defined here (see
** comments below for details on usage):
**
**	c3tolong(c);
**	atol(s);
**	h67read(lun,addr,nblks,buffer,cbyte);
**	h67write(lun,addr,nblks,buffer,cbyte);
**
** Libraries required:
**
** h67io.mac - low level Z-67 I/O routines (Macro 80)
**		getcon() - get controller attention
**		outcom(command) - output a command
**		read67(buffer) - read data from the Z-67
**		writ67(buffer) - write data to the Z-67
**		compst() - check completion status
**
** pio.mac - low level port I/O (Macro 80)
**		outp(port,c) - output to a port
**		inp(port) - input from a port
**
** Release: September, 2017
**
**	Glenn Roberts
**	glenn.f.roberts@gmail.com
**
********************************************************/
/* #define DEBUG for testing */
/* #define	DEBUG	1*/

#include "fprintf.h"

/* Class 0 Command Definitions */
#define D_REA   010
#define D_WRI   012

/* buffer to hold 6-byte command instruction */
char cmdq[6];

/* union used to break out the bytes in a long */
union u_tag {
  long lvalue;
  char cvalue[4];
} uval;

/********************************************************
**
** c3tolong
**
** The Z-67 uses a 21-bit sector addressing
** scheme, which requires a long data variable for
** storage.  This routine converts a 3-byte sector 
** address (stored in an array of char) to a long.
**
********************************************************/
long c3tolong(c)
char *c;
{
  uval.cvalue[0]=c[0];
  uval.cvalue[1]=c[1];
  uval.cvalue[2]=c[2];
  uval.cvalue[3]=0;
  return uval.lvalue;
}

/********************************************************
**
** atol
**
** This routine is functionally the same as the 
** standard atoi() function but it returns a long.
**
********************************************************/
long atol(s)
char *s;
{
  static long n, sign;
  sign = 1L;
  n = 0L;
  switch (*s) {
	case '-': sign = -1L;
	case '+': ++s;
  }
  while (*s >= '0' && *s <= '9')
	n = 10L * n + *s++ - '0';
  return (sign * n);
}

/********************************************************
**
** h67read
**
** This routine issues a read command to the Z-67
** controller.  Supplied parameters include:
**
**	lun = Logical Unit Number (0 or 1)
**	addr = 21-bit sector address (0 - 2,097,151)
**	nblks = number of 256-byte blocks to read
**	buffer = address of buffer space to hold data
**	cbyte = control byte passed to the controller
**
** The routine returns the completion status as
** defined in the compst() routine (0=normal)
**
********************************************************/
int h67read(lun,addr,nblks,buffer,cbyte)
long addr;
unsigned int lun, nblks, cbyte;
char *buffer;
{
  uval.lvalue=addr;
  cmdq[0] = D_REA;      /* opcode = READ                */
  /* top 3 bits are LUN; bottom 5 bits are addr 2	    */
  cmdq[1] = (lun<<5) | (uval.cvalue[2]&0x1F);
  cmdq[2] = uval.cvalue[1];   /* high byte (addr 1)     */
  cmdq[3] = uval.cvalue[0];   /* low byte  (addr 0)     */
  cmdq[4] = nblks;      /* number of blocks to read     */
  cmdq[5] = cbyte;          	/* control byte         */
 
#if DEBUG
  printf("read %02x %02x %02x %02x %02x %02x\n",
  cmdq[0],cmdq[1], cmdq[2], cmdq[3], cmdq[4], cmdq[5]);
#endif
 
  getcon();             /* get controller's attention   */
  outcom(cmdq);         /* send the command             */
  read67(buffer);      /* read the returned data       */
  return compst();     /* return completion status     */
}

/********************************************************
**
** h67write
**
** This routine issues a write command to the Z-67 
** controller.  Supplied parameters include:
**
**      lun = Logical Unit Number (typically 0 or 1)
**      addr = 21-bit sector address (0 - 2,097,151)
**      nblks = number of 256-byte blocks to write
**      buffer = address of buffer containing the data
**      cbyte = control byte
**
** The routine returns the completion status as
** defined in the compst() routine (0=normal)
**
********************************************************/
int h67write(lun,addr,nblks,buffer,cbyte)
long addr;
unsigned int lun, nblks, cbyte;
char *buffer;
{
  uval.lvalue=addr;
  cmdq[0] = D_WRI;      /* opcode = WRITE               */
  /* top 3 bits are LUN; bottom 5 bits are addr 2       */
  cmdq[1] = (lun<<5) | (uval.cvalue[2]&0x1F);
  cmdq[2] = uval.cvalue[1];   /* high byte (addr 1)     */
  cmdq[3] = uval.cvalue[0];   /* low byte  (addr 0)     */
  cmdq[4] = nblks;      /* number of blocks to write    */
  cmdq[5] = cbyte;          /* control byte             */
  
#if DEBUG
  printf("write %02x %02x %02x %02x %02x %02x\n",
  cmdq[0],cmdq[1], cmdq[2], cmdq[3], cmdq[4], cmdq[5]);
#endif 
   
  getcon();             /* get controller's attention   */
  outcom(cmdq);         /* send the command             */
  writ67(buffer);     /* now write the data           */
  return compst();     /* return completion status     */
}
