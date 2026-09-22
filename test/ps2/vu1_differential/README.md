# VU1 differential harness

This is a hardware/emulator semantic check for generic OpenVCL scheduling. It
builds one terminating VU1 program twice from the same source:

- `candidate.vsm`: the normal generic compiler path;
- `baseline.vsm`: the same source with generic software pipelining disabled.

The EE harness restores identical VU1 data memory before each run, executes both
programs, compares the complete output qwords bit-for-bit, and also checks a
small set of exact expected values. The fixture deliberately exercises a real
Q software-pipeline rewrite; the two VSM files are not expected to be textually
identical. The build also checks that the candidate contains the generated
pipeline prolog while the conservative baseline does not, so a runtime PASS
cannot silently degrade into a comparison of two unoptimized programs.

Build with the PS2 toolchain environment configured:

```sh
make -C examples/pdk
make -C test/ps2/vu1_differential
```

Run `vu1_differential.elf` on a PS2 or a PS2 emulator that exposes stdout. A
successful run prints:

```text
VU1_DIFFERENTIAL_PASS rows=4
```

With PS2Link reachable, for example:

```sh
ps2client execee test/ps2/vu1_differential/vu1_differential.elf
```

This first harness validates optimized OpenVCL against a conservative OpenVCL
route. It does not by itself establish equivalence to Sony VCL, nor does it
replace full ps2gl renderer tests. Its purpose is to provide a reusable runtime
oracle before scheduler changes are promoted.
