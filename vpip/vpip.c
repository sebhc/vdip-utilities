/********************************************************
** vpip - VDIP-1 utility fashioned after the the HDOS
** "peripheral interchange program" (PIP).  This
** utility reads and writes files to/from a USB device
** interfaced through the VDIP-1 usb breakout board.
**
** The program works in three phases: first, it builds
** a directory (in dynamically-allocated RAM) of the contents
** of the specified source device (either the USB device or
** the specified HDOS file device); second, it goes through
** the directory data structure and tags the files that
** match the user's file specification; and finally, it
** processes the command (e.g. either copying the specified
** files or listing their directory information).
**
** The program also tests for the presence of a real time clock
** card and, if found, uses that time and date as appropriate
** in file operations.
**
** Usage: VPIP {command}
**    or
** VP89 {command} (H89 version)
**
** where command is of the form:
**
**	DEST=SOURCE1,...SOURCEn/SWITCH1.../SWITCHn
**
** if nothing is specified on the command line then the
** utility goes into a line-by-line command mode by issuing
** the :V: prompt.
**
** On an H89 the executable file must be named "VP89" in order
** for the software to allocate the appropriate port number.
**
** The pseudo-drive specification "USB:" is used to designate
** the USB-attached device.  HDOS style device specifications
** (e.g. SY0:, DK1:, etc.) denote system drives.
**
** Files may only be copied between the USB device and the
** system devices.   USB to USB, and system to system copies
** are not supported.
**
**	Glenn Roberts
**	March 2019
**
**	Revisions
**	0.9.01		Added /T switch
**				Added option for H89 compile
**	0.9.02		Added /D switch (debugging)
**				Added vclf() before any file open or close
**				Fixed bug - not initializing properly
**	0.9.03		Automatically set ports based on ABS filename
**	0.9.04		Fixed bug in listmatch()
**	0.9.10		Code cleanup & comment; separate compiled modules
**
********************************************************/

#include "fprintf.h"

#define	TRUE	1
#define	FALSE	0

#define	NUL		'\0'
#define	NULSTR	""

/* default devices if none specified */
#define	SYSDFLT	"SY0"
#define	USBDFLT	"USB"

/* device types set by checkdev() */
#define	NULLD	0			/* No device given */
#define	STORD	1			/* storage device */
#define	USERD	2			/* user io device */
#define	USBD	3			/* USB device */
#define	UNKD	4			/* unknown format */

#define MAXD	256			/* maximum number of directory entries */
#define DIRBUFF 512			/* buffer space for directory */

/* default max time (ms) to wait for prompt */
#define	MAXWAIT	1000

/* Clock port and register offsets */
#define CLOCK	0240		/* Clock Base Port	*/
#define	S1		0			/* Seconds register	*/
#define	S10		1			/* Tens of Seconds	*/
#define	MI1		2			/* Minutes register */
#define	MI10	3			/* Tens of Minutes	*/
#define	H1		4			/* Hours register	*/
#define	H10		5			/* Tens of Hours	*/
#define	D1		6			/* Days register	*/
#define	D10		7			/* Tens of Days		*/
#define	MO1		8			/* Months register	*/
#define	MO10	9			/* Tens of Months	*/
#define	Y1		10			/* Years register	*/
#define	Y10		11			/* Tens of Years	*/
#define	W		12			/* Day of week reg.	*/

/*********************************************
**
**	Key Data Structures
**
*********************************************/
/* union used to dissect long (4 bytes) into pieces */
union u_fil {
	long l;			/* long            */
	unsigned i[2];	/* 2 unsigned ints */
	char b[4];		/* same as 4 bytes */
};

/* destination file specification (name and
** extension are one extra character to
** accommodate terminating NUL
*/
struct fspec {
	char fname[9];
	char fext[4];
} dstspec;

/* HDOS file directory entry data structure */
struct hdinfo {
	char hdname[8];
	char hdext[3];
	char prj;
	char ver;
	char clf;
	char flags;
	char rsvd;
	char fgn;
	char lgn;
	char lsi;
	int credate;
	int moddate;
} hdosentry;

/* Internally-used file directory data structure.
** The 'tag' field is used to flag entries that
** match the user-specified criteria.
*/
struct finfo {
	char name[9];
	char ext[4];
	int isdir;
	long size;
	unsigned mdate;
	unsigned mtime;
	char tag;
}fentry;

