# VU1 generic-kernel direct-refit differential

This fixture validates the opt-in
`OPENVCL_EXPERIMENT_SELF_CONTAINED_REFIT_EMISSION=1` path against the
conservative OpenVCL output using the real `indexed/xform_loop_lid` body.

The candidate must use the direct self-contained refit marker while the baseline
must not. Both VSMs are assembled with `dvp-as`, linked into one PS2 ELF, and
run over identical VU1 memory. The EE exercises 1, 2, 3, 4, 6 and 8 loop
iterations with different first/second vertex-index pairs and a non-degenerate
identity transform, comparing up to 48 output qwords plus the final index/output
pointers bit-for-bit after every case. Candidate/baseline execution order alternates
between cases so correctness cannot rely on the candidate always inheriting VU
register contents from an immediately preceding baseline run.

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
VU1_DIFFERENTIAL_PASS generic-kernel-direct cases=6 max-iterations=8
```

This is a focused semantic differential, not a proof for all loop shapes,
inputs, trip counts, or a hardware performance measurement.
