# VU1 integer zero-OR fold differential harness

This fixture validates the generic algebraic fold from:

```text
ior dst, src, vi00
iaddiu dst, dst, imm
```

to:

```text
iaddiu dst, src, imm
```

The candidate uses the normal compiler path. The baseline sets the diagnostic-only
`OPENVCL_DISABLE_INTEGER_ZERO_OR_FOLD=1` switch so the original two-instruction
sequence remains present. Both programs run on VU1 and their output qword is
compared bit-for-bit.

Build and run with the PS2 toolchain configured:

```sh
make -C test/ps2/vu1_integer_zero_or_fold_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_integer_zero_or_fold_differential/vu1_integer_zero_or_fold_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS integer-zero-or-fold value=0000002a
```
