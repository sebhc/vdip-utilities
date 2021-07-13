/********************************************************
** vutil.c
**
** These are utility routines used to access the Vinculum 
** VDP1 USB interface using the H-8 USB board designed 
** by Norberto Collado: http://koyado.com/Heathkit/H-8_USB.html
** They will also work with Norberto's H-89 version:
** http://koyado.com/Heathkit/H-89_USB_Serial.html
**
** With these routines you can write programs to transfer files
** to and from the H-8/H-89 system using USB memory sticks or 
** other compatible storage devices.
**
** Norberto's board provides a hardware interface to the 
** VDIP1 module containing the VNC1L chip from Future 
** Technology Devices International:
**
** http://www.ftdichip.com/Support/Documents/DataSheets/Modules/DS_VDIP1.pdf
**
** The board addresses the chip using the VDIP1's parallel FIFO mode.
**
** The VDIP1 module handles all aspects of the USB software
** interface, which would otherwise be impossible or impractical
** to implement on simple 8-bit hardware like the H-8.
**
** These routines communicate directly with the VDIP1's Command
** Monitor using simple ASCII commands defined in the Vinculum
** Firmware User's Manual:
**
** http://www.ftdichip.com/Firmware/Precompiled/UM_VinculumFirmware_V205.pdf
**
** There are 29 utility routines defined here (see
** comments in the code below for details on usage):
**
**  str_send()
**  str_flush()
**  str_read()
**  str_rdw()
**  out_v()
**  in_v()
**  in_vwait()
**  vfind_disk()
**  vpurge()
**  vhandshake()
**  vinit()
**  vsync()
**  vdirf()
**  vdird()
**  prndate()
**  prntime()
**  vprompt()
**  vropen()
**  vwopen()
**  vseek()
**  vclose()
**  vclf()
**  vipa()
**  vread()
**  vwrite()
**  gethexvals()
**  hexval()
**  wait()
**  mswait()
**
** The typical calling sequence is as follows: vinit() is
** called first to ensure communication and put the device
** in a known state, then vfind_disk() is used to ensure that
** a storage device is attached.  Files can then be opened
** with vropen() or vwopen(); I/O operations performed with
** vread(), vwrite(), and vseek(); and finally closed with
** vclose();
**
** This code is designed for use with the Software Toolworks C/80
** v. 3.1 compiler with the optional support for
** floats and longs.  The compiler should be configured
** to produce a Microsoft relocatable module (.REL file)
** file which can (optionally) be stored in a library
** (.LIB file) using the Microsoft LIB-80 Library Manager.
** The Microsoft LINK-80 loader program is then used to
** link this code, along with any other required modules,
** with the main (calling) program.
** Release: September, 2017
**
**	Glenn Roberts
**	glenn.f.roberts@gmail.com
**
********************************************************/

#define	  HDOS	1
/* #define	  CPM	1 */

#include "fprintf.h"

/* template for non-int function */
char *itoa();

/* tick counter RAM location differs between CP/M and HDOS */
#ifdef CPM
#define TICCNT	0x000B
#else
#define TICCNT	0x201B
#endif

/* default max time (ms) to wait for prompt */
#define	MAXWAIT	5000

#define	TRUE	1
#define	FALSE	0

/* FTDI VDIP bits */
#define VTXE	004			/* TXE# when hi ok to write */
#define VRXF	010			/* RXF# when hi data avail  */

/* standard VDIP command prompt */
#define PROMPT	"D:\\>"
#define CFERROR "Command Failed"

/* union used to dissect long (4 bytes) into pieces */
union u_fil {
	long l;		/* long            */
	unsigned i[2];	/* 2 unsigned ints */
	char b[4];	/* same as 4 bytes */
};

/* pointer to 2-ms clock */
unsigned *Ticptr = TICCNT;
unsigned timeout;

/* hex value of time/date kept here */
char td_string[15];

/* I/O line buffer 		*/
char linebuff[128];	

/* USB i/o ports (allocated in calling program )*/
extern int p_data;		/* USB data port */
extern int p_stat;		/* USB status port */

/********************************************************
**
** str_send
**
** Output a null-terminated string to the VDIP device.
** If you want a carriage return to be sent it must be
** in the string (\r).
**
********************************************************/
str_send(s)
char *s;
{
	char *c;

	for (c=s; *c!=0; c++)
		out_v(*c);
}

