# ps2gl optimization experiments

These scripts measure the compiler's **generic** path. They do not enable
`--enable-known-loop-optimizations`, change shaders, or install tools.
They require Python 3, CMake, locally built OpenVCL/MASP, and PS2DEV.

Build and test the host tools from the OpenVCL repository:

```sh
cmake -S . -B build-plan -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_EXAMPLES=OFF
cmake --build build-plan -j8
ctest --test-dir build-plan --output-on-failure
cmake -S ../masp -B ../masp/build-plan -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF
cmake --build ../masp/build-plan -j8
ctest --test-dir ../masp/build-plan --output-on-failure
```

Run a baseline in a **new** output directory:

```sh
python3 tools/ps2gl_baseline.py \
  --ps2dev /Users/fjtrujy/toolchains/ps2/ps2dev \
  --output build/baselines/run-001
```

Use `--ps2gl`, `--masp`, `--masp-source`, and `--openvcl` to override sibling
repository/tool locations. The script pins the local tool paths, removes
inherited `OPENVCL_*` variables, builds all 13 renderers with the SCE override
list empty, and separately assembles the 13 SCE references. It then compares:

- `default`: generic software pipelining enabled;
- `no-pipeline`: generic software pipelining disabled;
- `kernel`: experimental generic kernel rewrite;
- `expanded`: kernel rewrite plus the expanded-DDG experiment.

The initial CMake output must match a second explicit generic compilation.
Every candidate is assembled. The result directory retains preprocessing
stages, VSM/object files, command logs, loop diagnostics, hashes, repository
revisions/diffs, the script used, `.vutext` byte counts, and JSON cost reports.
`summary.md` compares weighted estimates; `metadata.json` must say
`complete: true` and have no `failures` before consuming results. An incomplete
run is never resumed in place. If output paths contain whitespace, JSON and
the summary remain available, but the existing CLI's whitespace-separated
pair-manifest Markdown report is skipped.

The cost model is **not** a hardware timing or semantic validator. Cost input
is parsed strictly: unknown instructions, malformed operands, and invalid
issue-slot layouts make analysis fail instead of being silently undercounted.
Assembly remains an independent syntax check, not a pipeline-correctness proof.
See the execution report for confirmed limitations. `.vutext` includes
assembler alignment padding, so its byte size can exceed eight times the
source instruction-row count.

Loop diagnostics expose both the Q-specific pipeline opportunities and the
complete set of detected loop candidates:

```sh
./openvcl --dump-loop-pipeline-info-json preprocessed.vcl
```

The `loop_candidates` array includes Q producer/consumer counts, loop shape,
memory accesses, pre/post-increment use, XGKICK presence, and explicit CFG-shape
counters: `internal_labels`, `internal_branches`, `exit_branches`, and
`nested_backedges`. In particular, loops with `q_producers: 0` remain visible
even though they are excluded from the Q-specific planner; eligible simple
counted loops may still be handled by the generic cyclic-prefix rewrite path.
These counters distinguish a multi-exit loop from an outer loop that contains a
nested backedge instead of collapsing both into `simple_counted_loop: false`.
`forward_exit_counted_loop` additionally identifies the conservative shape used
for branch-delay filling: all non-backedge branches leave the loop for the same
forward target, the backedge has one known induction update, and the loop has no
internal labels or nested backedges. This is a CFG classification, not a shader
name check.
`single_nested_counted_loop` identifies a separate outer-loop shape: exactly
one internal label and one internal backedge, that backedge targets a counted
`LoopCS`, there are no lateral exits, and the outer backedge has one known
induction update. The current branch-delay optimization additionally requires
the outer loop to have no Q producers.

For scheduler diagnostics only, `OPENVCL_DUMP_NO_Q_LOOP_MII=1` also prints
resource/recurrence lower bounds for no-Q cyclic-prefix candidates. The
`preciseRecmii` values track accumulator lanes independently, while
`preciseRenamedRecmii` additionally removes loop-carried VF edges that the
existing rename model can eliminate. These figures are lower-bound diagnostics,
not measured PS2 cycles and not optimization gates.

