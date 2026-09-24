EE_BIN = vu1_memory_base_update_coalesce_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_memory_base_update_coalesce.vcl

all: verify-coalesce $(EE_BIN)

.PHONY: pdk verify-coalesce
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmMemoryBaseUpdateCoalesce/.name vsmMemoryBaseUpdateCoalesceCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmMemoryBaseUpdateCoalesce/.name vsmMemoryBaseUpdateCoalesceBaseline/' $< | OPENVCL_DISABLE_MEMORY_BASE_UPDATE_COALESCE=1 $(OPENVCL) -o $@

verify-coalesce: candidate.vsm baseline.vsm
	python3 -c 'import pathlib; s=pathlib.Path("candidate.vsm").read_text(); assert "iaddiu VI03, VI03, 6" in s; assert "sq VF01, 3(VI03)" in s; assert "sq VF02, 4(VI03)" in s'
	python3 -c 'import pathlib; s=pathlib.Path("baseline.vsm").read_text(); assert s.count("iaddiu VI03, VI03, 3") == 2'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
