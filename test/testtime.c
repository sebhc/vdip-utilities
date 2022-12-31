/* test time routines */
#include "fprintf.h"

#include "vutil.h"
#define STIME	0x20CA	/* S.TIME = 040.312 */

char td_string[15];	/* time/date hex value */
char linebuff[128];	/* I/O line buffer */


/* tseconds - report time in seconds, since
** midnight.
*/
tseconds()
{
	int h, m, s;
	char *stime;
	long seconds;
	
	/* CP/M 3 date/time data structure */
	static struct datime {
		unsigned date;
		char hour;
		char minute;
	} dt;
	
	if (os == OSHDOS) {
		/* point to system time storage location
		** NOTE must have CK: device driver or CLOCK
		** task (if HDOS 3)
		*/
		stime = STIME;
	
		h = btod(*stime++);
		m = btod(*stime++);
		s = btod(*stime);
	}
	else if ((os == OSCPM) && (osver >= 0x30)) {
		s = btod(bdoshl(105, &dt));
		m = btod(dt.minute);
		h = btod(dt.hour);
	}
	else {
		/* no clock */
		h = 0;
		m = 0;
		s = 0;
	}
	
	printf("%02d:%02d:%02d   ", h, m, s);
	
	seconds = 3600L * h + 60L * m + s;
	printf("%ld seconds\n\n", seconds);
}

main(argc, argv)
int argc;
char *argv[];
{
	getosver();
	
	switch (os) {
		case OSHDOS:
			printf("HDOS");
			break;
		case OSCPM:
			printf("CP/M");
			break;
		case OSMPM:
			printf("MP/M");
			break;
		default:
			printf("Unknown");
			break;
	}
	printf(", Ver. %d.%d\n", ((osver & 0xF0) >> 4 ), osver & 0xF);

	while(1) {
		tseconds();
	}

}