/* Data structure to hold results from real-time
** clock query
*/
struct datetime {
	char day;
	char month;
	char year;
	char hours;
	char minutes;
	char seconds;
	char dow;
} mydate;

/*********************************************
**
**	Global Static Storage
**
*********************************************/

/* pointers to Active IO Area */
char *aiospg  = 0x2126;
char **aiogrt = 0x2124;

/* declared in vutil library */
extern char linebuff[128];		/* I/O line buffer 		*/
extern char td_string[15];		/* time/date hex value */

/* grt (for HDOS directory manipulation) */
char *grt;				/* Group Reservation Table */

/* timeout parameter for VDIP protocol */
int vmaxw;

/* flag if real time clock detected */
int havertc;

char cmdline[80];
char dstfname[15];
char srcfname[15];

/* devices for source and destination */
char srcdev[4], dstdev[4];
int srctype, dsttype;

/* array of pointers to source filespecs */
struct fspec *src[MAXD];
int nsrc;

/* array of pointers to directory entries */
struct finfo *direntry[MAXD];
int nentries;

/* buffer space for reading directory */
char buffer[DIRBUFF];

/* buffer used for read/write */
#define BUFFSIZE	256
char rwbuffer[BUFFSIZE];

/* global switch settings */
int f_list;		/* to list directory, long form */
int f_brief;	/* to list directory, short form */
int f_help;		/* print help */
int f_debug;	/* debug mode */

/* FTDI VDIP ports and bits */
#define H8DATA	0261		/* FTDI Data Port, H8 		*/
#define H8STAT	0262		/* FTDI Status Port, H8		*/
#define H89DATA	0331		/* FTDI Data Port, H89 		*/
#define H89STAT	0332		/* FTDI Status Port, H89 	*/

/* i/o ports - must be global, used by vutil utilities */
int p_data;		/* USB data port */
int	p_stat;		/* USB status port */

/*********************************************
**
**	Time and Date Manipulation Functions
**
*********************************************/

/* readdate - reads the date information and fills out the
** supplied date structure
*/
readdate(d)
struct datetime *d;
{
	/* read from registers - all are 4 bits (mask to lower 4) */
	d->seconds = (inp(CLOCK+S1)  & 0x0F) + 10 * (inp(CLOCK+S10)  & 0x0F);
	d->minutes = (inp(CLOCK+MI1) & 0x0F) + 10 * (inp(CLOCK+MI10) & 0x0F);
	d->hours   = (inp(CLOCK+H1)  & 0x0F) + 10 * (inp(CLOCK+H10)  & 0x0F);
	d->day     = (inp(CLOCK+D1)  & 0x0F) + 10 * (inp(CLOCK+D10)  & 0x0F);
	d->month   = (inp(CLOCK+MO1) & 0x0F) + 10 * (inp(CLOCK+MO10) & 0x0F);
	d->year    = (inp(CLOCK+Y1)  & 0x0F) + 10 * (inp(CLOCK+Y10)  & 0x0F);
	d->dow     = (inp(CLOCK+W)   & 0x0F);
}

/*********************************************
**
**	Utility Functions
**
*********************************************/

/* gettd - reads time/date from real time clock and turns it
** into a string in Hex format that is compatible with VDIP/FAT
** date format, stored in the global td_string
*/
gettd(d)
struct datetime *d;
{
	static unsigned utime, udate;
	
	/* set date structure to current time & date */
	readdate(d);
	
	/* create the appropriate hexadecimal ASCII string */
	utime = (d->seconds/2) + (d->minutes << 5) + (d->hours << 11);
	udate = d->day + (d->month << 5) + ((d->year+20)  << 9);
		
	td_string[0] = ' ';
	td_string[1] = '$';
	td_string[2] = NUL;

	hexcat(td_string, udate >> 8);
	hexcat(td_string, udate & 0xFF);
	hexcat(td_string, utime >> 8);
	hexcat(td_string, utime & 0xFF);
}

/* swval - extract decimal value from switch
** Takes a string of the form "X=1234\0" and
** returns the decimal value 1234 as the result.
** Returns 0 if no "=" found or if no numeric data or
** if value is too big for signed int (> 32767)
*/
swval(s)
char *s;
{
	int iv, v;

	v = 0;
	iv = index(s, "=");
	if (iv != -1) {
		while (isdigit(s[++iv]))
			v = v * 10 + (s[iv] - '0');
	}
	if (v < 0)
		v = 0;
	return v;
}

