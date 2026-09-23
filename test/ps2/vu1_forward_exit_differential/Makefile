EE_BIN = vu1_forward_exit_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_forward_exit.vcl

all: verify-rewrite $(EE_BIN)

.PHONY: pdk verify-rewrite
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmForwardExit/.name vsmForwardExitCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmForwardExit/.name vsmForwardExitBaseline/' $< | OPENVCL_DISABLE_FORWARD_EXIT_BRANCH_DELAY_FILL=1 $(OPENVCL) -o $@

verify-rewrite: candidate.vsm baseline.vsm
	grep -q '^pipe_lid__PROLOG:' candidate.vsm
	grep -q '^pipe_lid__PROLOG:' baseline.vsm
	grep -A1 'ibeq .*forward_done_lid' candidate.vsm | tail -1 | grep -q 'iand'
	grep -A1 'ibeq .*forward_done_lid' baseline.vsm | tail -1 | grep -q 'nop.*nop'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
