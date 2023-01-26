/* strip - strips carriage return characters from one or more
** ASCII files. GitHub are stored in Windows/CP/M format, 
** i.e. CR-LF at the end of each line, however HDOS prefers 
** just LF (NL) at the end of each line, and in particular
** the C/80 program will have issues if the CR characters are
** not removed
**
** usage:  strip <file1> <file2> ... <filen>
**
** wild cards are also supported, e.g.:
**
**	strip v*.c
**
** Link command:
**	L80 strip,command,seek,printf,stdlib/s,clibrary/s,strip/n/m/e
**
** Glenn Roberts - 24 January 2023
*/

#include "printf.h"

#define SECSIZE	256
#define EOF	-1
#define NUL '\0'

/* stripcr - opens file "fname" and copies it to a temporary
** file while removing any CR characters.  Then reads back
** the converted version, storing it back into the original
** file.  
*/
stripcr(fname)
char *fname;
{
	int n, fchan, tchan;
	char c;
	
	
	/* first open the input file in "read" mode and transfer
	** its contents to a TEMP file.
	*/
	if ((fchan = fopen(fname, "r")) == 0)
		printf("Error opening input file %s\n", fname);
	else if((tchan = fopen("TEMP.TMP", "w")) == 0)
		printf("Error opening TEMP.TMP\n");
	else {
		/* now strip CRs out as we read and write result to TEMP */
		n = 0;
	  while ((c=getc(fchan)) != EOF) {
			if (c != '\r') {
				putc(c, tchan);
				if (++n == SECSIZE)
					n = 0;
			}
		}
		
		/* finish out a full sector's worth of NUL characters
		** to complete the file (HDOS doesn't have an EOF 
		** byte - just pad the last sector with NUL).
		*/
		if (n > 0) 
			while (n < SECSIZE) {
				putc(NUL, tchan);
				++n;
			}
		fclose(fchan);
		
		/* TEMP file now has the converted version. Now position it
		** at the beginning and reopen the input file but this time 
		** as an output file (in write mode), then copy the contents back.
		*/
		seek(tchan, 0, 0);

		if ((fchan = fopen(fname, "w")) == 0)
			printf("Error opening output file %s\n", fname);
		else {
			/* simply copy the file */
			n = 0;
			while ((c=getc(tchan)) != EOF) {
				putc(c, fchan);
				if (++n == SECSIZE)
					n = 0;
			}
			/* finish out a full sector's worth of NUL characters
			** to complete the file (HDOS doesn't have an EOF 
			** byte - just pad the last sector with NUL).
			*/
			if (n > 0) 
				while (n < SECSIZE) {
					putc(NUL, fchan);
					++n;
				}			
			fclose(fchan);
		}
		fclose(tchan);
	}
}


main(argc, argv)
int argc;
char *argv[];
{
	int nfiles;

	/* expand wild cards on command line */
  command(&argc, &argv);

	nfiles = 0;
  while (--argc > 0) {
		++argv;
		printf("Converting %s ...", *argv);
		stripcr(*argv);
		printf(" Done!\n");
		++nfiles;
	}
	
	printf("%d files converted\n", nfiles);
}
