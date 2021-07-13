/* vutil.c -- These are utility routines used to access
** the Vinculum VDP-1 interfaced in parallel FIFO mode.
**
** Can be compiled for HDOS or CP/M (#define HDOS
** or CPM)
**
** Compiled with Software Toolworks C/80 V. 3.0
**
** Glenn Roberts 11 May 2013
*/

/* tick counter RAM location */
#ifdef HDOS
#define TICCNT	0x201B
#endif

#ifdef CPM
#define TICCNT	0x000B
#endif

/* FTDI VDIP ports and bits */
#define VDATA	0261		/* FTDI Data Port	*/
#define VSTAT	0262		/* FTDI Status Port	*/
#define VTXE	004			/* TXE# when hi ok to write */
#define VRXF	010			/* RXF# when hi data avail  */

#define PROMPT	"D:\\>"		/* standard VDIP command prompt */
#define CFERROR "Command Failed" /* command failed error msg	*/
#define	MAXD	256			/* maximum number of dir entries */
#define	MAXWAIT	1000		/* max time (ms) to wait for prompt */

/* union used to dissect long (4 bytes) into pieces */
union u_fil {
	long l;		/* long            */
	unsigned i[2];	/* 2 unsigned ints */
	char b[4];	/* same as 4 bytes */
};

/* file directory entry data structure */
struct finfo {
	char name[9];
	char ext[4];
	int isdir;
	long size;
	long mdate;
};

/*** Global static storage ***/

/* i/o line buffer */
char linebuff[128];

/* pointer to 2-ms clock */
unsigned *Ticptr = TICCNT;
unsigned timeout;

/* array of pointers to directory entries */
struct finfo *direntries[MAXD];
int nentries;

/* wait - wait a specified number of seconds */
wait(seconds)
unsigned seconds;
{
	mswait(seconds*1000);
}

/* mswait - wait a specified number of milliseconds */
mswait(milliseconds)
unsigned milliseconds;
{
	/* 2 ms per tick */
	timeout = *Ticptr + (milliseconds >> 1);
	while (*Ticptr != timeout)
		/* do nothing, just wait... */;
}

/* str_send - output a null-terminated string to the VDIP device
** if you want a carriage return to be sent it must be in the
** string (\r).
*/
str_send(s)
char *s;
{
	char *c;

#ifdef DEBUG
	printf("sending: ");
	for (c=s; *c!=0; c++)
		printf(" %02x",*c);
	printf("\n");
#endif

	for (c=s; *c!=0; c++)
		out_vdip(*c);
}

/* str_flush - read and discard bytes up to and including
** specified character. 
*/
str_flush(c)
int c;
{
	int i;
	
	for (i=0; (in_vdip() != c); i++)
		/* loop... */;
}

/* str_read - read a string from the VDIP device. Read and
** consume characters up to and including the character
** specified in 'tchar'.
*/
str_read(s, tchar)
char *s;
char tchar;
{
	char c;
	
	do {
		while((c = in_vdip()) == -1)
			;
		/* replace tchar with NULL */
		if (c == tchar)
			c = 0;
		*s++ = c;
	} while (c != 0);
}

/* str_rdw - read a CR-terminated string from the VDIP device 
** but wait no longer than a specified time for each
** byte, t specified in milliseconds.  return 0 if read
** or -1 if timed out.
*/
str_rdw(s, tchar, t)
char *s;
char tchar;
unsigned t;
{
	int c;
	int timedout;
	
	timedout = FALSE;
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
		return -1;
	}
	else
		return 0;
}


/* out_vdip - send a character to the VDIP-1 via the
** specified port
*/
out_vdip(c)
{
	/* Wait for ok to transmit */
	while ((inp(VSTAT) & VTXE) == 0)
		/* while bit is low just wait ... */
		;
	/* OK to transmit the character */
	outp(VDATA,c);
}

/* in_vdip - return a character from the
** VDIP-1 or -1 if none available
*/
in_vdip()
{
	/* check for Data Ready */
	if ((inp(VSTAT) & VRXF) != 0)
		/* read the character from the port and return it */
		return inp(VDATA);
	else
		/* return -1 if no data available */
		return -1;
}

/* in_vwait - read a character from the VDIP-1, waiting
** up to the specified time (in milliseconds).  return the
** character read, or -1 if timed out.
*/
in_vwait(milliseconds)
unsigned milliseconds;
{
	/* 2 ms per tick */
	timeout = *Ticptr + (milliseconds >> 1);

	while (*Ticptr != timeout) {
		/* check for Data Ready */
		if ((inp(VSTAT) & VRXF) != 0)
			/* Have data! read the byte and return it */
			return inp(VDATA);
	}
	/* Timed out! return -1 */
	return -1;
}