For simple counted multi-Q loops, `OPENVCL_DUMP_MULTI_Q_LOOP_MII=1` exposes
the same coarse, field-precise, and rename-aware recurrence bounds over the
complete prolog+main body. `OPENVCL_DUMP_MULTI_Q_KERNEL_PROBE=1` runs the
existing modulo placer on a copy of that body using the precise rename-aware
dependency edges and reports the achieved II/stage count without changing the
normal rewrite decision or emitted VSM. This remains diagnostic-only: a gap
between the lower bound and the placed II is not proof that the lower II is
semantically realizable by the production rewrite. Combine the probe with
`OPENVCL_DUMP_LOOP_BUMP_REASONS=1` to distinguish dependency-edge failures from
MRT resource failures.

The probe also derives ASAP/ALAP/height ordering from those same precise
intra-iteration edges. Mixing precise placement constraints with the older
coarse priority graph can itself create artificial placement failures; keeping
both views consistent is required before interpreting an II gap.

Characterize those accepted no-Q rewrites against a completed baseline:

```sh
python3 tools/analyze_ps2gl_no_q_rewrites.py \
  build/baselines/run-001 --output build/no-q-rewrites.json
```

The report separates the cyclic-prefix selector's isolated baseline/best cycle
estimate, its diagnostic prolog-to-main in-context estimate before and after the
branch-delay discount, the emitted steady-state VSM rows through the backedge
delay slot, and the one-shot fallthrough rows that follow it. `VsmCostAnalyzer`
keeps the architectural backedge delay slots in the repeated block and exposes
subsequent unlabeled rows as a synthetic `<label>__FALLTHROUGH` block with
repeat 1. The report recombines both pieces for compatibility with the old
textual-block metric and separately checks
`cost_steady_state_static_error` and `cost_fallthrough_tail_static_error`.
Both should be zero for the accepted corpus rewrites; the summary also publishes
`split_fallthrough_tail_count` and the maximum errors.
`selector_prefix_in_branch_delay` and `selector_branch_delay_discount` make the
selector's branch-delay heuristic explicit when investigating residual
planner/emitter gaps. None of these values are PS2 runtime measurements or
semantic-equivalence evidence.

Check label independence against a completed baseline:

```sh
python3 tools/check_ps2gl_label_invariance.py \
  build/baselines/run-001 --output build/label-invariance-001
```

This renames every source label in each renderer and checks exact VSM equality
after undoing the renaming, including derived loop labels. All renamed outputs
are also assembled. `--variants default` restricts the check to one mode.
Recorded hashes reject mismatching compiler binaries or edited baseline inputs.
This checks independence from label names; it does not prove correctness,
independence from arbitrary source changes, or equivalence to Sony output.

Validate the real ps2gl `general_quad` renderer in PCSX2 with deterministic
camera/light transforms and a VU1-memory oracle:

```sh
python3 tools/validate_ps2gl_general_quad.py \
  --ps2dev /Users/fjtrujy/toolchains/ps2/ps2dev \
  --output build/runtime/general-quad-oracle-001
```

The validator builds four complete ps2gl variants: default OpenVCL, OpenVCL
with no-Q cyclic-prefix rewrites disabled, OpenVCL with all generic software
pipelining disabled, and the checked-in Sony VSM reference. It runs the same
12 deterministic `linear, quads` frames in PCSX2 and compares a normalized VU1
semantic hash. ps2gl qwords 4..8 contain EE pointers/transport-header state
whose values depend on the linked ELF layout, so the oracle compares qwords
0..3 and 9..1023 instead of hashing raw address-bearing state. A successful run
prints `PS2GL_GENERAL_QUAD_PASS frames=12 variants=default,no-noq,no-pipeline,sce`.

The separate VU1 autoincrement fixture checks the newly covered no-Q case where
a postincrement store stays in the original loop suffix instead of being cloned:

```sh
make -C test/ps2/vu1_autoincrement_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_autoincrement_differential/vu1_autoincrement_differential.elf
```

It compares candidate and no-pipeline VU1 output bit-for-bit and checks the final
postincremented pointer. The expected marker is
`VU1_DIFFERENTIAL_PASS autoincrement rows=4 pointer=36`. This does not permit
arbitrary pre/postincrement motion: such an instruction is still rejected when
it would itself be part of the cloned cyclic prefix.

The known-induction alias fixture checks the separate case where a delayed
suffix store crosses an explicit constant pointer update and a later same-base
store is therefore provably a different address:

```sh
make -C test/ps2/vu1_alias_induction_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_alias_induction_differential/vu1_alias_induction_differential.elf
```

It compares eight output qwords bit-for-bit, including untouched lanes, and
requires the final output pointer to be 40. The expected marker is
`VU1_DIFFERENTIAL_PASS alias-induction rows=8 pointer=40`. Unknown induction
steps and pre/postincrement instructions cloned into the cyclic prefix remain
outside this proof.

