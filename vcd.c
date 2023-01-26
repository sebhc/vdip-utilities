/********************************************************
** vcd - Version 4 for CP/M and HDOS
**
** This program implements a simple Change Directory app to change
** the current directory on the attached USB flash drive.
**
** Usage:  vcd path
**
** It changes the directory to the value specified on
** the command line.  Unix-style forward slash characters ("/")
** are used to indicate directory levels.  If the specified
** directory path begins with '/' it is taken as an absolute
** path (rooted), otherwise the path is taken as relative to
** the current directory.
**
** This code is OS agnostic - should compile and run on
** HDOS and all versions of CP/M.
**
** Compiled with Software Toolworks C/80 V. 3.1 with support for
** floats and longs.  Typical link statement:
**
** L80 vcd,vinc,vutil,pio,fprintf,flibrary/s,stdlib/s,clibrary/s,vcd/n/e
**
** Our convention is to save all files with CP/M style
** line endings (CR-LF). The HDOS version of C/80 is picky
** and wants to see just LF (which is the HDOS standard line ending,
** therefore the CR characters must be stripped out before compilation.
** A separate STRIP.C program provides this capability.
**
** Glenn Roberts
** 31 January 2022
**
********************************************************/
#include "fprintf.h"

/* ensure globals live here */
#define EXTERN
#include "vutil.h"
#include "vinc.h"

char dircopy[80];

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
  char *d, *s;
  int slash;
  
  /* set globals 'os' and 'osver' to direct use of time and
  ** date functions
  */
  getosver();

  /* process any switches */
  dosw(argc, argv);

  printf("VCD v4 [%o]\n", p_data);
        
  if (vinit() == -1)
    printf("Error initializing VDIP-1 device!\n");
  else if (vfind_disk() == -1)
    printf("No flash drive found!\n");
  else if ((argc < 2) || (index(argv[1], "\\") != -1)) {
    printf("Usage: vcd <directory> <-pxxx>\n");
    printf("Use forward slash (/) for directory specification\n");
    printf("\txxx is USB optional port in octal (default is %o)\n", VDATA);
  }
  else {
    /* save a copy */
    strncpy(dircopy, argv[1], 80);
    
    d = argv[1];
    /* if rooted path start at /, otherwise relative path */
    if (*d == '/') {
      vcdroot();
      ++d;
    }
    slash = index(d, "/");
    while (slash != -1) {
      s = d;
      d += slash;
      *d++ = NUL;
      vcd(s);
      slash = index(d, "/");
    }
    if (strlen(d) > 0)
      vcd(d);
    
    printf("USB:%s\n", dircopy);
  }
}
