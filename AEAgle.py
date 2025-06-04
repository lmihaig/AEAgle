#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import logging
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, Final, List, Sequence, Tuple

import serial  # Requires pyserial

# --------------------------------------------------------------------------- #
# Directories
# --------------------------------------------------------------------------- #
PROJECT_ROOT: Final[Path] = Path(__file__).resolve().parent
TESTS_DIR: Final[Path] = PROJECT_ROOT / "tests"
APPS_DIR: Final[Path] = PROJECT_ROOT / "apps"
RESULTS_DIR: Final[Path] = PROJECT_ROOT / "results" / "reports"

# --------------------------------------------------------------------------- #
# Suite → demo-folder map
# --------------------------------------------------------------------------- #
_OS_MAP: Final[Dict[str, str]] = {
    "zephyr": "demo-zephyr",
    "newlib": "demo-newlib",
    "newlib-nano": "demo-newlib-nano",
    "freertosv1": "demo-freertos",
    "freertosv2": "demo-freertos",
    "freertosv4": "demo-freertos",
    "contiki-memb": "demo-contiki",
    "contiki-heapmem": "demo-contiki",
    "riot-tlsf": "demo-riot",
    "riot-mema": "demo-riot",
}
_ROOT_MAIN_DEMOS: Final[set[str]] = {"demo-freertos", "demo-contiki"}

_RETRIES: Final[Dict[str, int]] = {
    # "riot-tlsf": 1,
    # "riot-mema": 1,
}
_RETRY_DELAY: Final[float] = 1.0

# --------------------------------------------------------------------------- #
# Serial settings & expected prefixes
# --------------------------------------------------------------------------- #
SERIAL_PORT: Final[str] = "/dev/ttyACM0"
SERIAL_BAUDRATE: Final[int] = 115200
SERIAL_TIMEOUT: Final[float] = 30.0  # seconds to wait for "start"/"end"

EXPECTED_PREFIXES: Final[List[str]] = [
    "META,",
    "SNAP,",
    "TIME,",
    "FAULT,",
    "EXHAUSTED,",
    "LEAK,",
    "NOLEAK,",
]

# --------------------------------------------------------------------------- #
# Logging helpers
# --------------------------------------------------------------------------- #
class _ColourFmt(logging.Formatter):
    C = {
        logging.DEBUG: "\033[90m",
        logging.INFO: "\033[96m",
        logging.WARNING: "\033[93m",
        logging.ERROR: "\033[91m",
    }
    R = "\033[0m"

    def format(self, rec: logging.LogRecord) -> str:
        rec.msg = f"{self.C.get(rec.levelno, self.R)}{rec.msg}{self.R}"
        return super().format(rec)


def _setup_logging(verbose: bool) -> None:
    logging.basicConfig(
        level=logging.DEBUG if verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )
    logging.getLogger().handlers[0].setFormatter(_ColourFmt("%(message)s"))


# --------------------------------------------------------------------------- #
# Utility functions
# --------------------------------------------------------------------------- #
def _all_test_names(tests_dir: Path) -> List[str]:
    return sorted(p.stem for p in tests_dir.glob("*.c"))


def _resolve_paths(os_name: str, test_name: str) -> Tuple[Path, Path, Path]:
    if os_name not in _OS_MAP:
        raise KeyError(
            f"Unknown suite '{os_name}'. Supported: {', '.join(sorted(_OS_MAP))}"
        )
    src_test = TESTS_DIR / os_name / f"{test_name}.c"

    # For all freertos variants, the .c files live under tests/freertos
    if os_name.startswith("freertosv"):
        src_test = TESTS_DIR / "freertos" / f"{test_name}.c"

    if not src_test.is_file():
        raise FileNotFoundError(f"Test not found: {src_test.relative_to(PROJECT_ROOT)}")
    demo_dir = APPS_DIR / _OS_MAP[os_name]
    if not demo_dir.is_dir():
        raise FileNotFoundError(f"Demo dir missing: {demo_dir}")
    dest_main = (
        demo_dir / "main.c"
        if demo_dir.name in _ROOT_MAIN_DEMOS
        else demo_dir / "src" / "main.c"
    )
    dest_main.parent.mkdir(parents=True, exist_ok=True)
    return src_test, demo_dir, dest_main


