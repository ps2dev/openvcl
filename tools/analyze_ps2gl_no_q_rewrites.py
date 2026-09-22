#!/usr/bin/env python3
"""Characterize accepted generic no-Q cyclic-prefix rewrites in the ps2gl corpus.

The report deliberately separates four quantities:
  * selector baseline/best cycles reported by OpenVCL while choosing a rewrite;
  * the diagnostic in-context selector estimate;
  * rows in the emitted VSM steady-state loop through its branch delay slot;
  * loop-exit fallthrough rows that share the same textual VSM label block.

VsmCostAnalyzer label blocks are also reported explicitly as static and estimated
cycles. They are intentionally not called steady-state costs: a label block can
contain both the repeated backedge path and one-shot fallthrough after the delay
slot.

These are static diagnostics only. They are not PS2 runtime timings or a semantic
correctness oracle.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


RENDERERS = (
    "fast_nolights", "fast", "general", "general_quad", "general_tri",
    "general_nospec", "general_nospec_quad", "general_nospec_tri",
    "general_pv_diff", "general_pv_diff_quad", "general_pv_diff_tri",
    "indexed", "scei",
)

TRACE_START = re.compile(
    r"\[cyclic-prefix-trace\] loop=(\S+) branchOffset=(\d+) baselineCycles=(\d+)"
)
TRACE_RESULT = re.compile(
    r"\[cyclic-prefix-trace\] loop=(\S+) result=(ACCEPTED|no_improvement) "
    r"bestCycles=(\d+)(?: bestCyclesInContext=(\d+))?"
    r"(?: bestCyclesInContextRaw=(\d+))?"
    r"(?: branchDelayDiscount=(\d+))?"
    r"(?: prefixInBranchDelay=(\d+))?"
)


def run(argv, env=None):
    proc = subprocess.run(
        list(map(str, argv)),
        env=env,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if proc.returncode:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(map(str, argv))}\n{proc.stderr}"
        )
    return proc


def parse_accepted(stderr):
    baselines = {}
    accepted = {}
    for line in stderr.splitlines():
        match = TRACE_START.search(line)
        if match:
            baselines[match.group(1)] = int(match.group(3))
            continue
        match = TRACE_RESULT.search(line)
        if not match or match.group(2) != "ACCEPTED":
            continue
        label = match.group(1)
        accepted[label] = {
            "selector_baseline_cycles": baselines.get(label),
            "selector_best_cycles": int(match.group(3)),
            "selector_best_cycles_in_context": (
                int(match.group(4)) if match.group(4) is not None else None
            ),
            "selector_best_cycles_in_context_raw": (
                int(match.group(5)) if match.group(5) is not None else None
            ),
            "selector_branch_delay_discount": (
                int(match.group(6)) if match.group(6) is not None else None
            ),
            "selector_prefix_in_branch_delay": (
                bool(int(match.group(7))) if match.group(7) is not None else None
            ),
        }
    return accepted


def is_issue_line(line):
    stripped = line.strip()
    return bool(
        stripped
        and not stripped.endswith(":")
        and not stripped.startswith(".")
        and not stripped.startswith(";")
        and not stripped.startswith("#")
    )


def emitted_loop_region(vsm_text, label):
    lines = vsm_text.splitlines()
    start = next((i for i, line in enumerate(lines) if line.strip() == label + ":"), None)
    if start is None:
        return None

    branch_re = re.compile(
        r"\b(?:ibne|ibeq|ibgtz|ibgez|ibltz|iblez|b)\b.*\b"
        + re.escape(label)
        + r"\b"
    )
    issue_count = 0
    branch_issue = None
    delay_issue = None
    for index in range(start + 1, len(lines)):
        stripped = lines[index].strip()
        if index > start + 1 and stripped.endswith(":") and not lines[index][:1].isspace():
            break
        if not is_issue_line(lines[index]):
            continue
        issue_count += 1
        if branch_issue is None and branch_re.search(lines[index]):
            branch_issue = issue_count
            continue
        if branch_issue is not None and delay_issue is None:
            delay_issue = issue_count

    if branch_issue is None or delay_issue is None:
        return None

    # VU backedges have one architectural delay slot. The repeated path is the
    # label through that slot; subsequent rows in the same textual label block
    # are one-shot fallthrough and must not be multiplied by the loop repeat.
    return {
        "steady_state_static_cycles": delay_issue,
        "fallthrough_tail_static_cycles": issue_count - delay_issue,
        "label_block_static_cycles_from_vsm": issue_count,
        "backedge_static_cycle": branch_issue,
    }


def exact_block_cost(cost, label):
    for block in cost.get("blocks", []):
        if block.get("label") == label:
            return {
                "static": block.get("cycles"),
                "estimated": block.get("estimated_cycles", block.get("cycles")),
            }
    for block_label, block in cost.get("cost_by_label", {}).items():
        if block_label == label:
            return {
                "static": block.get("static_cycles"),
                "estimated": block.get("estimated_cycles", block.get("static_cycles")),
            }
    return None


def label_block_cost(cost, label):
    main = exact_block_cost(cost, label)
    if main is None:
        return {
            "static": None,
            "estimated": None,
            "steady_static": None,
            "tail_static": None,
            "split": False,
        }

    tail = exact_block_cost(cost, f"{label}__FALLTHROUGH")
    if tail is None:
        return {
            "static": main["static"],
            "estimated": main["estimated"],
            "steady_static": None,
            "tail_static": None,
            "split": False,
        }

    return {
        "static": main["static"] + tail["static"],
        "estimated": main["estimated"] + tail["estimated"],
        "steady_static": main["static"],
        "tail_static": tail["static"],
        "split": True,
    }


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "baseline",
        type=Path,
        help="Completed ps2gl_baseline.py output containing build-openvcl/vu1",
    )
    parser.add_argument("--openvcl", type=Path, default=root / "openvcl")
    parser.add_argument("--output", type=Path, help="Optional JSON report path")
    args = parser.parse_args()

    baseline = args.baseline.resolve()
    source_dir = baseline / "build-openvcl" / "vu1"
    if not args.openvcl.is_file():
        parser.error(f"OpenVCL executable not found: {args.openvcl}")
    if not source_dir.is_dir():
        parser.error(f"Baseline source directory not found: {source_dir}")

    env = {key: value for key, value in os.environ.items()
           if not key.startswith("OPENVCL_")}
    env["OPENVCL_DUMP_CYCLIC_PREFIX_REJECTIONS"] = "1"
    rows = []

    with tempfile.TemporaryDirectory(prefix="openvcl-no-q-") as temp_dir:
        temp_dir = Path(temp_dir)
        for renderer in RENDERERS:
            source = source_dir / f"{renderer}_pp4.vcl"
            if not source.is_file():
                raise RuntimeError(f"missing preprocessed source: {source}")

            vsm = temp_dir / f"{renderer}.vsm"
            compile_proc = run(
                [args.openvcl, "--disable-known-loop-optimizations", source, "-o", vsm],
                env=env,
            )
            accepted = parse_accepted(compile_proc.stderr)
            if not accepted:
                continue

            cost_proc = run(
                [args.openvcl, "--cost-json", "--cost-loop-preset", "ps2gl", vsm]
            )
            cost = json.loads(cost_proc.stdout)
            vsm_text = vsm.read_text()

            for label in sorted(accepted):
                region = emitted_loop_region(vsm_text, label)
                block_cost = label_block_cost(cost, label)
                row = {
                    "renderer": renderer,
                    "loop": label,
                    **accepted[label],
                    **(region or {
                        "steady_state_static_cycles": None,
                        "fallthrough_tail_static_cycles": None,
                        "label_block_static_cycles_from_vsm": None,
                        "backedge_static_cycle": None,
                    }),
                    "cost_label_block_static_cycles": block_cost["static"],
                    "cost_label_block_estimated_cycles": block_cost["estimated"],
                    "cost_steady_state_static_cycles": block_cost["steady_static"],
                    "cost_fallthrough_tail_static_cycles": block_cost["tail_static"],
                    "cost_split_fallthrough_tail": block_cost["split"],
                }
                baseline_cycles = row["selector_baseline_cycles"]
                best_cycles = row["selector_best_cycles"]
                context_cycles = row["selector_best_cycles_in_context"]
                context_cycles_raw = row["selector_best_cycles_in_context_raw"]
                emitted_cycles = row["steady_state_static_cycles"]
                block_static_cycles = row["cost_label_block_static_cycles"]
                row["selector_gain_cycles"] = (
                    baseline_cycles - best_cycles
                    if baseline_cycles is not None and best_cycles is not None
                    else None
                )
                row["emission_gap_vs_selector_best"] = (
                    emitted_cycles - best_cycles
                    if emitted_cycles is not None and best_cycles is not None
                    else None
                )
                row["emission_gap_vs_selector_context"] = (
                    emitted_cycles - context_cycles
                    if emitted_cycles is not None and context_cycles is not None
                    else None
                )
                row["emission_gap_vs_selector_context_raw"] = (
                    emitted_cycles - context_cycles_raw
                    if emitted_cycles is not None and context_cycles_raw is not None
                    else None
                )
                row["label_block_static_partition_error"] = (
                    block_static_cycles
                    - row["steady_state_static_cycles"]
                    - row["fallthrough_tail_static_cycles"]
                    if block_static_cycles is not None
                    and row["steady_state_static_cycles"] is not None
                    and row["fallthrough_tail_static_cycles"] is not None
                    else None
                )
                row["cost_steady_state_static_error"] = (
                    row["cost_steady_state_static_cycles"]
                    - row["steady_state_static_cycles"]
                    if row["cost_steady_state_static_cycles"] is not None
                    and row["steady_state_static_cycles"] is not None
                    else None
                )
                row["cost_fallthrough_tail_static_error"] = (
                    row["cost_fallthrough_tail_static_cycles"]
                    - row["fallthrough_tail_static_cycles"]
                    if row["cost_fallthrough_tail_static_cycles"] is not None
                    and row["fallthrough_tail_static_cycles"] is not None
                    else None
                )
                rows.append(row)

    report = {
        "baseline": str(baseline),
        "openvcl": str(args.openvcl.resolve()),
        "warning": (
            "Static diagnostics only: emitted rows and cost-model cycles are not "
            "hardware runtime measurements or semantic validation."
        ),
        "accepted_no_q_rewrites": rows,
        "summary": {
            "count": len(rows),
            "prefix_in_branch_delay_count": sum(
                1 for r in rows if r["selector_prefix_in_branch_delay"] is True
            ),
            "branch_delay_discount_count": sum(
                1 for r in rows if r["selector_branch_delay_discount"] not in (None, 0)
            ),
            "raw_context_matches_emitted_count": sum(
                1 for r in rows if r["emission_gap_vs_selector_context_raw"] == 0
            ),
            "discounted_context_matches_emitted_count": sum(
                1 for r in rows if r["emission_gap_vs_selector_context"] == 0
            ),
            "nonzero_context_gap_count": sum(
                1 for r in rows if r["emission_gap_vs_selector_context"] not in (None, 0)
            ),
            "nonzero_context_gap_with_prefix_in_branch_delay_count": sum(
                1 for r in rows
                if r["emission_gap_vs_selector_context"] not in (None, 0)
                and r["selector_prefix_in_branch_delay"] is True
            ),
            "max_emission_gap_vs_selector_best": max(
                (r["emission_gap_vs_selector_best"] for r in rows
                 if r["emission_gap_vs_selector_best"] is not None),
                default=0,
            ),
            "max_emission_gap_vs_selector_context": max(
                (abs(r["emission_gap_vs_selector_context"]) for r in rows
                 if r["emission_gap_vs_selector_context"] is not None),
                default=0,
            ),
            "max_emission_gap_vs_selector_context_raw": max(
                (abs(r["emission_gap_vs_selector_context_raw"]) for r in rows
                 if r["emission_gap_vs_selector_context_raw"] is not None),
                default=0,
            ),
            "max_fallthrough_tail_static_cycles": max(
                (r["fallthrough_tail_static_cycles"] for r in rows
                 if r["fallthrough_tail_static_cycles"] is not None),
                default=0,
            ),
            "max_label_block_static_partition_error": max(
                (abs(r["label_block_static_partition_error"]) for r in rows
                 if r["label_block_static_partition_error"] is not None),
                default=0,
            ),
            "split_fallthrough_tail_count": sum(
                1 for r in rows if r["cost_split_fallthrough_tail"]
            ),
            "max_cost_steady_state_static_error": max(
                (abs(r["cost_steady_state_static_error"]) for r in rows
                 if r["cost_steady_state_static_error"] is not None),
                default=0,
            ),
            "max_cost_fallthrough_tail_static_error": max(
                (abs(r["cost_fallthrough_tail_static_error"]) for r in rows
                 if r["cost_fallthrough_tail_static_error"] is not None),
                default=0,
            ),
        },
    }

    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired, json.JSONDecodeError) as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
