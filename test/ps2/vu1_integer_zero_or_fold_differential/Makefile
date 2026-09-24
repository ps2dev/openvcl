EE_BIN = vu1_integer_zero_or_fold_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_integer_zero_or_fold.vcl

all: verify-fold $(EE_BIN)

.PHONY: pdk verify-fold
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmIntegerZeroOrFold/.name vsmIntegerZeroOrFoldCandidate/' $< | $(OPENVCL) -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmIntegerZeroOrFold/.name vsmIntegerZeroOrFoldBaseline/' $< | OPENVCL_DISABLE_INTEGER_ZERO_OR_FOLD=1 $(OPENVCL) -o $@

verify-fold: candidate.vsm baseline.vsm
	python3 -c 'import pathlib; s=pathlib.Path("candidate.vsm").read_text().lower(); assert "ior " not in s'
	python3 -c 'import pathlib; s=pathlib.Path("baseline.vsm").read_text().lower(); assert "ior " in s'

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
