#!/usr/bin/env python3
"""Solve an OpenVCL modulo-placement diagnostic as a finite binary CSP.

The input is stderr produced with OPENVCL_DUMP_MODULO_FEASIBILITY_PROBLEM=1.
This tool is diagnostic only: it mirrors the placer's finite [lo, hi] domains,
DDG inequalities, and ModuloReservationTable resource conflicts without
changing compiler code generation.
"""

import argparse
import collections
import sys


def parse_fields(line):
    fields = {}
    for token in line.split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def load_problem(path, loop, ii):
    header = None
    nodes = {}
    edges = []
    with open(path, "r", encoding="utf-8") as source:
        for line in source:
            if not line.startswith("[modulo-feasibility-"):
                continue
            fields = parse_fields(line)
            if fields.get("loop") != loop or int(fields.get("II", -1)) != ii:
                continue
            if line.startswith("[modulo-feasibility-problem]"):
                header = {key: int(value) for key, value in fields.items()
                          if key not in ("loop",)}
            elif line.startswith("[modulo-feasibility-node]"):
                node = int(fields["node"])
                nodes[node] = {key: int(value) for key, value in fields.items()
                               if key not in ("loop",)}
            elif line.startswith("[modulo-feasibility-edge]"):
                edges.append({key: int(value) for key, value in fields.items()
                              if key not in ("loop",)})
    if header is None:
        raise ValueError("problem not found for loop=%s II=%d" % (loop, ii))
    if len(nodes) != header["nodes"]:
        raise ValueError("expected %d nodes, found %d" % (header["nodes"], len(nodes)))
    if len(edges) != header["edges"]:
        raise ValueError("expected %d edges, found %d" % (header["edges"], len(edges)))
    return header, nodes, edges


class ModuloCSP(object):
    def __init__(self, ii, nodes, edges, max_states):
        self.ii = ii
        self.nodes = nodes
        self.edges = edges
        self.max_states = max_states
        self.states = 0
        self.revisions = 0
        self.exhausted = False
        self.pair_edges = collections.defaultdict(list)
        self.neighbours = {node: set() for node in nodes}

        for edge in edges:
            before = edge["from"]
            after = edge["to"]
            bound = edge["lat"] - edge["dist"] * ii
            key = (min(before, after), max(before, after))
            self.pair_edges[key].append((before, after, bound))
            if before != after:
                self.neighbours[before].add(after)
                self.neighbours[after].add(before)

        by_pipe = collections.defaultdict(list)
        for node, info in nodes.items():
            if info["pipe"] != 0:
                by_pipe[info["pipe"]].append(node)
        for same_pipe in by_pipe.values():
            for index, first in enumerate(same_pipe):
                for second in same_pipe[index + 1:]:
                    self.neighbours[first].add(second)
                    self.neighbours[second].add(first)

    def resource_compatible(self, first, first_slot, second, second_slot):
        if first == second:
            return True
        first_info = self.nodes[first]
        second_info = self.nodes[second]
        if first_info["pipe"] == 0 or first_info["pipe"] != second_info["pipe"]:
            return True
        pipe = first_info["pipe"]
        if pipe in (1, 2):
            return first_slot % self.ii != second_slot % self.ii
        if pipe in (3, 4):
            first_cycles = set((first_slot + offset) % self.ii
                               for offset in range(first_info["duration"]))
            second_cycles = set((second_slot + offset) % self.ii
                                for offset in range(second_info["duration"]))
            return first_cycles.isdisjoint(second_cycles)
        return True

    def compatible(self, first, first_slot, second, second_slot):
        if not self.resource_compatible(first, first_slot, second, second_slot):
            return False
        key = (min(first, second), max(first, second))
        for before, after, bound in self.pair_edges.get(key, ()):
            before_slot = first_slot if before == first else second_slot
            after_slot = first_slot if after == first else second_slot
            if after_slot - before_slot < bound:
                return False
        return True

    def revise(self, domains, first, second):
        second_values = domains[second]
        kept = []
        changed = False
        for first_slot in domains[first]:
            if any(self.compatible(first, first_slot, second, second_slot)
                   for second_slot in second_values):
                kept.append(first_slot)
            else:
                changed = True
                self.revisions += 1
        if changed:
            domains[first] = kept
        return changed

    def propagate(self, domains, initial_arcs=None):
        if initial_arcs is None:
            queue = collections.deque(
                (first, second)
                for first in self.nodes
                for second in self.neighbours[first])
        else:
            queue = collections.deque(initial_arcs)
        while queue:
            first, second = queue.popleft()
            if self.revise(domains, first, second):
                if not domains[first]:
                    return False
                for other in self.neighbours[first]:
                    if other != second:
                        queue.append((other, first))
        return True

    def choose_node(self, domains):
        candidates = [node for node, values in domains.items() if len(values) > 1]
        if not candidates:
            return None
        return min(candidates,
                   key=lambda node: (len(domains[node]), -len(self.neighbours[node]), node))

    def ordered_values(self, domains, node):
        def pressure(slot):
            supported = 0
            for other in self.neighbours[node]:
                supported += sum(1 for other_slot in domains[other]
                                 if self.compatible(node, slot, other, other_slot))
            return -supported, slot
        return sorted(domains[node], key=pressure)

    def search(self, domains):
        if self.states >= self.max_states:
            self.exhausted = True
            return None
        self.states += 1
        node = self.choose_node(domains)
        if node is None:
            return {item: values[0] for item, values in domains.items()}
        for slot in self.ordered_values(domains, node):
            child = {item: list(values) for item, values in domains.items()}
            child[node] = [slot]
            arcs = [(other, node) for other in self.neighbours[node]]
            if self.propagate(child, arcs):
                result = self.search(child)
                if result is not None:
                    return result
            if self.exhausted:
                return None
        return None

    def solve(self):
        domains = {
            node: [slot for slot in range(info["lo"], info["hi"] + 1)
                   if self.compatible(node, slot, node, slot)]
            for node, info in self.nodes.items()
        }
        if not self.propagate(domains):
            return "UNSAT", None
        result = self.search(domains)
        if result is not None:
            return "SAT", result
        if self.exhausted:
            return "UNKNOWN", None
        return "UNSAT", None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump", help="OpenVCL feasibility diagnostic output")
    parser.add_argument("--loop", required=True)
    parser.add_argument("--ii", required=True, type=int)
    parser.add_argument("--max-states", type=int, default=200000)
    args = parser.parse_args()

    _, nodes, edges = load_problem(args.dump, args.loop, args.ii)
    solver = ModuloCSP(args.ii, nodes, edges, args.max_states)
    status, solution = solver.solve()
    print("MODULO_FEASIBILITY_%s loop=%s II=%d states=%d revisions=%d" %
          (status, args.loop, args.ii, solver.states, solver.revisions))
    if solution is not None:
        for node in sorted(solution):
            print("node=%d slot=%d mod=%d" %
                  (node, solution[node], solution[node] % args.ii))
    return 0 if status in ("SAT", "UNSAT") else 2


if __name__ == "__main__":
    sys.exit(main())
