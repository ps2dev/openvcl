#!/usr/bin/env python3
"""Compare the two-stage MVE fixture against its conservative baseline by trip count."""

import argparse
import json
import subprocess
import sys


def run_cost(openvcl, path):
    proc = subprocess.run(
        [openvcl, "--cost-json", path],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError("%s failed: %s" % (path, proc.stderr.strip()))
    data = json.loads(proc.stdout)
    for key in ("unknown_instructions", "slot_mismatches", "ignored_lines"):
        if data.get(key):
            raise RuntimeError("%s has %s=%s" % (path, key, data[key]))
    return data


def blocks_by_label(report):
    return {block["label"]: block for block in report["blocks"]}


def require(blocks, label):
    try:
        return blocks[label]
    except KeyError:
        raise RuntimeError("missing cost block %s" % label)


def block_cost(blocks, label, metric):
    return int(require(blocks, label)[metric])


def candidate_cost(blocks, loop, iterations, metric):
    if iterations < 1:
        raise ValueError("iterations must be >= 1")

    total = block_cost(blocks, loop, metric)
    total += block_cost(blocks, loop + "__MVE_PROLOG", metric)
    total += block_cost(blocks, loop + "__MVE_PROLOG_END", metric)

    for iteration in range(1, iterations):
        suffix = "__MVE_MAIN_B" if iteration & 1 else "__MVE_MAIN_A"
        total += block_cost(blocks, loop + suffix, metric)

    if iterations & 1:
        total += block_cost(blocks, loop + "__MVE_DRAIN_A", metric)
    else:
        total += block_cost(blocks, loop + "__MVE_DRAIN_B", metric)
        total += block_cost(blocks, loop + "__MVE_DRAIN_B_END", metric)

    total += block_cost(blocks, loop + "__MVE_EXIT", metric)
    return total


def baseline_cost(blocks, loop, iterations, metric):
    return (
        iterations * block_cost(blocks, loop, metric)
        + block_cost(blocks, loop + "__FALLTHROUGH", metric)
    )


def first_better(candidate_blocks, baseline_blocks, loop, max_iterations, metric):
    for iterations in range(1, max_iterations + 1):
        if candidate_cost(candidate_blocks, loop, iterations, metric) < baseline_cost(
            baseline_blocks, loop, iterations, metric
        ):
            return iterations
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--openvcl", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--loop-label", default="xform_loop_lid")
    parser.add_argument("--max-iterations", type=int, default=12)
    args = parser.parse_args()

    if args.max_iterations < 1:
        parser.error("--max-iterations must be >= 1")

    candidate = blocks_by_label(run_cost(args.openvcl, args.candidate))
    baseline = blocks_by_label(run_cost(args.openvcl, args.baseline))

    metrics = ("cycles", "estimated_cycles")
    print("| Iterations | Candidate static | Baseline static | Delta | Candidate estimated | Baseline estimated | Delta |")
    print("|---:|---:|---:|---:|---:|---:|---:|")
    for iterations in range(1, args.max_iterations + 1):
        c_static = candidate_cost(candidate, args.loop_label, iterations, "cycles")
        b_static = baseline_cost(baseline, args.loop_label, iterations, "cycles")
        c_est = candidate_cost(candidate, args.loop_label, iterations, "estimated_cycles")
        b_est = baseline_cost(baseline, args.loop_label, iterations, "estimated_cycles")
        print(
            "| %d | %d | %d | %+d | %d | %d | %+d |"
            % (
                iterations,
                c_static,
                b_static,
                c_static - b_static,
                c_est,
                b_est,
                c_est - b_est,
            )
        )

    for metric in metrics:
        crossover = first_better(
            candidate, baseline, args.loop_label, args.max_iterations, metric
        )
        label = "static" if metric == "cycles" else "estimated"
        print(
            "MVE_STATIC_CROSSOVER metric=%s first_better_iterations=%s"
            % (label, crossover if crossover is not None else "none")
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
