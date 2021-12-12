# For use with vcpm

BLD=./build

export CPMDrive_C = $(PWD)
export CPMDrive_D = $(PWD)/util
export CPMDrive_E = $(PWD)/vcd
export CPMDrive_F = $(PWD)/vdir
export CPMDrive_G = $(PWD)/vget
export CPMDrive_H = $(PWD)/vpip
export CPMDrive_I = $(PWD)/vput
export CPMDrive_J = $(PWD)/vtalk
export CPMDrive_K = $(BLD)
export CPMDrive_L = $(HOME)/git/MmsCpm3/3p/swtw-c80/math
export CPMDrive_M = $(HOME)/git/MmsCpm3/3p/swtw-c80/src

TARGETS = vtalk31.com vdir31.com
DEPS = $(CPMDrive_D)/vutil31.rel $(CPMDrive_D)/pio.rel

all: $(BLD) $(addprefix $(BLD)/,$(TARGETS))

$(BLD):
	mkdir -p $(BLD)

# l80 vget31,vutil31,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vget31/n/e
# l80 vdir31,vutil31,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vdir31/n/e
$(BLD)/vdir31.com: fprintf.rel $(CPMDrive_F)/vdir31.rel $(DEPS)
	vcpm link k:vdir31=f:vdir31,d:vutil31,d:pio,c:fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

# l80 vtalk31,vutil31,pio,b:fprintf,b:flibrary/s,b:stdlib/s,b:clibrary/s,vtalk31/n/e
$(BLD)/vtalk31.com: fprintf.rel $(CPMDrive_J)/vtalk31.rel $(DEPS)
	vcpm link k:vtalk31=j:vtalk31,d:vutil31,d:pio,c:fprintf,a:flibrary'[s]',a:stdlib'[s]',a:clibrary'[s,oc,nr]'

$(CPMDrive_D)/%.rel:
	$(MAKE) -C $(CPMDrive_D) $*.rel

$(CPMDrive_F)/%.rel:
	$(MAKE) -C $(CPMDrive_F) $*.rel

$(CPMDrive_J)/%.rel:
	$(MAKE) -C $(CPMDrive_J) $*.rel

%.rel: $(CPMDrive_L)/%.c
	vcpm c c:$*.asm=l:$*.c
	vcpm rmac c:$*.asm '$$szpz'

%.rel: $(CPMDrive_M)/%.c
	vcpm c c:$*.asm=m:$*.c
	vcpm rmac c:$*.asm '$$szpz'
