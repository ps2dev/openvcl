EE_BIN = vu1_postincrement_prefix_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_postincrement_prefix.vcl

all: verify-rewrite $(EE_BIN)

.PHONY: pdk verify-rewrite
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmPostIncrementPrefix/.name vsmPostIncrementPrefixCandidate/' $< | \
		OPENVCL_DISABLE_PIPELINE_PROFITABILITY_GATE=1 $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmPostIncrementPrefix/.name vsmPostIncrementPrefixBaseline/' $< | $(OPENVCL) --disable-generic-software-pipelining -o $@

verify-rewrite: candidate.vsm baseline.vsm
	grep -q '^loop_lid__PROLOG:' candidate.vsm
	grep -q '^loop_lid__DRAIN:' candidate.vsm
	! grep -q '^loop_lid__PROLOG:' baseline.vsm
	grep -q 'lqi.w .*VI03++' candidate.vsm
	grep -q 'lq.w .*0(VI03)' candidate.vsm
	grep -q 'ibne VI03, VI04, loop_lid' candidate.vsm
	grep -q 'iaddiu VI03, VI03, 1' candidate.vsm

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
