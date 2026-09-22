#!/usr/bin/env python3
"""Check that changing every source label leaves generic emitted instructions unchanged.

Consumes a completed ps2gl_baseline.py run, assembles renamed variants and
compares VSM after undoing label renaming. This is a generality regression
check, not a semantic oracle for the original programs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from ps2gl_baseline import RENDERERS, VARIANTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variants', nargs='+', choices=VARIANTS, default=list(VARIANTS))
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    metadata = json.loads((baseline / 'metadata.json').read_text())
    if not metadata['complete'] or metadata['failures']:
        parser.error('Baseline is incomplete or invalid')
    tools = metadata['tools']
    def verified_artifact(relative):
        path = baseline / relative
        expected = metadata['artifact_sha256'][relative]
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f'Baseline artifact changed: {relative}')
        return path
    for name in ('openvcl', 'dvp-as'):
        binary = Path(tools[name]['path'])
        if hashlib.sha256(binary.read_bytes()).hexdigest() != tools[name]['sha256']:
            parser.error(f'{name} differs from the recorded baseline; create a fresh baseline first')
    output = args.output.resolve()
    if output.exists():
        parser.error('Output must be a new directory')
    output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('OPENVCL_')}
    results = []
    for variant in args.variants:
        flags, extra = VARIANTS[variant]
        dest = output / variant
        dest.mkdir()
        for renderer in RENDERERS:
            text = verified_artifact(f'build-openvcl/vu1/{renderer}_pp4.vcl').read_text()
            labels = re.findall(r'^\s*([A-Za-z_.$][\w.$]*):', text, re.M)
            if not labels:
                raise RuntimeError(f'No labels found in {renderer}')
            mapping = {label: f'invariance_label_{i:04d}' for i, label in enumerate(dict.fromkeys(labels))}
            if any(name in text for name in mapping.values()):
                raise RuntimeError('Renaming collision')
            pattern = r'(?<![\w.$])(' + '|'.join(map(re.escape, sorted(mapping, key=len, reverse=True))) + r')(?![\w.$])'
            text = re.sub(pattern, lambda m: mapping[m.group()], text)
            source = dest / (renderer + '.vcl')
            candidate = dest / (renderer + '.vsm')
            source.write_text(text)
            proc = subprocess.run([tools['openvcl']['path'], '--disable-known-loop-optimizations',
                                   *flags, str(source), '-o', str(candidate)],
                                  env=dict(env, **extra), capture_output=True, text=True, timeout=300)
            (dest / (renderer + '.log')).write_text(proc.stdout + proc.stderr)
            if proc.returncode:
                raise RuntimeError(f'Compilation failed: {variant}/{renderer}')
            restored = candidate.read_text()
            for original, renamed in mapping.items():
                restored = restored.replace(renamed, original)
            reference = verified_artifact(f'{variant}/{renderer}_vcl.vsm').read_text()
            identical = restored == reference
            assembly = subprocess.run([tools['dvp-as']['path'], '-o', str(dest / (renderer + '.vo')), str(candidate)],
                                      capture_output=True, text=True, timeout=30)
            (dest / (renderer + '-assembly.log')).write_text(assembly.stdout + assembly.stderr)
            results.append({'variant': variant, 'renderer': renderer, 'labels': len(mapping),
                            'same_instructions': identical, 'assembled': assembly.returncode == 0})
            (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(variant, renderer, 'PASS' if identical and assembly.returncode == 0 else 'FAIL', flush=True)
    return 0 if all(r['same_instructions'] and r['assembled'] for r in results) else 1


if __name__ == '__main__':
    sys.exit(main())
