/********************************************************
** vutil.h
**
** template definitions for vutil library, plus useful
** shared values.
**
**      2 February 2022
**      Glenn Roberts
**
**	4.2 (Beta) 26 Feb 2025
**	4.3 (Beta) 4 Sep 2025
**		remove Epson clock dependencies
**
********************************************************/
#ifndef EXTERN
#define EXTERN extern
#endif

#define	VERSION	"4.3 (Beta)"

/* Define either HDOS or CPM here to cause appropriate 
** routines to be compiled.  If none is defined CPM is 
** assumed. Your code must call routine getosver() to 
** set osname and osver, which are used at run time to
** ensure the appropriate code segments are invoked.
**
** Also: HDOS files should be saved with unix style NL 
** line endings; CP/M files require MS-DOS style CR-LF
** line endings.
*/
/* #define      CPM     1 ** done on commandline */

#define NUL     '\0'
#define TRUE    1
#define FALSE   0

/* CP/M 3 date/time data structure */
struct datime {
  unsigned date;
  char hour;
  char minute;
};

/* key date/time locations in HDOS */
#define S_DATE  0x20BF
#define S_DATC  0x20C8
#define S_TIME  0x20CA

/* sdate[9] = ASCII date representation
** cdate    = coded date (Y2K format)
** stime[3] = BCD time representation
*/
static char *sdate = S_DATE;
static int *cdate  = S_DATC;
static char *stime = S_TIME;

/* key time/date locations in CP/M */

/* pointer to 2ms ISR in low RAM */
static char **ckptr = 0x009;

/* Operating System name and version used to activate
** appropriate OS-specific function calls
*/
EXTERN int os;
EXTERN int osver;

/* Possible OS variants. Main difference in coding is how
** system time is measured and how time and date information
** is represented
*/
#define OSHDOS  1
#define OSCPM   2
#define OSMPM   3

/********************************************************
**
** OS-specific timer definitions
**
********************************************************/
/* These definitions are for timer functions which are
** used to detect and avoid lockup, and are OS-specific.
** HDOS and CP/M 2.2 both use the 2ms. interrupt-driven
** timer accessed at TICCNT, however that location is
** different between the two OSes. CP/M 3 uses a one 
** second system clock, the address of which is retrieved
** via a BDOS call. 
*/

/* CP/M 3 BDOS functions */
#define GETCS   0x0B
#define GETSCB  0x31
#define GETDT   0x69


/* Offsets in the SCB */
#define SOSEC   0x5C            /* seconds count in clock */
#define SOSCB   0x3A            /* address of SCB itself */

/* BDOS call 49 - get/set System Control Block parameters */
struct scbstruct {
        char    offset;
        char    set;
        int     value;
};

/* MP/M BDOS function -Get System Data Area */
#define GETSDA  0x9A

/* 2ms. "tick" counter style clock is used only for
** CP/M 2.2 and HDOS short time duration measurements
*/
#ifdef HDOS
/* HDOS TICCNT = 040.033A */
#define TICCNT  0x201B
#else
/* CP/M 2.2 puts it in low RAM */
#define TICCNT  0x000B
#endif

/* if file PFILE exists it contains an optional
** port number to be used.
*/
#define	PFILE	"VPORT.DAT"

/* OS calls */
int getosver();
int bdoshl();
int chkport();

/* format conversion */
int btod();
int dtob();
int gethexvals();
int hexval();
int hexcat();
int commafmt();
int aotoi();

/* string functions */
int strrchr();
char *strncpy();
int strupr();
int isprint();

/* time and date */
int modays();
int is_leap();
int timer();
int settd();
int dodate();
int prndate();
int prntime();
