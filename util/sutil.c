/********************************************************
** sutil.c
**
** These are utility routines for manipulating strings
** including strings containing hexadecimal representations.
** They are designed for use with the Software Toolworks C/80
** v. 3.1 compiler.  The compiler should be configured
** to produce a Microsoft relocatable module (.REL file)
** file which can (optionally) be stored in a library
** (.LIB file) using the Microsoft LIB-80 Library Manager.
** The Microsoft LINK-80 loader program is then used to
** link this code, along with any other required modules,
** with the main (calling) program.
**
** There are seven utility routines defined here (see
** comments below for details on usage):
**
**  char *strncpy(s1, s2, n)
**  strupr(s)
**  int isprint(c)
**  hexcat(s, i)
**  commafmt(n, s, len)
**
** Release: March, 2019
**
**	Glenn Roberts
**	glenn.f.roberts@gmail.com
**
********************************************************/

#define	NUL	'\0'

/* strncpy - copies at most n characters from s2 to s1
**
** Copyright (c) 1999 Apple Computer, Inc. All rights reserved, see:
** https://www.opensource.apple.com/source/Libc/Libc-262/ppc/gen/strncpy.c 
*/
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

/* strupr - convert string to upper case */
strupr(s)
char *s;
{
	for ( ;(*s != NUL); ++s)
		*s = toupper(*s);

}

/* isprint - return TRUE if printable character
*/
isprint(c)
char c;
{
  return ((c>0x1F) && (c<0x7F));
}

/* hexcat - converts unsigned int i to a 2 byte hexadecimal
** representation in upper case ASCII and the catenates 
** that to the end of string s. 
*/
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

/* commafmt - create a string containing the representation
** of a long with commas every third position.  caution:
** len must be guaranteed big enough to hold any long.
*/
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

	/* pad the front with blanks */
	while (p != s)
		*--p = ' ';
}

