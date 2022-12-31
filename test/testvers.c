/* test version calling */
#include "fprintf.h"

#include "vutil.h"

char td_string[15];	/* time/date hex value */
char linebuff[128];	/* I/O line buffer */

main(argc, argv)
int argc;
char *argv[];
{
	
	getosver();
	
	printf("%s Operating System\n", osname);
	printf("Version: %x\n", osver);
}




