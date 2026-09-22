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

Characterize those accepted no-Q rewrites against a completed baseline:

```sh
python3 tools/analyze_ps2gl_no_q_rewrites.py \
  build/baselines/run-001 --output build/no-q-rewrites.json
```

The report separates the cyclic-prefix selector's isolated baseline/best cycle
estimate, its diagnostic prolog-to-main in-context estimate before and after the
branch-delay discount, the emitted steady-state VSM rows through the backedge
delay slot, and the one-shot fallthrough rows that happen to share the same
textual label block. It also reports the `VsmCostAnalyzer` label block as
separate static and estimated cycles and checks that
`steady_state + fallthrough_tail == label_block_static`.
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

For scheduler isolation only, the following environment switches disable
classes of rewrite without changing source shaders: `OPENVCL_DISABLE_MULTI_Q_SOFTWARE_PIPELINE`,
`OPENVCL_DISABLE_NO_Q_CYCLIC_PREFIX`, `OPENVCL_DISABLE_NO_Q_LOOP_EXTRA_CYCLIC_PREFIX`,
and `OPENVCL_DISABLE_NO_Q_PLAIN_CYCLIC_PREFIX`. They are diagnostic controls,
not scheduling heuristics or supported optimization policy.

Do not rebuild the root OpenVCL binary while these scripts are running: CMake
writes it into the source root even when using a separate build directory.
Keep the result directories outside version control; commit scripts and concise
reports instead. Run runtime/semantic validation before promoting experiments.
