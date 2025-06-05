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
from typing import Dict, Final, List, Sequence, Tuple, Literal

import serial

PROJECT_ROOT: Final[Path] = Path(__file__).resolve().parent
TESTS_DIR: Final[Path] = PROJECT_ROOT / "tests"
APPS_DIR: Final[Path] = PROJECT_ROOT / "apps"
RESULTS_DIR: Final[Path] = PROJECT_ROOT / "results" / "reports"

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

_RETRIES: Final[Dict[str, int]] = {}
_RETRY_DELAY: Final[float] = 1.0

SERIAL_PORT: Final[str] = "/dev/ttyACM0"
SERIAL_BAUDRATE: Final[int] = 115200
SERIAL_TIMEOUT: Final[float] = 30.0

EXPECTED_PREFIXES: Final[List[str]] = [
    "META,",
    "SNAP,",
    "TIME,",
    "FAULT,",
    "LEAK,",
    "NOLEAK,",
]

CaptureStatus = Literal["SUCCESS", "NO_START", "NO_END", "SERIAL_ERROR"]

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
    if logging.getLogger().handlers:
        logging.getLogger().handlers[0].setFormatter(_ColourFmt("%(levelname)s: %(message)s"))

def _all_test_names(tests_dir: Path) -> List[str]:
    return sorted(p.stem for p in tests_dir.glob("*.c"))

def _resolve_paths(os_name: str, test_name: str) -> Tuple[Path, Path, Path]:
    if os_name not in _OS_MAP:
        raise KeyError(
            f"Unknown suite '{os_name}'. Supported: {', '.join(sorted(_OS_MAP))}"
        )
    src_test_dir_name = "freertos" if os_name.startswith("freertosv") else os_name
    src_test = TESTS_DIR / src_test_dir_name / f"{test_name}.c"

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
        env["HEAP_IMPL"] = os_name[-1]
    return subprocess.run([str(flash_sh)], cwd=cwd, env=env, capture_output=True).returncode

def flash_pair(os_name: str, test_name: str) -> int:
    log = logging.getLogger("runner.flash")
    try:
        src_test, demo_dir, dest_main = _resolve_paths(os_name, test_name)
    except (KeyError, FileNotFoundError) as e:
        log.error(f"Path resolution error for {os_name}/{test_name}: {e}")
        return 1
        
    flash_sh = demo_dir / "flash.sh"
    if not flash_sh.is_file():
        log.error(f"'flash.sh' missing in {demo_dir}")
        return 1

    backup = dest_main.with_suffix(".aea_backup") if dest_main.exists() else None
    if backup:
        shutil.copy2(dest_main, backup)

    attempts = _RETRIES.get(os_name, 0) + 1 
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
        if backup and backup.exists():
            shutil.move(backup, dest_main)
        elif backup is None and dest_main.exists(): 
            dest_main.unlink(missing_ok=True)
    return rc

def _capture_and_write_csv(os_name: str, test_name: str, ser: serial.Serial) -> CaptureStatus:
    log = logging.getLogger("runner.serial")
    out_dir = RESULTS_DIR / os_name
    out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = out_dir / f"{test_name}.csv"

    log.info(f"Waiting for banners (overall timeout {SERIAL_TIMEOUT}s)...")
    overall_deadline = time.time() + SERIAL_TIMEOUT
    
    found_start_banner = False
    # found_end_banner = False # Not needed, status will track this
    collected_lines: List[str] = []
    status: CaptureStatus = "NO_START" 

    while time.time() < overall_deadline:
        try:
            raw = ser.readline()
        except Exception as e:
            log.error(f"Serial read error: {e}")
            status = "SERIAL_ERROR"
            break 

        if not raw:
            continue

        line = raw.decode("utf-8", errors="ignore").rstrip("\r\n")
        lowercase = line.lower()

        if not found_start_banner:
            if test_name.lower() in lowercase and "start" in lowercase:
                found_start_banner = True
                log.info(f"Detected start banner: '{line}'. Beginning to collect logs.")
                status = "NO_END" 
        else: 
            if test_name.lower() in lowercase and "end" in lowercase:
                log.info(f"Detected end banner: '{line}'. Stopping collection.")
                # Intentionally do not add end banner to collected_lines for CSV here,
                # unless it also matches an EXPECTED_PREFIX (unlikely for simple banners)
                status = "SUCCESS"
                break 
            collected_lines.append(line)

    if status == "NO_START":
        log.warning(f"TIMEOUT: Did not detect '{test_name} start' banner for {os_name}/{test_name}.")
    elif status == "NO_END":
        log.warning(f"CRASH/TIMEOUT: Detected '{test_name} start' but not '{test_name} end' banner for {os_name}/{test_name}.")
    
    if collected_lines or status == "SUCCESS": # Process if we have lines or if it was a success (even with no data lines)
        log.info(f"Parsing {len(collected_lines)} lines and writing CSV to {csv_path}")
        try:
            with open(csv_path, "w", newline="") as f:
                writer = csv.writer(f)
                for raw_line in collected_lines:
                    stripped = raw_line.strip()
                    if not stripped or stripped.startswith("#"):
                        continue
                    
                    # Banners should generally not be data lines unless they coincidentally match a prefix
                    is_banner_line = False
                    if test_name.lower() in stripped.lower(): # Heuristic for banner
                        if "start" in stripped.lower() or "end" in stripped.lower():
                             is_banner_line = not any(stripped.startswith(pref) for pref in EXPECTED_PREFIXES) # Only a banner if not a data line
                    
                    if is_banner_line:
                        log.debug(f"Ignoring likely banner line from data: '{stripped}'")
                        continue

                    if any(stripped.startswith(pref) for pref in EXPECTED_PREFIXES):
                        writer.writerow(stripped.split(","))
                    else:
                        log.debug(f"Ignoring unexpected line: '{stripped}'")
        except Exception as e:
            log.error(f"Failed to write CSV for {os_name}/{test_name}: {e}")
    elif status != "SERIAL_ERROR" and status != "SUCCESS": # No lines, not a success, not a serial error (e.g. NO_START or NO_END with no lines)
        log.warning(f"No standard log lines captured for {os_name}/{test_name}. CSV will be empty or not written if status indicates failure.")
        # Create an empty CSV to signify the test ran but produced no valid data or timed out
        with open(csv_path, "w", newline="") as f: 
            pass # Creates an empty file

    return status


