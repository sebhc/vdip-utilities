# For use with vcpm, SWTW C/80, and DRI RMAC/LINK

.PRECIOUS: %.asm %.h %.rel

BLD=./build

# need to get from vcpm...
Drive_A = $(HOME)/VCPM/a
# these are for vcpm...
export CPMDrive_C = $(PWD)
export CPMDrive_K = $(BLD)
export CPMDrive_L = $(HOME)/git/MmsCpm3/3p/swtw-c80/math
export CPMDrive_M = $(HOME)/git/MmsCpm3/3p/swtw-c80/src
export CPMDefault = c:

TARGETS = vcd.com vtalk.com vdir.com vget.com vput.com vpip.com
DEPS = vutil.rel pio.rel

all: $(BLD) fprintf.h $(addprefix $(BLD)/,$(TARGETS))

$(BLD):
	mkdir -p $(BLD)

printf.h:
	ln -s $(Drive_A)/printf.h $@

fprintf.h:
	ln -s $(Drive_A)/fprintf.h $@

pio.rel: pio.dri
	vcpm rmac pio.dri '$$szpz'

%.rel: %.asm
	vcpm rmac $*.asm '$$szpz'

%.asm: $(CPMDrive_L)/%.c
	vcpm c c:$*=l:$*.c

%.asm: $(CPMDrive_M)/%.c
	vcpm c c:$*=m:$*.c

%.asm: %.c
	vcpm c -qCPM=1 $*.c

# l80 vtalk,pio,b:printf,b:stdlib/s,b:clibrary/s,vtalk/n/e
$(BLD)/vtalk.com: printf.h printf.rel vtalk.rel $(DEPS)
	vcpm link k:vtalk=vtalk,pio,printf,a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vdir,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vdir/n/e
$(BLD)/vdir.com: fprintf.rel vdir.rel vinc.rel $(DEPS)
	vcpm link k:vdir=vdir,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vget,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vget/n/e
$(BLD)/vget.com: fprintf.rel vget.rel vinc.rel $(DEPS)
	vcpm link k:vget=vget,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vput,vutil,vinc,pio,b:command,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vput/n/e
$(BLD)/vput.com: fprintf.rel command.rel vput.rel vinc.rel $(DEPS)
	vcpm link k:vput=vput,vutil,vinc,pio,fprintf,command,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vpip,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vpip/n/e
$(BLD)/vpip.com: fprintf.rel vpip.rel vinc.rel $(DEPS)
	vcpm link k:vpip=vpip,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vcd,vutil,vinc,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vcd/n/e
$(BLD)/vcd.com: fprintf.rel vcd.rel vinc.rel $(DEPS)
	vcpm link k:vcd=vcd,vutil,vinc,pio,fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

__FRC__:
