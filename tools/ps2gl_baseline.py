#!/usr/bin/env python3
"""Build fresh, pure OpenVCL ps2gl VU1 artifacts and compare four generic modes.

Requires locally built OpenVCL/MASP and PS2DEV. Never installs tools or changes
shaders. Results are static model estimates, not correctness/runtime evidence.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
from datetime import datetime, timezone

RENDERERS = (
    'fast_nolights', 'fast', 'general', 'general_quad', 'general_tri',
    'general_nospec', 'general_nospec_quad', 'general_nospec_tri',
    'general_pv_diff', 'general_pv_diff_quad', 'general_pv_diff_tri',
    'indexed', 'scei',
)
VARIANTS = {
    'default': ([], {}),
    'no-pipeline': (['--disable-generic-software-pipelining'], {}),
    'kernel': ([], {'OPENVCL_USE_GENERIC_KERNEL_REWRITE': '1'}),
    'expanded': ([], {'OPENVCL_USE_GENERIC_KERNEL_REWRITE': '1',
                      'OPENVCL_USE_EXPANDED_DDG_PLACER': '1'}),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dump(path, obj):
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + '\n')


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ps2gl', type=Path, default=root.parent / 'ps2gl')
    parser.add_argument('--masp', type=Path, default=root.parent / 'masp/build-plan/src/masp')
    parser.add_argument('--masp-source', type=Path, default=root.parent / 'masp')
    parser.add_argument('--openvcl', type=Path, default=root / 'openvcl')
    parser.add_argument('--ps2dev', type=Path, default=os.environ.get('PS2DEV'))
    parser.add_argument('--output', type=Path, required=True, help='New directory; existing paths are rejected')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--timeout', type=int, default=300,
                        help='per-command timeout in seconds (default: 300)')
    args = parser.parse_args()
    if args.ps2dev is None:
        parser.error('Set PS2DEV or pass --ps2dev')
    args.output = args.output.resolve()
    args.ps2gl = args.ps2gl.resolve()
    args.ps2dev = args.ps2dev.resolve()
    args.openvcl = args.openvcl.resolve()
    args.masp = args.masp.resolve()
    args.masp_source = args.masp_source.resolve()
    assembler = args.ps2dev / 'dvp/bin/dvp-as'
    size_tool = args.ps2dev / 'dvp/bin/dvp-size'
    for tool in (args.openvcl, args.masp, assembler, size_tool):
        if not tool.is_file() or not os.access(tool, os.X_OK):
            parser.error(f'Executable not found: {tool}')
    if args.output.exists():
        parser.error(f'Refusing to reuse artifacts: {args.output}')
    args.output.mkdir(parents=True)
    out = args.output
    env = {k: v for k, v in os.environ.items() if not k.startswith('OPENVCL_')}
    env.update(PS2DEV=str(args.ps2dev), PS2SDK=str(args.ps2dev / 'ps2sdk'),
               GSKIT=str(args.ps2dev / 'gsKit'))
    tool_dirs = [args.ps2dev / x for x in ('bin', 'ee/bin', 'iop/bin', 'dvp/bin', 'ps2sdk/bin')]
    env['PATH'] = os.pathsep.join(map(str, tool_dirs)) + os.pathsep + env.get('PATH', '')
    metadata = {
        'started_utc': datetime.now(timezone.utc).isoformat(), 'complete': False,
        'arguments': {k: str(v) for k, v in vars(args).items()},
        'removed_environment': sorted(k for k in os.environ if k.startswith('OPENVCL_')),
        'environment': {k: env[k] for k in ('PS2DEV', 'PS2SDK', 'GSKIT', 'PATH')},
        'variants': VARIANTS, 'commands': [], 'failures': [], 'tools': {},
    }
    metadata_lock = threading.Lock()

    def run(argv, name, extra=None, check=True):
        argv = list(map(str, argv))
        proc = subprocess.run(argv, env=dict(env, **(extra or {})), cwd=root,
                              capture_output=True, text=True, timeout=args.timeout)
        (out / (name + '.stdout')).write_text(proc.stdout)
        (out / (name + '.stderr')).write_text(proc.stderr)
        with metadata_lock:
            metadata['commands'].append({'argv': argv, 'environment_override': extra or {},
                                         'returncode': proc.returncode, 'log': name})
            dump(out / 'metadata.json', metadata)
        if check and proc.returncode:
            raise RuntimeError(f'{name} failed ({proc.returncode}); see {out / (name + ".stderr")}')
        return proc

    def record_failure(message):
        with metadata_lock:
            metadata['failures'].append(message)
            dump(out / 'metadata.json', metadata)

    tool_paths = [('openvcl', args.openvcl), ('masp', args.masp),
                  ('dvp-as', assembler), ('dvp-size', size_tool)]
    for name, tool in tool_paths:
        metadata['tools'][name] = {'path': str(tool), 'sha256': sha(tool),
                                  'version': run([tool, '--version'], name + '-version', check=False).stdout}
    metadata['repositories'] = {}
    for name, path in [('openvcl', root), ('masp', args.masp_source), ('ps2gl', args.ps2gl)]:
        metadata['repositories'][name] = {
            'path': str(path),
            'commit': run(['git', '-C', path, 'rev-parse', 'HEAD'], name + '-commit').stdout.strip(),
            'status': run(['git', '-C', path, 'status', '--porcelain'], name + '-status').stdout,
        }
        run(['git', '-C', path, 'diff', 'HEAD'], name + '-diff')
        tracked = run(['git', '-C', path, 'ls-files', '-z'], name + '-files').stdout.split('\0')
        metadata['repositories'][name]['tracked_sha256'] = {
            name: sha(path / name) for name in tracked if name and (path / name).is_file()
        }
    shutil.copy2(Path(__file__), out / 'ps2gl_baseline.py')
    # Record all VU source/include inputs, not only the top-level VCL files.
    inputs = sorted(p for p in (args.ps2gl / 'vu1').rglob('*')
                    if p.is_file() and p.suffix in ('.vcl', '.vsm', '.i', '.h'))
    inputs += [args.ps2gl / 'CMakeLists.txt', args.ps2gl / 'cmake/preprocess_vu1.cmake']
    metadata['input_sha256'] = {str(p): sha(p) for p in inputs}
    (out / 'references').mkdir()
    for renderer in RENDERERS:
        name = 'scei_vcl.vsm' if renderer == 'scei' else f'sce_{renderer}_vcl.vsm'
        shutil.copy2(args.ps2gl / 'vu1' / name, out / 'references' / name)

    for mode in ('openvcl', 'sce'):
        build = out / ('build-' + mode)
        proc = run(['cmake', '-S', args.ps2gl, '-B', build,
                    f'-DOPENVCL_FOUND={args.openvcl}', f'-DMASP_FOUND={args.masp}',
                    f'-DDVP_AS_FOUND={assembler}', '-DPS2GL_USE_SCE_VSM_FOR:STRING=',
                    '-DPS2GL_USE_SCE_VSM=' + ('ON' if mode == 'sce' else 'OFF'),
                    '-DBUILD_EXAMPLES=OFF'], mode + '-configure')
        if mode == 'openvcl' and 'using open-source tools (openvcl + masp)' not in proc.stdout:
            raise RuntimeError('Pure open-source pipeline was not selected')
        run(['cmake', '--build', build, '--target', 'vu1_objects', '-j', args.jobs], mode + '-build')
        for renderer in RENDERERS:
            if not (build / 'vu1' / (renderer + '.vo')).is_file():
                raise RuntimeError(f'Missing object: {mode}/{renderer}')
            if mode == 'openvcl':
                for suffix in ('_pp1.vcl', '_pp2.vcl', '_pp3.vcl', '_pp4.vcl', '_vcl.vsm'):
                    if not (build / 'vu1' / (renderer + suffix)).is_file():
                        raise RuntimeError(f'Preprocessing bypass detected: {renderer}{suffix}')

    costs = {}
    sizes = {}
    for variant, (flags, extra) in VARIANTS.items():
        print(f'Comparing {variant}: {len(RENDERERS)} renderers', flush=True)
        dest = out / variant
        dest.mkdir()
        costs[variant] = {}
        sizes[variant] = {}

        def process_renderer(renderer):
            source = out / 'build-openvcl/vu1' / (renderer + '_pp4.vcl')
            candidate = dest / (renderer + '_vcl.vsm')
            log = f'{variant}/{renderer}'
            run([args.openvcl, '--disable-known-loop-optimizations', *flags, source, '-o', candidate],
                log + '-compile', extra)
            if variant == 'default':
                original = out / 'build-openvcl/vu1' / candidate.name
                if sha(candidate) != sha(original):
                    raise RuntimeError(f'Default output differs from pure CMake output: {renderer}')
            assembly = run([assembler, '-o', dest / (renderer + '.vo'), candidate], log + '-assemble', check=False)
            size_value = None
            if assembly.returncode:
                record_failure(f'Assembly failed: {variant}/{renderer}')
            else:
                size_report = run([size_tool, '-A', dest / (renderer + '.vo')], log + '-size')
                section = re.search(r'^\.vutext\s+(\d+)\s+', size_report.stdout, re.M)
                if not section:
                    raise RuntimeError(f'Missing .vutext section: {variant}/{renderer}')
                size_value = int(section.group(1))
            reference = out / 'references' / ('scei_vcl.vsm' if renderer == 'scei' else f'sce_{renderer}_vcl.vsm')
            report = run([args.openvcl, '--cost-json', '--cost-loop-preset', 'ps2gl', candidate], log + '-cost')
            cost = json.loads(report.stdout)
            if cost['unknown_instructions'] or cost['slot_mismatches'] or cost['ignored_lines']:
                record_failure(f'Invalid cost model input: {variant}/{renderer}')
            reference_cost = None
            if variant == 'default':
                baseline = run([args.openvcl, '--cost-json', '--cost-loop-preset', 'ps2gl', reference],
                               'references/' + renderer + '-cost')
                reference_cost = json.loads(baseline.stdout)
                if any(reference_cost[k] for k in ('unknown_instructions', 'slot_mismatches', 'ignored_lines')):
                    record_failure(f'Invalid reference cost input: {renderer}')
            run([args.openvcl, '--dump-loop-pipeline-info-json', '--disable-known-loop-optimizations',
                 *flags, source], log + '-loops', extra)
            return renderer, cost, size_value, reference_cost, f'{reference} {candidate}\n'

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
            results = list(executor.map(process_renderer, RENDERERS))

        pairs = []
        for renderer, cost, size_value, reference_cost, pair in results:
            costs[variant][renderer] = cost
            if size_value is not None:
                sizes[variant][renderer] = size_value
            if reference_cost is not None:
                costs.setdefault('sce', {})[renderer] = reference_cost
            pairs.append(pair)
        (dest / 'pairs.txt').write_text(''.join(pairs))
        # Whitespace manifests are an existing CLI limitation. JSON remains usable with spaces.
        if not re.search(r'\s', str(out)):
            table = run([args.openvcl, '--cost-compare-list-markdown', '--cost-loop-preset', 'ps2gl',
                         dest / 'pairs.txt'], variant + '/comparison')
            (dest / 'comparison.md').write_text(table.stdout)

    for name, tool in tool_paths:
        if sha(tool) != metadata['tools'][name]['sha256']:
            raise RuntimeError(f'Tool changed during run: {tool}')
    if any(sha(p) != metadata['input_sha256'][str(p)] for p in inputs):
        raise RuntimeError('Source inputs changed during run')
    dump(out / 'costs.json', costs)
    dump(out / 'vutext-bytes.json', sizes)
    lines = ['# ps2gl static cost baseline', '',
             'Estimates only; no functional equivalence or runtime claim. Weights use the ps2gl label preset.', '',
             '| Renderer | SCE weighted estimated | Default | No pipeline | Kernel | Expanded |',
             '|---|---:|---:|---:|---:|---:|']
    for renderer in RENDERERS:
        values = [costs[v][renderer]['weighted_estimated_total_cycles']
                  for v in ('sce', *VARIANTS)]
        lines.append('| ' + renderer + ' | ' + ' | '.join(map(str, values)) + ' |')
    (out / 'summary.md').write_text('\n'.join(lines) + '\n')
    artifacts = sorted(p for p in out.rglob('*') if p.is_file() and p.suffix in ('.vcl', '.vsm', '.vo'))
    metadata['artifact_sha256'] = {str(p.relative_to(out)): sha(p) for p in artifacts}
    metadata['commands'].sort(key=lambda command: command['log'])
    metadata['complete'] = True
    metadata['finished_utc'] = datetime.now(timezone.utc).isoformat()
    dump(out / 'metadata.json', metadata)
    print(out / 'summary.md')
    return 1 if metadata['failures'] else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
