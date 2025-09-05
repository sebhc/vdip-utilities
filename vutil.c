/********************************************************
** vutil.c
**
** This library contains a set of general purpose utility
** routines which support the various needs for accessing
** the FTDI VDIP device. Routines cover four broad types
** of functions: Operating System functions, format conversion,
** string functions and time/date functions.
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
**  Glenn Roberts
**  3 January 2022
**
**  24 October 2024 - added chkport()
**
********************************************************/
#include "fprintf.h"
#include "scanf.h"
#include "vinc.h"
#include "vutil.h"

/********************************************************
**
** getosver
**
** set globals 'osname' and 'osver' to reflect the current
** operating system and version. These are used to direct
** code to the appropriate OS-specific function calls.
**
********************************************************/
int getosver()
{
#ifdef HDOS
  os = OSHDOS;
  /* NOTE: code below will not work properly with HDOS 1
  ** since the 'C' is ignored on the SCALL to .VERS.  
  ** Assumed not to be an issue as HDOS 1 is quite ancient!
  */
  &osver; /* leave &osver in HL */
#asm
  DB  255,9
  MOV M,A
  INX H
  MVI M,0
#endasm

#else
  int ver;

  ver = bdoshl(0x0C, 0);
  /* H == 0 -> CP/M, H == 1 -> MP/M
  ** release is BCD stored in L
  */
  if ((ver>>8) == 0)
    os = OSCPM;
  else
    os = OSMPM;
  osver = ver & 0xFF;
#endif
}

/********************************************************
**
**  *** Valid for use only in CP/M ***
**
** bdoshl - Call CP/M BDOS function with given values
**    for registers C and DE.  Return the value
**    from the HL register.
**
********************************************************/
int bdoshl(c,de)
  int c, de;
{
#asm
        POP     H
        POP     D
        POP     B
        PUSH    B
        PUSH    D
        PUSH    H
        CALL    5
#endasm
}

/********************************************************
**
** btod
**
** convert a BCD byte to its decimal equivalent
**
********************************************************/
int btod(b)
char b;
{
  return ((b & 0xF0) >> 4) * 10 + (b & 0x0F);
}

/********************************************************
**
** dtob
**
** convert a Decimal byte to its BCD equivalent
**
********************************************************/
int dtob(b)
char b;
{
  return ((b/10) << 4) | (b % 10);
}

/********************************************************
**
** gethexvals
**
** Scan a string for values of the form $xx where xx
** is a hexadecimal upper case string, e.g. $34 $96 $8C ...
** Place these sequentially into the array of bytes provided.
** Search for at most n values and return the number found.
**
** NOTE: very minimal error checking is done!
**
********************************************************/
int gethexvals(s, n, val)
char *s;
int n;
char val[];
{
  int i;
  
  for (i=0; i<n ; i++) {
    while ((*s!='$') && (*s!=0))
      ++s;
    if (*s==0)
      return i;
    else
      val[i] = hexval(++s);
  }
  return i;
}

/********************************************************
**
** hexval
**
** Turn a two-byte upper case ASCII hex string into its
** binary representation.
**
** NOTE: no error checking is done (e.g. for non-hexadecimal
** characters).
**
********************************************************/
int hexval(s)
char *s;
{
  int n1, n2;
  
  n1 = *s++ - '0';
  if (n1>9)
    n1 -= 7;
  n2 = *s - '0';
  if (n2>9)
    n2 -= 7;
  return (n1<<4) + n2;
}

/********************************************************
**
** hexcat
**
** Converts unsigned int i to a 2 byte hexadecimal
** representation in upper case ASCII and then catenates 
** that to the end of string s
**
********************************************************/
int hexcat(s, i)
char *s;
unsigned i;
{
  static char b[3];
  unsigned n;
  
  n = (i & 0xFF) >> 4;
  b[0] = (n < 10) ? '0'+n : 'A'+n-10;
  n = i & 0xF;
  b[1] = (n < 10) ? '0'+n : 'A'+n-10;
  b[2] = '\0';
  strcat(s, b);
}

/********************************************************
**
** commafmt
**
** Create a string containing the representation of a 
** long with commas every third position.  len is the
** allocated size of the string s. caution: len must 
** be declared big enough to hold any long.
**
********************************************************/
int commafmt(n, s, len)
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

/********************************************************
**
** aotoi
**
** Convert octal string to an integer 
**
********************************************************/
int aotoi(s)
char s[];
{
  int i, n;
  
  n = 0;
  for (i = 0; s[i] >= '0' && s[i] <= '7'; ++i)
  n = 8 * n + s[i] - '0';
  return(n);
}


