/********************************************************
** vpip - Version 4 for CP/M and HDOS
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
** The program utilizes real-time clock suport to
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
**  DEST=SOURCE1,...SOURCEn -<switches>
**
** if no command is specified on the command line then the
** utility goes into a line-by-line command mode by issuing
** the :V: prompt.
**
** The pseudo-drive specification "USB:" is used to designate
** the USB-attached device.  Device specifications (e.g. A:, F:,
** for CP/M or SY0: SY1:... for HDOS) denote system drives.
**
** Files may only be copied between the USB device and the
** system devices. USB-to-USB, and system-to-system copies
** are not supported - use PIP for those.
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
** L80 vpip,vinc,vutil,pio,fprintf,flibrary/s,stdlib/s,clibrary/s,vpip/n/e
**
** This code uses ifndef to insert a call to CtlCk(), which 
** is necessary in CP/M to check for CTRL-C interrupts. This
** is not necessary for HDOS. If HDOS is not defined then the
** compiler will insert the CtrlCk() call. To compile for HDOS
** you should therfore use the command:
**
**    C -qHDOS=1 VPIP
**
** Our convention is to save all files with CP/M style
** line endings (CR-LF). The HDOS version of C/80 is picky
** and wants to see just LF (which is the HDOS standard line ending,
** therefore the CR characters must be stripped out before compilation.
** A separate STRIP.C program provides this capability.
**
**  Glenn Roberts
**  3 February 2022
**
********************************************************/
#include "fprintf.h"

/* ensure globals live here */
#define EXTERN
#include "vutil.h"
#include "vinc.h"

#define SPACE ' '
#define NULSTR  ""
#define DMA   0x80    /* CP/M DMA area */

/* default devices if none specified */
#define USBDFLT "USB"

/* device types set by checkdev() */
#define NULLD 0     /* No device given */
#define STORD 1     /* storage device */
#define USERD 2     /* user io device */
#define USBD  3     /* USB device */
#define UNKD  4     /* unknown format */

#define MAXD  400     /* maximum number of directory entries */
#define DIRBUFF 512     /* buffer space for directory */

/*********************************************
**
**  Key Data Structures
**
*********************************************/

/* destination file specification (name and
** extension are one extra character to
** accommodate terminating NUL
*/
struct fspec {
  char fname[9];
  char fext[4];
};

#ifdef HDOS
/* HDOS file directory entry data structure */
struct hdinfo {
  char hdname[8];
  char hdext[3];
  char prj;
  char ver;
  char clf;
  char flags;
  char rsvd;
  char fgn;
  char lgn;
  char lsi;
  int credate;
  int moddate;
};

/* pointers to Active IO Area */
char *aiospg  = 0x2126;
char **aiogrt = 0x2124;

/* grt (for HDOS directory manipulation) */
char *grt;        /* Group Reservation Table */

#else
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
#endif

/* The program builds an internal representation of
** the list of files to be copied using the following
** data structure. The 'tag' field is used to flag
** entries that match the user-specified criteria.
**
** time/date format follows FAT specification (used
** by the Vinculum software and USB flash drive):
**
** mdate:
**  9:15  Year  0..127  (0=1980, 127=2107)
**  5:8   Months  1..12 (1=Jan., 12=Dec.)
**  0:4   Days  1..31 (1=first day of mo.)
**
** mtime:
**  11:15   Hours 0..23   (24 hour clock)
**  5:10  Minutes 0..59
**  0:4   Sec./2  0..29   (0=0, 29=58 sec.)
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
};

/*********************************************
**
**  Global Static Storage
**
*********************************************/
char cmdline[80];
char dstfname[15];
char srcfname[15];

/* devices for source and destination */
char srcdev[4], dstdev[4];
int srctype, dsttype;

/* array of pointers to source filespecs */
struct fspec *src[MAXD];
int nsrc;

/* filespec for destination */
struct fspec dstspec;

/* array of pointers to directory entries */
struct finfo *direntry[MAXD];
int nentries;

/* buffer space for reading directory */
char buffer[DIRBUFF];

/* buffer used for read/write */
#define BUFFSIZE  256
char rwbuffer[BUFFSIZE];

/* global switch settings */
int f_list;   /* to list directory (no file copy) */


#ifdef HDOS
/*********************************************
**
**  HDOS File/Directory Functions
**
*********************************************/

