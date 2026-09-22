#!/usr/bin/env python3
"""Characterize accepted generic no-Q cyclic-prefix rewrites in the ps2gl corpus.

The report deliberately separates three quantities:
  * selector baseline/best cycles reported by OpenVCL while choosing a rewrite;
  * rows in the emitted VSM steady-state loop through its branch delay slot;
  * VsmCostAnalyzer label-block cycles, which may include loop-exit fallthrough.

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
    r"\[cyclic-prefix-trace\] loop=(\S+) result=(ACCEPTED|no_improvement) bestCycles=(\d+)"
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
        }
    return accepted


def emitted_loop_cycles(vsm_text, label):
    lines = vsm_text.splitlines()
    start = next((i for i, line in enumerate(lines) if line.strip() == label + ":"), None)
    if start is None:
        return None

    branch_re = re.compile(
        r"\b(?:ibne|ibeq|ibgtz|ibgez|ibltz|iblez|b)\b.*\b"
        + re.escape(label)
        + r"\b"
    )
    for index in range(start + 1, len(lines)):
        stripped = lines[index].strip()
        if index > start + 1 and stripped.endswith(":") and not lines[index][:1].isspace():
            return None
        if branch_re.search(lines[index]):
            # The label itself consumes no cycle. Include the branch and its
            # architectural one-cycle delay slot.
            return (index - start) + 1
    return None


def label_block_cycles(cost, label):
    for block in cost.get("blocks", []):
        if block.get("label") == label:
            return block.get("estimated_cycles", block.get("cycles"))
    return None


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
                row = {
                    "renderer": renderer,
                    "loop": label,
                    **accepted[label],
                    "emitted_loop_cycles_through_delay": emitted_loop_cycles(vsm_text, label),
                    "cost_label_block_cycles": label_block_cycles(cost, label),
                }
                baseline_cycles = row["selector_baseline_cycles"]
                best_cycles = row["selector_best_cycles"]
                emitted_cycles = row["emitted_loop_cycles_through_delay"]
                block_cycles = row["cost_label_block_cycles"]
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
                row["label_block_exit_tail_cycles"] = (
                    block_cycles - emitted_cycles
                    if block_cycles is not None and emitted_cycles is not None
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
            "max_emission_gap_vs_selector_best": max(
                (r["emission_gap_vs_selector_best"] for r in rows
                 if r["emission_gap_vs_selector_best"] is not None),
                default=0,
            ),
            "max_label_block_exit_tail_cycles": max(
                (r["label_block_exit_tail_cycles"] for r in rows
                 if r["label_block_exit_tail_cycles"] is not None),
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
