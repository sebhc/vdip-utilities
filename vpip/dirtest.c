/********************************************************
**
** dirtest - test jig program to make sure I understand
**	how to call the CP/M directory functions... used to
**	develop/debug the bldcdir() routine which builds a
**	dynamically allocated array of directory entries.
**	This routine is a key part of the vpip program.
**
**	This program reads the directory of drive A: and prints
**	out file name, size and time/date the file was last
**	updated.
**
**	Utilizes routines in vutil32.c
**
**	For Software Toolworks C/80 Rev. 3.1 with float/long
**
**	gfr 9 Mar 2020
*/
#include "fprintf.h"

#define	TRUE	1
#define	FALSE	0

#define	NUL		'\0'
#define	DMA		0x80		/* CP/M DMA area */
#define MAXD	256			/* maximum number of directory entries */

/* CP/M file directory entry data structure */
struct cpminfo {
	char user;
	char fname[8];
	char fext[3];
	char extent;
	int  reserved;
	char recused;
	char abused[16];
} cpmentry;

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

/* hex value of time/date kept here */
char td_string[15];

/* array of pointers to directory entries */
struct finfo *direntry[MAXD];
int nentries;


/* bldcdir - read CP/M system directory file for specified
** device and populate directory array, dynamically 
** allocating memory for each entry.  Device is the drive
** identifier, e.g. "A", "B", etc.
*/
bldcdir(device)
char *device;
{	
	int i, j, bfn;
	char *l;
	struct finfo *entry;
	static int date;
	static char hour, minute;
	static char fcb[36];
	static char dfname[20];
	static long filesize;
	static int mydate[3];	/* day, month, year */
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
			/* copy pertinent HDOS fields to our entry */
			for (j=0; j<8; j++)
				entry->name[j] = ourentry->fname[j];
			entry->name[8] = NUL;
			for (j=0; j<3; j++)
				entry->ext[j] = ourentry->fext[j];
			entry->ext[3] = NUL;
			
			/* tag all as 'TRUE' for list matching */
			entry->tag = TRUE;
			
			/* now store the entry and bump the count */
			direntry[nentries++] = entry;
		}
		bfn = 18;
	}
	printf("%d matching entries found\n", nentries);
	
	/* now loop through entries computing file size */
	for (i=0; i<nentries; i++) {
		/* first build up an appropriate FCB */
		strcpy(dfname,device);
		strcat(dfname,":");
		strcat(dfname,direntry[i]->name);
		strcat(dfname,".");
		strcat(dfname,direntry[i]->ext);
		makfcb(dfname, fcb);
		
		/* Use BDOS function 35 to compute file size */
		bdos(35,fcb);
		l = (char *) &filesize;
		/* random record no. in fcb[33..35] */
		for (j=33; j<36; j++)
			*l++ = fcb[j];
		*l = 0;
		filesize *= 128L;
		direntry[i]->size = filesize;
		
		/* Use BDOS function 102 to read date stamp */
		bdos(102,fcb);
		l = (char *) &date;
		/* Use "update" time in fcb[28..31] */
		*l++ = fcb[28];
		*l = fcb[29];
		hour = fcb[30];
		minute = fcb[31];
		
		/* debugging...
		printf("%s  size: %ld   date: %d  hour: %d  minute: %d\n",
			dfname, filesize, date, hour, minute);
		*/

		/* convert and store time value */
		direntry[i]->mtime = (btod(minute) << 5) + (btod(hour) << 11);
		
		/* convert and store date */
		dodate(date, mydate);
		direntry[i]->mdate = mydate[0] + 
			(mydate[1] << 5) + 
			((mydate[2]-1980) << 9);
	}
}

/* listmatch - print device directory listing from
** stored array.  Lists only entries with the "tag"
** field set to TRUE.  if the argument "brief" is TRUE
** then just do an abbreviated listing.
*/
listmatch(brief)
int brief;
{
	int i, nfiles;
	static char fsize[15];
	
	nfiles = 0;
	for (i=0; i<nentries; i++) {
		if(direntry[i]->tag) {
			++nfiles;
			printf("%-8s.%-3s    ", 
				direntry[i]->name,
				direntry[i]->ext);
			commafmt(direntry[i]->size, fsize, 15);
			printf("%10s  ", fsize);
			prndate(direntry[i]->mdate);
			printf("  ");
			prntime(direntry[i]->mtime);
			printf("\n");
		}
	}
	printf("\n%d Files\n", nfiles);
}

main(argc,argv)
int argc;
char *argv[];
{
	printf("dirtest - 1 March 2020 - gfr\n");

	bldcdir("A");
	listmatch(TRUE);
}