/* bldldir (HDOS version) - read HDOS system directory file
** (DIRECT.SYS) for the specified device and populate the 
** directory array, dynamically allocating memory for each
** entry.  Device is of the form "SY0", "DK0", etc.
*/
bldldir(device)
char *device;
{ 
  int i, j, cc, channel, done, block, rc;
  unsigned clu;
  struct hdinfo hdosentry;
  struct finfo *entry;
  char *src, *dst;
  char spg;
  static char dfname[20];

  rc = 0;

  strcpy(dfname, device);
  strcat(dfname, ":DIRECT.SYS");
  if ((channel = fopen(dfname, "rb")) == 0) {
    printf("Error - unable to open %s\n", dfname);
    rc = -1;
  }
  else {
    /* fetch SPG and GRT from Active I/O area */
    spg = *aiospg;
    grt = *aiogrt;
    
    done = FALSE;
    nentries = 0;
    block = 0;
    do {
      /* read a chunk of the directory
      ** (512 bytes at a time)
      */
      if (read(channel, buffer, DIRBUFF) < DIRBUFF)
        done = TRUE;
      else {
        ++block;
        
        /* each of these chunks holds 22 directory
        ** entries. loop over them and store them
        ** in the array (allocating space as we go
        */
        src = buffer;
        for (i=0; ((i<22) && (!done)); i++) {
          if (isprint(*src)){
            /* fill out the HDOS file entry */
            dst = (char *) &hdosentry;
            for (j=0; j<sizeof(hdosentry); j++)
              *dst++ = *src++;
                
            /* trace cluster chain to compute cluster count */
            clu = hdosentry.fgn & 0xFF;
            cc = 0;
            do {
              ++cc;
              clu = grt[clu] & 0xFF;
            } while (clu != 0);
              
            /* allocate an entry for our directory */
            if ((entry = alloc(sizeof(struct finfo))) == 0) {
              printf("Error allocating directory entry!\n");
              done = TRUE;
              break;
            }
            /* copy pertinent HDOS fields to our entry */
            for (j=0; j<8; j++)
              entry->name[j] = hdosentry.hdname[j];
            entry->name[8] = NUL;
            for (j=0; j<3; j++)
              entry->ext[j] = hdosentry.hdext[j];
            entry->ext[3] = NUL;

            /* convert file size to bytes */
            entry->size = 256L * ((cc-1)*spg + hdosentry.lsi);
            
            /* convert HDOS date */
            /* (HDOS uses 1970 as base year, adjust down by 10 */
            entry->mdate = hdosentry.moddate - 0x1400;
            
            /* clear other fields */
            entry->mtime = 0;
            entry->isdir = FALSE;
            entry->tag = FALSE;
            
            /* now store it in our directory list */
            direntry[nentries++] = entry;
          }
          else
            /* deleted entry, skip to next */
            src += sizeof(hdosentry);
        }
      }
    } while (!done);
    fclose(channel);
  }
  return rc;
}

#else
  
/*********************************************
**
**  CP/M File/Directory Functions
**
*********************************************/
/* bldldir (CP/M version) - read CP/M system directory
** using BDOS functions 17/18 and *.* to match all files
** on the specified device, then populate the directory
** array, dynamically allocating memory for each entry.
** Device is the drive identifier, e.g. "A", "B", etc.
*/
int bldldir(device)
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
  /* loop until there are either no more entries in the
  ** CP/M directory or no more space to store them in
  ** our local copy.
  */
  while (((i = bdos(bfn,fcb)) != -1) && (nentries < MAXD)) {
    /* have a match */
    if ((entry = alloc(sizeof(struct finfo))) == 0)
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
  if (nentries == MAXD)
    printf("Warning: too many files on local disk\n");
}
#endif

/* wcexpand - expand wild cards in string.  if '*' is first
** then terminate the string after it, otherwise expand
** '*' to one or more '?' characters.  no error checking
** is performed.
*/
int wcexpand(s, l)
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
** string. This routine uses islegal() to check for the 
** legality of each character in forming the destination
** string (rules for HDOS and CP/M are different).
** If illegal characters are found they are simply omitted.
** This is a rather simplistic way to handle the problem and
** can lead to issues, so a more thorough approach to file name
** validation would be better.
*/
int dstexpand(entry, dspec, dname)
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
      if (islegal(c,i))
        *d++ = c;
    }
    else if (islegal(s,i))
      *d++ = s;
  }

  /* do 3 character file extension */
  wild = (dspec->fext[0] == '*');
  for (i=0; i<3; i++) {
    s = dspec->fext[i];
    if (wild || (s == '?')) {
      c = entry->ext[i];
      if (islegal(c,i+8)) {
        if (i == 0)
          *d++ = '.';
        *d++ = c;
      }
    }
    else if (islegal(s,i+8)) {
      if (i == 0)
        *d++ = '.';
      *d++ = s;
    }
  }

  /* make sure string is terminated! */
  *d = NUL;
}