/* outp - output a byte to a specified port number */
outp(port,c)
{
#asm
	POP	H		; return address
	POP	D		; char to output
	POP	B		; port to use
	PUSH	B		; fix the stack...
	PUSH	D
	PUSH	H
	MOV	A,C		; port
	STA	OPORT		; patch the OUT port
	MOV	A,E		; Char to output
OPORT	EQU	*+1
	OUT	0		; port gets patched dynamically
#endasm
}

/* inp - input a byte from a specified port number */
inp(port)
{
#asm
	POP	H		; return address
	POP	B		; port to use
	PUSH	B
	PUSH	H
	MOV	A,C		; port
	STA	IPORT		; patch the IN port
IPORT	EQU	*+1
	IN	0		; port gets patched dynamically
	MOV	L,A
	MVI	H,0		; return with result in HL
#endasm
}

/* vfind_disk - If there is a drive available then
** a \r will cause the prompt to come back, so simply
** send \r and return the result (-1 on error).
*/
vfind_disk()
{
	/* prompt with carriage return and read response */
	str_send("\r");
	
	return vprompt(MAXWAIT);
}

/* vpurge - read all the pending data from the VDIP device
** and throw it away.  Wait for up to m milliseconds each
** time before deciding we're done.
*/
vpurge(m)
unsigned m;
{
	int c;
	
	do {
		c = in_vwait(m);
	} while (c != -1);
}

/* vhandshake - send a 'E' character and see if it is
** echoed back.  If so return TRUE (have handshake)
** else FALSE.
*/
vhandshake()
{
	str_send("E\r");
	str_read(linebuff, '\r');
	if (strcmp(linebuff,"E") == 0)
		return TRUE;
	else
		return FALSE;
}


/* vinit - initialize the VDIP connection.
** this routine tests for the presence of the
** device, does a synchronization to a known
** condition and issues any setup commands to
** initialize the desired settings.  returns
** -1 if any errors encountered, otherwise 0.
*/
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


/* vsync - return TRUE after synchronization 
** first flush any waiting input, then send an "E"
** and see if an "E" is sent back (sync!).  Return
** FALSE if something goes wrong and can't get synced.
*/
vsync()
{
	int i, done;
	
	/* try up to 10 times to sync */
	for (i=0, done=FALSE; ((i<10) && (!done)); i++) {
		/* first purge any waiting data wait only for
		** a short time... (50 ms)
		*/
		vpurge(MAXWAIT);
	
		if (vhandshake())
			/* we're done! */
			done = TRUE;
		else
			/* sometimes it takes more time, try 2s ... */
			wait(2);
	}
	return done;
}

/* vdir1 - This routine does "pass 1" of the directory
** using the 'dir' command fill out the array of directory 
** entries (direntries) by allocating space for each one
*/
vdir1()
{
	int i, ind, done;
	struct finfo *entry;
	
	/* Issue directory command */
	str_send("dir\r");
	
	/* the first line is always blank - toss it! */
	str_read(linebuff, '\r');

	done = FALSE;
	nentries = 0;
	
	/* read each line and add it to the list,
	** when the D:\> prompt appears, we're done. the
	** list entries are dynamically allocated.
	*/
	do {
		str_read(linebuff, '\r');
		if (strcmp(linebuff, "D:\\>") == 0)
			done = TRUE;
		else if ((entry = sbrk(23)) == -1)
			printf("error allocating directory entry!\n");
		else {
			/* process directory entry */
			entry->name[0] = 0;
			entry->ext[0] = 0;
			entry->isdir = FALSE;
			if ((ind=index(linebuff, " DIR")) != -1) {
				/* have a directory entry */
				entry->isdir = TRUE;
				linebuff[ind] = 0;
				strcpy(entry->name, linebuff);
			} else if ((ind=index(linebuff, ".")) != -1) {
				/* NAME.EXT filename */
				linebuff[ind] = 0;
				strcpy(entry->name, linebuff);
				strcpy(entry->ext, linebuff+ind+1);
			} else {
				/* NAME filename */
				strcpy(entry->name, linebuff);
			}
			/* now store the entry and bump the count */
			direntries[nentries++] = entry;
		}
	} while (!done);
}

/* dirstr - return a directory entry as a string 
** this routine essentially concatenates the name
** and extension portions with a '.' in the middle
** returning the result as a nul-terminated string.
*/
dirstr(e, s)
int e;
char *s;
{
	strcpy(s,direntries[e]->name);
	if (direntries[e]->ext[0] != 0) {
		strcat(s,".");
		strcat(s,direntries[e]->ext);
	}
}


/* gethexvals - scan a string for values of the form
** $xx where xx is a hexadecimal upper case string,
** e.g. $34 $96 $8C ...  place these sequentially
** into the array of bytes provided.  Search for at
** most n values and return the number found.  only
** minimal error checking is done...
*/
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

/* hexval - turn a two-byte upper case ASCII hex string
** into its binary representation
*/
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


/* vdirf - issue "DIR" command for specified file name 
** to obtain the file size, which is returned as a long
** in 'len'.  Returns -1 if error, 0 otherwise.
*/
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

