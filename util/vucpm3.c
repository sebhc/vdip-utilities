/********************************************************
** vutil.c - CP/M 3 version
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
** The following utility routines are 0000000000000defined here (see
** comments in the code below for details on usage):
**
**  str_send()
**  str_rdw()
**  in_v()
**  out_v()
**  in_vwait()
**  out_vwait()
**  vfind_disk()
**  vpurge()
**  vhandshake()
**  vinit()
**  vsync()
**  vdirf()
**  vdird()
**  vprompt()
**  vropen()
**  vwopen()
**  vseek()
**  vclose()
**  vclf()
**  vipa()
**  vread()
**  vwrite()
**  prndate()
**  prntime()
**  vcd()
**  vcdroot()
**  vcdup()
**  bdoshl()
**  btod()
**  dtob()
**  gethexvals()
**  hexval()
**
** The typical calling sequence is as follows: vinit() is
** called first to ensure communication and put the device
** in a known state, then vfind_disk() is used to ensure that
** a storage device is attached.  Directory level can be
** changed with vcd() and vcdroot(). Files can then be opened
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
**
** Release: September, 2017
**
**	Updated to support CP/M3	May, 2019 (gfr)
**
**	Glenn Roberts
**	glenn.f.roberts@gmail.com
**
********************************************************/
#include "fprintf.h"

/* CP/M 3 BDOS functions */
#define	GETCS	0x0B
#define GETSCB	0x31
#define GETDT   0x69
#define GETSDA	0x9a	/* MP/M System Data Area */

/* Offsets in the SCB */
#define	SOSEC	0x5C		/* seconds count in clock */
#define	SOSCB	0x3A		/* address of SCB itself */

/* default max time (sec) to wait for response */
#define	MAXWAIT	5

#define	TRUE	1
#define	FALSE	0

/* FTDI VDIP bits */
#define VTXE	004			/* TXE# when hi ok to write */
#define VRXF	010			/* RXF# when hi data avail  */

/* standard VDIP command prompt */
#define PROMPT	"D:\\>"
#define CFERROR "Command Failed"

/* BDOS call 49 - get/set System Control Block parameters */
struct scbstruct {
	char	offset;
	char	set;
	int		value;
} scbs;

/* union used to dissect long (4 bytes) into pieces */
union u_fil {
	long l;		/* long            */
	unsigned i[2];	/* 2 unsigned ints */
	char b[4];	/* same as 4 bytes */
};

/* data structure for GETDT bdos call */
struct timedate {
	int days;
	char hour;
	char minute;
} td;

/* hex value of time/date kept here */
char td_string[15];

/* I/O line buffer 		*/
char linebuff[128];	

/* pointer to the one-second tick counter in CP/M 3 */
char *secp;

/* USB i/o ports (allocated in calling program )*/
extern int p_data;		/* USB data port */
extern int p_stat;		/* USB status port */

/* template for non-int function */
char *itoa();

/********************************************************
**
** str_send
**
** Output a null-terminated string to the VDIP device.
** If you want a carriage return to be sent it must be
** in the string (\r).
**
** Return:
**		0	Success
**		-1	I/O error
**
********************************************************/
str_send(s)
char *s;
{
	char *c;
	int rc;
	
#ifdef DEBUG
	printf("->str_send %s\n", s);
#endif
	rc = 0;

	for (c=s; ((*c!=0) && (rc!=-1)); c++)
		rc = out_vwait(*c, MAXWAIT);
	
	return rc;
}

