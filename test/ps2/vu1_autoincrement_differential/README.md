# VU1 autoincrement differential harness

This fixture validates the generic no-Q cyclic-prefix path when a counted loop
contains a postincrement store that remains in the loop suffix.

The source deliberately mirrors the memory shape of ps2gl's indexed directional
light loop: it reads the current accumulation value, stores the updated value
with `sqi.xyz (...++)`, advances a separate input pointer, and branches on that
input pointer. OpenVCL builds the source twice:

- `candidate.vsm`: normal generic scheduling, which must contain
  `loop_lid__PROLOG`;
- `baseline.vsm`: the same source with
  `--disable-generic-software-pipelining`.

The EE harness restores identical VU1 data memory before each run and compares
all four output qwords bit-for-bit. It also stores the final postincremented
output pointer in VU1 memory and requires it to equal 36, proving the side effect
still occurs exactly once for each of the four iterations.

Build with the PS2 toolchain environment configured:

```sh
make -C test/ps2/vu1_autoincrement_differential
```

Run in PCSX2 with the shared runner:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_autoincrement_differential/vu1_autoincrement_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS autoincrement rows=4 pointer=36
```

This proves the checked fixture, not arbitrary pre/postincrement motion. The
planner must still reject any candidate whose cloned cyclic prefix itself
contains a pre/postincrement memory side effect.
