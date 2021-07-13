/* vtalk - CP/M 3 version
**
** This program implements a simple polling loop to send and
** receive characters to/from FTDI VDIP-1 connected
** via a  first-in-first-out (FIFO) hardware interface at
** octal ports 261-262.  This utility lets you "talk" directly
** to the VDIP-1 device and issue commands as described in
** the "Vinculum Firmware User Manual" available at
** www.ftdichip.com.  To exit type Ctrl-C.
**
** Compiled with Software Toolworks C 3.0
**
** Version 1.3
**
** Glenn Roberts, 10 May 2013
**
**	Revised for CP/M 3 31 May 2019 - gfr
**	- added switch to set port number
**
*/
#include "printf.h"

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

/* out_str - output a string directly to the specified serial port
*/
out_str(s)
char *s;
{
	char *c;
	
	for (c=s; *c!=0; c++)
		conout(*c);
}

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

/* aotoi - convert octal s to int */
aotoi(s)
char s[];
{
  int i, n;
	
  n = 0;
  for (i = 0; s[i] >= '0' && s[i] <= '7'; ++i)
	n = 8 * n + s[i] - '0';
  return(n);
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

	printf("Using USB ports: %o,%o\n", p_data, p_stat);
	
	/* terminal emulation loop, ctl-C exits */
	done = FALSE;
	cr_pending = FALSE;
	
	/* intro banner */
	out_str("Enter VDIP commands, Ctrl-C to exit\r\n\n");
	
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
		
		/* process VDIP input, if any.  the VDIP has a nasty
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
}
