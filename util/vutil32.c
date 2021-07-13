/********************************************************
** vutil.c - V3.12 - Combined HDOS-CP/M2.2-CP/M 3 version
**
** These are utility routines are called by the various
** vinculum utilities including routines in 'vinc.c'.
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
**	separated VUTIL and VINC into two compilable units
**		5 Mar 2020
**
**	Glenn Roberts
**	glenn.f.roberts@gmail.com
**
********************************************************/
#include "fprintf.h"

#define	NUL	'\0'

/* declared in vinc library */
extern char td_string[15];		/* time/date hex value */

/********************************************************
**
** prndate
**
** This routine extrates the month, day and year information
** from a 16-bit word and displays them on the standard
** output device.  The date format is as defined in the FTDI
** Vinculum Firmware User Manual.
**
********************************************************/
prndate(udate)
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
** This routine extrates the time information (hour and minute)
** from a 16-bit word and displays them on the standard
** output device.  A 12-hour clock format is used including
** "AM" or "PM" as appropriate.  The time format is as defined
** in the FTDI Vinculum Firmware User Manual.
**
********************************************************/
prntime(utime)
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

/********************************************************
**
**	*** Valid for use only in CP/M ***
**
** bdoshl - Call CP/M BDOS function with given values
**		for registers C and DE.  Return the value
**		from the HL register.
**
********************************************************/
bdoshl() {
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
btod(b)
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
dtob(b)
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
gethexvals(s, n, val)
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
hexval(s)
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
** strrchr
**
** Return pointer to last occurrence of character ch in
** string str.  Return NULL if no occurrence. 
**
********************************************************/
strrchr(str, ch)
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
** commafmt
**
** Create a string containing the representation of a 
** long with commas every third position.  len is the
** allocated size of the string s. caution: len must 
** be declared big enough to hold any long.
**
********************************************************/
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

	/* left justify in the string */
	while (*p)
		*s++ = *p++;
	*s = NUL;
}

/********************************************************
**
** aotoi
**
** Convert octal string to an integer 
**
********************************************************/
aotoi(s)
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
strupr(s)
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
isprint(c)
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
modays(month, year)
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
is_leap(year) {
	return (((year%4==0)&&((year%100)!=0)) || ((year%400)==0));
}

/********************************************************
**
** settd
**
** Reads time/date via BDOS and stores result in
** hexadecimal ASCII global string td_string[] using format
** desired by VDIP chip.  This is used by vwopen() to
** set the date on the file.
**
********************************************************/
settd()
{
	int seconds;
	static int mydate[3];	/* day, month, year */
	static unsigned utime, udate;
	/* date data structure expected by BDOS 105 */
	static struct datime {
		unsigned date;
		char hour;
		char minute;
	} dt;
	
	/* snapshot time */
	seconds = bdoshl(105, &dt);
		
	/* convert day count to day, month, year */
	dodate(dt.date, mydate);
	
	/* store time in td_string */
	utime = (btod(seconds)/2) + 
			(btod(dt.minute) << 5) + 
			(btod(dt.hour) << 11);
	udate = mydate[0] + 
			(mydate[1] << 5) + 
			((mydate[2]-1980)  << 9);
				
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
** hexcat
**
** Converts unsigned int i to a 2 byte hexadecimal
** representation in upper case ASCII and the catenates 
** that to the end of string s
**
********************************************************/
hexcat(s, i)
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
** dodate
**
** takes a CP/M 3 day count (from directory entry) and
** computes day, month and year based on definition that
** 1/1/1978 is day 1.
**
********************************************************/
dodate(days, date)
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

