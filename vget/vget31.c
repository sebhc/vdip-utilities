/* vget - Version 3.1 for CP/M3 and HDOS w/ Rev 3.1 Z80 CPU
**
** This program copies a file from the FTDI Vinculum VDIP-1
** device (interfaced in parallel FIFO mode) to the CP/M or
** HDOS file system.
**
** Usage: vget file <dest> <switches>
**
** file is the name of the source file on the USB drive. Only
** "8.3" file names are supported (no long file names).
** <dest> is an optional drive ID, e.g. D: (CP/M) or SY1: (HDOS)
** or a complete file specification (e.g. D:NEWFILE.DAT or 
** DK0:ANYNAME.TXT).  If <dest> is not specified the destination
** file takes the same name as the source file and is created
** on the current or default drive.  If only a device name is
** specified for <dest> then the source file name is used but the
** file is saved on the specified device. Existing files are
** overwritten without warning.
**
** switches:
**		-p<port>	to specify octal port (default is 0331)
**		-v			"verbose" - continuous display of progress
**
** Version 3.1	- Joint HDOS/CP/M3 release
**
** Compiled with Software Toolworks C/80 V. 3.1 with support for
** floats and longs.  Typical link statement:
**
** vget31,pio,vutil31,fprintf,stdlib/s,flibrary/s,clibrary,vget31/n/e
**
**	V1.1: Modified for separate compile & link - 5/21/16
**	v1.2: Allow destination device specification.
**	v1.5: CP/M 3 release 6/1/19  (gfr)
**	v3.1: Shared source for CP/M3 and HDOS 10/13/19 (gfr)
**		Use #define HDOS, CPM2 or CPM3 to trigger
**		appropriate compilations.
**
** Glenn Roberts 16 October 2019
**
*/

#include "fprintf.h"

/* Must define HDOS, CPM2 or CPM3 here to select the
** appropriate compilation options.  Also: HDOS files
** should be saved with unix style NL line endings;
** CP/M files require MS-DOS style CR-LF line endings.
*/
#define CPM3	1

/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	FALSE	0
#define	TRUE	1

#define BUFFSIZE	256
#define FSLEN		20

/* USB i/o ports - declared globally as these are
** also referenced by the utility routines
*/
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

/* switch values */
/* if verbose is TRUE then show progress updates */
int verbose;

/* source and destination filespecs */
char srcfile[FSLEN], destfile[FSLEN];

/* vcp - copy from source file to dest file */
vcp(source, dest)
char *source, *dest;
{
	int nblocks, nbytes, i, channel;
	static long filesize, pctdone;
	static char fsize[FSLEN], rwbuffer[BUFFSIZE];
	
	if (vdirf(source, &filesize) == -1)
		printf("Unable to open file %s\n", source);
	else {
		commafmt(filesize, fsize, FSLEN);
		printf("Copying %s to %s [ %s bytes ]\n", source, dest, fsize);
	
		/* bug: assumes file size < 8 meg */
		nblocks = filesize/BUFFSIZE;
		nbytes = filesize % BUFFSIZE;
		
		/* open source file on flash device for read */
		if (vropen(source) == -1)
			printf("\nUnable to open source file %s\n", source);
		else if	((channel = fopen(dest, "wb")) == 0)
			printf("\nError opening destination file %s\n", dest);
		else {
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

			/* close file on VDIP */
			vclose(source);
	
			/* close output file */
			fclose(channel);
			
			if (verbose)
				putchar('\n');
		}
	}
}

/* oscheck - check for OK version of Operating System
** and report if there's a problem (currently only
** needed for CP/M 3)
**
**	Return: 	0 if OK, -1 if bad
*/
oscheck()
{
	int rc;
	
	rc = 0;
#ifdef CPM3	
	if ((bdoshl(12,0) & 0xF0) != 0x30) {
		printf("CP/M Version 3 is required!\n");
		rc = -1;
	}
#endif

	return rc;
}

/* dofiles - parse out the source and destination
** file specifications.  source file *must* be in
** argv[1].  If destination filespec is just a drive
** identifier then pre-pend that to the source name.
** Bug: if srcfile length > FSLEN this code may overwrite
** data.
*/
dofiles(argc, argv)
int argc;
char *argv[];
{
	int i, havedest, cindex;
	char *s;
	
	havedest = FALSE;
	/* source file must be argv[1] */
	strcpy(srcfile, argv[1]);
		
	/* destination file may not be designated, in which
	** case it defaults to same as source file, or it
	** may simply be a drive specification, in which case
	** the destination file name is the same as the
	** source, or it may be a full filespec.
	*/
	for (i=2; (i<argc) && (!havedest); i++) {
		s = argv[i];
		/* ignore switches */
		if (*s != '-') {
			/* first non-switch taken as dest filespec */
			havedest = TRUE;
			if ((cindex = index(s, ":")) == -1)
				/* no device spec found - just copy */
				strncpy(destfile, s, FSLEN-1);
			else if (s[cindex+1] == '\0') {
				/* device only - bug here if srcfile
				** is too long!
				*/
				strcpy(destfile, s);
				strcat(destfile, srcfile);
			}
			else
				/* full filespec given, just copy */
				strncpy(destfile, s, FSLEN-1);
		}
	}
	
	/* if no destination filespec was found after
	** scanning all of the arguments then just copy
	** source filespec to destination one.
	*/
	if (!havedest)
		strcpy(destfile, srcfile);
}

/* dosw - process any switches on the command line.
** Switches are preceded by "-".  If a numeric value
** is given there should be no space, for example:
** -p261 specifies port 261.  Switches must be to
** the right of any file specifications on the
** command line.  This routine also sets the defaults 
** for the port and "verbose" variables which are
** globals.  Since argv[1] is required (file name)
** this routine only looks at argv[2] and above.
*/
dosw(argc, argv)
int argc;
char *argv[];
{
	int i;
	char *s;
	
	/* Set default values */
	p_data = VDATA;
	p_stat = VSTAT;
	verbose = FALSE;
	
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

/* error - print error message.
** This routine merely prints the message.  Error
** handling is up to the calling routine.
*/
error(errno)
int errno;
{
	switch(errno) {
		case 1:
			/* incorrect OS version detected */
			printf("Wrong operating sytem version!\n");
			break;
		case 2:
			/* general help */
			printf("Usage: VGET usbfile <local> <-pxxx> <-v>\n");
			printf("\tlocal is local drive and/or filespec\n");
			printf("\txxx is USB optional port in octal (default is %o)\n", VDATA);
			printf("\t-v specifies verbose mode\n");
			break;
		case 3:
			/* error initializing USB device */
			printf("Error initializing VDIP-1 device!\n");
			break;
		case 4:
			/* Flash drive inserted in USB device */
			printf("No flash drive found!\n");
			break;
	}
}


main(argc,argv)
int argc;
char *argv[];
{	
	/* process any switches and set defaults */
	dosw(argc, argv);
	
	/* parse source and destination file specs */
	dofiles(argc, argv);

	printf("VGET v3.1 - G. Roberts.  Using USB ports: %o,%o\n",
			p_data, p_stat);

	if (oscheck() == -1)
		error(1);
	else if (argc < 2)
		error(2);
	else if (vinit() == -1)
		error(3);
	else if (vfind_disk() == -1)
		error(4);
	else
		vcp(srcfile, destfile);
}