/* scansw - scan and process switches */
scansw(s)
char *s;
{
	int l;
	char *c;
	char *swch;
	
	f_list = FALSE;
	f_brief = FALSE;
	f_help = FALSE;
	
	l = strlen(s);
	if (l>0)
		/* scan backward from last character */
		for (c=s+l-1; c>=s; c--)
			if (*c == '/') {
				*c = NUL;
				swch = c+1;
				switch (*swch) {
				/* B = brief listing */
				case 'B':
					f_brief = TRUE;
					break;
				/* D = debug mode */
				case 'D':
					f_debug = TRUE;
					printf("DEBUG mode!\n");
					break;
				/* H = help */
				case 'H':
					f_help = TRUE;
					break;
				/* L = long form listing */
				case 'L':
					f_list = TRUE;
					break;
				/* T = set VDIP timeout parameter */
				case 'T':
					vmaxw = swval(swch);
					printf("Timeout=%d ms.\n", vmaxw);
					break;
				default:
					printf("Unrecognized option!\n");
					break;
				}
			}
}

/* freeall - free dynamically allocated space */
freeall()
{
	int i;
	
	/* free filespec list */
	for (i=0; i<nsrc; i++)
		free(src[i]);
	
	/* free HDOS directory entries */
	for (i=0; i<nentries; i++)
		free(direntry[i]);
}


/* wcexpand - expand wild cards in string.  if '*' is first
** then terminate the string after it, otherwise expand
** '*' to one or more '?' characters.  no error checking
** is performed.
*/
wcexpand(s, l)
char *s;
int l;
{
	int i, havestar;
	
	havestar = FALSE;
	for (i=0; i<l; i++, s++) {
		if (*s == '*') {
			/* if first is '*' we're done */
			if (i == 0) {
				++s;
				break;
			}
			havestar = TRUE;
		}
		/* expand '*' into '?'s */
		if (havestar)
			*s = '?';
	}
	/* now terminate the string */
	*s = NUL;
}

/* dstexpand - this routine takes a directory entry (file name
** and extension) and then uses the destination filespec to
** create a destination file name, which is returned as a
** string.
*/
dstexpand(entry, dspec, dname)
struct finfo *entry;
struct fspec *dspec;
char *dname;
{
	int i, wild;
	char *d;
	char c, s;
	
	d = dname;

	/* do 8 character file name */
	wild = (dspec->fname[0] == '*');
	for (i=0; i<8; i++) {
		s = dspec->fname[i];
		if (wild || (s == '?')) {
			c = entry->name[i];
			if (isalpha(c) || isdigit(c))
				*d++ = c;
		}
		else if (isalpha(s) || isdigit(s))
			*d++ = s;
	}

	/* do 3 character file extension */
	wild = (dspec->fext[0] == '*');
	for (i=0; i<3; i++) {
		s = dspec->fext[i];
		if (wild || (s == '?')) {
			c = entry->ext[i];
			if (isalpha(c) || isdigit(c)) {
				if (i == 0)
					*d++ = '.';
				*d++ = c;
			}
		}
		else if (isalpha(s) || isdigit(s)) {
			if (i == 0)
				*d++ = '.';
			*d++ = s;
		}
	}

	/* make sure string is terminated! */
	*d = NUL;
}

/*********************************************
**
**	Vinculum (USB) File/Directory Functions
**
*********************************************/

/* bldudir - perform directory on the USB device and
** populate the directory array, dynamically allocating
** memory for each entry.
*/
bldudir()
{
	/* pass 1 - populate the directory array */
	vdir1();
	
	/* pass 2 - look up details on each file */
	vdir2();
}

