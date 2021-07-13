/* testd - test date computation algorithm.  given a number representing
** days since 12/31/1977 compute the day, month and year
**
**	g. roberts 6/2/2019
*/
#include "printf.h"
#include "scanf.h"

#define	FALSE	0
#define TRUE	1

int mydate[3];	/* day, month, year */

/* modays - return days in given month and year */
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

/* return TRUE if year is a leap year */
is_leap(year) {
	return (((year%4==0)&&((year%100)!=0)) || ((year%400)==0));
}

/* takes a CP/M 3 day count (from directory entry) and
** computes day, month and year based on definition that
** 1/1/1978 is day 1.
*/
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


main(argc, argv)
int argc;
char *argv[];
{
	int daycount;
	
	do {
		printf("\nEnter day count (0 to exit): ");
		scanf("%d", &daycount);
		
		/* convert to day, month, year */
		dodate(daycount, mydate);
		
		printf("For count %d date is %2d - %2d - %2d)",
			daycount, mydate[0], mydate[1], mydate[2]);
	} while (daycount>0);
}
