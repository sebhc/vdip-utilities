# Generic Makefile using vcpm
SHELL = /bin/bash
export CPMDrive_D = $(shell pwd)
export CPMDefault = d:

.PRECIOUS: %.asm %.h

%.asm: %.c printf.h
	vcpm c $*

%.rel: %.asm
	vcpm rmac $*.asm '$$szpz'

%.h: ~/VCPM/a/%.h
	cp $< $@