/********************************************************
**
** strrchr
**
** Return pointer to last occurrence of character ch in
** string str.  Return NULL if no occurrence. 
**
********************************************************/
int strrchr(str, ch)
char *str;
char ch;
{
  char *s;
  int l;
  
  l = strlen(str);
  s = str + strlen(str);
  
  do {
    --s;
    if (*s == ch)
      return s;
  } while (s != str);
  
  
  /* fell through - no match */
  return 0;
}

/********************************************************
**
** strncpy
**
** copy at most n characters from s2 to s1 
**
** Copyright (c) 1999 Apple Computer, Inc. All rights reserved, see:
** https://www.opensource.apple.com/source/Libc/Libc-262/ppc/gen/strncpy.c 
**
********************************************************/
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

/********************************************************
**
** strupr
**
** Convert string to upper case
**
********************************************************/
int strupr(s)
char *s;
{
  for ( ;(*s != NUL); ++s)
    *s = toupper(*s);

}

/********************************************************
**
** isprint
**
** return TRUE if printable character 
**
********************************************************/
int isprint(c)
char c;
{
  return ((c>0x1F) && (c<0x7F));
}

/********************************************************
**
** modays
**
** return days in given month and year 
**
********************************************************/
int modays(month, year)
int month;
int year;
{
  int days;
  
  switch(month)
  {
    /* 30 days hath september, april, june and november */
    case 9:
    case 4:
    case 6:
    case 11:
      days = 30;
      break;

    case 2:
      days = is_leap(year) ? 29 : 28;
      break;

    /* all the rest have 31! */
    default:
      days = 31;
      break;
  }
  
  return days;
}

/********************************************************
**
** is_leap
**
** return TRUE if year is a leap year 
**
********************************************************/
int is_leap(year) {
  return (((year%4==0)&&((year%100)!=0)) || ((year%400)==0));
}

/********************************************************
**
** timer
**
** maintains a short-duration timer used to avoid lockup.
**
** if init is TRUE then initialize the timer with t
** as the number of seconds to time down to. returns t.
**
** if init is FALSE then test for timeout (parameter
** t is not used). NOTE: this routine is designed to be 
** continuously polled so as to not miss a 2ms window.
** returns 0 when time is up.
**
********************************************************/
int timer(init, t)
int init, t;
{
  int rc;
  int *tmp;
  
  /* statics to retain value across multiple calls */
  /* For CP/M2 and HDOS we use a pointer to 2-ms clock */
  static unsigned *Ticptr = TICCNT;
  static unsigned timeout;  
  /* CP/M 3 and MP/M use BDOS calls to find system clock */
  struct scbstruct scbs;
  /* pointer to the one-second tick counter in CP/M 3.
  ** initalized to NULL to force it to be set up when
  ** timer is first accessed
  */
  static char *secp = 0;
  static int snapshot;
  static int countdown;

  /* if init is TRUE then initialize the timer */
  if (init) {
    /* CP/M 3 and MP/M use one-second system clock */
    if ((os==OSMPM) || ((os==OSCPM) && (osver>=0x30))) {
      if (secp == 0) {
        /* The first time this is called we need to set
        ** up a pointer to the one-second tick counter.
        ** We'll use this to test for timeouts when reading 
        ** data. CP/M 3 and MP/M use different techniques
        ** for finding this pointer
        */
        if (os==OSMPM) {
          /* MP/M */
          tmp = (int *)bdoshl(GETSDA, 0);
          tmp = tmp[126]; /* offset 252: XDOS internal data */
          secp = (char *)tmp + 4;
        } else {
          /* CP/M 3 */
          scbs.offset = SOSCB;  /* offset for SCB address (undocumented) */
          scbs.set  = 0;    /* do a "get" not a "set" */
          secp = (char *)bdoshl(GETSCB, &scbs) + SOSEC;
        }
      }
    
      /* snapshot the time in seconds and start the coundown */
      snapshot = *secp;
      countdown = t;
  
    } else {
      /* HDOS and CP/M 2.2 use 2ms TICCNT timer. set
      ** timeout to TICCNT value when timer is to expire.
      **
      ** time (t) is in seconds but we want to convert
      ** that to units of 2 ms so we multiply by 512 = 2^9
      ** (close enough to 500 for our purposes ...)
      */
      timeout = *Ticptr + (t << 9);
    }
    rc = t;
  }
  /* if init is FALSE then check for timeout. return
  ** zero when time is up
  */
  else {
    /* for CP/M 3 or MP/M check 1-second system clock */
    if ((os==OSMPM) || ((os==OSCPM) && (osver>=0x30))) {
      if (snapshot != *secp) {
        /* the seconds clock has ticked (1 sec.),
        ** decrease the countdown and reset
        ** the snapshot.
        */
        --countdown;
        snapshot = *secp;

      }
      rc = (countdown != 0);
    /* HDOS and CP/M 2.2 just check tic counter */
    } else {
      rc = (*Ticptr != timeout);
    }
  }
  
  return rc;
}


/********************************************************
**
** settd
**
** Reads the system time and date and stores result in the
** hexadecimal ASCII global string td_string[] using the
** format expected by the VDIP chip.  This ensures that
** when vwopen() opens the file the correct date/time
** information is used.
**
** If show is TRUE then the date is also displayed on the
** console.
**
** CP/M 3 has an OS-supported time function accessed via
** a call to BDOS function 105.
**
** HDOS stores time and date information in a well-known
** location. The date is set at boot time based on user input.
** If a real-time clock is present it can be used to set
** date and time and either a CK: driver (HDOS 2) or CLOCK
** task (HDOS 3) will then maintain the time field.
**
** CP/M 2.2.0x also maintains time and date fields in a
** well-known location. If a real-time clock is present
** it will set the time/date and time will be maintained
** by the BIOS, Otherwise date will default to 0/0/0 and
** time represents time since boot. There are third-party
** enhancements to CP/M that manage time/date but those
** have not been tested with this code.
**
********************************************************/
int settd(show)
int show;
{
  int h, m, s;
  char *c;
  int dd, mm, yyyy, idate;
  unsigned utime, udate;
  static char *month[] = {
    "???", "Jan", "Feb", "Mar", "Apr",  "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  
  /* CP/M 3 specific fields */
  int mydate[3];  /* day, month, year */
  /* date/time data structure expected by BDOS 105 */
  struct datime dt;
  
  /* Since CP/M 3 and MP/M have OS-level date/time support
  ** we use the associated BDOS call here.
  */
  if ((os==OSMPM) || ((os==OSCPM) && (osver>=0x30))) {
    /* get the current time & date */
    s = btod(bdoshl(105, &dt));
    
    /* convert day count to day, month, year */
    dodate(dt.date, mydate);

    m = btod(dt.minute);
    h = btod(dt.hour);
  
    dd = mydate[0];
    mm = mydate[1];
    yyyy = mydate[2];

  }
  else if (os==OSHDOS) {
    /* for HDOS 2 and HDOS 3 use well known location
    ** HH MM SS are three successive bytes stored in
    ** BCD format. Date is stored as 16-bit value
    ** as shown below.
    ** NOTE: this code assumes no sign extension
    ** converting char to int (switch -x in C/80)
    */
    c = stime;
    
    h = btod(*c++);
    m = btod(*c++);
    s = btod(*c);

    idate = *cdate;
    
    /* HDOS internal date is: yyyyyyymmmmddddd */
    dd = idate & 0x1F;
    mm = (idate >> 5) & 0x0F;
    yyyy = (idate >> 9) + 2000;
  }
  else if ((os==OSCPM) && (osver<0x30)) {
    /* CP/M 2.2.0x */
    
    /* point to time/date values in RAM stored
    ** in 6 successive bytes: SS MM HH DD MM YY
    ** NOTE: this code assumes no sign extension
    ** converting char to int (switch -x in C/80)
    */
    c = *ckptr - 11;
    
    s = *c++;
    m = *c++;
    h = *c++;
    
    dd = *c++;
    mm = *c++;
    yyyy = *c + 2000;
  }
  else {
    /* use all zeros for default */
    h = m = s = 0;
    dd = mm = yyyy = 0;
    
  }
  /* optionally display the date and time... */
  if (show && (mm > 0)) {
    printf("%02d-%02d-%02d %02d:%02d:%02d ", mm, dd, yyyy%100,
        h, m, s);
  }
  
  
  /* now store time in td_string using format
  ** documented in FTDI Vinculum Firmware
  ** User Manual. Then build the Hex string
  ** to be used by vwopen();
  */
  utime = s/2 | m<<5  | h<<11;
  udate = dd  | mm<<5 | (yyyy-1980)<<9;
      
  td_string[0] = ' ';
  td_string[1] = '$';
  td_string[2] = '\0';
  hexcat(td_string, udate >> 8);
  hexcat(td_string, udate & 0xFF);
  hexcat(td_string, utime >> 8);
  hexcat(td_string, utime & 0xFF);
}

/********************************************************
**
** dodate
**
** takes a CP/M 3 day count (from directory entry) and
** computes day, month and year based on definition that
** 1/1/1978 is day 1.
**
********************************************************/
int dodate(days, date)
int days;
int date[];
{
  int yyyy, dd, m, mm;
  
  yyyy = 1978;
  dd = days;
  
  while ((dd>365) && ((dd!=366) || !is_leap(yyyy))) {
    dd -= is_leap(yyyy) ? 366 : 365;
    ++yyyy;
  }

  /* have year & days in the year; convert to dd/mm/yyyy */
  mm = 1;
  while (dd > (m=modays(mm, yyyy))) {
    dd -= m;
    ++mm;
  }
  date[0] = dd;
  date[1] = mm;
  date[2] = yyyy;
}


/********************************************************
**
** prndate
**
** This routine extracts the month, day and year information
** from a 16-bit word stored using the Microsoft FAT format:
**
**  9:15  Year  0..127  (0=1980, 127=2107)
**  5:8   Months  1..12 (1=Jan., 12=Dec.)
**  0:4   Days  1..31 (1=first day of mo.)
**
** Files stored on the USB file device use this date format,
** as documented in the FTDI Vinculum Firmware User Manual.
**
** The results are displayed  on the standard output device.
**
********************************************************/
int prndate(udate)
unsigned udate;
{
  unsigned mo, dy, yr;
  
  dy = udate & 0x1f;
  mo = (udate >> 5) & 0xf;
  yr = 1980 + ((udate >> 9) & 0x7f);
  
  printf("%2d/%02d/%02d", mo, dy, (yr % 100));
}

/********************************************************
**
** prntime
**
** This routine extracts the time information (hour and minute)
** from a 16-bit word stored using the Microsoft FAT format:
**
**  11:15   Hours 0..23   (24 hour clock)
**  5:10  Minutes 0..59
**  0:4   Sec./2  0..29   (0=0, 29=58 sec.)
**
** Files stored on the USB file device use this time format,
** as documented in the FTDI Vinculum Firmware User Manual.
**
** The results are displayed on the standard output device.
** A 12-hour clock format is used including "AM" or "PM".
**
********************************************************/
int prntime(utime)
unsigned utime;
{
  unsigned hr, min, sec;
  char *am_pm;
  
  sec = 2*(utime & 0x1f);
  min = (utime >> 5) & 0x3f;
  hr  = (utime >> 11) & 0x1f;
  if (hr > 12) {
    am_pm = "PM";
    hr = hr - 12;
  }
  else
    am_pm = "AM";
  
  printf("%2d:%02d %s", hr, min, am_pm);
}

/* chkport - Look for a specially-named file (see definition
** of PFILE) and, if it exists, open it and read the user-specified
** port number (supplied in octal format).
**
** Here are the rules:
**
** 1) try to just open PFILE without any disk qualifier. In CP/M
** this will try the current default drive; in HDOS this will try
** SY0:
** 2) if first attempt fails then try the following: the string
** that was passed should contain either a drive qualifier or
** a fully qualified name of the invoking program. strip off the
** drive name and colon and append PFILE and try to open that.
**
** If these fail, do nothing, otherwise set the globals
** p_data and p_stat are set using the specified port.
**
** update: gfr 12 April 2025
** Returns TRUE if VPORT file successfully found and used, otherwise
** returns FALSE.
*/
int chkport(s)
char *s;
{
	int pch, pval, iscan, fok;
	static char pfname[20];

  fok = FALSE;
	
	strcpy(pfname, PFILE);
	if ((pch = fopen(pfname, "r")) != 0) {
		/* option 1 - success! */
		fok = TRUE;
	}
	else {
		/* try option 2. find the drive prefix from the string
		** that was passed and then append PFILE to that and
		** try to open it
		*/
		strcpy(pfname, s);
		if ((iscan = index(pfname, ":")) != -1) {
			/* found a ":" - skip past it and then
			** append PFILE to create the fully-qualified
			** file name
			*/
			++iscan;
			strcpy(pfname+iscan, PFILE);

			/* now try to open it ... */
			if ((pch = fopen(pfname, "r")) != 0) {
				/* option 2 - success! */
				fok = TRUE;
			}
		}
	}
	
	if (fok) {
		/* file found and opened - read the port number */
		fscanf(pch, "%o\n", &pval);
/*		printf("User-specified port: [%03o] in file %s\n", pval, pfname); */
		p_data = pval;
		p_stat = p_data + 1;

		fclose(pch);
	}
  return fok;
}