/********************************************************
**
** str_flush
**
** Read and discard bytes up to and including the specified
** character. 
**
********************************************************/
str_flush(c)
int c;
{
	int i;

	for (i=0; (in_v() != c); i++)
		;
}

/********************************************************
**
** str_read
**
** Used to read a string from the VDIP device. Read and
** consume characters up to and including the character
** specified in 'tchar'.
**
********************************************************/
str_read(s, tchar)
char *s;
char tchar;
{
	char c;

	do {
		while((c = in_v()) == -1)
			;
		/* replace tchar with NULL */
		if (c == tchar)
			c = 0;
		*s++ = c;
	} while (c != 0);
}

/********************************************************
**
** str_rdw
**
** "Read with wait" - Used to read a string from the VDIP
** device but detect hung conditions by monitoring the time.
** Read and consume characters up to and including the character 
** specified in 'tchar', but wait no longer than the 
** specified time 't' for each byte.  t is specified in
** milliseconds.
**
** Returns:
**		0 if read was successful
**		-1 if read timed out.
**
********************************************************/
str_rdw(s, tchar, t)
char *s;
char tchar;
unsigned t;
{
	int c, rc;
	int timedout;
	
	timedout = FALSE;
	rc = 0;

	do {
		if((c = in_vwait(t)) == -1)
			timedout = TRUE;
		else {
			/* got a byte, check for end */
			if (c == tchar)
				c = 0;
			*s++ = c;
		}
	} while ((c != 0) && (!timedout));
	if (timedout) {
		/* terminate the string and exit */
		*s = 0;
		rc = -1;
	}

	return rc;
}

/********************************************************
**
** out_v
**
** Send a character to the VDIP1 via the specified port.
** Performs I/O handshaking with the VDIP1 device.
**
********************************************************/
out_v(c)
{
	/* Wait for ok to transmit (VTXE high) */
	while ((inp(p_stat) & VTXE) == 0)
		;
	/* OK to transmit the character */
	outp(p_data,c);
}

/********************************************************
**
** in_v
**
** Read a character from the VDIP1 
**
** Returns:
**		Character read if successful
**		-1 if none available
**
** NOTE: Usage is deprecated - use in_vwait to avoid hung
** conditions!
**
********************************************************/
in_v()
{
	int b;
	
	/* check for Data Ready */
	if ((inp(p_stat) & VRXF) != 0) {
		/* read the character from the port and return it */
		b = inp(p_data);
		return b;
	}
	else
		/* return -1 if no data available */
		return -1;
}

/********************************************************
**
** in_vwait
**
** "Input with Wait" - Input a character from the VDIP1 but 
** detect hung conditions by monitoring the time. Wait no 
** longer than the specified time 't' (in milliseconds.)
**
** Returns:
**		Character read if successful
**		-1 if timed out
**
** NOTE: in_vwait() is preferred over in_v() in order to 
** avoid a hung condition!
**
********************************************************/
in_vwait(milliseconds)
unsigned milliseconds;
{
	int b;
	
	/* 2 ms per tick */
	timeout = *Ticptr + (milliseconds >> 1);

	while (*Ticptr != timeout) {
		/* check for Data Ready */
		if ((inp(p_stat) & VRXF) != 0) {
			/* Have data! read the byte and return it */
			b = inp(p_data);
			return b;
		}	
	}
	/* Timed out! return -1 */
	return -1;
}

