/* vget - CP/M 3 version
**
** This program copies a file from the FTDI Vinculum VDIP-1
** device (interfaced in parallel FIFO mode) to the CP/M file
** system. The destination file name is the same as the source
** file name.  Existing files are overwritten without warning.
**
** Usage: vget <file> <dest> <switches>
**
** (where <dest> is an optional drive ID, e.g. D:)
**
** switches:
**		-p<port>	to specify octal port (default is 0331)
**		-v			"verbose" - continuous display of progress
**
** Version 1.5	- CP/M3 release
**
** Compiled with Software Toolworks C/80 V. 3.0.  Requires
** the following modules/libraries: PIO, VUCPM3, FPRINTF, FLIBRARY
**
**	V1.1: Modified for separate compile & link - 5/21/16
**	v1.2: Allow destination device specification.
**	v1.5: CP/M 3 release 6/1/19  (gfr)
**
** Glenn Roberts 1 June 2019
**
*/
/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	FALSE	0
#define	TRUE	1

#include "fprintf.h"

/* buffer used for read/write */
#define BUFFSIZE	256	/* buffer size used for read/write */
char rwbuffer[BUFFSIZE];

/* buffer to hold destination filespec */
char destfile[20];

/* USB i/o ports */
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

int verbose;	/* if TRUE then show progress updates */

struct datime {
	unsigned date;
	char hour;
	char minute;
} dt;

/* vcp - copy from source file to dest file */
vcp(source, dest)
char *source, *dest;
{
	int nblocks, nbytes, i, channel, seconds;
	static long filesize;
	static long pctdone;
	static long start, finish, ttime;
	static char fsize[15];
	static char frate[7];
	
	if (vdirf(source, &filesize) == -1)
		printf("Unable to open file %s\n", source);
	else {
				
		commafmt(filesize, fsize, 15);
		printf("%-12s  %s bytes --> ", source, fsize);
		if (verbose)
			putchar('\n');
	
		/* assumes file size < 8 meg */
		nblocks = filesize/BUFFSIZE;
		nbytes = filesize % BUFFSIZE;
		
		/* open source file on flash device for read */
		if (vropen(source) == -1)
			printf("\nUnable to open source file %s\n", source);
		else if	((channel = fopen(dest, "wb")) == 0)
			printf("\nError opening destination file %s\n", dest);
		else {
		    /* snapshot time */
			seconds = bdoshl(105, &dt);
			start = btod(dt.hour) * 3600L + 
					btod(dt.minute) * 60L + 
					btod(seconds);
					
			/* copy one block at a time */
			for (i=1; i<=nblocks; i++) {
				/* read a block from input file */
				if (vread(rwbuffer, BUFFSIZE) == -1)
					printf("\nError reading block %d\n", i);
				else {
					write(channel, rwbuffer, BUFFSIZE);
					if (verbose) {
						/* show user we're working ... */
						pctdone = 100L * i/nblocks;
						printf("Percent done: %ld\r", pctdone);
					}
				}
			}
			/* NUL fill the buffer before last write */
			for (i=0; i<BUFFSIZE; i++)
				rwbuffer[i]=0;
	
			/* if any extra bytes process them ... */
			if (nbytes > 0) {
				/* read final remaining bytes */
				if (vread(rwbuffer, nbytes) == -1)
					printf("Error reading final block\n");
				else
					write(channel, rwbuffer, BUFFSIZE);
			}	
			
			/* snapshot time */
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

			/* close file on VDIP */
			vclose(source);
	
			/* close output file */
			fclose(channel);
		}
	}
}

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
	/* default port values */
	p_data = VDATA;
	p_stat = VSTAT;
	
	verbose = FALSE;

	/* process any switches */
	dosw(argc, argv);

	printf("VGET v1.5 (CP/M 3) - G. Roberts.  Using USB ports: %o,%o\n",
			p_data, p_stat);

    /* CP/M3 is required! */
	if ((bdoshl(12,0) & 0xF0) != 0x30)
		printf("CP/M Version 3 is required!\n");
	else if (argc < 2)
		printf("Usage: %s <file> <dest> <switches>\n", argv[0]);
	else if (vinit() == -1)
		printf("Error initializing VDIP-1 device!\n");
	else if (vfind_disk() == -1)
		printf("No flash drive found!\n");
	else {
		destfile[0] = 0;
		if ((argc > 2) && (argv[2][1] == ':'))
			/* user specified destination device */
			strcat(destfile, argv[2]);
		strcat(destfile, argv[1]);
		vcp(argv[1], destfile);
	}
}
