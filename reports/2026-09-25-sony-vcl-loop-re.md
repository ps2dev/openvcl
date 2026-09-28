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

That comparison now has a concrete result. After coalescing hazardous fields of
one architectural VF into one scratch decision and preserving a vector op as a
single instruction whenever its complete destination mask is covered by that
decision, the expanded-DDG refit produces:

| Exact source candidate | Expanded nodes | Expanded MII / achieved II |
| ---: | ---: | ---: |
| 35 | 71 | 41 / 41 |
| 36 | 71 | 41 / 41 |
| 37 | 68 | **38 / 38** |
| 38 | 70 | 40 / 40 |

The II=37 witness is therefore currently the best full-expansion candidate and
refits to exactly the **38 rows emitted by Sony**. Its expanded body contains 53
passthrough nodes, 4 remaining split clones, 8 whole-destination retargets and 3
materialization MOVEs. The resource occupancy of the resulting 38-row refit is
30 upper and 36 lower slots (FDIV occupancy 14), so its lower-pipe/resource
pressure explains the final MII=38 more directly than a recurrence does.

This also sharpens the interpretation of Sony's outer candidate loop: choosing
the numerically smallest pre-rename schedule is not necessarily optimal after
register-version repair. OpenVCL's exact II=35 witness creates more rename
hazards and materialization pressure and expands to II=41, while the II=37
witness needs fewer decisions and survives the complete shadow refit at 38.
OpenVCL now encodes that policy as a diagnostic bounded sweep: source candidates
must have an independently validated exact witness, then pass through rename and
expanded-DDG refit before ranking. The generic deterministic score compares
final refit II first, then final stage count, expanded node count and finally
source II. With the default four-candidate/two-stage bounds, the automatic sweep
selects **37 -> 38** without inspecting shader names, labels, hashes or renderer
identity. A generic source-II ceiling and per-candidate solver-state budget keep
the diagnostic bounded on much larger search spaces; skipped/`UNKNOWN` cases are
not treated as evidence of infeasibility. It remains deliberately disconnected
from production codegen.

## Next reverse-engineering questions

1. Determine whether Sony's pre-finalization candidate corresponding to its
   38-row loop similarly starts below 38 and expands/repairs upward, or whether
   its internal `+0x38` field already denotes the post-repair size.
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
source DDG/MRT model admits a two-stage II=35 schedule. An earlier version of the
expanded-DDG experiment appeared to find a **37 -> 38** post-rename candidate
matching Sony's steady-state row count. That comparison is now **invalidated**:
the expanded node sequence was built by walking the already-scheduled placement
grid, while the precise DDG derives same-iteration versus carried distance from
sequence order. This could turn original `dist=0` RAW dependencies into synthetic
`dist=1` recurrences and make the refit artificially compact.

The historical `candidate-sweep-001` run still establishes that the diagnostic
sweep itself left all 52 default/no-pipeline/kernel/expanded VSM outputs
bit-identical to `kernel-rewrite-safety-001`. Its 37 -> 38 post-refit number,
however, predates the semantic-order fix and is superseded by the corrected
measurements below.

After rebuilding expanded nodes in original semantic order, the same bounded
source sweep produces **35 -> 49, 36 -> 49, 37 -> 44, 38 -> 48** and selects
source II 37 -> refit II 44. The selected refit has two stages, 74 MAIN nodes,
54 PROLOG nodes and 20 DRAIN nodes. The current OpenVCL model therefore no longer
matches Sony's 38-row reference; the gap is again a genuine modeling/scheduling
question rather than evidence that candidate selection alone solved it.

The direct-emission experiment also has a corrected interpretation. With the
semantic-order DDG, the expanded refit for source II 37 has MII 44 before the
single-stage branch-tail constraint is applied. Thus the old branch-aware II=38
question is below the corrected model's own lower bound, not an `UNKNOWN` caused
by insufficient search budget. The exact solver now optionally adds a
Hall-style bipartite matching check for upper/lower modulo resources, but that
global resource check is not the cause of this particular UNSAT result.

