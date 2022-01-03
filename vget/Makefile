# Generic Makefile using vcpm
SHELL = /bin/bash
export CPMDrive_D = $(shell pwd)
export CPMDefault = d:

.PRECIOUS: %.asm %.h

%.asm: %.c fprintf.h
	vcpm c -qCPM3=1 $*

%.rel: %.asm
	vcpm rmac $*.asm '$$szpz'

%.h: ~/VCPM/a/%.h
	cp $< $@
