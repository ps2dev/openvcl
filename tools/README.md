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

The cost model is **not** a hardware timing or semantic validator. Its current
unknown-instruction reporting is incomplete; assembly is an independent
syntax check, not a pipeline-correctness proof. See the execution report for
confirmed limitations. `.vutext` includes assembler alignment padding, so its
byte size can exceed eight times the source instruction-row count.

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

Do not rebuild the root OpenVCL binary while either script is running: CMake
writes it into the source root even when using a separate build directory.
Keep the result directories outside version control; commit scripts and concise
reports instead. Run runtime/semantic validation before promoting experiments.
