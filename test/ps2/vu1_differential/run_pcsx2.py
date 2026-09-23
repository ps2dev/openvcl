#!/usr/bin/env python3
"""Run the VU1 differential ELF in PCSX2 and return its semantic result."""

import argparse
from pathlib import Path
import selectors
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from typing import Optional


PASS_MARKER = "VU1_DIFFERENTIAL_PASS"
FAIL_MARKER = "VU1_DIFFERENTIAL_FAIL"


def default_pcsx2() -> Optional[Path]:
    if sys.platform == "darwin":
        candidate = Path("/Applications/PCSX2.app/Contents/MacOS/PCSX2")
        if candidate.is_file():
            return candidate

    for name in ("pcsx2-qt", "pcsx2", "PCSX2"):
        candidate = shutil.which(name)
        if candidate:
            return Path(candidate)
    return None


def default_pcsx2_data() -> Optional[Path]:
    if sys.platform == "darwin":
        candidate = Path.home() / "Library/Application Support/PCSX2"
        if candidate.is_dir():
            return candidate
    return None


def patch_config(source_config: Path, source_data: Path, destination: Path) -> None:
    section = ""
    saw_bios = False
    saw_system_console = False
    saw_ee_console = False
    saw_iop_console = False
    output = []

    for original in source_config.read_text().splitlines():
        stripped = original.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped[1:-1]
            output.append(original)
            continue

        if "=" not in original:
            output.append(original)
            continue

        key, value = original.split("=", 1)
        key_name = key.strip()
        if section == "Folders" and key_name == "Bios":
            bios = Path(value.strip()).expanduser()
            if not bios.is_absolute():
                bios = source_data / bios
            output.append(f"Bios = {bios.resolve()}")
            saw_bios = True
        elif section == "Logging" and key_name == "EnableSystemConsole":
            output.append("EnableSystemConsole = true")
            saw_system_console = True
        elif section == "Logging" and key_name == "EnableEEConsole":
            output.append("EnableEEConsole = true")
            saw_ee_console = True
        elif section == "Logging" and key_name == "EnableIOPConsole":
            output.append("EnableIOPConsole = true")
            saw_iop_console = True
        else:
            output.append(original)

    missing = []
    if not saw_bios:
        missing.append("[Folders] Bios")
    if not saw_system_console:
        missing.append("[Logging] EnableSystemConsole")
    if not saw_ee_console:
        missing.append("[Logging] EnableEEConsole")
    if not saw_iop_console:
        missing.append("[Logging] EnableIOPConsole")
    if missing:
        raise RuntimeError("PCSX2.ini is missing required keys: " + ", ".join(missing))

    destination.write_text("\n".join(output) + "\n")


def prepare_isolated_data(source_data: Path, temporary_root: Path) -> None:
    source_config = source_data / "inis/PCSX2.ini"
    if not source_config.is_file():
        raise RuntimeError(f"PCSX2 configuration not found: {source_config}")

    destination_root = temporary_root / "PCSX2"
    destination_inis = destination_root / "inis"
    destination_inis.mkdir(parents=True)
    patch_config(source_config, source_data, destination_inis / "PCSX2.ini")

    secrets = source_data / "inis/secrets.ini"
    if secrets.is_file():
        shutil.copy2(secrets, destination_inis / "secrets.ini")


def run_pcsx2(pcsx2: Path, pcsx2_data: Path, elf: Path, timeout: float) -> int:
    with tempfile.TemporaryDirectory(prefix="openvcl-pcsx2-") as temporary:
        temporary_root = Path(temporary)
        prepare_isolated_data(pcsx2_data, temporary_root)
        logfile = temporary_root / "pcsx2.log"
        command = [
            str(pcsx2),
            "-datapath",
            str(temporary_root),
            "-nogui",
            "-batch",
            "-earlyconsolelog",
            "-logfile",
            str(logfile),
            "-elf",
            str(elf),
        ]

        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None

        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        deadline = time.monotonic() + timeout
        result = None

        try:
            while time.monotonic() < deadline:
                remaining = max(0.0, deadline - time.monotonic())
                events = selector.select(min(0.25, remaining))
                for key, _ in events:
                    line = key.fileobj.readline()
                    if not line:
                        continue
                    sys.stdout.write(line)
                    sys.stdout.flush()
                    if FAIL_MARKER in line:
                        result = 1
                        break
                    if PASS_MARKER in line:
                        result = 0
                        break
                if result is not None or process.poll() is not None:
                    break
        finally:
            selector.close()
            if process.poll() is None:
                process.send_signal(signal.SIGINT)
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(timeout=3)

        if result is not None:
            return result
        if process.returncode not in (None, 0):
            print(f"PCSX2 exited before producing a result (status {process.returncode}).",
                  file=sys.stderr)
        else:
            print(f"Timed out waiting for {PASS_MARKER}/{FAIL_MARKER}.",
                  file=sys.stderr)
        if logfile.is_file():
            print(f"PCSX2 log was: {logfile}", file=sys.stderr)
        return 2


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pcsx2",
        type=Path,
        default=default_pcsx2(),
        help="PCSX2 executable (auto-detected on macOS or PATH)",
    )
    parser.add_argument(
        "--pcsx2-data",
        type=Path,
        default=default_pcsx2_data(),
        help="Existing PCSX2 data directory used only as a configuration/BIOS source",
    )
    parser.add_argument(
        "--elf",
        type=Path,
        default=script_dir / "vu1_differential.elf",
        help="Built differential ELF",
    )
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    if args.pcsx2 is None or not args.pcsx2.is_file():
        parser.error("PCSX2 executable not found; pass --pcsx2")
    if args.pcsx2_data is None or not args.pcsx2_data.is_dir():
        parser.error("PCSX2 data directory not found; pass --pcsx2-data")
    if not args.elf.is_file():
        parser.error(f"ELF not found: {args.elf}")

    try:
        return run_pcsx2(
            args.pcsx2.resolve(),
            args.pcsx2_data.resolve(),
            args.elf.resolve(),
            args.timeout,
        )
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
