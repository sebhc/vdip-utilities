/********************************************************
** vget - Version 4.1 for CP/M and HDOS
**
** This program copies a single file from a USB flash device
** to the local CP/M or HDOS file system. For multiple-file
** copies use VPIP. VGET may be preferred over VPIP for 
** copying only a few files because it is fast.
**
** Usage: vget file {dest} {-pxxx}
**
**    'file' is the name of the source file on the USB drive.
**    Only "8.3" file names are supported (no long file names).
**
**    {dest} is an optional drive ID, e.g. D: (CP/M) or
**    SY1: (HDOS), or a complete file specification (e.g.
**    D:NEWFILE.DAT or DK0:ANYNAME.TXT). If {dest} is not
**    specified the destination file takes the same name as
**    the source file and is created on the current or default
**    drive.  If only a device name is specified for {dest}
**    then the source file name is used but the file is saved
**    on the specified device. Existing files are overwritten
**    without warning. For operating systems that support time
**    and date stamping the current time/date are used when
**    creating the file.
**
**    switches:
**      -pxxx to specify octal port (default is 0331)
**
** Compiled with Software Toolworks C/80 V. 3.1 with support for
** floats and longs.  Typical link statement:
**
** L80 vget,vinc,vutil,pio,fprintf,scanf,flibrary/s,stdlib/s,clibrary/s,vget/n/e
**
** Our convention is to save all files with CP/M style
** line endings (CR-LF). The HDOS version of C/80 is picky
** and wants to see just LF (which is the HDOS standard line ending,
** therefore the CR characters must be stripped out before compilation.
** A separate STRIP.C program provides this capability.
**
** Glenn Roberts
** 30 January 2022
**
** 27 October 2024 - added ability to read port number from
** configuration file. Updated to V4.1.
**
********************************************************/
#include "fprintf.h"

/* ensure globals live here */
#define EXTERN
#include "vutil.h"
#include "vinc.h"

#define BUFFSIZE  256
#define FSLEN   20

/* buffer used for read/write */
char rwbuffer[BUFFSIZE];

/* source and destination filespecs */
char srcfile[FSLEN], destfile[FSLEN];

/* vcget - get file from USB source to local destination
**  (derived from code in VGET)
**
**  Source: USB (VDIP)
**  Destination:  local file
**
**  Returns:   -1 on error
*/
vcget(source, dest)
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
      printf("\nUnable to open source file %s\n", source);
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
          printf("Error reading final block\n");
          rc = -1;
        }
        else if ((write(channel, rwbuffer, BUFFSIZE)) == -1) {
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
  printf("VGET v%s\n", VERSION);

  /* Set default values */
  p_data = VDATA;
  p_stat = VSTAT;

  /* set globals 'os' and 'osver' to direct use of time and
  ** date functions
  */
  getosver();
  
	/* check if user has a file specifying the port. For
	** HDOS we can provide the location of this program executable
	** but for CP/M we can only suggest looking on A:
	*/
#ifdef HDOS
	chkport("SY0:");
#else
	chkport("A:");
#endif

  /* process any switches and set defaults */
  dosw(argc, argv);

  printf("Using port: [%o]\n", p_data);
  
  /* parse source and destination file specs */
  dofiles(argc, argv);


  if (argc < 2) {
  printf("Usage: VGET usbfile {local} <-pxxx>\n");
    printf("\tlocal is local drive and/or filespec\n");
    printf("\txxx is USB optional port in octal (default is %o)\n", VDATA);
  }
  else if (vinit() == -1) {
    printf("Error initializing VDIP-1 device!\n");
  }
  else if (vfind_disk() == -1) {
    printf("No flash drive found!\n");
  }
  else
    /* all is good - execute single file copy */
    vcget(srcfile, destfile);
}
