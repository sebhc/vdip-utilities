/********************************************************
** vtalk - Version 4 for CP/M3 and HDOS
**
** USAGE: vtalk {-pxxx}
**
** xxx is optional octal base port (default is 0331)
**
** HDOS operation requires H89 or H8 with H8-4 serial card.
** The software will check for this and abort with a message 
** if not found. It would not be difficult to add H8-5 support
** if there is a need or interest.
**
** This program implements a simple polling loop to send and
** receive characters to/from FTDI VDIP-1 connected
** via a  first-in-first-out (FIFO) hardware interface.
** This utility lets you "talk" directly to the VDIP-1
** device and issue commands as described in the "Vinculum
** Firmware User Manual" available at www.ftdichip.com.
**
** To exit type Ctrl-C.
**
** Compiled with Software Toolworks C 3.1
**
**  External routines to resolve at link time:
**    printf, inp, outp
**
**  Suggested link statement:
**    l80 vtalk,printf,pio,clibrary/s,vtalk/n/e
**
**  This code uses ifdef to choose between HDOS and CP/M
**  I/O calls. if HDOS is defined then the HDOS code will
**  be compiled, otherwise the CP/M code will be compiled.
**  You can either add the appropriate #ifdef line below,
**  or add the following switch to your compile line:
**
**    C -qHDOS=1 VTALK
**
**  Also, our convention is to save all files with CP/M
**  style line endings (CR-LF). The HDOS version of C/80
**  is picky and wants to see just LF (which is the HDOS
**  standard line ending, therefore the CR characters must
**  be stripped out before compilation.  A separate STRIP.C
**  program provides this capability.
**
**  Note: 
**    For HDOS this bypasses the OS and accesses the 8250
**    UART directly for console I/O. This makes it dependent
**    on H8-4 (H8) or H89 console at the standard port 0350.
**    HDOS does not appear to have a non-blocking console
**    read function, and attempts to use .SCIN with console
**    in RAW mode were unsuccessful.
**
**  Glenn Roberts
**  28 January 2022
**
********************************************************/
#include "printf.h"

#define FALSE 0
#define TRUE  1

#define CTLC  3   /* control-C to exit loop */

/* FTDI VDIP ports and bits */
#define VDATA 0331  /* FTDI Data Port */
#define VSTAT 0332  /* FTDI Status Port */
#define VTXE  004   /* TXE# when hi ok to write */
#define VRXF  010   /* RXF# when hi data avail  */

/* USB i/o ports */
int p_data;   /* USB data port */
int p_stat;   /* USB status port */


#ifndef HDOS
/************************************
**
**     CP/M-specific code here
**
*************************************
*/

/* copen - initialize console. no action
** required for CP/M version.
*/
int copen()
{
  return 0;
}

/* cclose - close out settings on console
** no action required for CP/M version
*/
int cclose()
{
  return 0;
}

/* conout - output a character to the console using CP/M
** Direct Console I/O BDOS call (function 6)
*/
conout(c)
char c;
{
  bdos(6, c);
}

/* conin - input a character from the console using CP/M
** Direct Console I/O BDOS call (function 6).
** Returns the character or NUL if no data avail.
*/
int conin()
{
  return bdos(6, 0xFF);
}

/************************************
**
**     End CP/M-specific code
**
*************************************
*/
#endif


#ifdef HDOS
/************************************
**
**     HDOS-specific code here
**
*************************************
*/

#define CONSOLE 0350  /* console serial port  */
#define SCDB  0x20E3  /* S.CDB = 040.343      */

char *ctype = SCDB;

/* copen - initialize console. first check for 
** 8250 UART by examining location S.CDB (the 
** 8251 USART on the H5-5 is not currently
** supported in the HDOS version of this code).
** Then disable console interrupts return 0 if
** successful or -1 if no 8250-style UART found.
*/
int copen()
{
  int rc;
  
  if (*ctype == 1) {
    /* have 8250 UART. disable console serial
    ** port interrupts so that we can talk 
    ** directly to the console serial port.
    */
    outp(CONSOLE+1,0);
    rc = 0;
  }
  else
    rc = -1;
  
  return rc;
}

