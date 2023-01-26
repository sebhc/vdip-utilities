/********************************************************
** vdir - Version 4 for CP/M and HDOS
**
** This program reads and prints the directory information
** for a USB flash device. It lists the file names along with
** file size and date of last modification.
**
** Because of the way the FTDI Vinculum software is designed,
** a full directory listing requires two passes - one to assemble
** a list of files and the second pass to query each file for
** detailed information.  If only a list of files is neeeded
** the user may specify the '-b' ("brief") switch, which requires
** only a single pass and is much faster.
**
** Usage:  vdir {-pxxx} {-b}
**
** Compiled with Software Toolworks C/80 V. 3.1 with support for
** floats and longs.  Typical link statement:
**
** L80 vdir,vinc,vutil,pio,fprintf,flibrary/s,stdlib/s,clibrary/s,vdir/n/e
**
** This code uses ifndef to insert a call to CtlCk(), which 
** is necessary in CP/M to check for CTRL-C interrupts. This
** is not necessary for HDOS. If HDOS is not defined then the
** compiler will insert the CtrlCk() call. To compile for HDOS
** you should therfore use the command:
**
**    C -qHDOS=1 VDIR
**
** Our convention is to save all files with CP/M style
** line endings (CR-LF). The HDOS version of C/80 is picky
** and wants to see just LF (which is the HDOS standard line ending,
** therefore the CR characters must be stripped out before compilation.
** A separate STRIP.C program provides this capability.
**
** Glenn Roberts 1 February 2022
**
********************************************************/
#include "fprintf.h"

/* ensure globals live here */
#define EXTERN
#include "vutil.h"
#include "vinc.h"

/* maximum number of dir entries */
#define MAXD  400

int brief;    /* TRUE for brief listing */
int nfiles;   /* file counter */

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
};

/* array of pointers to directory entries */
struct finfo *direntry[MAXD];
int nentries;

/*********************************************
**
**  Vinculum (USB) File/Directory Functions
**
*********************************************/

/* vdir1 - This routine does "pass 1" of the directory
** using the 'dir' command fill out the array of directory 
** entries (direntry) by allocating space for each one.
**
** returns 0 on success, -1 on error.
*/
int vdir1()
{
  int i, ind, done, rc;
  struct finfo *entry;
  
  /* Issue directory command */
  str_send("dir\r");
  
  /* the first line is always blank - toss it! */
  str_rdw(linebuff, '\r');

  done = FALSE;
  nentries = 0;
  rc = 0;
  
  /* Read each line and add it to the list,
  ** when the D:\> prompt appears, we're done. the
  ** list entries are dynamically allocated.
  */
  do {
    str_rdw(linebuff, '\r');
    /* VDIP will return prompt line when the 
    ** directory listing is complete.
    */
    if (strcmp(linebuff, PROMPT) == 0) {
      done = TRUE;
    }
    /* check for too many files on drive  */
    else if (nentries == MAXD) {
      printf("Error: more than %d files on drive.\n", MAXD);
      rc = -1;
      done = TRUE;
    }
    /* dynamically alloocate an entry */
    else if ((entry = alloc(sizeof(struct finfo))) == 0) {
      printf("Error: Insufficient memory.\n");
      rc = 1;
      done = TRUE;
    }
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
  
  return rc;
}

/* vdir2 - This routine does "pass 2" of the directory 
** for each entry in the table. It is called only if
** a "long" listing is needed. It does a more extensive
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
    if (direntry[i]->isdir) {
      direntry[i]->size  = 0L;
      direntry[i]->mdate = 0;
      direntry[i]->mtime = 0;
    }
    else {
      vdirf(dirtemp, &direntry[i]->size);
      vdird(dirtemp, &direntry[i]->mdate, &direntry[i]->mtime);
    }
    if (!brief)
      prentry(i);
    
#ifndef HDOS
    /* check for ^C */
    CtlCk();
#endif
  }
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

/* process switches */
dosw(argc, argv)
int argc;
char *argv[];
{
  int i;
  char *s;

  /* Set default values */
  p_data = VDATA;
  p_stat = VSTAT;
  brief = FALSE;
  
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

/* prentry - print an entry in the directory
** in "long" form
*/
int prentry(i)
{
  static char fsize[15];
  
  printf("%-8s", direntry[i]->name);
  if (direntry[i]->isdir)
    printf(" <DIR>  ");
  else {
    /* files only: display estention, size,
    ** date and time (if non-zero)
    */
    ++nfiles;
    printf(".%-3s    ", direntry[i]->ext);
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


main(argc,argv)
int argc;
char *argv[];
{ 
  int i;

  /* set globals 'os' and 'osver' to direct use of time and
  ** date functions
  */
  getosver();
  
  /* process any switches */
  dosw(argc, argv);

  printf("VDIR v4 [%o]\n", p_data);
        
  if (vinit() == -1)
    printf("Error initializing VDIP-1 device!\n");
  else if (vfind_disk() == -1)
    printf("No flash drive found!\n");
  else {
    /* count only files (not directories) */
    nfiles = 0;
    
    /* pass 1 - populate the directory array */
    vdir1();
    
    if (!brief) {
      /* pass 2 - look up details on each file and list
      ** results
      */
      vdir2();
      
    } else {
      /* print brief 4-column file listing */
      for (i=0; i<nentries; i++) {
        printf("%-8s", direntry[i]->name);
        if (direntry[i]->isdir)
          printf(" <DIR>  ");
        else {
          ++nfiles;
          printf(".%-3s    ", direntry[i]->ext);
        }
        
        /* add line break every four entries */
        if (((i+1) % 4) == 0)
          printf("\n");
#ifndef HDOS
        /* check for ^C */
        CtlCk();
#endif
      }
      /* terminate the line */
      printf("\n");
    }
    
    /* sign off with total file count */
    printf("\n%d Files\n", nfiles);   
  }
}