def _expand_jobs(os_opt: str | None, test_opt: str | None) -> Sequence[Tuple[str, str]]:
    if not os_opt and not test_opt:
        raise ValueError("Specify at least --os or --test")

    if os_opt and test_opt:
        return [(os_opt, test_opt)]

    if os_opt:
        tests_path_name = "freertos" if os_opt.startswith("freertosv") else os_opt
        tests = _all_test_names(TESTS_DIR / tests_path_name)
        if not tests:
            raise FileNotFoundError(f"No tests found in {TESTS_DIR / tests_path_name}")
        return [(os_opt, t) for t in tests]

    if test_opt:
        jobs: List[Tuple[str, str]] = []
        for suite in _OS_MAP:
            suite_path_name = "freertos" if suite.startswith("freertosv") else suite
            if (TESTS_DIR / suite_path_name / f"{test_opt}.c").is_file():
                jobs.append((suite, test_opt))
        if not jobs:
            raise FileNotFoundError(f"No suite contains test '{test_opt}.c'")
        return jobs
    return [] 


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

    flash_failures: List[Tuple[str, str]] = []
    crashed_tests: List[Tuple[str, str]] = [] # Tests that started but didn't end
    no_start_timeouts: List[Tuple[str, str]] = [] # Tests that never showed start banner
    serial_errors: List[Tuple[str, str]] = [] # Other serial communication issues
    
    for os_name, test_name in jobs:
        try:
            ser = serial.Serial(
                port=SERIAL_PORT,
                baudrate=SERIAL_BAUDRATE,
                timeout=0.1, 
            )
            drain_deadline = time.time() + 0.2
            while time.time() < drain_deadline:
                ser.read(ser.in_waiting or 1) 
        except Exception as e:
            log.error(f"Could not open or drain {SERIAL_PORT}: {e}")
            flash_failures.append((os_name, test_name))
            continue

        rc = flash_pair(os_name, test_name)
        if rc != 0:
            flash_failures.append((os_name, test_name))
            ser.close()
            continue

        capture_status = _capture_and_write_csv(os_name, test_name, ser)
        
        if capture_status == "NO_END":
            crashed_tests.append((os_name, test_name))
        elif capture_status == "NO_START":
            no_start_timeouts.append((os_name, test_name))
        elif capture_status == "SERIAL_ERROR":
            serial_errors.append((os_name, test_name))
        
        ser.close()

    log.info("\n───────── SUMMARY ─────────")
    has_issues = False
    if flash_failures:
        has_issues = True
        log.error("Flash/Setup Failures:")
        for os_name, test_name in flash_failures:
            log.error(f"  {os_name:12} {test_name}  ❌ (Flash/Setup)")
    
    if no_start_timeouts:
        has_issues = True
        log.warning("No Start Banner Timeouts:")
        for os_name, test_name in no_start_timeouts:
            log.warning(f"  {os_name:12} {test_name}  ⚠️ (No Start)")

    if crashed_tests:
        has_issues = True
        log.warning("Crashed/Hanged Tests (Started but Did Not End Normally):")
        for os_name, test_name in crashed_tests:
            log.warning(f"  {os_name:12} {test_name}  💥 (Crash/Hang)")
    
    if serial_errors:
        has_issues = True
        log.error("Serial Communication Errors During Capture:")
        for os_name, test_name in serial_errors:
            log.error(f"  {os_name:12} {test_name}  🔌 (Serial Err)")


    if not has_issues:
        log.info("All tests processed successfully. 🎉")
        sys.exit(0)
    else:
        log.info("Some tests had issues. Please review logs. 씁")
        sys.exit(1)
    

if __name__ == "__main__":
    main()