/* vdir1 - This routine does "pass 1" of the directory
** using the 'dir' command fill out the array of directory 
** entries (direntry) by allocating space for each one
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
		else if ((entry = alloc(sizeof(fentry))) == 0)
			printf("error allocating directory entry!\n");
		else {
			/* process directory entry */
			entry->name[0] = NUL;
			entry->ext[0] = NUL;
			entry->tag = FALSE;
			entry->isdir = FALSE;
			if ((ind=index(linebuff, " DIR")) != -1) {
				/* have a directory entry */
				entry->isdir = TRUE;
				linebuff[ind] = 0;
				strncpy(entry->name, linebuff, 8);
				entry->name[8] = NUL;
			} else if ((ind=index(linebuff, ".")) != -1) {
				/* NAME.EXT filename */
				linebuff[ind] = 0;
				strncpy(entry->name, linebuff, 8);
				entry->name[8] = NUL;
				strncpy(entry->ext, linebuff+ind+1, 3);
				entry->ext[3] = NUL;
			} else {
				/* NAME filename */
				strncpy(entry->name, linebuff, 8);
				entry->name[8] = NUL;
			}
			/* now store the entry and bump the count */
			direntry[nentries++] = entry;
		}
	} while (!done);
}

/* vdir2 - This routine does "pass 2" of the directory 
** for each entry in the table it does a more extensive
** query gathering file size and other information
*/
vdir2()
{
	int i;
	static char dirtemp[20];

	for (i=0; i<nentries; i++){
		/* return entry as a string, e.g. "HELLO.TXT" */
		dirstr(i, dirtemp);
		
		/* look up the file size and date modified */
		if (direntries[i]->isdir) {
			direntries[i]->size  = 0L;
			direntries[i]->mdate = 0;
			direntries[i]->mtime = 0;
		}
		else {
			vdirf(dirtemp, &direntries[i]->size);
			vdird(dirtemp, &direntries[i]->mdate, &direntries[i]->mtime);
		}
	}
}

/* vcput - copy from HDOS source file to VDIP dest file 
** return -1 on error
*/
vcput(source, dest)
char *source, *dest;
{
	int i, nbytes, channel, done, result, rc;
	static long fsize;
	
	rc = 0;
	if((channel = fopen(source, "rb")) == 0) {
		printf("Unable to open source file %s\n", source);
		rc = -1;
	}
	else if (vwopen(dest) == -1) {
		printf("Unable to open destination file %s\n", dest);
		rc = -1;
		fclose(channel);
	}
	else {
		/* start writing at beginning of file */
		vseek(0);
		fsize = 0L;
		printf("%s --> %s\n", source, dest);
		for (i=1, done=FALSE; !done; fsize+=nbytes, i++) {
			nbytes = read(channel, rwbuffer, BUFFSIZE);
			if (nbytes == 0)
				done = TRUE;
			else {
				result = vwrite(rwbuffer, nbytes);
				if (result == -1) {
					printf("\nError writing to VDIP device\n");
					rc = -1;
					done = TRUE;
				} else {
					/* show user we're working ... */
					putchar('.');
					if ((i%60) == 0)
						printf("\n");
				}
			}
		}
		printf("\n%ld bytes\n", fsize);
		
		/* important - close files! */
		fclose(channel);
		vclose(dest);
	}
	return rc;
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
	strcpy(s,direntry[e]->name);
	if (direntry[e]->ext[0] != 0) {
		strcat(s,".");
		strcat(s,direntry[e]->ext);
	}
}

/*********************************************
**
**	HDOS File/Directory Functions
**
*********************************************/

