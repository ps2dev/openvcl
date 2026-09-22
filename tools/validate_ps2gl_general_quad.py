#!/usr/bin/env python3
"""Build and run a deterministic ps2gl general_quad semantic oracle in PCSX2."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

FRAME_RE = re.compile(
    r"PS2GL_GENERAL_QUAD_FRAME frame=(\d+) renderer=(.*?) semantic=([0-9a-fA-F]{8})"
)
FRAME_COUNT = 12
EXPECTED_RENDERER = "linear, quads"


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ps2gl", type=Path, default=root.parent / "ps2gl")
    parser.add_argument("--masp", type=Path, default=root.parent / "masp/build-plan/src/masp")
    parser.add_argument("--openvcl", type=Path, default=root / "openvcl")
    parser.add_argument("--ps2dev", type=Path, default=os.environ.get("PS2DEV"))
    parser.add_argument("--output", type=Path, required=True,
                        help="New directory; existing paths are rejected")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--timeout", type=float, default=25.0)
    parser.add_argument("--pcsx2", type=Path)
    parser.add_argument("--pcsx2-data", type=Path)
    args = parser.parse_args()

    if args.ps2dev is None:
        parser.error("Set PS2DEV or pass --ps2dev")

    args.ps2gl = args.ps2gl.resolve()
    args.masp = args.masp.resolve()
    args.openvcl = args.openvcl.resolve()
    args.ps2dev = args.ps2dev.resolve()
    args.output = args.output.resolve()

    if args.output.exists():
        parser.error(f"Refusing to reuse output directory: {args.output}")
    args.output.mkdir(parents=True)

    ps2sdk = args.ps2dev / "ps2sdk"
    gskit = args.ps2dev / "gsKit"
    toolchain = args.ps2dev / "share/ps2dev.cmake"
    cxx = args.ps2dev / "ee/bin/mips64r5900el-ps2-elf-g++"
    assembler = args.ps2dev / "dvp/bin/dvp-as"
    runner = root / "test/ps2/vu1_differential/run_pcsx2.py"
    oracle_source = root / "test/ps2/ps2gl_general_quad/box_oracle.cpp"

    required_files = [
        args.openvcl, args.masp, toolchain, cxx, assembler, runner, oracle_source,
        args.ps2gl / "CMakeLists.txt",
    ]
    for path in required_files:
        if not path.is_file():
            parser.error(f"Required file not found: {path}")

    env = {k: v for k, v in os.environ.items() if not k.startswith("OPENVCL_")}
    env.update(
        PS2DEV=str(args.ps2dev),
        PS2SDK=str(ps2sdk),
        GSKIT=str(gskit),
    )
    tool_dirs = [
        args.ps2dev / "bin",
        args.ps2dev / "ee/bin",
        args.ps2dev / "iop/bin",
        args.ps2dev / "dvp/bin",
        ps2sdk / "bin",
    ]
    env["PATH"] = os.pathsep.join(str(p) for p in tool_dirs) + os.pathsep + env.get("PATH", "")

    metadata = {
        "complete": False,
        "variants": {},
        "commands": [],
        "semantic_hashes": {},
        "renderer": EXPECTED_RENDERER,
        "frame_count": FRAME_COUNT,
    }

    def run(argv, log_name, cwd=root):
        argv = [str(x) for x in argv]
        proc = subprocess.run(
            argv,
            cwd=cwd,
            env=env,
            capture_output=True,
            text=True,
            timeout=300,
        )
        (args.output / f"{log_name}.stdout").write_text(proc.stdout)
        (args.output / f"{log_name}.stderr").write_text(proc.stderr)
        metadata["commands"].append(
            {"argv": argv, "cwd": str(cwd), "returncode": proc.returncode}
        )
        (args.output / "metadata.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n"
        )
        if proc.returncode:
            raise RuntimeError(
                f"{log_name} failed ({proc.returncode}); "
                f"see {args.output / (log_name + '.stderr')}"
            )
        return proc

    wrapper_root = args.output / "wrappers"
    wrapper_root.mkdir()
    wrappers = {}

    def make_wrapper(name, body):
        path = wrapper_root / name / "openvcl"
        path.parent.mkdir()
        path.write_text("#!/bin/sh\n" + body + "\n")
        path.chmod(0o755)
        wrappers[name] = path
        return path

    quoted_openvcl = shlex.quote(str(args.openvcl))
    wrappers["default"] = args.openvcl
    make_wrapper(
        "no-noq",
        f'exec env OPENVCL_DISABLE_NO_Q_CYCLIC_PREFIX=1 {quoted_openvcl} "$@"',
    )
    make_wrapper(
        "no-pipeline",
        f'exec {quoted_openvcl} --disable-generic-software-pipelining "$@"',
    )
    wrappers["sce"] = args.openvcl

    variants = ("default", "no-noq", "no-pipeline", "sce")
    builds = {}

    for variant in variants:
        build = args.output / f"build-{variant}"
        builds[variant] = build
        cmake_args = [
            "cmake", "-S", args.ps2gl, "-B", build,
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DOPENVCL_FOUND={wrappers[variant]}",
            f"-DMASP_FOUND={args.masp}",
            f"-DDVP_AS_FOUND={assembler}",
            "-DPS2GL_USE_SCE_VSM_FOR:STRING=",
            "-DPS2GL_USE_SCE_VSM=" + ("ON" if variant == "sce" else "OFF"),
            "-DBUILD_EXAMPLES=OFF",
        ]
        configured = run(cmake_args, f"{variant}-configure")
        if variant != "sce" and "using open-source tools (openvcl + masp)" not in configured.stdout:
            raise RuntimeError(f"{variant}: pure OpenVCL/MASP pipeline was not selected")
        run(
            ["cmake", "--build", build, "--target", "ps2gl", "ps2glut",
             "-j", str(args.jobs)],
            f"{variant}-build",
        )
        metadata["variants"][variant] = {
            "build": str(build),
            "openvcl": str(wrappers[variant]),
            "sce_vsm": variant == "sce",
        }

    common_compile = [
        cxx,
        f"-I{ps2sdk / 'ee/include'}",
        f"-I{ps2sdk / 'common/include'}",
        f"-I{gskit / 'include'}",
        f"-I{ps2sdk / 'ports/include'}",
        f"-I{args.ps2gl / 'include'}",
        "-D_EE", "-DPS2", "-D__PS2__", "-O2", "-G0",
    ]
    oracle_object = args.output / "box_oracle.o"
    run(
        [*common_compile, "-c", oracle_source, "-o", oracle_object],
        "oracle-compile",
    )

    link_common = [
        f"-L{ps2sdk / 'ee/lib'}",
        f"-L{gskit / 'lib'}",
        f"-L{ps2sdk / 'ports/lib'}",
        "-Wl,-zmax-page-size=128",
        f"-T{ps2sdk / 'ee/startup/linkfile'}",
    ]

    for variant in variants:
        elf = args.output / f"box-{variant}.elf"
        build = builds[variant]
        run(
            [
                *common_compile,
                *link_common,
                oracle_object,
                "-o", elf,
                build / "glut/libps2glut.a",
                build / "libps2gl.a",
                "-lps2stuff", "-lpad", "-ldma",
            ],
            f"{variant}-link",
        )

        runner_args = [
            sys.executable, runner,
            "--elf", elf,
            "--timeout", str(args.timeout),
        ]
        if args.pcsx2 is not None:
            runner_args += ["--pcsx2", args.pcsx2.resolve()]
        if args.pcsx2_data is not None:
            runner_args += ["--pcsx2-data", args.pcsx2_data.resolve()]
        proc = run(runner_args, f"{variant}-pcsx2")

        frames = {}
        renderers = {}
        for match in FRAME_RE.finditer(proc.stdout):
            frame = int(match.group(1))
            frames[frame] = match.group(3).lower()
            renderers[frame] = match.group(2)
        if sorted(frames) != list(range(FRAME_COUNT)):
            raise RuntimeError(
                f"{variant}: expected frames 0..{FRAME_COUNT - 1}, got {sorted(frames)}"
            )
        bad_renderers = {
            frame: name for frame, name in renderers.items()
            if name != EXPECTED_RENDERER
        }
        if bad_renderers:
            raise RuntimeError(f"{variant}: unexpected renderer(s): {bad_renderers}")
        metadata["semantic_hashes"][variant] = [frames[i] for i in range(FRAME_COUNT)]

    reference = metadata["semantic_hashes"]["sce"]
    mismatches = {}
    for variant in variants[:-1]:
        if metadata["semantic_hashes"][variant] != reference:
            mismatches[variant] = [
                i for i, (actual, expected) in enumerate(
                    zip(metadata["semantic_hashes"][variant], reference)
                )
                if actual != expected
            ]
    if mismatches:
        raise RuntimeError(f"Semantic mismatch against SCE: {mismatches}")

    metadata["complete"] = True
    (args.output / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n"
    )
    print(
        f"PS2GL_GENERAL_QUAD_PASS frames={FRAME_COUNT} "
        f"variants={','.join(variants)}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
