OPENVCL ?= ../../../openvcl
DVP_AS ?= dvp-as

VCL := vu1_two_stage_refit.vcl
CANDIDATE := candidate.vsm
BASELINE := baseline.vsm
CANDIDATE_LOG := candidate.log
BASELINE_LOG := baseline.log

.PHONY: all clean verify

all: $(CANDIDATE) $(BASELINE) verify candidate.o baseline.o

$(CANDIDATE): $(VCL) $(OPENVCL)
	env OPENVCL_EXPERIMENT_TWO_STAGE_REFIT_ENVELOPE=1 \
	    OPENVCL_EXPERIMENT_SOURCE_RENAME_SUBSET_SWEEP=1 \
	    $(OPENVCL) --disable-known-loop-optimizations $(VCL) -o $@ 2> $(CANDIDATE_LOG)

$(BASELINE): $(VCL) $(OPENVCL)
	$(OPENVCL) --disable-known-loop-optimizations $(VCL) -o $@ 2> $(BASELINE_LOG)

verify: $(CANDIDATE) $(BASELINE)
	grep -E -q '^\[exact-candidate-sweep\].*refitRenameHints=[1-9][0-9]*.*twoStageEnvelope=1' $(CANDIDATE_LOG)
	grep -E -q '^\[source-rename-subset\].*mask=0 .*finalDecisions=[1-9][0-9]* .*closureRan=1 .*closureHints=[1-9][0-9]*' $(CANDIDATE_LOG)
	! grep -q '__REFIT_MAIN:' $(CANDIDATE)
	cmp -s $(CANDIDATE) $(BASELINE)

candidate.o: $(CANDIDATE)
	$(DVP_AS) $< -o $@

baseline.o: $(BASELINE)
	$(DVP_AS) $< -o $@

clean:
	rm -f $(CANDIDATE) $(BASELINE) $(CANDIDATE_LOG) $(BASELINE_LOG) candidate.o baseline.o
