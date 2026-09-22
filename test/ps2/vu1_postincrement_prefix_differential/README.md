# VU1 guarded postincrement-prefix differential harness

This fixture validates the narrow multi-Q transformation where a postincrement
load belongs to the cyclic prefix and its base register is also the loop
branch induction variable.

The candidate build disables only the generic profitability gate so this
semantic fixture keeps exercising the transformation even when its synthetic
loop is not selected by the default cost policy. It keeps the original
`LQI (...++)` in the prolog. In steady state it must:

1. guard the speculative next-iteration prefix with the inverted loop branch;
2. replace the cloned `LQI` with a plain `LQ 0(base)`, so the prefetch has
   no integer side effect;
3. execute `iaddiu base, base, 1` in the original loop branch delay slot.

The no-pipeline baseline executes the original `LQI` once per iteration.
The profitability bypass is diagnostic/test-only; normal compilation keeps the
gate enabled.

The EE harness runs both versions with trip counts 1, 2, and 4. Before every
run it restores identical VU1 data memory, then compares all eight output
qwords bit-for-bit (including untouched sentinel lanes) and the final input
pointer. The one-iteration case specifically proves that the guarded drain
does not execute the deferred increment.

Build with the PS2 toolchain environment configured:

```sh
make -C test/ps2/vu1_postincrement_prefix_differential
```

Run in PCSX2 with the shared runner:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_postincrement_prefix_differential/vu1_postincrement_prefix_differential.elf
```

A successful run prints:

```text
VU1_DIFFERENTIAL_PASS postincrement-prefix cases=1,2,4
```

The planner remains conservative: this proof requires exactly one postincrement
load in the loop, a one-slot invertible `IBEQ/IBNE` branch that reads the same
base register, no other writes to that register, and no later reads of it before
the branch.
