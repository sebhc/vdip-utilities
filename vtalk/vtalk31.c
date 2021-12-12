/* vtalk - Version 3.1 for CP/M3 and HDOS
**
** USAGE: vtalk [-pxxx]
**
** xxx is optional octal base port (default is 0331)
**
** HDOS operation requires H-8-4 serial card.
**
** This program implements a simple polling loop to send and
** receive characters to/from FTDI VDIP-1 connected
** via a  first-in-first-out (FIFO) hardware interface at
** octal ports 0331-0332.  This utility lets you "talk" directly
** to the VDIP-1 device and issue commands as described in
** the "Vinculum Firmware User Manual" available at
** www.ftdichip.com.  To exit type Ctrl-C.
**
** Compiled with Software Toolworks C 3.0
**
**	Revised for CP/M 3 31 May 2019 - gfr
**	- added switch to set port number
**  Revised to accomodate all 3 OS versions - gfr
**			- 18 October 2019
**
** 	External routines to resolve at link time:
**		printf, inp, outp, aotoi
**
**	Suggested link statement:
**		l80 vtalk31,printf,pio,vutil31,clibrary/s,vtalk/n/e
**		
**  Note: for HDOS this bypasses the OS and accesses the 8250
**		UART directly. This makes it dependent on H-8-4 at
**		standard console port 0350.
**
**	Initial GitHub release 17 July 2021
**
*/
#include "fprintf.h"

/* define "HDOS" here to compile for HDOS, otherwise
** compilation will assume CP/M.  HDOS files
** should be saved with unix style NL line endings;
** CP/M files require MS-DOS style CR-LF line endings.
*/
#define HDOS	1

#define FALSE	0
#define TRUE	1

#define CTLC	3		/* control-C to exit loop */

/* FTDI VDIP ports and bits */
#define VDATA	0331	/* FTDI Data Port	*/
#define VSTAT	0332	/* FTDI Status Port	*/
#define VTXE	004		/* TXE# when hi ok to write */
#define VRXF	010		/* RXF# when hi data avail  */

/* USB i/o ports */
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

/* *** CP/M specific code here *** */
#ifndef HDOS
/* copen - initialize console
*/
copen()
{
	/* no action required for CP/M */
}

/* cclose - close out settings on console
*/
cclose()
{
	/* no action required for CP/M */
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
conin()
{
	return bdos(6, 0xFF);
}
#endif
/* *** end CP/M specific code *** */


/* *** HDOS specific code here *** */
#ifdef HDOS

#define CONSOLE 0350	/* console serial port	*/

/* conout - (HDOS version) output a character directly 
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

/* conin - (HDOS version) return a character directly
** from the console port, return NUL if none available
*/
conin()
{
	/* check for Data Ready */
	if ((inp(CONSOLE+5) & 0x01) == 0)
		/* return -1 if no data available */
		return 0;
	else
		/* read the character from the port and return it */
		return inp(CONSOLE);
}

/* copen - initialize console */
copen()
{
	/* disable console serial port interrupts
	** so that we can talk directly to the console
	** serial port (HDOS version)
	*/
	outp(CONSOLE+1,0);
}

/* cclose - close out settings on console */
cclose()
{
	/* re-enable console serial port interrupts */
	outp(CONSOLE+1,0x01);	
}
#endif	
/* *** end HDOS specific code *** */

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
in_vdip()
{
	/* check for Data Ready */
	if ((inp(p_stat) & VRXF) != 0)
		/* read the character from the port and return it */
		return inp(p_data);
	else
		/* return -1 if no data available */
		return -1;
}

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
	printf("VTALK v3.1 - G. Roberts.  Using USB ports: %o,%o\n",
			p_data, p_stat);
	printf("Enter VDIP commands, Ctrl-C to exit\n\n");
	
	/* perform any console initialization */
	copen();
	
	done = FALSE;
	cr_pending = FALSE;
	
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
}
