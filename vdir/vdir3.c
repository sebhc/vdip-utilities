/* vdir - CP/M 3 version
**
** This program implements a simple directory app for the
** Vinculum VDP-1 interfaced in parallel FIFO mode.
**
** It lists the files on the flash drive along with file size
** and date of last modification.
**
** Version 1.5
**
** Compiled with Software Toolworks C/80 V. 3.0
**
** Glenn Roberts 20 April 2013
**
**	CP/M 3 revisions 31 May 2019 - gfr
**
*/

/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	FALSE	0
#define	TRUE	1

#define	MAXD	256			/* maximum number of dir entries */
#define PROMPT	"D:\\>"		/* standard VDIP command prompt */
#define	NUL		'\0'

#include "fprintf.h"

char *strncpy();

/* USB i/o ports */
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

/* switch values */
int brief;		/* TRUE for brief listing */

/* declared in vutil library */
extern char linebuff[128];		/* I/O line buffer 		*/

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

/* union used to dissect long (4 bytes) into pieces */
union u_fil {
	long l;		/* long            */
	unsigned i[2];	/* 2 unsigned ints */
	char b[4];	/* same as 4 bytes */
};

/* array of pointers to directory entries */
struct finfo *direntries[MAXD];
int nentries;

/*********************************************
**
**	Vinculum (USB) File/Directory Functions
**
*********************************************/

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
		if (strcmp(linebuff, PROMPT) == 0)
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

/* strncpy - copies at most n characters from s2 to s1
**
** Copyright (c) 1999 Apple Computer, Inc. All rights reserved, see:
** https://www.opensource.apple.com/source/Libc/Libc-262/ppc/gen/strncpy.c 
*/
char *strncpy(s1, s2, n)
char *s1, *s2;
int n;
{
    char *s;

	s = s1;
	
	/* copy n characters but not terminating NUL */
    while ((n > 0) && (*s2 != NUL)) {
		*s++ = *s2++;
		--n;
    }

	/* if necessary, pad destination with NUL */
    while (n > 0) {
		*s++ = NUL;
		--n;
    }
	
    return s1;
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

/* aotoi - convert octal s to int */
aotoi(s)
char s[];
{
  int i, n;
	
  n = 0;
  for (i = 0; s[i] >= '0' && s[i] <= '7'; ++i)
	n = 8 * n + s[i] - '0';
  return(n);
}

/* process switches */
dosw(argc, argv)
int argc;
char *argv[];
{
	int i;
	char *s;
	
	/* process right to left */
	for (i=argc; i>0; i--) {
		s = argv[i];
		if (*s++ == '-') {
			/* have a switch! */
			switch (*s) {
			case 'P':
				++s;
				p_data = aotoi(s);
				p_stat = p_data + 1;
			    break;
			case 'B':
				brief = TRUE;
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
	int i, nfiles;
	static char fsize[15];

	/* default port values */
	p_data = VDATA;
	p_stat = VSTAT;
	
	brief = FALSE;
	
	/* process any switches */
	dosw(argc, argv);

	printf("VDIR v1.5 (CP/M 3) - G. Roberts.  Using USB ports: %o,%o\n",
			p_data, p_stat);
				
	if (vinit() == -1)
		printf("Error initializing VDIP-1 device!\n");
	else if (vfind_disk() == -1)
		printf("No flash drive found!\n");
	else {
		/* pass 1 - populate the directory array */
		vdir1();
			
		if (!brief)
			/* pass 2 - look up details on each file */
			vdir2();
			
		/* now print the listing */
		nfiles = 0;
		for (i=0; i<nentries; i++) {
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
		printf("\n%d Files\n", nfiles);		
	}
}

