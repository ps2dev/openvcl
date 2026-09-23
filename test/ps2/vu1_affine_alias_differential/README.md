# VU1 affine-alias differential harness

This fixture validates the narrow scheduler case where two VU memory bases are
derived from the same VI root by a known constant offset.  The candidate removes
an explicit scheduling barrier and is required to move the load before the store;
the baseline keeps source order.  The two effective addresses differ by four
qwords, so the reordering is safe even though distinct VI register names alone
would not be a no-alias proof.

Build and run with the PS2 toolchain configured:

```sh
make -C test/ps2/vu1_affine_alias_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_affine_alias_differential/vu1_affine_alias_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS affine-alias value=40e00000
```

The proof applies only to plain VU memory operations whose bases have a common
tracked root and a non-zero constant address delta modulo the VU0 memory size.
Unknown roots and pre/postincrement operations remain alias-conservative.
