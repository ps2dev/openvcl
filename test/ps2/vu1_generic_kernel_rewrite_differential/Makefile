EE_BIN = vu1_generic_kernel_rewrite_differential.elf
EE_OBJS = main.o candidate.o baseline.o
EE_INCS = -I../../../examples/pdk/src
EE_LDFLAGS = -L../../../examples/pdk/lib
EE_LIBS = -lpdk
EE_DVP = dvp-as

OPENVCL = ../../../openvcl
SOURCE = vu1_generic_kernel_rewrite.vcl

all: verify-rewrite $(EE_BIN)

.PHONY: pdk verify-rewrite
pdk:
	$(MAKE) -C ../../../examples/pdk

$(EE_BIN): pdk

candidate.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmGenericKernelRewrite/.name vsmGenericKernelRewriteCandidate/' $< | env OPENVCL_EXPERIMENT_SELF_CONTAINED_REFIT_EMISSION=1 $(OPENVCL) --disable-known-loop-optimizations -o $@

baseline.vsm: $(SOURCE) $(OPENVCL)
	sed 's/\.name vsmGenericKernelRewrite/.name vsmGenericKernelRewriteBaseline/' $< | $(OPENVCL) --disable-known-loop-optimizations -o $@

verify-rewrite: candidate.vsm baseline.vsm
	grep -q '^xform_loop_lid__DIRECT_END:' candidate.vsm
	! grep -q '^xform_loop_lid__DIRECT_END:' baseline.vsm

candidate.o: candidate.vsm
	$(EE_DVP) $< -o $@

baseline.o: baseline.vsm
	$(EE_DVP) $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) candidate.vsm baseline.vsm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
