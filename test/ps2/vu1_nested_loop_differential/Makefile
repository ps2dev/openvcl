EE_BIN = vu1_nested_loop_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_nested_loop.vcl

all: verify-rewrite $(EE_BIN)

.PHONY: pdk verify-rewrite
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmNestedLoop/.name vsmNestedLoopCandidate/' $< | \
		OPENVCL_DISABLE_NO_Q_CYCLIC_PREFIX=1 $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmNestedLoop/.name vsmNestedLoopBaseline/' $< | \
		OPENVCL_DISABLE_NO_Q_CYCLIC_PREFIX=1 OPENVCL_DISABLE_NESTED_LOOP_BRANCH_DELAY_FILL=1 $(OPENVCL) -o $@

verify-rewrite: candidate.vsm baseline.vsm
	grep -A1 'ibne .*outer_lid' candidate.vsm | tail -1 | grep -q 'ilw.x'
	grep -A1 'ibne .*outer_lid' baseline.vsm | tail -1 | grep -q 'nop.*nop'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
