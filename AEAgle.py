#!/usr/bin/env python3
"""
AEAgle – test-runner & flasher
… (doc-string unchanged) …
"""

from __future__ import annotations

import argparse
import logging
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, Final, List, Sequence, Tuple

# --------------------------------------------------------------------------- #
# Directories
# --------------------------------------------------------------------------- #
PROJECT_ROOT: Final[Path] = Path(__file__).resolve().parent
TESTS_DIR: Final[Path] = PROJECT_ROOT / "tests"
APPS_DIR: Final[Path] = PROJECT_ROOT / "apps"

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
_RETRY_DELAY: Final[float] = 1


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


def _run_flash(flash_sh: Path, cwd: Path) -> int:
    flash_sh.chmod(0o755)
    return subprocess.run([str(flash_sh)], cwd=cwd).returncode


# --------------------------------------------------------------------------- #
# Flash one (suite, test) with automatic retries
# --------------------------------------------------------------------------- #
def flash_pair(os_name: str, test_name: str) -> int:  # ▶ CHANGED
    log = logging.getLogger("runner")
    src_test, demo_dir, dest_main = _resolve_paths(os_name, test_name)
    flash_sh = demo_dir / "flash.sh"
    if not flash_sh.is_file():
        log.error(f"'flash.sh' missing in {demo_dir}")
        return 1

    backup = dest_main.with_suffix(".aea_backup") if dest_main.exists() else None
    if backup:
        shutil.copy2(dest_main, backup)

    attempts = _RETRIES.get(os_name, 1)
    rc = 1
    try:
        for n in range(1, attempts + 1):
            log.info(f"📄  {os_name:12} ← {test_name}  (try {n}/{attempts})")
            shutil.copy2(src_test, dest_main)

            rc = _run_flash(flash_sh, demo_dir)
            if rc == 0:
                log.info("    ✅ success")
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


# --------------------------------------------------------------------------- #


def _expand_jobs(os_opt: str | None, test_opt: str | None) -> Sequence[Tuple[str, str]]:
    if not os_opt and not test_opt:
        raise ValueError("Specify at least --os or --test")
    if os_opt and test_opt:
        return [(os_opt, test_opt)]
    if os_opt:
        tests = _all_test_names(TESTS_DIR / os_opt)
        if not tests:
            raise FileNotFoundError(f"No tests found in tests/{os_opt}")
        return [(os_opt, t) for t in tests]
    jobs: List[Tuple[str, str]] = []
    for suite in _OS_MAP:
        if (TESTS_DIR / suite / f"{test_opt}.c").is_file():
            jobs.append((suite, test_opt))
    if not jobs:
        raise FileNotFoundError(f"No suite contains test '{test_opt}.c'")
    return jobs


def main() -> None:
    p = argparse.ArgumentParser(description="Deploy and flash allocator tests")
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
        try:
            if flash_pair(os_name, test_name) != 0:
                failures.append((os_name, test_name))
        except Exception as exc:  # noqa: BLE001
            log.error(exc)
            failures.append((os_name, test_name))

    log.info("\n───────── SUMMARY ─────────")
    if failures:
        for os_name, test_name in failures:
            log.error(f"{os_name:12} {test_name}  ❌")
        sys.exit(1)
    log.info("All flashes succeeded 🎉")
    sys.exit(0)


if __name__ == "__main__":
    main()