/* islegal - examines the provided character and
** returns TRUE if it is an allowable filename
** character. The rules are different for HDOS and
** CP/M. The parameter 'p' is the position of the
** character in the 11-character file name (0..10)
** This is needed because in HDOS the first character
** (p=0) must be a letter.
*/
int islegal(c, p)
char c;
int p;
{
	int ok;
	char s[2];
	
	ok = (isalpha(c) || isdigit(c)) ? TRUE : FALSE;
#ifdef HDOS
  /* in HDOS file names must start with a letter */
  if ((p==0) && !isalpha(c))
		ok = FALSE;
#else
  /* CP/M allows more characters ...*/
	s[0] = c;
	s[1] = '\0';
	if (!ok && (index("_$!%-@`^~#&{}'()",s) != -1))
		ok = TRUE;
#endif

/* DEBUGGING - print message if illegal character found */
	if (!ok)
		printf("*** Illegal filename character '%c' [%02x]\n", c, c & 0xFF);
	
	
	return ok;
}

/* dirstr - return a directory entry as a string 
** this routine essentially concatenates the name
** and extension portions with a '.' in the middle
** returning the result as a null-terminated string.
*/
int dirstr(e, s)
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
**  Vinculum (USB) File/Directory Functions
**
*********************************************/

/* bldudir - perform directory on the USB device and
** populate the directory array, dynamically allocating
** memory for each entry.
*/
int bldudir()
{
  int n;
  
  printf("Building USB directory...  ");
  /* pass 1 - populate the directory array */
  n = vdir1();
  printf("%d entries\n", n);
  
  printf("Standby - cataloging USB file details...\n");
  /* pass 2 - look up details on each file */
  vdir2();
}

/* vdir1 - This routine does "pass 1" of the directory
** using the 'dir' command fill out the array of directory 
** entries (direntry) by allocating space for each one
**
** (code is from VDIR utility)
**
** returns number of directory entries on success, 
** -1 on error.
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
  
  /* read each line and add it to the list,
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
      printf("error allocating directory entry!\n");
      rc = -1;
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
#ifndef HDOS
    /* check for ^C */
    CtlCk();
#endif
  } while (!done);
  
  /* if no errors return number of entries */
  return (rc == 0) ? nentries : rc;
}

/* vdir2 - This routine does "pass 2" of the directory 
** for each entry in the table it does a more extensive
** query gathering file size and other information
*/
int vdir2()
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
#ifndef HDOS
    /* check for ^C */
    CtlCk();
#endif
  }
}

