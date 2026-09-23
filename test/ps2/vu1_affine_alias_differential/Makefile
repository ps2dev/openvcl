EE_BIN = vu1_affine_alias_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_affine_alias.vcl

all: verify-schedule $(EE_BIN)

.PHONY: pdk verify-schedule
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed '/--barrier/d; s/\.name vsmAffineAlias/.name vsmAffineAliasCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmAffineAlias/.name vsmAffineAliasBaseline/' $< | $(OPENVCL) -o $@

verify-schedule: candidate.vsm baseline.vsm
	python3 -c 'import pathlib; s=pathlib.Path("candidate.vsm").read_text(); assert s.index("lq.w VF02, 0(VI02)") < s.index("sq.w VF01, 0(VI01)")'
	python3 -c 'import pathlib; s=pathlib.Path("baseline.vsm").read_text(); assert s.index("sq.w VF01, 0(VI01)") < s.index("lq.w VF02, 0(VI02)")'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