def _run_flash(flash_sh: Path, cwd: Path, os_name: str) -> int:
    flash_sh.chmod(0o755)

    env = os.environ.copy()
    if os_name.startswith("freertosv"):
        heap_impl = os_name[-1]
        env["HEAP_IMPL"] = heap_impl

    return subprocess.run([str(flash_sh)], cwd=cwd, env=env).returncode


# --------------------------------------------------------------------------- #
# Flash one (suite, test) with automatic retries
# --------------------------------------------------------------------------- #
def flash_pair(os_name: str, test_name: str) -> int:
    log = logging.getLogger("runner")
    src_test, demo_dir, dest_main = _resolve_paths(os_name, test_name)
    flash_sh = demo_dir / "flash.sh"
    if not flash_sh.is_file():
        log.error(f"'flash.sh' missing in {demo_dir}")
        return 1

    # Backup existing main.c
    backup = dest_main.with_suffix(".aea_backup") if dest_main.exists() else None
    if backup:
        shutil.copy2(dest_main, backup)

    attempts = _RETRIES.get(os_name, 1)
    rc = 1
    try:
        for n in range(1, attempts + 1):
            log.info(f"📄  {os_name:12} ← {test_name}  (try {n}/{attempts})")
            shutil.copy2(src_test, dest_main)

            rc = _run_flash(flash_sh, demo_dir, os_name)
            if rc == 0:
                log.info("    ✅ flash succeeded")
                break

            log.warning(f"    ⚠️  flash failed (code {rc})")
            if n < attempts:
                log.debug(f"    retrying in {_RETRY_DELAY}s …")
                time.sleep(_RETRY_DELAY)

    finally:
        # Restore or delete the patched main.c
        if backup and backup.exists():
            shutil.move(backup, dest_main)
        elif backup is None and dest_main.exists():
            dest_main.unlink(missing_ok=True)

    return rc


# --------------------------------------------------------------------------- #
# Serial capture and CSV writing (updated)
# --------------------------------------------------------------------------- #
def _capture_and_write_csv(os_name: str, test_name: str, ser: serial.Serial) -> None:
    """
    Assumes `ser` is already opened (and drained) BEFORE flash.sh is run.
    Wait up to SERIAL_TIMEOUT for "<test_name> start", then collect every
    log line until "<test_name> end". Parse only EXPECTED_PREFIXES into CSV.
    """
    log = logging.getLogger("serial")
    out_dir = RESULTS_DIR / os_name
    out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = out_dir / f"{test_name}.csv"

    log.info(f"Waiting (up to {SERIAL_TIMEOUT}s) for banner '{test_name} start'…")
    start_deadline = time.time() + SERIAL_TIMEOUT
    collecting = False
    collected_lines: List[str] = []

    while True:
        if time.time() > start_deadline:
            log.warning(f"Timeout waiting for '{test_name} start'. Giving up.")
            break

        try:
            raw = ser.readline()
        except Exception as e:
            log.error(f"Serial read error: {e}")
            break

        if not raw:
            continue

        line = raw.decode("utf-8", errors="ignore").rstrip("\r\n")
        lowercase = line.lower()

        if not collecting:
            # Loose‐match: does line contain test_name (case‐insensitive) AND "start"?
            if test_name.lower() in lowercase and "start" in lowercase:
                collecting = True
                log.info(f"Detected start banner: '{line}'. Beginning to collect logs.")
            continue

        # Once collecting, check for "test_name" + "end"
        if test_name.lower() in lowercase and "end" in lowercase:
            log.info(f"Detected end banner: '{line}'. Stopping collection.")
            break

        # Otherwise, buffer everything for post‐processing
        collected_lines.append(line)

    # Write out whatever was collected into CSV
    if not collected_lines:
        log.warning(f"No log lines captured for {os_name}/{test_name}. Writing empty CSV.")
    else:
        log.info(f"Parsing {len(collected_lines)} lines and writing CSV to {csv_path}")

    try:
        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            for raw_line in collected_lines:
                stripped = raw_line.strip()
                if not stripped or stripped.startswith("#"):
                    continue
                if any(stripped.startswith(pref) for pref in EXPECTED_PREFIXES):
                    writer.writerow(stripped.split(","))
                else:
                    log.debug(f"Ignoring unexpected line: '{stripped}'")
    except Exception as e:
        log.error(f"Failed to write CSV for {os_name}/{test_name}: {e}")