/* vcput - put a file from local source to USB destination
**  (derived from code in VPUT)
**
**  Source: local file
**  Destination: USB (VDIP)
**
**  Returns:   -1 on error
*/
int vcput(source, dest)
char *source, *dest;
{
  int nbytes, channel, done, rc;
  long filesize;
  char fsize[15];
  
  rc = 0;
  
  if((channel = fopen(source, "rb")) == 0) {
    printf("Unable to open source file %s\n", source);
    rc = -1;
  }
  else {
    /* first set up the file date for vwopen(), display
    ** it on console the first time around
    */
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
    
      done = FALSE;
      while (!done) {
        nbytes = read(channel, rwbuffer, BUFFSIZE);
        filesize += nbytes;
        if (nbytes == 0)
          done = TRUE;
        else if (vwrite(rwbuffer, nbytes) == -1) {
          printf("\nError writing to VDIP device\n");
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

/* vcget - get file from USB source to local destination
**  (derived from code in VGET)
**
**  Source: USB (VDIP)
**  Destination:  local file
**
**  Returns:   -1 on error
*/
int vcget(source, dest)
char *source, *dest;
{
  int nblocks, nbytes, i, channel, rc, done;
  long filesize;
  char fsize[15];
  
  rc = 0;
  
  if (vdirf(source, &filesize) == -1) {
    printf("Unable to open file %s\n", source);
    rc = -1;
  }
  else {
    commafmt(filesize, fsize, 15);
    printf("USB:%-12s  %s bytes --> ", source, fsize);
  
    /* NOTE: assumes file size < 8 meg */
    nblocks = filesize/BUFFSIZE;
    nbytes = filesize % BUFFSIZE;
    
    /* open source file on flash device for read */
    if (vropen(source) == -1) {
      printf("Unable to open source file %s\n", source);
      rc = -1;
    }
    else if ((channel = fopen(dest, "wb")) == 0) {
      printf("\nError opening destination file %s\n", dest);
      rc = -1;
    }
    else {
      /* source and destination files open - begin copying */
      for (done=FALSE, i=1; ((i<=nblocks) && (!done)); i++) {
        /* read a block from input file */
        if (vread(rwbuffer, BUFFSIZE) == -1) {
          printf("\nError reading block %d\n", i);
          done = TRUE;
          rc = -1;
        }
        else if ((write(channel, rwbuffer, BUFFSIZE)) == -1) {
          printf("\nError writing to %s\n", dest);
          rc = -1;
          done = TRUE;
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
      
      /* report results */
      printf("%-12s\n", dest);

      /* close file on VDIP */
      vclose(source);
  
      /* close output file */
      fclose(channel);
    }
  }

  return rc;
}

/* listmatch - print device directory listing from
** stored array (direntry).  Lists only entries with the 
** "tag" field set to TRUE.  For USB files the size and
** time/date information is included.
*/
int listmatch()
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
int copyfiles()
{
  int i, ncopied;
  static char fullname[20];
  
  /* loop over entries and perform copy */
  for (i=0, ncopied=0; i<nentries; i++) {
    /* copy tagged files (but not directories!) */
    if(direntry[i]->tag && !direntry[i]->isdir) {
      dirstr(i, srcfname);
      if ((srctype == STORD) && (dsttype == USBD)) {
        /* do a "put" (local file --> USB) */
        fullname[0] = NUL;
        strcat(fullname, srcdev);
        strcat(fullname,":");
        strcat(fullname, srcfname);
        dstexpand(direntry[i], &dstspec, dstfname);
        if ((vcput(fullname, dstfname)) != -1);
          ++ncopied;
      }
      else if ((srctype == USBD) && (dsttype == STORD)){
        /* do a "get" (USB --> local file) */
        fullname[0] = NUL;
        strcat(fullname, dstdev);
        strcat(fullname,":");
        dstexpand(direntry[i], &dstspec, dstfname);
        strcat(fullname, dstfname);
        if ((vcget(srcfname, fullname)) != -1)
          ++ncopied;
      }
    }
  }
  printf("\n%d Files Copied\n", ncopied);
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
int parsefs(sfs, dev, s)
struct fspec *sfs;
char *dev, *s;
{
  int i, iscan;

  for (i=0; i<4; i++)
    dev[i] = NUL;
  sfs->fname[0] = NUL;
  sfs->ext[0] = NUL;
  
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
  
  /* We allow just a device specification, e.g. A: or
  ** SY0:.  This means A:*.* or SY0:*.*.  Here we 
  ** check for xxx: and expand it to xxx:*.*
  */
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
int padblanks(s, l)
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
*/
int devtype(d)
char *d;
{
  int dtype;
  
  if (strlen(d) == 0)
    dtype = NULLD;
  else if (strcmp(d, "USB") == 0)
    dtype = USBD;
#ifdef HDOS
  /* HDOS has 2 or 3 letters, e.g. TT: or SY0: */
  else if ((strlen(d) == 2) && isalpha(d[0]) && isalpha(d[1]))
    dtype = USERD;
  else if ((strlen(d) == 3) && isalpha(d[0]) && isalpha(d[1]) && isdigit(d[2]))
    dtype = STORD;
#else
  /* CP/M uses single-letter drive designation */
  else if ((strlen(d) == 1) && isalpha(d[0]))
    /* Storage device, e.g. disk drive */
    dtype = STORD;
#endif
  else
    dtype = UNKD;
  
  return dtype;
}

/* checkdev - do validation check on specified devices
**
**  Return codes:
**    0: normal return, no error
**    1: one or more unknown devices specified
**    2: no USB device specified
**    3: both source and dest are USB (not allowed)
**    4: one or both devices are user devices (e.g. TT: LP:, etc.)
*/
int checkdev()
{
  int rc;
#ifdef HDOS
  static char sysdflt[] = "SY0";
#else
  static char sysdflt[] = "A";
  
  /* for CP/M set current default disk */
  sysdflt[0] = 'A' + bdos(25,0);
#endif

  rc = 0;
  
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
** single character.  
**
** This routine traverses the directory structure
** (in memory) and sets the 'tag' field TRUE for
** any entries that match.
**
** wild cards never match directory entries ("<DIR>").
*/
int domatch(cname, cext)
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
int docmd(s)
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
    if ((entry = alloc(sizeof(struct fspec))) == NUL) {
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
          /* build local directory */
          bldldir(srcdev);
        else
          /* build USB directory */
          bldudir();
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

/* freeall - free dynamically allocated space */
int freeall()
{
  int i;
  
  /* free filespec list */
  for (i=0; i<nsrc; i++)
    free(src[i]);
  
  /* free directory entries */
  for (i=0; i<nentries; i++)
    free(direntry[i]);
}

/* process switches */
int dosw(argc, argv)
int argc;
char *argv[];
{
  int i;
  char *s;
  
  /* default port values */
  p_data = VDATA;
  p_stat = VSTAT;
  
  /* default flag settings */
  f_list = FALSE;

  /* process right to left */
  for (i=argc; i>0; i--) {
    s = argv[i];
    if (*s++ == '-') {
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

main(argc,argv)
int argc;
char *argv[];
{
  int l;

  /* set globals 'os' and 'osver' to direct use of time and
  ** date functions
  */
  getosver();
  
  /* process any switches */
  dosw(argc, argv);

  printf("VPIP v4 [%o]\n",  p_data);

  if (argc < 2) {
    /* interactive mode */
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
