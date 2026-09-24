EE_BIN = vu1_dead_integer_source_reuse_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_dead_integer_source_reuse.vcl

all: verify-reuse $(EE_BIN)

.PHONY: pdk verify-reuse
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmDeadIntegerSourceReuse/.name vsmDeadIntegerSourceReuseCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmDeadIntegerSourceReuse/.name vsmDeadIntegerSourceReuseBaseline/' $< | OPENVCL_DISABLE_DEAD_INTEGER_SOURCE_REUSE=1 $(OPENVCL) -o $@

verify-reuse: candidate.vsm baseline.vsm
	python3 -c 'import pathlib; s=pathlib.Path("candidate.vsm").read_text(); assert "iaddiu VI01, VI01, 7" in s'
	python3 -c 'import pathlib; s=pathlib.Path("baseline.vsm").read_text(); assert "iaddiu VI01, VI01, 7" not in s'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
