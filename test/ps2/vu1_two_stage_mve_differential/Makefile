EE_BIN = vu1_two_stage_mve_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = ../vu1_generic_kernel_rewrite_differential/vu1_generic_kernel_rewrite.vcl
CANDIDATE_LOG = candidate.log
BASELINE_LOG = baseline.log
PYTHON ?= python3

all: verify-rewrite $(EE_BIN)

.PHONY: pdk verify-rewrite cost
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmGenericKernelRewrite/.name vsmGenericKernelRewriteCandidate/' $< | env OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1 $(OPENVCL) --disable-known-loop-optimizations -o $@ 2> $(CANDIDATE_LOG)

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmGenericKernelRewrite/.name vsmGenericKernelRewriteBaseline/' $< | $(OPENVCL) --disable-known-loop-optimizations -o $@ 2> $(BASELINE_LOG)

verify-rewrite: candidate.vsm baseline.vsm
	grep -E -q '^\[two-stage-mve-plan\].*eligible=1' $(CANDIDATE_LOG)
	grep -q '^xform_loop_lid__MVE_MAIN_B:' candidate.vsm
	grep -q '^xform_loop_lid__MVE_MAIN_A:' candidate.vsm
	! grep -q '__MVE_' baseline.vsm

cost: candidate.vsm baseline.vsm
	$(PYTHON) analyze_cost.py --openvcl $(OPENVCL) --candidate candidate.vsm --baseline baseline.vsm --max-iterations 12

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm $(CANDIDATE_LOG) $(BASELINE_LOG)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
