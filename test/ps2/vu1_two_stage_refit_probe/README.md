# VU1 two-stage refit probe

This probe documents the current safety boundary for the experimental two-stage
expanded-DDG refit.

For the real `indexed/xform_loop_lid` body, the corrected global candidate
search finds a structurally complete two-stage refit envelope. That structure is
not sufficient for emission: the final refit still has cross-stage VF hazards
that require modulo variable expansion (MVE) or an equivalent stage-aware
register rotation.

The probe intentionally verifies that OpenVCL **does not** emit the tentative
`__REFIT_MAIN` envelope while those hazards remain. It also runs the diagnostic
source-rename subset sweep. The sweep shows that recursively applying another
ordinary scratch/materialize layer does not close the hazards, so the safety
gate must remain in place.

Run:

```sh
export PS2DEV=/Users/fjtrujy/toolchains/ps2/ps2dev
export PATH="$PS2DEV/dvp/bin:$PATH"
make -C test/ps2/vu1_two_stage_refit_probe clean all
```

The target checks that:

- the selected refit is structurally a two-stage envelope with non-zero final
  rename hints;
- the zero-source-rename closure probe still produces non-zero hazards after a
  second rename/refit pass;
- the experimental compile emits no `__REFIT_MAIN` label;
- candidate and default VSM are byte-identical while emission is blocked;
- both VSM files assemble with `dvp-as`.

This remains a blocker/research probe for the non-MVE two-stage path. The
stage-aware follow-up now lives in `../vu1_two_stage_mve_differential`, which uses
`OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1` and validates the resulting A/B envelope in
PCSX2 while keeping this probe as a regression test for the original safety gate.
