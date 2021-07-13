/* vdir.c -- This program implements a simple directory app for the
** Vinculum VDP-1 interfaced in parallel FIFO mode.
**
** It lists the files on the flash drive along with file size
** and date of last modification.
**
** Version 1.0
**
** Compiled with Software Toolworks C/80 V. 3.0
**
** Glenn Roberts 20 April 2013
*/

#define	FALSE	0
#define	TRUE	1

/* include float/long printf code */
#include "fprintf.c"

/* VDIP utility routines */
#include "vutil.c"

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
			direntries[i]->mdate = 0L;
		}
		else {
			vdirf(dirtemp, &direntries[i]->size);
			vdird(dirtemp, &direntries[i]->mdate);
		}
	}
}


main(argc,argv)
int argc;
char *argv[];
{	
	int i;
	static char fsize[15];
	
	if (vinit() != -1) {
		if (vfind_disk()) {
			/* pass 1 - populate the directory array */
			vdir1();
			
			/* pass 2 - look up details on each file */
			vdir2();
			
			/* now print the listing */
			for (i=0; i<nentries; i++)
				if (direntries[i]->isdir) {
					/* directory entry */
					printf("%-8s      <DIR>\n",
						direntries[i]->name);
				}
				else {
					/* file name entry */
					commafmt(direntries[i]->size, fsize, 15);
					printf("%-8s %-3s %s  ",
						direntries[i]->name,
						direntries[i]->ext,
						fsize);
					/* print date */
					prndt(direntries[i]->mdate);
					printf("\n");
				}
			printf("%d file(s)\n", nentries);
		}
		else
			printf("No flash drive found!\n");
	}
	else
		printf("Error initializing VDIP-1 device!\n");
}

#include "stdlib.c"