The memory-alias fixture guards the more fundamental rule that different VI
register names are not a no-alias proof. It deliberately gives two base
registers the same runtime VU1 address, with a store followed by a load:

```sh
make -C test/ps2/vu1_memory_alias_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_memory_alias_differential/vu1_memory_alias_differential.elf
```

Before the conservative alias fix, normal scheduling moved the load ahead of
the store and PCSX2 observed `1.0` instead of `42.0`. The expected marker after
the fix is `VU1_DIFFERENTIAL_PASS memory-alias value=42280000`. Any future
recovery of cross-base scheduling must therefore carry an independent affine or
provenance proof; a different VI name is insufficient.

The guarded postincrement-prefix fixture covers the separate multi-Q case where
the cyclic prefix itself contains one `LQI (...base++)` and the loop branch
uses that same base as its induction value:

```sh
make -C test/ps2/vu1_postincrement_prefix_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_postincrement_prefix_differential/vu1_postincrement_prefix_differential.elf
```

The candidate keeps the original `LQI` in the prolog, emits a plain
`LQ 0(base)` for the steady-state speculative prefix, and defers the `+1` to
the original loop branch delay slot. An inverted guard skips the speculative
prefix on the final iteration. The harness compares eight output qwords and the
final pointer for trip counts 1, 2, and 4; the expected marker is
`VU1_DIFFERENTIAL_PASS postincrement-prefix cases=1,2,4`. The planner requires
exactly one incrementing access, a one-slot invertible branch that reads the same
base, no other writes to that register, and no later reads before the branch.
Predecrement, multiple incrementing accesses, and later base reads remain blocked.

The forward-exit differential fixture checks branch-delay filling when an
unrelated loop in the same microprogram is already software-pipelined:

```sh
make -C test/ps2/vu1_forward_exit_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_forward_exit_differential/vu1_forward_exit_differential.elf
```

The candidate fills proven-safe delay slots only inside structurally classified
forward-exit loops. The baseline keeps the old empty-delay behavior while still
rewriting the independent Q loop. The expected runtime marker is
`VU1_DIFFERENTIAL_PASS forward-exit rows=3 pointer=3`.

The nested-loop differential fixture checks the separate case where a plain load
of next-outer state can cross one independent integer update into the outer
backedge delay slot:

```sh
make -C test/ps2/vu1_nested_loop_differential
python3 test/ps2/vu1_differential/run_pcsx2.py \
  --elf test/ps2/vu1_nested_loop_differential/vu1_nested_loop_differential.elf
```

The load already executes unconditionally in the source and still executes on
both branch outcomes in the delay slot. The proof does not move pre/postincrement
memory operations and does not rely on the inner loop's induction. The expected
marker is `VU1_DIFFERENTIAL_PASS nested-loop outputs=2 pointer=3 load=333`.

For scheduler isolation only, the following environment switches disable
classes of rewrite without changing source shaders: `OPENVCL_DISABLE_MULTI_Q_SOFTWARE_PIPELINE`,
`OPENVCL_DISABLE_NO_Q_CYCLIC_PREFIX`, `OPENVCL_DISABLE_NO_Q_LOOP_EXTRA_CYCLIC_PREFIX`,
and `OPENVCL_DISABLE_NO_Q_PLAIN_CYCLIC_PREFIX`.
`OPENVCL_DISABLE_PIPELINE_PROFITABILITY_GATE=1` is a separate diagnostic escape
hatch used by semantic fixtures when they must force an otherwise unprofitable
multi-Q transformation. Normal compilation requires the candidate main,
scheduled in prolog context, to have a strictly lower estimated cycle count than
the original loop. `OPENVCL_DISABLE_FORWARD_EXIT_BRANCH_DELAY_FILL=1` is likewise
diagnostic-only and restores the previous empty-delay behavior for the dedicated
runtime fixture. `OPENVCL_DISABLE_NESTED_LOOP_BRANCH_DELAY_FILL=1` similarly
disables only the nested-loop delay-slot movement for its differential fixture.
These variables are diagnostic controls, not shader-specific scheduling
heuristics or supported optimization policy.

Do not rebuild the root OpenVCL binary while these scripts are running: CMake
writes it into the source root even when using a separate build directory.
Keep the result directories outside version control; commit scripts and concise
reports instead. Run runtime/semantic validation before promoting experiments.
