#!/usr/bin/env python3
import os
import argparse
import shutil
import subprocess
from pathlib import Path


def deploy_and_flash(project_root: Path, os_name: str, test_name: str):
    # Paths relative to project_root
    tests_dir = project_root / "tests" / os_name
    src_test = tests_dir / f"{test_name}.c"
    if not src_test.is_file():
        raise FileNotFoundError(f"Test source not found: {src_test}")

    demo_dir = project_root / "apps" / f"demo-{os_name}"
    if not demo_dir.is_dir():
        raise FileNotFoundError(f"OS demo directory not found: {demo_dir}")

    dest_main = demo_dir / "src" / "main.c"
    dest_main.parent.mkdir(parents=True, exist_ok=True)

    flash_sh = demo_dir / "flash.sh"
    if not flash_sh.is_file():
        raise FileNotFoundError(f"Flash script not found: {flash_sh}")

    # Copy test file to main.c
    shutil.copyfile(src_test, dest_main)
    print(f"Copied {src_test} → {dest_main}")

    # Ensure flash.sh is executable
    flash_sh.chmod(0o755)

    # Run the flash script
    print(f"Flashing board for {os_name}/{test_name}...")
    result = subprocess.run(
        [str(flash_sh)], cwd=demo_dir, capture_output=True, text=True
    )
    if result.returncode != 0:
        print("❌ Flash failed:")
        print(result.stderr)
    else:
        print("✅ Flash succeeded:")
        print(result.stdout)


def main():
    parser = argparse.ArgumentParser(
        description="Deploy and flash embedded allocator tests"
    )
    parser.add_argument(
        "--os", required=True, help="Target OS (e.g., zephyr, freertos)"
    )
    parser.add_argument(
        "--test", required=True, help="Test name (source file without .c)"
    )
    args = parser.parse_args()

    # runner.py lives in tools/, so project root is one level up
    tools_dir = Path(__file__).parent.resolve()
    project_root = tools_dir.parent

    try:
        deploy_and_flash(project_root, args.os, args.test)
    except Exception as e:
        print(f"Error: {e}")
        exit(1)


if __name__ == "__main__":
    main()
