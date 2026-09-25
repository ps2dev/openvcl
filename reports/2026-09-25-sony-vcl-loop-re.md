# Sony VCL 1.4beta7 loop optimizer: directed reverse-engineering notes

## Scope

This report records only the subset of Sony VCL 1.4beta7 needed to understand
why its software-pipelined VU loops can outperform OpenVCL's current greedy
modulo placer. The goal is behavioral comparison and reconstruction of general
algorithms, not reproduction of proprietary source code. The Sony binaries are
not checked into this repository.

The public package was reached from the PS2 Linux VUC/Vector Unit Command Line
archive and inspected from a temporary directory. The relevant artifacts are:

- `vcl.1.4.beta7.x86.tgz`
  SHA-256 `8c6f6cb5e80efe3a80fb9ad821f81f7903a474146ce6c86042250bcb3724b248`
- `vcl_14beta7_x86`
  SHA-256 `83bee75205b5b0f37dbefc2c2769eba2c353befa532472c49aedb2ebaf168274`
  (ELF32 i386, dynamically linked, stripped)
- `vcl_14beta7.exe`
  SHA-256 `c1fe1fe4a79875437559c451b469279da5a6e955f0cfa17c370af0a475bcabd6`
  (PE32 i386 console executable)
- `vcl.1.4.beta7.tgz`
  SHA-256 `76d8802e9cb80b07cb889902e5cf319f3f1e5676348d2db2abaf1309085b361f`
  containing the stripped MIPS-III PS2 Linux build.

The packaged license identifies the software as Sony Computer Entertainment
America VU Command Line 1.4.beta7 and permits no-cost use and unmodified
redistribution with the license. The analysis below uses only the public binary
and its observable behavior/strings.

## Why this matters now

The OpenVCL exact feasibility solver gives a much stronger result than the
heuristic experiments that preceded it. For `indexed/xform_loop_lid`, under the
same precise DDG and MRT model used by the experimental kernel placer, every
II from 35 through 46 is SAT even when all slots are restricted to two pipeline
stages. II=35 takes 45 search states and 2212 AC-3 revisions. The C++ solver
independently validates the II=35 witness by rebuilding the MRT and rechecking
every DDG inequality.

The ordinary greedy placer at II=35 places 61/62 nodes and fails on node 60
(`IADDIU next_output`). The exact witness changes 39 of those 61 already-placed
slots. The gap is therefore a global coordinated-placement problem, not a local
repair around the final `MFIR`/`SQ`/`IADDIU` chain.

The checked-in Sony VCL 1.4beta7 reference reports `optimal=38` for the same
loop and emits a 38-row MAIN. Because OpenVCL's simplified model can fit 35,
Sony's 38 should not be interpreted as evidence that Sony merely drops a DDG
edge. The more useful question is what additional full-pipeline constraints or
search/finalization stages make 38 the first accepted candidate.

## High-confidence optimizer map

Addresses below refer to `vcl_14beta7_x86`. Names are descriptive labels added
for this investigation; the binary is stripped.

| Address | Evidence | Working interpretation |
| --- | --- | --- |
| `0x08079778` | xref to `Opt::Loop %16s  %d/%d`; single caller identified | Core loop optimization driver (`Opt::Loop`) |
| `0x0805f29c` | trivial getter of stream field `+0x2e4`; used to seed candidate loop size | minimum/current loop-size value |
| `0x08076b3c` | failure path reaches `failed to generate a work table for loop size %d` | build candidate loop work table |
| `0x08077260` | failure path reaches `failed to fix the new work table for loop opt` | fix/finalize new work table |
| `0x08076638` | called immediately after `lpgen[%d](limit: %s) - '%s' flow %s` diagnostic | loop generation (`lpgen`) |
| `0x0806bf24` | failure path says `failed to reset stats for cpu` | reset scheduling/statistics state |
| `0x0805f654` | failure path says `failed to calc stats for loop` | calculate loop statistics |
| `0x0807d458` | failure path says `failed to reduce extents for code!` | reduce candidate code extents |
| `0x08078aa8` | failure path says `failed to validate/patch moves - so continue` | validate/fix moves and post-placement loop structure |
| `0x08089878` | failure path says `failed to assign reg size would be %d!` | register assignment / register-size validation |
| `0x080741b4` | exactly returns candidate field `+0x38` | getter for candidate loop size/II-like quantity |
| `0x0807a3dc` | xref to `can't do modular reduction` | modular-reduction eligibility / transformation path |
| `0x0807a560` | xref to `failed to get II for cap` | derive II-dependent cap state/instruction information |
| `0x0807a868` | xref to `cap_ii not set` | consume the cap-II state / build cap-related representation |
| `0x0807aa28` / `0x0807aaf4` | xrefs to `failed to sred a table from %s` | table-level scheduling reduction (`sred`) path |

