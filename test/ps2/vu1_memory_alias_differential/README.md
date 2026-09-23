# VU1 memory-alias differential harness

This fixture guards the scheduler rule for plain VU memory accesses whose base
registers are different. Different VI register names are not a no-alias proof:
two VI registers can contain the same runtime address.

The source intentionally initializes `vi01` and `vi02` to the same qword (16),
then stores `42.0` through `vi01` and loads through `vi02`. OpenVCL builds it in
two forms:

- `candidate.vsm`: the normal source, where the ready scheduler is free to move
  instructions only when its generic dependency rules prove that movement safe;
- `baseline.vsm`: the same source with a `--barrier` between the store and load.

With the old different-base-is-distinct rule, the candidate moved the load ahead
of the store and PCSX2 observed `1.0` (`3f800000`) instead of the baseline's
`42.0` (`42280000`). The corrected rule keeps the potentially aliasing accesses
ordered, so both executions return `42.0`.

Build and run with the PS2 toolchain environment configured:

```sh
make -C test/ps2/vu1_memory_alias_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_memory_alias_differential/vu1_memory_alias_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS memory-alias value=42280000
```

This fixture proves only that different VI base names must remain potentially
aliasing unless a separate address/provenance analysis establishes otherwise.
