/********************************************************
** vpip (CP/M 3 version)
**
** This program implements a PIP-like (Peripheral Interchange
** Program) interface to the Vinculum VDIP-1 device for
** H8 and H89 computers.  It is combines functionality found
** in both the CP/M and HDOS versions of PIP.  The program
** reads and writes files to/from a USB device and also
** performs directory operations.
**
** The program works in three phases: First, it builds
** a directory (in dynamically-allocated RAM) of the contents
** of the specified source device (either the USB device or
** the specified CP/M filespec); Second, it goes through
** the directory data structure and tags the files that
** match the user's file specification; and finally, it
** processes the command (e.g. either copying the specified
** files or listing their directory information).
**
** The program utilizes CP/M 3's real-time clock suport to
** time-and-date stamp file operations.
**
** The program can be run in line mode or command mode. In
** line mode the arguments are all specified on the command
** line.  In command mode the user is presented with a :V:
** prompt and commands are executed interactively.
**
** Usage: VPIP {command}
**
** where command is of the form:
**
**	DEST=SOURCE1,...SOURCEn/SWITCH1.../SWITCHn
**
** if no command is specified on the command line then the
** utility goes into a line-by-line command mode by issuing
** the :V: prompt.
**
** The pseudo-drive specification "USB:" is used to designate
** the USB-attached device.  CP/M style device specifications
** (e.g. A:, F:, etc.) denote system drives.
**
** Files may only be copied between the USB device and the
** system devices.   USB to USB, and system to system copies
** are not supported.
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
** Typical link command:
**
** L80 vpip,vinc,vutil,pio,fprintf,stdlib/s,flibrary/s,clibrary,vpip/n/e
**
**	Glenn Roberts
**	March 2020
**
**	Revisions
**
**	3.2			Initial CP/M 3 version
**
********************************************************/

#include "fprintf.h"

/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	TRUE	1
#define	FALSE	0

#define	NUL		'\0'
#define	SPACE	' '
#define	SWCHAR	'-'			/* command switch designator */
#define	NULSTR	""
#define	DMA		0x80		/* CP/M DMA area */

/* default devices if none specified */
#define	USBDFLT	"USB"

/* device types set by checkdev() */
#define	NULLD	0			/* No device given */
#define	STORD	1			/* storage device */
#define	USERD	2			/* user io device */
#define	USBD	3			/* USB device */
#define	UNKD	4			/* unknown format */

#define MAXD	256			/* maximum number of directory entries */
#define DIRBUFF 512			/* buffer space for directory */

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

/* CP/M file directory entry data structure */
struct cpminfo {
	char user;
	char cname[8];
	char cext[3];
	char extent;
	int reserved;
	char recused;
	char abused[16];
};

/* Internally-used file directory data structure.
** The 'tag' field is used to flag entries that
** match the user-specified criteria.
**
** time/date format follows FAT specification:
**
** mdate:
**	9:15 	Year	0..127	(0=1980, 127=2107)
**	5:8 	Months	1..12	(1=Jan., 12=Dec.)
**	0:4 	Days	1..31	(1=first day of mo.)
**
** mtime:
**	11:15 	Hours	0..23 	(24 hour clock)
**	5:10	Minutes 0..59
**	0:4		Sec./2	0..29 	(0=0, 29=58 sec.)
**
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


/*********************************************
**
**	Global Static Storage
**
*********************************************/

/* declared in vinc library */
extern char linebuff[128];		/* I/O line buffer 		*/

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
int f_list;		/* to list directory (no file copy) */

/* i/o ports - must be global, used by vinc utilities */
int p_data;		/* USB data port */
int	p_stat;		/* USB status port */

/* declared in vinc library */
extern char td_string[15];		/* time/date hex value */

/*********************************************
**
**	Utility Functions
**
*********************************************/

/* modays - return days in given month and year */
modays(month, year)
int month;
int year;
{
	int days;
	
	switch(month)
	{
		/* 30 days hath september, april, june and november */
		case 9:
		case 4:
		case 6:
		case 11:
			days = 30;
			break;

		case 2:
			days = is_leap(year) ? 29 : 28;
			break;

		/* all the rest have 31! */
		default:
			days = 31;
			break;
	}
	
	return days;
}

/* return TRUE if year is a leap year */
is_leap(year) {
	return (((year%4==0)&&((year%100)!=0)) || ((year%400)==0));
}

