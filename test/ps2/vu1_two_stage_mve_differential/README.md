# VU1 two-stage MVE differential

This fixture validates the opt-in `OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1` path
against conservative OpenVCL output using the real `indexed/xform_loop_lid`
body shared with the generic-kernel differential.

The candidate uses a two-bank A/B modulo-variable expansion for the final
two-stage refit. The EE runs 11 cases spanning 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
and 12 iterations, varied vertex-index pairs, three transform matrices and
different geometry/color seeds. Candidate and baseline are compared bit-for-bit
over all output qwords and the final index/output pointers.

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
VU1_DIFFERENTIAL_PASS two-stage-mve cases=11 max-iterations=12
```

The fixture also has a path-aware static cost diagnostic:

```sh
make -C test/ps2/vu1_two_stage_mve_differential cost
```

It sums only blocks that execute for each trip count instead of treating the
alternating `MAIN_A`/`MAIN_B` branches as one flat program. With the current
fixture, both raw static rows and the latency-aware estimate first favour MVE at
one iteration. At 12 iterations the transformed region is 504 estimated cycles
versus 1673 for the conservative baseline. These are OpenVCL model values, not
hardware timings.

The fixture is a focused semantic differential. It does not prove correctness
for every loop shape or input, and neither PCSX2 wall-clock time nor the static
cost model is a PS2 performance measurement.