/********************************************************
**
** vfind_disk
**
** Determine if there is a flash drive available on the
** USB port. 
**
**
********************************************************/
vfind_disk()
{
	/* If there is a drive available then a \r will cause 
	** the prompt to come back, so simply send \r and 
	** then test for a command prompt...
	*/
	str_send("\r");
	
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vpurge
**
** Read all the pending data from the VDIP device and 
** throw it away.  Wait for up to t milliseconds each time
** before deciding we're done. 
**
********************************************************/
vpurge(t)
unsigned t;
{
	int c;
	
	do {
		c = in_vwait(t);
	} while (c != -1);
}

/********************************************************
**
** vhandshake
**
** This routine basically checks to see if two-way
** communication with the Command Monitor is working. It
** sends an ASCII 'E' and waits up to 't' microseconds
** for the 'E' to be echoed back.
**
** Returns:
**		TRUE	if the Monitor responded in time
**		FALSE	if no response before timeout
**
********************************************************/
vhandshake(t)
unsigned t;
{
	int i, done;
	
	for (done=FALSE, i=0; (i<3) && (!done); i++) {
		str_send("E\r");
		done = (str_rdw(linebuff, '\r', t) == 0);
	}
	if (strcmp(linebuff,"E") == 0)
		return TRUE;
	else
		return FALSE;
}

/********************************************************
**
** vinit
**
** Initialize the VDIP connection.
**
** This routine tests for the presence of the device, does
** a synchronization to a known condition, ensures that
** the device is in ASCII I/O mode and (optionally) issues 
** any setup commands to initialize the desired settings.
**
** Returns:
**		0: Normal
**		-1: Error
**
********************************************************/
vinit()
{
	int rc;
	
	rc = 0;
	
	/*first try to talk to the device */
	if (!vsync())
		rc = -1;
	else {
		/* initialization commands */
		
		/* ASCII mode (more friendly) */
		rc = vipa();
		/* Close any open file */
		if (rc == 0)
			rc = vclf();
	}
	
	return rc;
}

/********************************************************
**
** vsync
**
** Flush the input buffer and attempt to handshake with
** the VDIP1 device.  This should put things in a known
** state.
**
** Returns:
**		TRUE after successful synchronization
**		FALSE if something goes wrong and can't sync
**
********************************************************/
vsync()
{
	int i, done;
	
	/* try up to 3 times to sync */
	for (i=0, done=FALSE; ((i<3) && (!done)); i++) {
		/* first purge any waiting data */
		vpurge(MAXWAIT);
	
		if (vhandshake(MAXWAIT))
			/* we're done! */
			done = TRUE;
		else {
			/* sometimes it takes more time, try 2s ... */
			printf("Synchronizing...\n");
			wait(2);
		}
	}
	return done;
}

/********************************************************
**
** vdirf
**
** This is an interface to the Vinculum "DIR" command
** (Directory) for a specified file.
**
** Issue a "DIR" command for the specified file name. The
** USB drive will be searched for a matching file name and
** will return the file size, which is returned as a long
** in 'len'.
**
** Returns:
**		0: Normal
**		-1: Error (most likely means file not found)
**
********************************************************/
vdirf(s, len)
char *s;
long *len;
{
	int rc;
	char *c;
	static union u_fil flen;

	rc = 0;
	
	str_send("dir ");
	str_send(s);
	str_send("\r");
	
	/* first line is always blank, just read it */
	str_read(linebuff, '\r');
	
	/* the result will either be the file name or
	** "Command Failed". if the latter then return error.
	*/
	str_read(linebuff, '\r');

	if (strcmp(linebuff, CFERROR) == 0) {
		/* flag an error! */
		rc = -1;
	}
	else {
		/* skip over file name (to first blank) */
		for (c=linebuff; ((*c!=' ') && (*c!=0)); c++)
			;
		/* read file length as 4 hex values */
		gethexvals(c, 4, &flen.b[0]);

		/* return file size */
		*len = flen.l;
		
		/* success - gobble up the prompt */
		str_read(linebuff, '\r');
	}
	
	return rc;
}

/********************************************************
**
** vdird
**
** This is an interface to the Vinculum "DIRT" command
** (Directory) for a specified file.
**
** Issue a "DIRT" command for the specified file name. The
** USB drive will be searched for a matching file name and
** will return the last modified date and time in the
** specified locations, which are passed by reference.
**
** Returns:
**		0: Normal
**		-1: Error (most likely means file not found)
**
********************************************************/
vdird(s, udate, utime)
char *s;
unsigned *udate, *utime;
{
	int i, rc;
	char *c;
	static union u_fil fdate;
	static char dates[10];
	
	rc = 0;
	
	str_send("dirt ");
	str_send(s);
	str_send("\r");
	
	/* first line is always blank, just read it */
	str_read(linebuff, '\r');
	
	/* result will either be the file name followed
	** by 10 bytes, or "Command Failed".
	*/
	str_read(linebuff, '\r');

	if (strcmp(linebuff, CFERROR) == 0) {
		/* flag an error! */
		rc = -1;
	}
	else {
		/* skip over the file name (to first blank) */
		for (c=linebuff; ((*c!=' ') && (*c!=0)); c++)
			;
		/* read all 3 date fields */
		gethexvals(c, 10, dates);

		/* last 4 bytes are the modification date */
		for (i=0; i<4; i++)
			fdate.b[i] = dates[i+6];

		/* return date and time */
		*utime = fdate.i[0];
		*udate = fdate.i[1];
		
		/* success - gobble up the prompt */
		str_read(linebuff, '\r');
	}
	return rc;
}

/********************************************************
**
** prndate
**
** This routine extrates the month, day and year information
** from a 16-bit word and displays them on the standard
** output device.  The date format is as defined in the FTDI
** Vinculum Firmware User Manual.
**
********************************************************/
prndate(udate)
unsigned udate;
{
	unsigned mo, dy, yr;
	
	dy = udate & 0x1f;
	mo = (udate >> 5) & 0xf;
	yr = 1980 + ((udate >> 9) & 0x7f);
	
	printf("%2d/%02d/%02d", mo, dy, (yr % 100));
}

/********************************************************
**
** prntime
**
** This routine extrates the time information (hour and minute)
** from a 16-bit word and displays them on the standard
** output device.  A 12-hour clock format is used including
** "AM" or "PM" as appropriate.  The time format is as defined
** in the FTDI Vinculum Firmware User Manual.
**
********************************************************/
prntime(utime)
unsigned utime;
{
	unsigned hr, min, sec;
	char *am_pm;
	
	sec = 2*(utime & 0x1f);
	min = (utime >> 5) & 0x3f;
	hr  = (utime >> 11) & 0x1f;
	if (hr > 12) {
		am_pm = "PM";
		hr = hr - 12;
	}
	else
		am_pm = "AM";
	
	printf("%2d:%02d %s", hr, min, am_pm);
}
/********************************************************
**
** vprompt
**
** check for "D:\>" prompt. wait 'ms' milliseconds
** before declaring a timeout.
**
** Returns:
**		0 Normal
**		-1 on error
**
********************************************************/
vprompt(ms)
unsigned ms;
{
	/* check for normal prompt return */
	if (str_rdw(linebuff, '\r', ms) == -1)
		return -1;
	else if (strcmp(linebuff, PROMPT) != 0 )
		return -1;
	else
		return 0;
}

/********************************************************
**
** vropen
**
** This is an interface to the Vinculum "OPR" command
** (Open File For Read).
**
** Open a file for reading.
**
** Returns:
**		0 normal
**		-1 on error
**
********************************************************/
vropen(s)
char *s;
{
	/* as a safety measure, close any open file */
	vclf();
	
	str_send("opr ");
	str_send(s);
	str_send("\r");
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vwopen
**
** This is an interface to the Vinculum "OPW" command
** (Open File For Write).
**
** Open a file for writing, including a user-specified
** time/date stamp.
**
** Returns:
**		0 normal
**		-1 on error
**
********************************************************/
vwopen(s)
char *s;
{
	/* as a safety measure, close any open file */
	vclf();
	
	str_send("opw ");
	str_send(s);
	str_send(td_string);
	str_send("\r");
	
	/* allow a little extra time if new file */
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vseek
**
** This is an interface to the Vinculum "SEK" command
** (Seek).
**
** This routine seeks to an absolute offset position
** in an open file. 
**
********************************************************/
vseek(p)
int p;
{
	static char fpos[7];
	
	str_send("sek ");
	str_send(itoa(p, fpos));
	str_send("\r");
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vclose
**
** This is an interface to the Vinculum "CLF" command
** (Close File).
**
** This closes the specified file on the USB device.
** This should be called after a vropen() and vread()
** sequence, or a vwopen() and vwrite() sequence of calls.
**
********************************************************/
vclose(s)
char *s;
{
	str_send("clf ");
	str_send(s);
	str_send("\r");
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vclf
**
** This is an interface to the Vinculum "CLF" command
** (Close File).
**
** This closes the currently open file on the USB device.
** This should be called after a vropen() and vread()
** sequence, or a vwopen() and vwrite() sequence of calls.
**
********************************************************/
vclf()
{
	str_send("clf\r");
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vipa
**
** This is an interface to the Vinculum "IPA" command
** (Input In ASCII).
**
** This routine ensures that the monitor’s mode for 
** inputting and displaying values is printable ASCII
** characters. 
**
********************************************************/
vipa()
{
	str_send("ipa\r");
	return vprompt(MAXWAIT);
}

/********************************************************
**
** vread
**
** This is an interface to the Vinculum "RDF" command
** (Read From File).
**
** This routine should only be called after a successful
** call to vropen(), which opens a file on the device
** for reading.  This routine reads n bytes from the file
** on the USB device, storing them in the provided buffer.
** Once all bytes have been written it then waits for the
** VDIP to reply with the D:\> prompt which is read and
** discarded.
**
** The bytes are read one at a time and a timeout value
** (MAXWAIT) determines how long to wait before assuming
** the command failed.
**
** Returns:
**		0 on Success
**		-1 on Error
**
********************************************************/
vread(buff, n)
char *buff;
int n;
{
	int i, ioresult, rc;
	static char fsize[7];

	rc = 0;
	
	/* send read from file (RDF) command */
	str_send("rdf ");
	str_send(itoa(n, fsize));
	str_send("\r");
	
	/* now capture the result in the buffer */
	for (i=0; i<n ; i++) {
		if ((ioresult = in_vwait(MAXWAIT)) != -1)
			buff[i] = ioresult;
		else {
			printf("vread: timed out reading data\n");
			return -1;
		}
	}

	return vprompt(MAXWAIT);
}

/********************************************************
**
** vwrite
**
** This is an interface to the Vinculum "WRF" command
** (Write To File).
**
** This routine should only be called after a successful
** call to vwopen(), which opens a file on the device
** for writing.  This routine writes n bytes from the
** provided buffer to the file on the USB device.  Once
** all bytes have been written it then waits for the
** VDIP to reply with the D:\> prompt which is read and
** discarded
**
** Returns:
**		0 on Success
**		-1 on Error
**
********************************************************/
vwrite(buff, n)
char *buff;
int n;
{
	int i, rc;
	static char wsize[7];

	rc = 0;
	
	/* write to file (WRF) command */
	str_send("wrf ");
	str_send(itoa(n, wsize));
	str_send("\r");
	
	/* now output the n bytes to the device */
	for (i=0; i<n; i++) {
		out_v(*buff++);
	}

	return vprompt(MAXWAIT);
}

/********************************************************
**
** gethexvals
**
** Scan a string for values of the form $xx where xx
** is a hexadecimal upper case string, e.g. $34 $96 $8C ...
** Place these sequentially into the array of bytes provided.
** Search for at most n values and return the number found.
**
** NOTE: very minimal error checking is done!
**
********************************************************/
gethexvals(s, n, val)
char *s;
int n;
char val[];
{
	int i;
	
	for (i=0; i<n ; i++) {
		while ((*s!='$') && (*s!=0))
			++s;
		if (*s==0)
			return i;
		else
			val[i] = hexval(++s);
	}
	return i;
}

/********************************************************
**
** hexval
**
** Turn a two-byte upper case ASCII hex string into its
** binary representation.
**
** NOTE: no error checking is done (e.g. for non-hexadecimal
** characters).
**
********************************************************/
hexval(s)
char *s;
{
	int n1, n2;
	
	n1 = *s++ - '0';
	if (n1>9)
		n1 -= 7;
	n2 = *s - '0';
	if (n2>9)
		n2 -= 7;
	return (n1<<4) + n2;
}

/********************************************************
**
** wait
**
** Wait the specified number of seconds.
**
** NOTE: Intended for relatively small values.  Values over 
** 32 will yield unexpected results but no range checking
** is performed.
**
********************************************************/
wait(seconds)
unsigned seconds;
{
	mswait(seconds*1000);
}

/********************************************************
**
** mswait
**
** Wait the specified number of milliseconds
**
********************************************************/
mswait(milliseconds)
unsigned milliseconds;
{
	/* 2 ms per tick */
	timeout = *Ticptr + (milliseconds >> 1);
	while (*Ticptr != timeout)
		/* do nothing, just wait... */;
}
