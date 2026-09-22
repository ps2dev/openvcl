# VU1 known-induction alias differential harness

This fixture validates suffix-store draining when two same-base stores are
separated by a known self-induction update.

The source writes two output qwords per iteration through `vi03`. The first
store is followed by `iaddiu vi03, vi03, 1`, so a later store written as
`0(vi03)` is physically distinct from the first store. The planner may only
use that fact when the intervening update has a proven constant step.

OpenVCL builds the source twice:

- `candidate.vsm`: normal generic software pipelining, which must contain a
  `loop_lid__PROLOG` and delayed negative-offset stores;
- `baseline.vsm`: the same source with
  `--disable-generic-software-pipelining`.

The EE harness restores identical VU1 data memory before each run and compares
all eight output qwords bit-for-bit, including untouched W lanes. It also
compares and validates the final output pointer (40), proving that the two
pointer advances per iteration remain exact.

Build with the PS2 toolchain environment configured:

```sh
make -C test/ps2/vu1_alias_induction_differential
```

Run in PCSX2 with the shared runner:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_alias_induction_differential/vu1_alias_induction_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS alias-induction rows=8 pointer=40
```

This proves only address translation through known constant self-induction.
Unknown base updates and arbitrary aliasing remain conservative.
