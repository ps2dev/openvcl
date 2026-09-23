EE_BIN = vu1_memory_alias_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_memory_alias.vcl

all: verify-schedule $(EE_BIN)

.PHONY: pdk verify-schedule
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed '/--barrier/d; s/\.name vsmMemoryAlias/.name vsmMemoryAliasCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmMemoryAlias/.name vsmMemoryAliasBaseline/' $< | $(OPENVCL) -o $@

verify-schedule: candidate.vsm baseline.vsm
	grep -q 'lq.w VF02, 0(VI02)' candidate.vsm
	grep -q 'sq.w VF01, 0(VI01)' candidate.vsm
	grep -q 'lq.w VF02, 0(VI02)' baseline.vsm
	grep -q 'sq.w VF01, 0(VI01)' baseline.vsm

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
