# VU1 memory-base update coalescing differential harness

This fixture validates the conservative pointer-update fold:

```text
base += A
<plain memory accesses using base>
base += B
```

When the intervening uses of `base` are only plain loads/stores with constant
offsets, OpenVCL may remove the first update, add `A` to those memory offsets,
and replace the second update with `base += A+B`. Labels, scheduling barriers,
visible non-memory reads/writes, pre/postincrement memory accesses, and offsets
that no longer fit the VU memory immediate block the transformation.

The baseline disables only this transformation with
`OPENVCL_DISABLE_MEMORY_BASE_UPDATE_COALESCE=1`.

Build and run with the PS2 toolchain configured:

```sh
make -C test/ps2/vu1_memory_base_update_coalesce_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_memory_base_update_coalesce_differential/vu1_memory_base_update_coalesce_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS memory-base-update-coalesce pointer=38
```
