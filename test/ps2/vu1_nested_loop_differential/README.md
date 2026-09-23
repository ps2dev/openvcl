# VU1 nested-loop branch-delay differential harness

This fixture validates the conservative nested-loop optimization used for the
outer directional-light shape. The outer loop contains exactly one counted
`LoopCS` backedge. Immediately before the outer counter update, a plain load
fetches state for the next outer iteration.

The candidate delays that load across one independent integer update into the
outer branch delay slot. The load still executes on both the taken and
fallthrough paths; no memory access is introduced or removed. The baseline
sets `OPENVCL_DISABLE_NESTED_LOOP_BRANCH_DELAY_FILL=1` and leaves the delay
slot empty. Both variants disable no-Q cyclic-prefix rewriting so the
differential isolates only this movement.

The EE harness compares two output words, the final table pointer, and the
value loaded on the final outer iteration.

Build:

```sh
make -C test/ps2/vu1_nested_loop_differential
```

Run in PCSX2:

```sh
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_nested_loop_differential/vu1_nested_loop_differential.elf
```

Expected marker:

```text
VU1_DIFFERENTIAL_PASS nested-loop outputs=2 pointer=3 load=333
```