/* vdird - issue "DIRT" command for specified file name 
** to obtain the last modified date and time, which is
** returned as a 32 bit value (long).
*/
vdird(s, ldate)
char *s;
long *ldate;
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

		/* return file size */
		*ldate = fdate.l;
		
		/* success - gobble up the prompt */
		str_read(linebuff, '\r');
	}
	return rc;
}

/* prndt - expand date and time fields from a 32 bit entry
** and print the result.
*/
prndt(ldt)
long ldt;
{
	static union u_fil fdt;
	unsigned mo, dy, yr, hr, min, sec;
	char *am_pm;
	
	fdt.l = ldt;
	
	sec = 2*(fdt.i[0] & 0x1f);
	min = (fdt.i[0] >> 5) & 0x3f;
	hr  = (fdt.i[0] >> 11) & 0x1f;
	if (hr > 12) {
		am_pm = "PM";
		hr = hr - 12;
	}
	else
		am_pm = "AM";
	
	dy = fdt.i[1] & 0x1f;
	mo = (fdt.i[1] >> 5) & 0xf;
	yr = 1980 + ((fdt.i[1] >> 9) & 0x7f);
	
	
	printf("%2d/%02d/%02d %2d:%02d %s", 
		mo, dy, (yr % 100), hr, min, am_pm);
}

/* vprompt - check for "D:\>" prompt. wait 'ms' milliseconds
** before declaring a timeout.  return -1 on error */
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

/* vropen - open a file for reading.  return -1 on error
*/
vropen(s)
char *s;
{
	str_send("opr ");
	str_send(s);
	str_send("\r");
	return vprompt(MAXWAIT);
}

/* vwopen - open a file for writing.  return -1 on error
** (note: in future add date stamp information)
*/
vwopen(s)
char *s;
{
	str_send("opw ");
	str_send(s);
	str_send("\r");
	
	/* allow a little extra time if new file */
	return vprompt(MAXWAIT);
}

/* vseek - set absolute position of file pointer
*/
vseek(p)
int p;
{
	static char fpos[7];
	
	/* use sprintf due to bug in C80 itoa function! */
	sprintf(fpos, "%d", p);

	str_send("sek ");
	str_send(fpos);
	str_send("\r");
	return vprompt(MAXWAIT);
}

/* vclose - close a previously opened file.  file name
** must be same as used in open command.  return -1 on error
*/
vclose(s)
char *s;
{
	str_send("clf ");
	str_send(s);
	str_send("\r");
	return vprompt(MAXWAIT);
}

/* vclf - close any file that is open */
vclf()
{
	str_send("clf\r");
	return vprompt(MAXWAIT);
}

/* vipa - set input to ASCII mode */
vipa()
{
	str_send("ipa\r");
	return vprompt(MAXWAIT);
}


/* vread - read n bytes from vinculum channel into the buffer
** and consume the D:\> prompt that follows. return -1
** on error.  This command requires the VDIP to be in
** ASCII input/output mode (IPA).
*/
vread(buff, n)
char *buff;
int n;
{
	int i, ioresult, rc;
	static char fsize[7];

	rc = 0;
	
	/* use sprintf due to bug in C80 itoa function! */
	sprintf(fsize, "%d", n);
	
	/* send read from file (RDF) command */
	str_send("rdf ");
	str_send(fsize);
	str_send("\r");
	
	/* now capture the result in the buffer */
	for (i=0; i<n ; i++) {
		if ((ioresult = in_vwait(1000)) != -1)
			buff[i] = ioresult;
		else {
			printf("vread: timed out reading data\n");
			return -1;
		}
	}

	return vprompt(MAXWAIT);
}

/* vwrite - write n bytes from buffer to vinculum channel
** and consume the D:\> prompt that follows. return -1
** on error.  This command requires the VDIP to be in
** ASCII input/output mode (IPA).
*/
vwrite(buff, n)
char *buff;
int n;
{
	int i, rc;
	static char wsize[7];

	rc = 0;
	
	/* use sprintf due to bug in C80 itoa function! */
	sprintf(wsize, "%d", n);
	
	/* write to file (WRF) command */
	str_send("wrf ");
	str_send(wsize);
	str_send("\r");
	
	/* now output the n bytes to the device */
	for (i=0; i<n; i++) {
		out_vdip(*buff++);
	}

	return vprompt(MAXWAIT);
}

/* commafmt - create a string containing the representation
** of a long with commas every third position.  caution:
** len must be guaranteed big enough to hold any long.
*/
commafmt(n, s, len)
long n;
char *s;
int len;
{
	char *p;
	int i;

	/* work backward from end of string */
	p = s + len - 1;
	*p = 0;

	i = 0;
	do {
		if(((i % 3) == 0) && (i != 0))
			*--p = ',';
		*--p = '0' + (n % 10);
		n /= 10;
		i++;
	} while(n != 0);

	/* pad the front with blanks */
	while (p != s)
		*--p = ' ';
}
