#include "printf.h"
#include "seek.c"

main(argc, argv)
int argc;
char *argv[];
{
	int chan;
	
	if (argc<2)
		printf("no file given!\n");
	else if ((chan = fopen(argv[1], "rb") == 0))
		printf("cannot open file %s\n", argv[1]);
	else {
		seek(chan, 0, 2);
		printf("ftell results %x %x\n", ftellr(chan), ftell(chan));
	}
}