/* cclose - close out settings on console */
int cclose()
{
  /* re-enable console serial port interrupts */
  outp(CONSOLE+1,0x01);
  return 0;
}

/* conout - output a character directly 
** to the console serial port
*/
conout(c)
char c;
{
  /* Wait for Transmit Hold Register empty */
  while ((inp(CONSOLE+5) & 0x20) == 0)
    /* wait ... */;
  /* Then transmit the character */
  outp(CONSOLE,c);
}

/* conin - return a character directly from the
** console port, return NUL if none available
*/
int conin()
{
  /* check for Data Ready */
  if ((inp(CONSOLE+5) & 0x01) == 0)
    /* return -1 if no data available */
    return 0;
  else
    /* read the character from the port and return it */
    return inp(CONSOLE);
}

/************************************
**
**     End HDOS-specific code
**
*************************************
*/
#endif  

/* out_vdip - send a character to the VDIP-1 via the
** specified port
*/
out_vdip(c)
{
  /* Wait for ok to transmit */
  while ((inp(p_stat) & VTXE) == 0)
    /* while bit is low just wait ... */
    ;
  /* OK to transmit the character */
  outp(p_data,c);
}

/* in_vdip - return a character from the
** VDIP-1 or -1 if none available
*/
int in_vdip()
{
  /* check for Data Ready */
  if ((inp(p_stat) & VRXF) != 0)
    /* read the character from the port and return it */
    return inp(p_data);
  else
    /* return -1 if no data available */
    return -1;
}

/* aotoi - convert octal string to an integer 
*/
int aotoi(s)
char s[];
{
  int i, n;
  
  n = 0;
  for (i = 0; s[i] >= '0' && s[i] <= '7'; ++i)
  n = 8 * n + s[i] - '0';
  return(n);
}

/* dosw - process '-' switches and arguments
*/
dosw(argc, argv)
int argc;
char *argv[];
{
  int i;
  char *s;
  
  /* process right to left */
  for (i=argc-1; i>0; i--) {
    s = argv[i];
    if (*s++ == '-') {
      /* have a switch! */
      switch (*s) {
      case 'P':
        ++s;
        p_data = aotoi(s);
        p_stat = p_data + 1;
          break;
      default:
          printf("Invalid switch %c\n", *s);
        break;
      }
    }
  }
}

main(argc,argv)
int argc;
char *argv[];
{
  static int c, done, cr_pending;

  /* default port values */
  p_data = VDATA;
  p_stat = VSTAT;

  /* process any switches */
  dosw(argc, argv);

  /* print welcome */
  printf("VTALK v4 [%o]\n", p_data);
  printf("Enter Vinculum commands, Ctrl-C to exit\n\n");

  
  /* perform any console initialization */
  if (copen() != -1) {
  
    done = FALSE;
    cr_pending = FALSE;

    /* tickle the VDIP to get a prompt */
    out_vdip('\r');
  
    /* this loop repeatedly polls for input on the
    ** console, echos that to the VDIP, then polls
    ** the VDIP and echos that to the console. Ctrl-C
    ** on the console exits the loop.
    */
    while(!done) {

      /* process console input, if any */
      if ((c=conin()) != 0) {
        /* control-C is escape character */
        if (c == CTLC)
          done = TRUE;
        else {
          /* send to VDIP */
          out_vdip(c);
          /* echo to console; add LF to CR */
          conout(c);
          if (c == '\r') {
            conout('\n');
            /* don't care about pending CR */
            cr_pending = FALSE;
          }
        }
      }
    
      /* process VDIP input, if any.  the VDIP has an annoying
      ** habit of sending CR after every output, including
      ** the prompt.  here we supress that CR
      */
      if ((c=in_vdip()) != -1) {
        /* check if CR is pending output */
        if (cr_pending) {
          conout('\r');
          conout('\n');
          cr_pending = FALSE;
        }
        if (c == '\r')
          /* if CR just mark as pending */
          cr_pending = TRUE;
        else
          conout(c);
      }
    }
    /* Ctrl-c has been hit. reset any console changes and exit */
    cclose();
  } else {
    printf("Unable to open console - 8250 UART not detected.\n");
  }
}