# --------------------------------------------------------------------------- #
# Job expansion helpers
# --------------------------------------------------------------------------- #
def _expand_jobs(os_opt: str | None, test_opt: str | None) -> Sequence[Tuple[str, str]]:
    if not os_opt and not test_opt:
        raise ValueError("Specify at least --os or --test")

    if os_opt and test_opt:
        return [(os_opt, test_opt)]

    if os_opt:
        tests_path = TESTS_DIR / (
            "freertos" if os_opt.startswith("freertosv") else os_opt
        )
        tests = _all_test_names(tests_path)
        if not tests:
            raise FileNotFoundError(f"No tests found in {tests_path}")
        return [(os_opt, t) for t in tests]

    jobs: List[Tuple[str, str]] = []
    for suite in _OS_MAP:
        suite_path = "freertos" if suite.startswith("freertosv") else suite
        if (TESTS_DIR / suite_path / f"{test_opt}.c").is_file():
            jobs.append((suite, test_opt))
    if not jobs:
        raise FileNotFoundError(f"No suite contains test '{test_opt}.c'")
    return jobs


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main() -> None:
    p = argparse.ArgumentParser(description="Deploy, flash, and capture allocator tests")
    p.add_argument("-o", "--os", help="Test-suite name (folder under tests/)")
    p.add_argument("-t", "--test", help="Test name (without .c)")
    p.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    args = p.parse_args()

    _setup_logging(args.verbose)
    log = logging.getLogger("runner")

    try:
        jobs = _expand_jobs(args.os, args.test)
    except Exception as exc:
        log.error(exc)
        sys.exit(1)

    failures: List[Tuple[str, str]] = []
    for os_name, test_name in jobs:
        # 1) Open and immediately drain the serial port BEFORE flashing
        try:
            ser = serial.Serial(
                port=SERIAL_PORT,
                baudrate=SERIAL_BAUDRATE,
                timeout=0.1,  # short timeout for reads
            )
            # Drain any leftover data (non-blocking reads for ~0.2s)
            drain_deadline = time.time() + 0.2
            while time.time() < drain_deadline:
                if not ser.read(ser.in_waiting or 1):
                    break
        except Exception as e:
            log.error(f"Could not open {SERIAL_PORT}: {e}")
            failures.append((os_name, test_name))
            continue

        # 2) Flash the board; as soon as it resets, Zephyr will print "<os_name> <test_name> start"
        rc = flash_pair(os_name, test_name)
        if rc != 0:
            failures.append((os_name, test_name))
            ser.close()
            continue

        # 3) Capture until "<test_name> end" or timeout
        _capture_and_write_csv(os_name, test_name, ser)
        ser.close()

    log.info("\n───────── SUMMARY ─────────")
    if failures:
        for os_name, test_name in failures:
            log.error(f"{os_name:12} {test_name}  ❌")
        sys.exit(1)

    log.info("All tests flashed and captured successfully 🎉")
    sys.exit(0)


if __name__ == "__main__":
    main()
