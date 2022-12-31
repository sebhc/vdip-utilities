/********************************************************
** vput - Version 4 for CP/M and  HDOS
**
** This program copies one or more files from CP/M or HDOS to a
** USB flash device. Under CP/M wild card copies are supported;
** HDOS users are limited to single file copies (use VPIP for
** multiple files).  VPUT may be preferred to VPIP for copying
** only a few files because it is fast.
**
** Usage: 
**
**	vput file1 {file2} {file3} ... {-pxxx}
**
** "wildcard" expansion with "*" and "?" are supported in CP/M version
**
** switches:
**		-pxxx	to specify octal port (default is 0331)
**
** Compiled with Software Toolworks C/80 V. 3.1.
**
** L80 vput,vinc,vutil,pio,fprintf,command,flibrary/s,stdlib/s,clibrary/s,vput/n/e
**
** Glenn Roberts
** 1 February 2022
**
********************************************************/
#include "fprintf.h"

/* ensure globals live here */
#define EXTERN
#include "vutil.h"
#include "vinc.h"

#define BUFFSIZE	256

/* buffer used for read/write */
char rwbuffer[BUFFSIZE];

/* vcput - copy from CP/M or HDOS source file to 
** USB: destination file
**
** 	Returns:	 -1 on error
*/
int vcput(source, dest)
char *source, *dest;
{
	int nbytes, channel, done, rc;
	long filesize;
	long fsize[15];
	
	rc = 0;
	
	if((channel = fopen(source, "rb")) == 0) {
		printf("Unable to open source file %s\n", source);
		rc = -1;
	}
	else {
		/* first set up the file date for vwopen() */
		settd(TRUE);
		
		/* now open destination file on USB: and do the copy */
		if (vwopen(dest) == -1) {
			printf("Unable to open destination file %s\n", dest);
			rc = -1;
			fclose(channel);
		}
		else {
			/* start writing at beginning of file */
			vseek(0);
			filesize = 0L;
			printf("%-16s --> ", source);

			/* copy one block at a time */
			done = FALSE;
			while (!done) {
				nbytes = read(channel, rwbuffer, BUFFSIZE);
				filesize += nbytes;
				if (nbytes == 0) {
					done = TRUE;
				}
				else if (vwrite(rwbuffer, nbytes) == -1) {
					printf("Error writing to VDIP device\n");
					rc = -1;
						done = TRUE;
				} 
			}
		}
		commafmt(filesize, fsize, 15);
		
		
		/* report results */
		printf("USB:%-12s  %s bytes\n", dest, fsize);

		/* close input file */
		fclose(channel);

		/* important - close file on VDIP */
		vclose(dest);
	}
		
	return rc;
}


/* dosw - process any switches on the command line.
** Switches are preceded by "-".  If a numeric value
** is given there should be no space, for example:
** -p261 specifies port 261.  Switches must be to
** the right of any file specifications on the
** command line.  This routine also sets the defaults 
** for the port variables which are globals.
** Since argv[1] is required (file name)
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
	char *srcfile, *destfile;
	
	/* set globals 'os' and 'osver' to direct use of time and
	** date functions
	*/
	getosver();

#ifndef HDOS	
	/* for CP/M expand any wild cards on command line,
	** (unfortunately not available in HDOS)
	*/
	command(&argc, &argv);
#endif 

	/* process any switches */
	dosw(argc, argv);

	printf("VPUT v4 [%o]\n", p_data);
			
	if (argc < 2) {
		printf("Usage: VPUT file {file} {file} ... <-pxxx>\n");
		printf("\tlocal is local drive and/or filespec\n");
		printf("\txxx is USB optional port in octal (default is %o)\n", VDATA);
	}
	else if (vinit() == -1)
		printf("Error initializing VDIP-1 device!\n");
	else if (vfind_disk() == -1)
		printf("No flash drive found!\n");
	else {
		/* all is good copy the file(s) */
		for (i=1; i<argc; i++) {
#ifndef HDOS
			/* check for ^C */
			CtlCk();
#endif
			srcfile = argv[i];
			/* switch fields are ignored! */
			if (*srcfile != '-') {
				/* need to strip off any drive ID on destination*/
				destfile = argv[i];
				if ((offset = index(destfile, ":")) != -1)
					/* found! skip past ':' */
					destfile += offset + 1;
	
				vcput(srcfile, destfile);
			}
		}
	}
}