/********************************************************
**
** str_rdw
**
** "Read with wait" - Used to read a string from the VDIP
** device but detect hung conditions by monitoring the time.
** Read and consume characters up to and including the character 
** specified in 'tchar', but wait no longer than the globally
** specified maximum time MAXWAIT for each byte.
**
** Returns:
**		0 if read was successful
**		-1 if read timed out.
**
********************************************************/
str_rdw(s, tchar)
char *s;
char tchar;
{
	int c, rc;
	int timedout;
	
	timedout = FALSE;
	rc = 0;

	do {
		if((c = in_vwait(MAXWAIT)) == -1)
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
** out_v
**
** Send a character to the VDIP1 via the specified port.
** Performs I/O handshaking with the VDIP1 device.
**
********************************************************/
out_v(c)
char c;
{
	/* Wait for ok to transmit (VTXE high) */
	while ((inp(p_stat) & VTXE) == 0)
		;
	/* OK to transmit the character */
	outp(p_data,c);
}


/********************************************************
**
** in_vwait
**
** "Input with Wait" - Input a character from the VDIP1 but 
** detect hung conditions by monitoring the time. Wait no 
** longer than t seconds.
**
** Returns:
**		Character read if successful
**		-1 if timed out
**
********************************************************/
in_vwait(t)
int t;
{
	int seconds, countdown;
	
	countdown = t;
	
	while(countdown > 0) {
        /* snapshot the real time clock seconds count */
		seconds = *secp;
		/* now wait for port activity or clock tick ... */
		while (seconds==*secp) {
			/* check for port activity and if so
			** then read the data and return
			*/
			if (inp(p_stat) & VRXF)
				return inp(p_data);
		}
		/* if we fall through it means the clock
		** ticked. Continue the countdown...
		*/
		--countdown;
	}

	/* Timed out! return -1 */
	return -1;
}

/********************************************************
**
** out_vwait
**
** Send a character to the VDIP1 via the specified port,
** but detect hung conditions by monitoring the time.
** Wait no more than t seconds.
**
** Returns:
**		0	if successful
**		-1	if timed out
**
********************************************************/
out_vwait(c,t)
char c;
int t;
{
	int seconds, countdown;

	countdown = t;
	
	while(countdown > 0) {
        /* snapshot the real time clock seconds count */
		seconds = *secp;
		/* now wait for port activity or clock tick ... */
		while (seconds==*secp) {
			/* check for ok to transmit (VTXE high)
			** and if so then transmit and finish
			*/
			if (inp(p_stat) & VTXE) {
				/* OK to transmit the character */
				outp(p_data,c);
				return 0;
			}
		}
		/* if we fall through it means the clock
		** ticked. Continue the countdown...
		*/
		--countdown;
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
	
	return vprompt();
}

/********************************************************
**
** vpurge
**
** Read all the pending data from the VDIP device and 
** throw it away.  Wait for up to 1 second each time
** before deciding we're done. 
**
********************************************************/
vpurge()
{
	int c;
	
	do {
		c = in_vwait(1);
	} while (c != -1);
}

/********************************************************
**
** vhandshake
**
** This routine checks to see if two-way communication with 
** the VDIP Command Monitor is working. It sends an ASCII 'E'
** and waits up to MAXWAIT for the 'E' to be echoed back.
**
** Returns:
**		0	Success
**		-1	Error, timed out or no response
**
********************************************************/
vhandshake()
{
	int rc;
	
	/* Attempt to send an "E" */
	if (str_send("E\r") == -1)
		/* time out on send! */
		rc = -1;
	else if (str_rdw(linebuff, '\r') == -1)
		/* time out on reading response! */
		rc = -1;
	else if (strcmp(linebuff,"E") != 0)
		/* wrong response! */
		rc = -1;
	else
		/* success! */
		rc = 0;

	return rc;
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
	int *tmp;

	/* Set up a pointer to the one-second tick counter
	** maintained by CP/M 3 in the System Control Block.
	** we'll use this to test for timeouts when reading
	** data.
	*/
	scbs.offset = SOSCB;	/* offset for SCB address (undocumented) */
	scbs.set	= 0;		/* do a "get" not a "set" */
	
    /* Find SCB address; add offset to seconds location */
    if ((bdoshl(12, 0) & 0x0100) != 0) { /* MP/M */
	tmp = (int *)bdoshl(GETSDA, 0);
	tmp = tmp[126]; /* offset 252: XDOS internal data */
	secp = (char *)tmp + 4;
    } else {	/* CP/M 3 */
        secp = (char *)bdoshl(GETSCB, &scbs) + SOSEC;
    }
	rc = 0;
	
	/*first try to talk to the device */
	if (vsync() == -1)
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
**		0 	successful synchronization
**		-1	can't sync with device
**
********************************************************/
vsync()
{
	int i, rc;
	
	rc = -1;
	
	/* try up to 3 times to sync */
	for (i=0; i<3; i++) {
		/* first purge any waiting data */
		vpurge();

		/* now attempt two-way communication */
		if (vhandshake()==0) {
			/* we're talking! */
			rc = 0;
			break;
		}
	}
	
	return rc;
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
	str_rdw(linebuff, '\r');
	
	/* the result will either be the file name or
	** "Command Failed". if the latter then return error.
	*/
	str_rdw(linebuff, '\r');

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
		str_rdw(linebuff, '\r');
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
	str_rdw(linebuff, '\r');
	
	/* result will either be the file name followed
	** by 10 bytes, or "Command Failed".
	*/
	str_rdw(linebuff, '\r');

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
		str_rdw(linebuff, '\r');
	}
	return rc;
}


/********************************************************
**
** vprompt
**
** check for "D:\>" prompt. 
**
** Returns:
**		0 Normal
**		-1 on error (no prompt or timeout)
**
********************************************************/
vprompt()
{
#ifdef DEBUG
	printf("->vprompt\n");
#endif

	/* check for normal prompt return (return if timeout) */
	if (str_rdw(linebuff, '\r') == -1)
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
	return vprompt();
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
	return vprompt();
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
	return vprompt();
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
	return vprompt();
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
	return vprompt();
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
	return vprompt();
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
	int i;
	char *nxt;
	static char fsize[7];
	
#ifdef DEBUG
	printf("->vread\n");
#endif
	/* send read from file (RDF) command */
	str_send("rdf ");
	str_send(itoa(n, fsize));
	str_send("\r");
	
	/* immediately capture the result in the buffer */
	nxt=buff;
	for (i=0; i<n ; i++) {
		/* wait for RX flag, then read the byte */
		while ((inp(p_stat) & VRXF) == 0)
			; /* wait... */
		*nxt++ = inp(p_data);
	}
#ifdef DEBUG
    printf("%d bytes read\n", n);
#endif

	return vprompt();
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

	return vprompt();
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
** vcd
**
** Change to the specified directory, relative to the
** current location.  Levels can be changed only one
** at a time.
**
** Return:
**	0	= success
**	-1	= fail
**
********************************************************/
vcd(dir)
char *dir;
{
	int rc;
	
	rc = 0;
	
	str_send("cd ");
	str_send(dir);
	str_send("\r");
	
	/* The result will either be the Prompt or an error
	** message. If the latter then return error.
	*/
	str_rdw(linebuff, '\r');

	if (strcmp(linebuff, PROMPT) != 0) {
		printf("CD %s: %s\n", dir, linebuff);
		rc = -1;
	}
	
	return rc;
}

/********************************************************
**
** vcdroot
**
** Change the directory to be root ('/').  This is done
** by issuing successive "CD .." commands until the command
** fails.
**
********************************************************/
vcdroot()
{
	while(vcdup() == 0)
		;
}

/********************************************************
**
** vcdup
**
** Change the directory up one level by issuing 
** the "CD .." command
**
** Returns:
**		0 if success
**		-1 if fail (i.e. at "root" level)
**
********************************************************/
vcdup()
{
	int rc;
	
	rc = 0;
	
	str_send("cd ..\r");
	
	/* The result will either be the Prompt or
	** "Command Failed". If the latter then return error.
	*/
	str_rdw(linebuff, '\r');

	if (strcmp(linebuff, CFERROR) == 0) {
		/* flag an error! */
		rc = -1;
	}
	
	return rc;
}

/********************************************************
**
** bdoshl - Call CP/M BDOS function with given values
**		for registers C and DE.  Return the value
**		from the HL register.
**
********************************************************/
bdoshl() {
#asm
        POP     H
        POP     D
        POP     B
        PUSH    B
        PUSH    D
        PUSH    H
        CALL    5
#endasm
}

/********************************************************
**
** btod
**
** convert a BCD byte to its decimal equivalent
**
********************************************************/
btod(b)
char b;
{
	return ((b & 0xF0) >> 4) * 10 + (b & 0x0F);
}

/********************************************************
**
** dtob
**
** convert a Decimal byte to its BCD equivalent
**
********************************************************/
dtob(b)
char b;
{
	return ((b/10) << 4) | (b % 10);
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
** strrchr
**
** Return pointer to last occurrence of character ch in
** string str.  Return NULL if no occurrence. 
**
********************************************************/
strrchr(str, ch)
char *str;
char ch;
{
	char *s;
	int l;
	
	l = strlen(str);
	s = str + strlen(str);
	
	do {
		--s;
		if (*s == ch)
			return s;
	} while (s != str);
	
	/* fell through - no match */
	return 0;
}
