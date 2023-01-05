# For use with vcpm, SWTW C/80, and DRI RMAC/LINK.
#
# See README.Makefile

.PRECIOUS: %.asm %.h %.rel h%.asm %.bin h%.c

FMT =
BLD = ./build
CSRC = $(HOME)/swtw-c80-cpm
HSRC = $(HOME)/swtw-c80-hdos

# need to get from vcpm...
VCPM_A = $(shell CPMShow=A vcpm)
# these are for vcpm...
export CPMDrive_C = $(PWD)
export CPMDrive_B = $(BLD)/cpm
export CPMDrive_E = $(BLD)/hdos
export CPMDrive_D = $(CSRC)
export CPMDrive_H = $(HSRC)
export CPMDrive_L = $(PWD)/c-hdos
export CPMDefault = c:

HDOSABS = $(CPMDrive_L)/hdosabs

TARGETS = vcd.com vtalk.com vdir.com vget.com vput.com vpip.com
DEPS = vutil.rel pio.rel

CIMG = $(BLD)/vdip-cpm.zip
HIMG = $(BLD)/vdip-hdos.zip

ifneq ($(FMT),)
# TODO: get current version from ...
CIMG += $(BLD)/vdip-cpm.h8d
HIMG += $(BLD)/vdip-hdos.h8d

$(BLD)/vdip-cpm.h8d: __FRC__
	rm -f $@
	$(FMT) files=$(CPMDrive_B) $@ bare z17 ss sd

$(BLD)/vdip-hdos.h8d: __FRC__
	rm -f $@
	$(FMT) hdos lab="VDIP 4.0 HDOS" files=$(CPMDrive_E) $@ bare z17 ss sd
endif

all: cpm hdos

img: cpm-img hdos-img

cpm: $(CPMDrive_B) fprintf.h $(addprefix $(CPMDrive_B)/,$(TARGETS))

cpm-img: $(CIMG)

clean:
	rm -f *.asm *.rel *.bin hcommand.c

clobber: clean
	rm -rf $(BLD) fprintf.h printf.h

$(CPMDrive_B) $(CPMDrive_E):
	mkdir -p $@

%.h: $(VCPM_A)/%.h
	ln -s $? $@

pio.rel: pio.dri
	vcpm rmac pio.dri '$$szpz'
	@test -s $@

%.rel: %.asm
	vcpm rmac $*.asm '$$szpz'
	@test -s $@

%.asm: $(CPMDrive_D)/%.c
	vcpm c -m2 c:$*=d:$*.c
	@test -s $@

%.asm: %.c
	vcpm c -m2 -qCPM=1 $*.c
	@test -s $@

############## CP/M ##############

# l80 vtalk,pio,b:printf,b:stdlib/s,b:clibrary/s,vtalk/n/e
$(CPMDrive_B)/vtalk.com: printf.h printf.rel vtalk.rel $(DEPS)
	vcpm link b:vtalk=vtalk,pio,printf,a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

# l80 vdir,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vdir/n/e
$(CPMDrive_B)/vdir.com: fprintf.rel vdir.rel vinc.rel $(DEPS)
	vcpm link b:vdir=vdir,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

# l80 vget,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vget/n/e
$(CPMDrive_B)/vget.com: fprintf.rel vget.rel vinc.rel $(DEPS)
	vcpm link b:vget=vget,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

# l80 vput,vutil,vinc,pio,b:command,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vput/n/e
$(CPMDrive_B)/vput.com: fprintf.rel command.rel vput.rel vinc.rel $(DEPS)
	vcpm link b:vput=vput,vutil,vinc,pio,fprintf,command,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

# l80 vpip,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vpip/n/e
$(CPMDrive_B)/vpip.com: fprintf.rel vpip.rel vinc.rel $(DEPS)
	vcpm link b:vpip=vpip,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

# l80 vcd,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vcd/n/e
$(CPMDrive_B)/vcd.com: fprintf.rel vcd.rel vinc.rel $(DEPS)
	vcpm link b:vcd=vcd,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'
	@test -s $@

$(BLD)/vdip-cpm.zip: __FRC__
	zip -j $@ $(CPMDrive_B)/*.com

############## HDOS ##############

HTARGS = vtalk.abs vcd.abs vdir.abs vget.abs vput.abs vpip.abs
HDEPS = hvutil.rel pio.rel

hdos: $(CPMDrive_E) fprintf.h $(addprefix $(CPMDrive_E)/,$(HTARGS))

hdos-img: $(HIMG)

# Ugly, but -qRc=XRc is not working...
# (because, CP/M commandline is forced to uppercase)
hcommand.c: $(CPMDrive_H)/command.c
	sed -e 's/Rc/XRc/g' $? >$@

hfprintf.asm: $(CPMDrive_H)/fprintf.c
	vcpm c -m2 hfprintf=h:fprintf.c
	@test -s $@

h%.asm: %.c
	vcpm c -m2 -qHDOS=1 h$*=$*.c
	@test -s $@

$(CPMDrive_E)/%.abs: %.bin
	$(HDOSABS) $? >$@

# l80 vtalk,pio,b:printf,b:stdlib/s,b:clibrary/s,vtalk/n/e
vtalk.bin: printf.h printf.rel hvtalk.rel $(HDEPS)
	vcpm link vtalk.bin=hvtalk,pio,printf,h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

vdir.bin: hfprintf.rel hvdir.rel hvinc.rel $(HDEPS)
	vcpm link vdir.bin=hvdir,hvutil,hvinc,pio,hfprintf,h:flibrary'[s]',h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

vget.bin: hfprintf.rel hvget.rel hvinc.rel $(HDEPS)
	vcpm link vget.bin=hvget,hvutil,hvinc,pio,hfprintf,h:flibrary'[s]',h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

vput.bin: hfprintf.rel hcommand.rel hvput.rel hvinc.rel $(HDEPS)
	vcpm link vput.bin=hvput,hvutil,hvinc,pio,hfprintf,hcommand,h:flibrary'[s]',h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

vpip.bin: hfprintf.rel hvpip.rel hvinc.rel $(HDEPS)
	vcpm link vpip.bin=hvpip,hvutil,hvinc,pio,hfprintf,h:flibrary'[s]',h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

vcd.bin: hfprintf.rel hvcd.rel hvinc.rel $(HDEPS)
	vcpm link vcd.bin=hvcd,hvutil,hvinc,pio,hfprintf,h:flibrary'[s]',h:stdlib'[s]',l:clibrary'[s,l2280,nr]'
	@test -s $@

$(BLD)/vdip-hdos.zip: __FRC__
	zip -j $@ $(CPMDrive_E)/*.abs

__FRC__:
