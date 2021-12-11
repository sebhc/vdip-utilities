# For use with vcpm

BLD=./build

export CPMDrive_D = $(PWD)/util
export CPMDrive_E = $(PWD)/vcd
export CPMDrive_F = $(PWD)/vdir
export CPMDrive_G = $(PWD)/vget
export CPMDrive_H = $(PWD)/vpip
export CPMDrive_I = $(PWD)/vput
export CPMDrive_J = $(PWD)/vtalk
export CPMDrive_K = $(BLD)

TARGETS = vtalk3.com

all: $(BLD) $(addprefix $(BLD)/,$(TARGETS))

$(BLD):
	mkdir -p $(BLD)

$(BLD)/vtalk3.com: $(CPMDrive_J)/vtalk3.rel $(CPMDrive_D)/vucpm3.rel $(CPMDrive_D)/pio.rel
	vcpm link k:vtalk3=j:vtalk3,d:vucpm3,d:pio,a:clibrary'[s]',a:flibrary'[s,oc,nr]'

$(CPMDrive_D)/%.rel:
	$(MAKE) -C $(CPMDrive_D) $*.rel

$(CPMDrive_J)/%.rel:
	$(MAKE) -C $(CPMDrive_J) $*.rel
