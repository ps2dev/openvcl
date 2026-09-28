# VU1 two-stage MVE differential

This fixture validates the opt-in `OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1` path
against conservative OpenVCL output using the real `indexed/xform_loop_lid`
body shared with the generic-kernel differential.

The candidate uses a two-bank A/B modulo-variable expansion for the final
two-stage refit. The EE runs the same six cases that previously exposed stale
rename state: 1, 2, 3, 4, 6 and 8 iterations, varied vertex-index pairs and an
identity transform. Candidate and baseline are compared bit-for-bit over all
output qwords and the final index/output pointers.

For the current indexed loop the bounded candidate sweep selects a two-stage
II=35 final refit with 62 MAIN nodes and five final VF versioning decisions, all
assigned. This is deliberately an experimental result; the fixture candidate is
larger than the conservative VU object and no real-hardware timing claim is made.

Build with PS2SDK configured:

```sh
make -C test/ps2/vu1_two_stage_mve_differential clean all
```

Run in PCSX2:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_two_stage_mve_differential/vu1_two_stage_mve_differential.elf
```

Expected marker:

```text
VU1_DIFFERENTIAL_PASS two-stage-mve cases=6 max-iterations=8
```

The fixture is a focused semantic differential. It does not prove correctness
for every loop shape or input, and PCSX2 wall-clock time is not PS2 performance
measurement.