The binary also exposes useful vocabulary: `LATENCY`, `THRUPUT`,
`instruction latency`, `instruction thruput`, `external input had latency for
loop`, `external input had thruput for loop`, `latency bigger than loop!`,
`thruput bigger than loop!`, `SYNTH_LOOP_MINSAFE`, `SYNTH_LOOP_EXTRA`, and
`sidefx : loop@ %d critical at %d recoverable at %d minSafe=%d..so abort`.
These strings strongly indicate that loop generation is coupled to latency,
throughput, side-effect safety and synthetic-loop metadata beyond a plain
binary DDG/MRT feasibility test.

## Candidate-size search reconstructed from `Opt::Loop`

The current high-confidence control-flow reconstruction is:

1. Obtain a minimum loop-size value from the input stream.
2. Apply user/configuration bounds corresponding to `--LoopExtra` and
   `--LoopAbs`. The binary contains the diagnostic
   `the minimal possible loop size + --LoopExtra was greather than the
   --LoopAbs setting, so ignoring --LoopAbs`.
3. Build a work table for the current candidate loop size.
4. Try a bounded set of internal optimization strategies. Strings adjacent to
   this subsystem include `ldumb`, `normal1`, `hDown`, `vuta`, `normal`,
   `another`, `vuta1`, `normal2`, and `dumb2`.
5. For each strategy, run loop generation and the subsequent validation chain:
   stats, optional reductions, move validation/patching and register assignment.
6. Keep the best fully valid candidate for that size according to an internal
   candidate field at `+0x3c`.
7. If no strategy produces a complete valid candidate, increment the candidate
   loop size and retry. Once the current size has at least one valid candidate,
   finalize the best candidate rather than continuing an unrestricted search
   across larger sizes.

The important consequence is that Sony appears to search for the **smallest
loop size that survives the complete optimization/finalization pipeline**, not
merely the smallest DDG/MRT-feasible II. This is consistent with OpenVCL finding
model-only II=35 while the Sony reference settles at 38.

## Candidate fields: confirmed vs inferred

### Field `+0x38`: strong identification as loop size / II-like value

- `0x080741b4` returns this field directly.
- `Opt::Loop` uses that getter for the new value printed in
  `Opt::Loop %16s  %d/%d`.
- move/post-opt validation compares `+0x38` against the candidate table extent
  (`end - start`).

Treating this as the candidate loop size is therefore a strong inference.

### Field `+0x3c`: candidate-quality / loop-structure metric, exact meaning TBD

Within the per-size search, `Opt::Loop` compares candidate `+0x3c` values and
keeps the lower value (ties can replace the previous candidate). The move
validation path computes/updates this field from loop-position metadata. It is
therefore clearly a selection metric or structural property of the generated
loop, but calling it a generic runtime "cost" would currently overstate the
evidence. A stage-depth / loop-distance (`lid`-like) interpretation is plausible
and remains to be confirmed.

## Implications for OpenVCL

The exact solver and Sony control flow point in the same direction:

- Stop spending effort on one-node or small-neighbourhood greedy repairs for
  `xform_loop_lid`; the II=35 witness requires coordinated changes across most
  of the loop.
- A production-quality placer should use propagation plus bounded global
  backtracking (or another deterministic global scheduling method), not just
  earliest-slot placement with increasingly elaborate local ordering rules.
- Model-only SAT is necessary but not sufficient for promotion. A candidate
  must survive register allocation, prolog/kernel/drain construction, move
  insertion, branch/delay-slot rules and side-effect safety.
- Sony's 38-row result is now a useful full-pipeline target rather than a target
  for the raw DDG/MRT solver. OpenVCL can use the II=35 witness diagnostically,
  but should compare 35, 36, 37 and 38 after complete rewrite construction.

## Next reverse-engineering questions

1. Determine which post-placement phase first rejects candidate sizes 35, 36
   and 37 for the indexed transform loop (or an equivalent minimal repro).
2. Confirm the exact semantics of candidate field `+0x3c`.
3. Reconstruct the strategy selector and the differences between the `ldumb`,
   `vuta`, `hDown`, `normal*` and related paths.
4. Map the `cap_ii` and `sred` paths sufficiently to determine whether they are
   feasibility constraints, canonicalization passes, or profitability/size
   reductions.
5. Build minimal `.vcl` black-box probes once an executable environment for the
   i386/Windows build is available; vary one latency/resource/dependency at a
   time and correlate changes with the static map.
6. Use those findings to design an OpenVCL global placer that remains generic
   and independently validated rather than reproducing Sony implementation
   details.

## Current conclusion

OpenVCL's current II=46/47 placement result is demonstrably heuristic: its own
DDG/MRT model admits a two-stage II=35 schedule. Sony VCL's loop optimizer also
appears substantially more global than OpenVCL's current placer and evaluates
candidate loop sizes through a larger validation/finalization pipeline. The
most productive next step is therefore to identify the first full-pipeline
constraint that separates Sony's accepted size 38 from OpenVCL's model-only
35, then incorporate that distinction into a bounded global OpenVCL scheduler.
