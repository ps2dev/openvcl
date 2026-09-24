# VU1 dead integer source reuse differential harness

This fixture validates the conservative destructive-reuse rule for:

```text
iaddiu temporary, viNN, imm
mfir.w vfMM, temporary
...
<write viNN before any read or control-flow boundary>
```

When the temporary appears only in the defining `IADDIU` and adjacent `MFIR`,
the compiler may use the physical source VI as the destination if that old source
value is provably dead before any control-flow boundary. The baseline disables
only this transformation with `OPENVCL_DISABLE_DEAD_INTEGER_SOURCE_REUSE=1`.

Build and run with the PS2 toolchain configured:

```sh
make -C test/ps2/vu1_dead_integer_source_reuse_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_dead_integer_source_reuse_differential/vu1_dead_integer_source_reuse_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS dead-integer-source-reuse value=0000002a
```