/* bldhdir - read HDOS system directory file for specified
** device and populate directory array, dynamically 
** allocating memory for each entry.  Device is of the form
** "SY0", "DK0", etc.
*/
bldhdir(device)
char *device;
{	
	int i, j, cc, channel, done, block, rc;
	unsigned clu;
	struct finfo *entry;
	char *src, *dst;
	char spg;
	static char dfname[20];

	rc = 0;

	strcpy(dfname, device);
	strcat(dfname, ":DIRECT.SYS");
	if ((channel = fopen(dfname, "rb")) == 0) {
		printf("Error - unable to open %s\n", dfname);
		rc = -1;
	}
	else {
		/* fetch SPG and GRT from Active I/O area */
		spg = *aiospg;
		grt = *aiogrt;
		
		done = FALSE;
		nentries = 0;
		block = 0;
		do {
			/* read a chunk of the directory
			** (512 bytes at a time)
			*/
			if (read(channel, buffer, DIRBUFF) < DIRBUFF)
				done = TRUE;
			else {
				++block;
				
				/* each of these chunks holds 22 directory
				** entries. loop over them and store them
				** in the array (allocating space as we go
				*/
				src = buffer;
				for (i=0; ((i<22) && (!done)); i++) {
					if (isprint(*src)){
						/* fill out the HDOS file entry */
						dst = (char *) &hdosentry;
						for (j=0; j<sizeof(hdosentry); j++)
							*dst++ = *src++;
								
						/* trace cluster chain to compute cluster count */
						clu = hdosentry.fgn & 0xFF;
						cc = 0;
						do {
							++cc;
							clu = grt[clu] & 0xFF;
						} while (clu != 0);
							
						/* allocate an entry for our directory */
						if ((entry = alloc(sizeof(fentry))) == 0) {
							printf("Error allocating directory entry!\n");
							done = TRUE;
							break;
						}
						/* copy pertinent HDOS fields to our entry */
						for (j=0; j<8; j++)
							entry->name[j] = hdosentry.hdname[j];
						entry->name[8] = NUL;
						for (j=0; j<3; j++)
							entry->ext[j] = hdosentry.hdext[j];
						entry->ext[3] = NUL;

						/* convert file size to bytes */
						entry->size = 256L * ((cc-1)*spg + hdosentry.lsi);
						
						/* convert HDOS date */
						/* (HDOS uses 1970 as base year, adjust down by 10 */
						entry->mdate = hdosentry.moddate - 0x1400;
						
						/* clear other fields */
						entry->mtime = 0;
						entry->isdir = FALSE;
						entry->tag = FALSE;
						
						/* now store it in our directory list */
						direntry[nentries++] = entry;
					}
					else
						/* deleted entry, skip to next */
						src += sizeof(hdosentry);
				}
			}
		} while (!done);
		fclose(channel);
	}
	return rc;
}

/* vcp - copy from USB source file to system dest file
** return -1 on error
*/
vcp(source, dest)
char *source, *dest;
{
	int nblocks, nbytes, i, channel, rc, done;
	static long filesize;
	
	rc = 0;
	if (vdirf(source, &filesize) == -1) {
		printf("Unable to open file %s\n", source);
		rc = -1;
	}
	else {
		/* assumes file size < 8 meg */
		nblocks = filesize/BUFFSIZE;
		nbytes = filesize % BUFFSIZE;
		
		/* open source file on flash device for read */
		if (vropen(source) == -1) {
			printf("Unable to open source file %s\n", source);
			rc = -1;
		}
		else if	((channel = fopen(dest, "wb")) == 0) {
			printf("\nError opening destination file %s\n", dest);
			rc = -1;
		}
		else {
			printf("%s --> %s\n", source, dest);
			/* copy one block at a time */
			for (done = FALSE, i=1; ((i<=nblocks) && (!done)); i++) {
				/* read a block from input file */
				if (vread(rwbuffer, BUFFSIZE) == -1) {
					printf("\nError reading block %d\n", i);
					done = TRUE;
					rc = -1;
				}
				else {
					if ((write(channel, rwbuffer, BUFFSIZE)) == -1) {
						printf("\nError writing to %s\n", dest);
						rc = -1;
						done = TRUE;
					}
					/* show user we're working ... */
					putchar('.');
					if ((i%60) == 0)
						printf("\n");
				}
			}
			/* NUL fill the buffer before last write */
			for (i=0; i<BUFFSIZE; i++)
				rwbuffer[i]=0;
	
			/* if any extra bytes process them ... */
			if ((nbytes > 0) && !done) {
				/* read final remaining bytes */
				if (vread(rwbuffer, nbytes) == -1) {
					printf("\nError reading final block\n");
					rc = -1;
				}
				else if((write(channel, rwbuffer, BUFFSIZE)) == -1) {
					printf("\nError writing to %s\n", dest);
					rc = -1;
				}
			}	
			printf("\n%ld bytes\n", filesize);

			/* important - close files! */
			fclose(channel);
			vclose(source);
		}
	}
	return rc;
}

