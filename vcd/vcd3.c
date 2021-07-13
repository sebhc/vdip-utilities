/* vcd - CP/M 3 version
**
** This program implements a simple Change Directory app for the
** Vinculum VDP-1 interfaced in parallel FIFO mode.
**
** It changes the directory to the value specified on
** the command line.  Unix-style forward slash characters ("/")
** are used to indicate directory levels.  If the specified
** directory path begins with '/' it is taken as an absolute
** path (rooted), otherwise the path is taken as relative to
** the current directory.
**
** Version 1.1
**
** Compiled with Software Toolworks C/80 V. 3.0
**
** Glenn Roberts 29 June 2019
**
**	Revisions:
**	 use Unix style directory notation
**	 enhancements to VDIP utility routines
**
*/

/* FTDI VDIP default ports */
#define VDATA	0331
#define VSTAT	0332

#define	FALSE	0
#define	TRUE	1

#define PROMPT	"D:\\>"		/* standard VDIP command prompt */
#define CFERROR "Command Failed"
#define NUL '\0'

#include "fprintf.h"

/* USB i/o ports */
int p_data;		/* USB data port */
int p_stat;		/* USB status port */

/* declared in vutil library */
extern char linebuff[128];		/* I/O line buffer 		*/

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
	
	/* default port values */
	p_data = VDATA;
	p_stat = VSTAT;

	/* process any switches */
	dosw(argc, argv);

	printf("VCD v1.0.1 (CP/M 3) - G. Roberts.  Using USB ports: %o,%o\n",
		p_data, p_stat);
				
	if (vinit() == -1)
		printf("Error initializing VDIP-1 device!\n");
	else if (vfind_disk() == -1)
		printf("No flash drive found!\n");
	else if ((argc < 2) || (index(argv[1], "\\") != -1)) {
		printf("Usage: vcd <directory>\n");
		printf("Use forward slash (/) for directory specification\n");
	}
	else {
		d = argv[1];
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
	}
}