The one-stage direct route remains implemented only behind
`OPENVCL_EXPERIMENT_SELF_CONTAINED_REFIT_EMISSION=1` as a semantic testbed. With
the corrected DDG and conservative rename materialization its best indexed result
is source 37 -> refit 66, not a performance candidate. Expanding the PCSX2
differential exposed a second correctness issue: a renamed producer could leave
its architectural base stale for a later consumer. OpenVCL now tracks dirty
rename decisions and materializes scratch back to the base immediately before the
next required read, without redirecting pre-write live-in reads to uninitialized
scratch. The differential passes six cases from 1 through 8 loop iterations with
varied index pairs, up to 48 output qwords and final loop pointers compared
bit-for-bit. A fresh full corpus run (`corrected-refit-final-014`) has no failures
and remains byte-identical to `direct-refit-default-001` for all 52 VSM outputs,
with identical static-cost and `.vutext` summaries.

The first complete two-stage envelope transport exposes a further distinction
between structural scheduling and executable modulo scheduling. The corrected
37 -> 44 layout is structurally partitionable into PROLOG/MAIN/DRAIN, but its
final register plan still reports 117 cross-stage rename hints across six VF
families. Only four final scratch decisions are allocatable after the existing
source-level renames. Exhaustively trying the eight subsets of the three source
rename decisions shows that nesting the current rename transformation is not a
solution: with no source decisions the first refit reaches II=35 and all five
final decisions fit, but reapplying those five scratch/materialize decisions and
refitting produces II=49, seven further decisions and only two assignments.

This strengthens the interpretation of Sony's loop optimizer as having a
register-allocation/finalization phase materially coupled to modulo scheduling.
OpenVCL should next model modulo variable expansion or an equivalent stage-aware
rotation explicitly rather than recursively adding ordinary scratch copies. The
experimental two-stage emitter remains gated off until that register plan is
semantically closed. The corresponding Release safety baseline
`two-stage-refit-safety-003` completes the full 52-VSM corpus with outputs,
static-cost metrics and `.vutext` sizes identical to `corrected-refit-final-014`.

That stage-aware experiment is now available behind
`OPENVCL_EXPERIMENT_TWO_STAGE_MVE=1`. Instead of consuming scratch registers with
the first-level rename and then trying to repair the final layout again, OpenVCL
lets the final two-stage refit identify the cross-stage VF families and assigns an
alternate physical version to each one. On the indexed loop all four bounded
source candidates (II 35..38) converge to a two-stage **II=35** refit with 62 MAIN
nodes; the envelope contains 37 PROLOG and 25 DRAIN nodes. Five final VF families
need alternate versions and all five fit.

The resulting A/B emitter has passed a focused PCSX2 differential over 11 trip
counts/cases (1..10 and 12 iterations), varied vertex-index inputs, three transform
matrices and different geometry/color seeds, including final pointer state. This
is evidence that stage-aware versioning solves the semantic overlap which ordinary
recursive scratch/materialize renaming did not. It is not evidence that Sony uses
the same allocator or A/B encoding. In particular, the extracted experimental VU
object is larger than the conservative one (1728 versus 1200 bytes), and runtime
performance on real PS2 hardware has not been measured. The Release safety corpus
`two-stage-mve-safety-004` remains byte-identical to `two-stage-refit-safety-003`
for all 52 normal-path VSM outputs, with identical costs and `.vutext` sizes.

A path-aware static diagnostic now quantifies the A/B envelope rather than adding
all mutually-exclusive blocks as one flat program. Under OpenVCL's cost model the
MVE path is already cheaper at one iteration (79 estimated cycles versus 144) and
reaches 504 versus 1673 at 12 iterations. Thus this loop's remaining uncertainty
is not static amortization but whether the larger code footprint and modeled
schedule translate into a real PS2 speedup. No claim about Sony's profitability
threshold or runtime policy follows from these OpenVCL-only estimates.

The same MVE mechanism is now observed on a second ps2gl loop family. Under the
opt-in corpus sweep, all three general no-spec variants transform
`pt_light_vert_loop_lid` with source II 49 -> refit II 49, two stages, 31 MAIN
nodes, 18 PROLOG nodes, 13 DRAIN nodes and three assigned VF versions. A focused
PCSX2 differential using that arithmetic with non-zero inputs passes 1..8 loop
iterations bit-for-bit. This materially strengthens the OpenVCL-side evidence
that stage-aware versioning is a reusable mechanism rather than an indexed-loop
special case. It still says nothing about whether Sony uses the same A/B scheme.

The broader corpus also gives a useful negative boundary. Specular/pv-diff
families have source MIIs above the current bounded-sweep ceiling (104..145 in
the observed transform loops and 114 for point-light), while fast/fast_nolights/
scei expose only the existing single-Q planner shape. These cases were not made
eligible by weakening safety constraints, so no comparison with Sony should
treat their current lack of MVE output as an infeasibility result.