/* listmatch - print device directory listing from
** stored array.  Lists only entries with the "tag"
** field set to TRUE.  if the argument "brief" is TRUE
** then just do an abbreviated listing.
*/
listmatch(brief)
int brief;
{
	int i, j, nfiles;
	char *c;
	static char fsize[15];
	
	nfiles = 0;
	for (i=0; i<nentries; i++) {
		if(direntry[i]->tag) {
			printf("%-8s", direntry[i]->name);
			if (direntry[i]->isdir)
				/* directory entry */
				printf(" <DIR>  ");
			else {
				/* file entry */
				++nfiles;
				printf(".%-3s    ", direntry[i]->ext);
			}
			if (brief) {
				/* brief form, 4 entries per line */
				if (((i+1) % 4) == 0)
					printf("\n");
			}
			else {
				/* long form */
				if (!direntry[i]->isdir) {
					/* files only: display size, date and
					** time (if non-zero)
					*/
					commafmt(direntry[i]->size, fsize, 15);
					printf("%s  ", fsize);

					prndate(direntry[i]->mdate);
					if (direntry[i]->mtime) {
						printf("  ");
						prntime(direntry[i]->mtime);
					}
				}
				/* terminate the line */
				printf("\n");
			}
		}
	}
	printf("\n%d Files\n", nfiles);
}

/* copyfiles - copy files from source device to
** destination.  Copies only entries with the "tag"
** field set to TRUE.
**
** NOTE to be added: If destination is a single unique
** file then just concatenate all files to that, otherwise
** open source and destination files in pairs.
*/
copyfiles()
{
	int i, ncp;
	static char fullname[20];
	
	/* check if destination is single file or wildcard filespec */
/*	if ((index(dstspec.fname, "*") != -1) || (index(dstspec.fname, "?") != -1) ||
		(index(dstspec.fext, "*")  != -1) || (index(dstspec.fext, "?")  != -1))
		onedest = FALSE;
	else
		onedest = TRUE;
	
	if (onedest)
		printf("Concatenating to single file %s.%s\n", dstspec.fname,dstspec.fext);
*/
	
	/* loop over entries and perform copy */
	for (i=0, ncp=0; i<nentries; i++) {
		/* copy tagged files (but not directories!) */
		if(direntry[i]->tag && !direntry[i]->isdir) {
			/* get current time & date */
			gettd(&mydate);
			dirstr(i, srcfname);
/*			if (onedest)
				printf("--> %s:%s\n", srcdev, srcfname);
			else  */
			if ((srctype == STORD) && (dsttype == USBD)) {
				fullname[0] = NUL;
				strcat(fullname, srcdev);
				strcat(fullname,":");
				strcat(fullname, srcfname);
				dstexpand(direntry[i], &dstspec, dstfname);
				if ((vcput(fullname, dstfname)) != -1);
					++ncp;
			}
			else if ((srctype == USBD) && (dsttype == STORD)){
				fullname[0] = NUL;
				strcat(fullname, dstdev);
				strcat(fullname,":");
				dstexpand(direntry[i], &dstspec, dstfname);
				strcat(fullname, dstfname);
				if ((vcp(srcfname, fullname)) != -1)
					++ncp;
			}
		}
	}
	printf("\n%d Files Copied\n", ncp);
}


/* parsefs - parse a string into a file spec data structure
**
** s is of the form xxx:yyyyyyyy.zzz where xxx is a 
** device specification and yyyyyyyy.zzz is a standard
** "8.3" file name specification.  '*' wild cards are 
** expanded.  The device is returned in dev while the
** file name is returned in sfs.
**
*/
parsefs(sfs, dev, s)
struct fspec *sfs;
char *dev, *s;
{
	int i, iscan;
	
	/* first zero the contents */
	for (i=0; i<9; i++)
		sfs->fname[i] = NUL;
	for (i=0; i<4; i++)
		sfs->fext[i] = NUL;
	
	/* scan for source drive specification and save it */
	iscan = index(s, ":");
	if (iscan != -1) {
		s[iscan] = NUL;
		/* take only at most first 3 chars, null terminate */
		strncpy(dev, s, 3);
		dev[3] = NUL;
		s += iscan + 1;
	}
	
	/* scan for file extension, if any */
	iscan = index(s, ".");
	if (iscan != -1) {
		s[iscan] = NUL;
		strncpy(sfs->fext, (s + iscan + 1), 3);
	}
	
	/* now go back and copy file name portion */
	strncpy(sfs->fname, s, 8);
	
	/* check for xxx: -> make it xxx:*.* */
	if ((sfs->fname[0] == NUL) && (sfs->fext[0] == NUL)) {
		sfs->fname[0] = '*';
		sfs->fext[0] = '*';
	}

	/* expand any wild cards in name or extension */
	wcexpand(sfs->fname, 8);
	wcexpand(sfs->fext, 3);
}

