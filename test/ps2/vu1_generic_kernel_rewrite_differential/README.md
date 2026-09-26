# VU1 generic-kernel direct-refit differential

This fixture validates the opt-in
`OPENVCL_EXPERIMENT_SELF_CONTAINED_REFIT_EMISSION=1` path against the
conservative OpenVCL output using the real `indexed/xform_loop_lid` body.

The candidate must use the direct self-contained refit marker while the baseline
must not. Both VSMs are assembled with `dvp-as`, linked into one PS2 ELF, and
run over identical VU1 memory. The EE compares 12 output qwords plus the final
index/output pointers bit-for-bit.

Build with the PS2SDK environment configured:

```sh
make -C test/ps2/vu1_generic_kernel_rewrite_differential
```

Run in PCSX2 with the shared isolated-data runner:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_generic_kernel_rewrite_differential/vu1_generic_kernel_rewrite_differential.elf
```

The expected result is:

```text
VU1_DIFFERENTIAL_PASS generic-kernel-direct rows=12 index=18 output=236
```

This is a focused semantic differential, not a proof for all loop shapes,
inputs, trip counts, or a hardware performance measurement.