/* process switches */
dosw(argc, argv)
int argc;
char *argv[];
{
	int i;
	char *s;
	
	f_list = FALSE;

	/* process right to left */
	for (i=argc; i>0; i--) {
		s = argv[i];
		if (*s++ == SWCHAR) {
			/* have a switch! */
			switch (*s) {
			/* P = specify alternate I/O port */
			case 'P':
				++s;
				p_data = aotoi(s);
				p_stat = p_data + 1;
			    break;
			/* L = list files */
			case 'L':
				f_list = TRUE;
				break;
			default:
			    printf("Invalid switch %c\n", *s);
				break;
			}
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
	
	/* free directory entries */
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

/********************************************************
**
** settd
**
** Reads time/date via BDOS and stores result in
** hexadecimal ASCII global string td_string[] using format
** desired by VDIP chip.  This is used by vwopen() to
** set the date on the file.
**
********************************************************/
settd()
{
	int seconds;
	static int mydate[3];	/* day, month, year */
	static unsigned utime, udate;
	/* date data structure expected by BDOS 105 */
	static struct datime {
		unsigned date;
		char hour;
		char minute;
	} dt;
	
	/* snapshot time */
	seconds = bdoshl(105, &dt);
		
	/* convert day count to day, month, year */
	dodate(dt.date, mydate);
	
	/* store time in td_string */
	utime = (btod(seconds)/2) + 
			(btod(dt.minute) << 5) + 
			(btod(dt.hour) << 11);
	udate = mydate[0] + 
			(mydate[1] << 5) + 
			((mydate[2]-1980)  << 9);
				
	td_string[0] = ' ';
	td_string[1] = '$';
	td_string[2] = '\0';

	hexcat(td_string, udate >> 8);
	hexcat(td_string, udate & 0xFF);
	hexcat(td_string, utime >> 8);
	hexcat(td_string, utime & 0xFF);	
}

/********************************************************
**
** hexcat
**
** Converts unsigned int i to a 2 byte hexadecimal
** representation in upper case ASCII and then catenates 
** that to the end of string s
**
********************************************************/
hexcat(s, i)
char *s;
unsigned i;
{
	static char b[3];
	unsigned n;
	
	n = (i & 0xFF) >> 4;
	b[0] = (n < 10) ? '0'+n : 'A'+n-10;
	n = i & 0xF;
	b[1] = (n < 10) ? '0'+n : 'A'+n-10;
	b[2] = '\0';
	strcat(s, b);
}


/********************************************************
**
** dodate
**
** takes a CP/M 3 day count (from directory entry) and
** computes day, month and year based on definition that
** 1/1/1978 is day 1.
**
********************************************************/
dodate(days, date)
int days;
int date[];
{
	int yyyy, dd, m, mm;
	
	yyyy = 1978;
	dd = days;
	
	while ((dd>365) && ((dd!=366) || !is_leap(yyyy))) {
		dd -= is_leap(yyyy) ? 366 : 365;
		++yyyy;
	}

	/* have year & days in the year; convert to dd/mm/yyyy */
	mm = 1;
	while (dd > (m=modays(mm, yyyy))) {
		dd -= m;
		++mm;
	}
	date[0] = dd;
	date[1] = mm;
	date[2] = yyyy;
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
	printf("Building USB directory...\n");
	/* pass 1 - populate the directory array */
	vdir1();
	
	printf("Cataloging USB file details...\n");
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
	str_rdw(linebuff, '\r');

	done = FALSE;
	nentries = 0;
	
	/* read each line and add it to the list,
	** when the D:\> prompt appears, we're done. the
	** list entries are dynamically allocated.
	*/
	do {
		str_rdw(linebuff, '\r');
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

/* vcput - copy from source file to VDIP dest file 
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
** returning the result as a null-terminated string.
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
**	CP/M File/Directory Functions
**
*********************************************/
/* bldcdir - read CP/M system directory file for specified
** device and populate directory array, dynamically 
** allocating memory for each entry.  Device is the drive
** identifier, e.g. "A", "B", etc.
*/
bldcdir(device)
char *device;
{	
	int i, j, bfn;
	char c;
	struct finfo *entry;
	static char fcb[36];
	static char dfname[20];
	/* DMA area will contain an array of 4
	** directory entries after BDOS calls
	*/
	struct cpminfo *dmaentry;
	struct cpminfo *ourentry;

	strcpy(dfname, device);
	strcat(dfname, ":????????.???");
	makfcb(dfname, fcb);

	/* DMA will contain an array [0..3] of
	** CP/M file entries after BDOS calls.
	*/
	dmaentry = (struct finfo *) DMA;

	nentries = 0;
	
	/* use BDOS functions 17 and 18 to scan directory */
	bfn=17;
	while ((i = bdos(bfn,fcb)) != -1) {
		/* have a match */
		if ((entry = alloc(sizeof(fentry))) == 0)
			printf("Error allocating directory entry!\n");
		else {
			ourentry = &dmaentry[i];
			/* copy pertinent CP/M fields to our entry and
			** set other fields to defaults.  In our directory
			** structure we pad with NUL not SPACE so make
			** that adjustment here...
			*/
			for (j=0; j<8; j++) {
				c = ourentry->cname[j];
				entry->name[j] = (c == SPACE) ? NUL : c;
			}
			entry->name[8] = NUL;
			
			for (j=0; j<3; j++) {
				c = ourentry->cext[j];
				entry->ext[j] = (c == SPACE) ? NUL : c;
			}
			entry->ext[3] = NUL;
			
			entry->tag = FALSE;
			entry->isdir = FALSE;
			
			/* now store the entry and bump the count */
			direntry[nentries++] = entry;
		}
		bfn = 18;
	}
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
		/* BEWARE: assumes file size < 8 meg */
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
** stored array (direntry).  Lists only entries with the 
** "tag" field set to TRUE.  For USB files the size and
** time/date information is included.
*/
listmatch()
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
				printf(" <DIR>\n");
			else {
				/* file entry */
				++nfiles;
				printf(".%-3s", direntry[i]->ext);
				/* list size and time/date only for USB device */
				if (srctype == USBD) {
					/* files only: display size, date and
					** time (if non-zero)
					*/
					commafmt(direntry[i]->size, fsize, 15);
					printf(" %15s  ", fsize);

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
	
	/* loop over entries and perform copy */
	for (i=0, ncp=0; i<nentries; i++) {
		/* copy tagged files (but not directories!) */
		if(direntry[i]->tag && !direntry[i]->isdir) {
			/* get current time & date and save
			** for use by vwopen() */
			settd();
	
			dirstr(i, srcfname);
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
** s is of the form x:yyyyyyyy.zzz where x is a 
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

	for (i=0; i<4; i++)
		dev[i] = NUL;
	
	/* scan for source drive specification and save it */
	iscan = index(s, ":");
	if (iscan != -1) {
		s[iscan] = NUL;
		/* take only at most first 3 chars - will be
		** null terminated due to initializing of dev[].
		** could be USB: or just x: (drive id).
		*/
		strncpy(dev, s, 3);
		
		/* point to next character after ':' (typically
		** file name.
		*/
		s += iscan + 1;
	}
	
	/* first, scan ahead for file extension, if any */
	iscan = index(s, ".");
	if (iscan != -1) {
		/* if extension found terminate filename portion
		** there.
		*/
		s[iscan] = NUL;
		/* copy up to 3 characters into the extension */
		strncpy(sfs->fext, (s + iscan + 1), 3);
	}
	
	/* now go back and copy file name portion */
	strncpy(sfs->fname, s, 8);
	
	/* check for xxx: -> make it xxx:*.* */
	if ((sfs->fname[0] == NUL) && (sfs->fext[0] == NUL)) {
		sfs->fname[0] = '*';
		sfs->fext[0] = '*';
	}

	/* CP/M stores blanks to the right */
	padblanks(sfs->fname, 8);
	padblanks(sfs->ext, 3);

	/* expand any wild cards in name or extension */
	wcexpand(sfs->fname, 8);
	wcexpand(sfs->fext, 3);
}

/* padblanks - replaces NUL with SPACE up to specified
** length l.
*/
padblanks(s, l)
char *s;
int l;
{
	int i;
	
	for (i=0; i<l; i++) {
		if (*s == NUL)
			*s = SPACE;
	}
}


/* devtype - returns type code based on device specified
**
**	CP/M version.  only USB: or x: are allowed (where x is
**	a drive designation.
**
*/
devtype(d)
char *d;
{
	int dtype;
	
	if (strlen(d) == 0)
		dtype = NULLD;
	else if (strcmp(d, "USB") == 0)
		dtype = USBD;
	/* CP/M uses single letter drive designation */
	else if ((strlen(d) == 1) && isalpha(d[0]))
		/* Storage device, e.g. disk drive */
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
	static char sysdflt[] = "A";
	
	rc = 0;
	
	/* set current default disk */
	sysdflt[0] = 'A' + bdos(25,0);
	
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
			strcpy(dstdev, sysdflt);
			srctype = USBD;
			strcpy(srcdev, USBDFLT);
		}
		else if (dsttype == USBD) {
			/* destination is USB but source unspecified */
			srctype = STORD;
			strcpy(srcdev, sysdflt);
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
			strcpy(dstdev, sysdflt);
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
			direntry[i]->tag = TRUE;
	}
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

	
	/* process destination, if specified */
	iscan = index(s, "=");
	if (iscan != -1) {
		/* break s into source and destination strings */
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
					bldcdir(srcdev);	/* CP/M */
				else
					bldudir();			/* USB */
				/* now tag file entries that match any of the source filespecs */
				for (i=0; i<nsrc; i++)
					domatch(src[i]->fname, src[i]->fext);
			}
			if (f_list)
				listmatch();
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

	/* default port values */
	p_data = VDATA;
	p_stat = VSTAT;
	
	/* process any switches */
	dosw(argc, argv);

	printf("VPIP Ver. 3.2 (CP/M 3) - G. Roberts.  Using USB ports: %o,%o\n",
		p_data, p_stat);

    /* CP/M3 is required! */
	if ((bdoshl(12,0) & 0xF0) != 0x30)
		printf("CP/M Version 3 is required!\n");
	else if (argc < 2) {
		/* interactive mode	*/
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