/* devtype - returns type code based on device specified */
devtype(d)
char *d;
{
	int dtype;
	
	if (strlen(d) == 0)
		dtype = NULLD;
	else if (strcmp(d, "USB") == 0)
		dtype = USBD;
	else if ((strlen(d) == 2) && isalpha(d[0]) && isalpha(d[1]))
		dtype = USERD;
	else if ((strlen(d) == 3) && isalpha(d[0]) && isalpha(d[1]) && isdigit(d[2]))
		dtype = STORD;
	else
		dtype = UNKD;
	
	return dtype;
}

/* checkdev - do validation check on specified devices
**
**	Return codes:
**		0: normal return, no error
**		1: one or more unknown devices specified
**		2: no USB device specified
**		3: both source and dest are USB (not allowed)
**		4: one or both devices are user devices (e.g. TT: LP:, etc.)
*/
checkdev()
{
	int rc;
	
	rc = 0;
	
	/* identify types of devices specified */
	dsttype = devtype(dstdev);
	srctype = devtype(srcdev);
	
	if ((dsttype == UNKD) || (srctype == UNKD))
		rc = 1;
	/* otherwise replace NULL devices with something appropriate */
	else if (srctype == NULLD) {
		/* no source device specified */
		if (dsttype == NULLD) {
			/* neither specified, use appropriate defaults */
			dsttype = STORD;
			strcpy(dstdev, SYSDFLT);
			srctype = USBD;
			strcpy(srcdev, USBDFLT);
		}
		else if (dsttype == USBD) {
			/* destination is USB but source unspecified */
			srctype = STORD;
			strcpy(srcdev, SYSDFLT);
		} else {
			/* destination not USB but source unspecified */
			srctype = USBD;
			strcpy(srcdev, USBDFLT);
		}
	
	} else if (dsttype == NULLD) {
		/* source type specified but not dest type */
		if (srctype == USBD) {
			/* source is USB but dest unspecified */
			dsttype = STORD;
			strcpy(dstdev, SYSDFLT);
		}
		else {
			/* source not USB but dest unspecified */
			dsttype = USBD;
			strcpy(dstdev, USBDFLT);
		}
	}
	
	/* at least one device needs to be USB: */
	if ((dsttype != USBD) && (srctype != USBD))
		rc = 2;
	
	/* USB to USB copies not supported */
	if ((dsttype == USBD) && (srctype == USBD))
		rc = 3;
	
	/* currently only disk devices allowed */
	if ((dsttype == USERD) || (srctype == USERD))
		rc = 4;
		
	return rc;
}

/* domatch - tag files that match a filespec
**
** the 'cname' and 'cext' strings contain file name
** and extension specifications including possible
** '*' and '?' wild cards.  if '*' is the first 
** char then anything matches.  '?' matches any
** single character.  the 'tag' field in the
** directory is updated to indicate any matches.
** wild cards never match directory entries for 
** subdirectories ("<DIR>").
*/
domatch(cname, cext)
char *cname, *cext;
{
	int i, j;
	char match;
	
	for (i=0; i<nentries; i++) {
		match = TRUE;
		if (cname[0] != '*') {
			for (j=0; (j<8) && match; j++) {
				if ((cname[j] != '?') && (cname[j] != direntry[i]->name[j]))
					match = FALSE;
			}
		}
		if ((cext[0] != '*') && match) {
			for (j=0; (j<3) && match; j++){
				if ((cext[j] != '?') && (cext[j] != direntry[i]->ext[j]))
					match = FALSE;
			}
		}
		/* mark only the matches */
		if (match)
			direntry[i]->tag = match;
	}
}

/* showhelp - print help screen */
showhelp()
{
	printf("Usage: VPIP DEST=SOURCE1,...SOURCEn/SWITCH1.../SWITCHn\n");
}

