# VU1 forward-exit branch-delay differential harness

This fixture validates branch-delay filling in a no-Q counted loop with a forward
early exit while an unrelated loop in the same microprogram is software
pipelined.

Historically, applying any generic software-pipeline rewrite disabled the
whole-program branch-delay filler. The candidate keeps the generic pipeline
enabled and fills proven-safe delay slots in the explicitly classified
forward-exit loop. The baseline uses the diagnostic
`OPENVCL_DISABLE_FORWARD_EXIT_BRANCH_DELAY_FILL=1` switch to retain the old
empty-delay behavior while keeping the same `pipe_lid` software pipeline.

The forward loop processes three values and exits on the fourth. The instruction
moved into the early branch delay slot is required on the fallthrough path and
dead on the taken path. The EE harness restores identical VU1 memory for both
programs and compares the written rows plus the final input pointer.

Build:

```sh
make -C test/ps2/vu1_forward_exit_differential
```

Run in PCSX2:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_forward_exit_differential/vu1_forward_exit_differential.elf
```

Expected marker:

```text
VU1_DIFFERENTIAL_PASS forward-exit rows=3 pointer=3
```
