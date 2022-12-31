/********************************************************
** vutil.h
**
** template definitions for vutil library, plus useful
** shared values.
**
**	2 February 2022
**	Glenn Roberts
**
********************************************************/
#ifndef EXTERN
#define EXTERN extern
#endif

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
#define	CPM	1

#define NUL 	'\0'
#define	TRUE	1
#define	FALSE	0

/* CP/M 3 date/time data structure */
struct datime {
  unsigned date;
  char hour;
  char minute;
};

/* Clock port and register offsets (for versions that read
** the time directly from the clock vs. an OS call
*/
#define CLOCK	0240	/* Clock Base Port	*/
#define	S1	0	/* Seconds register	*/
#define	S10	1	/* Tens of Seconds	*/
#define	MI1	2	/* Minutes register */
#define	MI10	3	/* Tens of Minutes	*/
#define	H1	4	/* Hours register	*/
#define	H10	5	/* Tens of Hours	*/
#define	D1	6	/* Days register	*/
#define	D10	7	/* Tens of Days		*/
#define	MO1	8	/* Months register	*/
#define	MO10	9	/* Tens of Months	*/
#define	Y1	10	/* Years register	*/
#define	Y10	11	/* Tens of Years	*/
#define	W	12	/* Day of week reg.	*/

/* Data structure to hold results from Epson real-time
** clock query
*/
struct datetime {
  int day;
  int month;
  int year;
  int hours;
  int minutes;
  int seconds;
  int dow;
};

/* Operating System name and version used to activate
** appropriate OS-specific function calls
*/
EXTERN int os;
EXTERN int osver;

/* Possible OS variants. Main difference in coding is how
** system time is measured and how time and date information
** is represented
*/
#define	OSHDOS	1
#define	OSCPM	2
#define	OSMPM	3

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
#define	GETCS	0x0B
#define GETSCB	0x31
#define GETDT   0x69


/* Offsets in the SCB */
#define	SOSEC	0x5C		/* seconds count in clock */
#define	SOSCB	0x3A		/* address of SCB itself */

/* BDOS call 49 - get/set System Control Block parameters */
struct scbstruct {
	char	offset;
	char	set;
	int	value;
};

/* MP/M BDOS function -Get System Data Area */
#define GETSDA	0x9A

/* 2ms. "tick" counter style clock is used only for
** CP/M 2.2 and HDOS short time duration measurements
*/
#ifdef HDOS
/* HDOS TICCNT = 040.033A */
#define TICCNT	0x201B
#else
/* CP/M 2.2 puts it in low RAM */
#define TICCNT	0x000B
#endif

/* OS calls */
int getosver();
int bdoshl();

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
int readdate();
int settd();
int dodate();
long tseconds();
int prndate();
int prntime();