/* docmd - execute a command string */
docmd(s)
char *s;
{
	char *srcstr, *dststr;
	int i, iscan, rc;
	struct fspec *entry;
	char tmpdev[4];

	*dstdev = NUL;
	*srcdev = NUL;
	*tmpdev = NUL;
	dststr  = NULSTR;
	dstspec.fname[0] = NUL;
	dstspec.fext[0] = NUL;
	srcstr = s;
	nentries = 0;
	nsrc = 0;
	vmaxw = MAXWAIT;
	f_debug = FALSE;

	/* scan for, process, and remove switches */
	scansw(s);
	
	/* If "help" then do now and exit */
	if (f_help) {
		showhelp();
		return;
	}
	
	/* spaces not allowed - truncate at first one! */
	if ((iscan = index(s, " "))!= -1)
		s[iscan] = NUL;

	/* process destination, if specified */
	iscan = index(s, "=");
	if (iscan != -1) {
		dststr = s;
		s[iscan] = NUL;
		srcstr = s + iscan + 1;
		parsefs(&dstspec, dstdev, dststr);
	}
		
	/* now process comma-separated source filespec list */
	do {
		iscan = index(srcstr, ",");
		if (iscan != -1)
			/* found comma, terminate string there */
			srcstr[iscan] = NUL;
		if ((entry = alloc(sizeof(dstspec))) == NUL) {
			printf("ERROR allocating memory for entry!\n");
			break;
		}
		src[nsrc] = entry;
		parsefs(src[nsrc], tmpdev, srcstr);
		nsrc++;
		/* If source device is specified it must be the same for all */
		if (*tmpdev != NUL) {
			/* source device specified, make sure same as before */
			if (*srcdev == NUL)
				/* first one we've seen so take it */
				strcpy(srcdev, tmpdev);
			else if (strcmp(srcdev, tmpdev) != 0)
				printf("Only one source device allowed! - assuming %s\n", srcdev);
		}
		/* now look for next source */
		srcstr += (iscan + 1);
	} while (iscan != -1);
	
	/* set defaults for anything not specified */
	if (strlen(dstspec.fname) == 0)
		strcpy(dstspec.fname, "*");
	if (strlen(dstspec.fext) == 0)
		strcpy(dstspec.fext, "*");
		
	/* do validation check on specified devices and set defaults */
	rc = checkdev();
	if (rc == 0) {
		/* initialize VDIP */
		if (vinit() == -1) {
			rc = 5;
			printf("Error initializing VDIP-1 device!\n");
		}
		/* make sure there's a drive inserted */
		else if (vfind_disk() == -1) {
			rc = 6;
			printf("No flash drive found!\n");
		}
		else {
			/* build directory and tag matching files */
			if ((srctype == STORD) || (srctype == USBD)) {
				/* first build the directory tree in memory */
				if (srctype == STORD)
					bldhdir(srcdev);	/* HDOS */
				else
					bldudir();			/* USB */
				/* now tag file entries that match any of the source filespecs */
				for (i=0; i<nsrc; i++)
					domatch(src[i]->fname, src[i]->fext);
			}
			if (f_brief || f_list)
				listmatch(f_brief);
			else
				copyfiles();
		}
	}
	else if (rc == 1)
		printf("Illegal device specified\n");
	else if (rc == 2)
		printf("Either source or destination need to be the USB\n");
	else if (rc == 3)
		printf("USB to USB copies not supported\n");
	else if (rc == 4)
		printf("Both source and destination must be storage devices\n");
	else
		printf("Device code error %d\n", rc);
		
	return rc;
}

main(argc,argv)
int argc;
char *argv[];
{
	int l;

	printf("VPIP Ver. 0.9.1 (Beta) - G. Roberts\n");
	if (strcmp(argv[0], "VP89") == 0) {
		p_data = H89DATA;
		p_stat = H89STAT;
		printf("Config: H89, ");
	}
	else {
		p_data = H8DATA;
		p_stat = H8STAT;
		printf("Config: H8, ");
	}
	printf("USB ports: %o,%o\n", p_data, p_stat);

	/* try to read date/time from RTC (use month to validate) */
	readdate(&mydate);
	if ((mydate.month >= 1) && (mydate.month <= 12)) {
		printf("%02d/%02d/%02d  %02d:%02d:%02d\n", mydate.month, 
			mydate.day, mydate.year, mydate.hours, mydate.minutes,
			mydate.seconds);
		havertc = TRUE;
	}
	else {
		havertc = FALSE;
		printf("No real time clock found!\n");
	}

	if (argc < 2) {
		/* interactive mode (bug: might be smarter
		** to save the directory structure if possible
		** between subsequent commands - currently
		** we start over each time!)
		*/
		do {
			printf(":V:");
			if ((l = getline(cmdline,80)) > 0) {
				strupr(cmdline);
				docmd(cmdline);
				freeall();
			}
		} while (l > 0);
	}
	else
		/* command line mode */
		docmd(argv[1]);
}
