# VU1 two-stage MVE point-light differential

This fixture validates the opt-in `OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1`
emitter on a second loop shape: the no-specular `pt_light_vert_loop_lid`
family used by the general no-spec renderers.

The VCL keeps the real point-light arithmetic but makes loop bounds and input
state self-contained in VU memory. The EE exercises 1 through 8 iterations with
different non-zero light/material/input data. Candidate and conservative
baseline are compared bit-for-bit for every written output vector and the final
input/output pointers.

Build and run with:

```sh
make -C test/ps2/vu1_two_stage_mve_point_light_differential clean all
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_two_stage_mve_point_light_differential/vu1_two_stage_mve_point_light_differential.elf
```

Expected marker:

```text
VU1_DIFFERENTIAL_PASS two-stage-mve-point-light cases=8 max-iterations=8
```

This is semantic validation for one additional loop family, not a proof for all
MVE-eligible loops and not a real-PS2 performance measurement.
