/* vput - CP/M 3 version
**
** This program copies one or more files from CP/M to the FTDI
** Vinculum VDIP-1 device (interfaced in parallel FIFO mode).
** The destination file names are the same as the source file names.
** Existing files are overwritten without warning.
**
** Usage: vput <file1> <file2> ... <filen>
**
** "wildcard" expansion with "*" and "?" are supported
**
** switches:
**		-p<port>	to specify octal port (default is 0331)
**		-v			"verbose" - continuous display of progress
**
** Version 1.5	- CP/M 3 release
**
** Compiled with Software Toolworks C/80 V. 3.0.  Requires
** the following modules/libraries: PIO, VUCPM3, FPRINT, FLIBRARY
**
** Glenn Roberts 27 May 2013
**
** v1.5: CP/M 3 release 6/2/19 (gfr)
**
*/

/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	FALSE	0
#define	TRUE	1

#include "fprintf.h"

/*********************************************
**
**	Global Static Storage
**
*********************************************/

/* buffer used for read/write */
#define BUFFSIZE	256	/* buffer size used for read/write */
char rwbuffer[BUFFSIZE];

/* USB i/o ports */
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

int verbose;	/* if TRUE then show progress updates */

struct datime {
	unsigned date;
	char hour;
	char minute;
} dt;


/* declared in vutil library */
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

/* takes a CP/M 3 day count (from directory entry) and
** computes day, month and year based on definition that
** 1/1/1978 is day 1.
*/
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

/* settd - reads time/date via BDOS and stores result in
** hexadecimal ASCII global string td_string[] using format
** desired by VDIP chip.  This is used by vwopen() to
** set the date on the file.j
**
** updates global datastructure dt
*/
settd()
{
	int seconds;
	static int mydate[3];	/* day, month, year */
	static unsigned utime, udate;
	
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


/* hexcat - converts unsigned int i to a 2 byte hexadecimal
** representation in upper case ASCII and the catenates 
** that to the end of string s */
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

/* vcput - copy from CP/M source file to VDIP dest file */
vcput(source, dest)
char *source, *dest;
{
	int i, nblocks, nbytes, channel, done, result, seconds;
	char *l;
	static char fcb[36];
	static long filesize;
	static long pctdone;
	static long start, finish, ttime;
	static char fsize[15];
	static char frate[7];
	
	if((channel = fopen(source, "rb")) == 0)
		printf("Unable to open source file %s\n", source);
	else {
		/* first set up the file date for vwopen() */
		settd();
		if (vwopen(dest) == -1) {
			printf("Unable to open destination file %s\n", dest);
			fclose(channel);
		}
		else {
			/* get input file size */
			makfcb(source, fcb);
			bdos(35,fcb);
			l = (char *) &filesize;
			for (i=33; i<36; i++)
				*l++ = fcb[i];
			*l = 0;
			filesize *= 128L;
			nblocks = (filesize+128L)/BUFFSIZE;
			commafmt(filesize, fsize, 15);
			printf("%-12s  %s bytes --> ", source, fsize);
			if (verbose)
				putchar('\n');
		
			/* start writing at beginning of file */
			vseek(0);

			/* snapshot time */
			seconds = bdoshl(105, &dt);
		
			start = btod(dt.hour) * 3600L + 
					btod(dt.minute) * 60L + 
					btod(seconds);

			/* copy one block at a time */
			done = FALSE;
			for (i=0; !done; i++) {
				nbytes = read(channel, rwbuffer, BUFFSIZE);
				if (nbytes == 0)
					done = TRUE;
				else {
					result = vwrite(rwbuffer, nbytes);
					if (result == -1) {
						printf("Error writing to VDIP device\n");
						done = TRUE;
					} else if (verbose) {
						/* show user we're working ... */
						pctdone = 100L * i/nblocks;
						printf("Percent done: %ld\r", pctdone);
					}
				}
			}
		
			/* done! snapshot time */
			seconds = bdoshl(105, &dt);
			finish = btod(dt.hour) * 3600L + 
					btod(dt.minute) * 60L + 
					btod(seconds);
			if (finish < start)
				/* must have passed midnight! */
				finish += 86400L;
			ttime = finish - start;
			if (ttime == 0)
				ttime = 1;

			commafmt(filesize/ttime, frate, 7);
			printf("%-12s : %ld sec. (%s BPS)\n", dest, ttime, frate);

			/* close input file */
			fclose(channel);

			/* important - close file on VDIP */
			vclose(dest);
		}
	}
}


/* process switches */
dosw(argc, argv)
int argc;
char *argv[];
{
	int i;
	char *s;
	
	/* process right to left */
	for (i=argc-1; i>1; i--) {
		s = argv[i];
		if (*s++ == '-') {
			/* have a switch! */
			switch (*s) {
			case 'P':
				++s;
				p_data = aotoi(s);
				p_stat = p_data + 1;
			    break;
			case 'V':
				verbose = TRUE;
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
	int i, offset;
	char *srcfile;
	char *dstfile;
	
	/* default port values */
	p_data = VDATA;
	p_stat = VSTAT;
	
	verbose = FALSE;

	/* first expand any wild cards in command line */
	command(&argc, &argv);

	/* process any switches */
	dosw(argc, argv);

	printf("VPUT v1.5 (CP/M 3) - G. Roberts.  Using USB ports: %o,%o\n",
			p_data, p_stat);
			
    /* CP/M3 is required! */
	if ((bdoshl(12,0) & 0xF0) != 0x30)
		printf("CP/M Version 3 is required!\n");
	else if (argc < 2)
		printf("Usage: vput <file1> ... <filen>\n");
	else if (vinit() == -1)
		printf("Error initializing VDIP-1 device!\n");
	else if (vfind_disk() == -1)
		printf("No flash drive found!\n");
	else {
		for (i=1; i<argc; i++) {
			srcfile = argv[i];
			/* don't process switch values! */
			if (*srcfile != '-') {
				/* need to strip off any drive ID on destination*/
				dstfile = argv[i];
				if ((offset = index(dstfile, ":")) != -1)
					/* found! skip past ':' */
					dstfile += offset + 1;
	
				vcput(srcfile, dstfile);
			}
		}
	}
}
