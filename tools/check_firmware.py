"""Compile the active pressure path and startup for Cortex-M0+ (not a HEX build)."""
import argparse
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config
from tools.generate_config import header


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmsis-include", type=Path, default=os.environ.get("CMSIS_INCLUDE"),
                        help="ARM/CMSIS/5.9.0/CMSIS/Core/Include from an installed pack")
    parser.add_argument("--toolchain-include", type=Path, action="append", default=[],
                        help="Additional compiler headers (repeat for LAPIS Clang and Picolibc)")
    args = parser.parse_args()
    if args.cmsis_include is None:
        parser.error("Pass --cmsis-include or set CMSIS_INCLUDE to the CMSIS Core include directory")
    if not (args.cmsis_include / "core_cm0plus.h").is_file():
        parser.error("CMSIS Core include directory is missing core_cm0plus.h")
    project = ROOT / config.FIRMWARE_DIR
    if (project / "S_PneuTouch/pneu_config.h").read_text(encoding="utf-8") != header():
        raise SystemExit("Run python tools/generate_config.py first")
    sources = sorted((project / "S_PneuTouch").glob("*.c"))
    sources += [project / "S_System/main.c"]
    sources += sorted((project / "RTE").glob("*.c"))
    sources += [project / "S_Driver" / name for name in ("smpl_common.c", "wdt.c", "codeoption.c")]
    output = ROOT / "build/arm-check"
    output.mkdir(parents=True, exist_ok=True)
    flags = [os.environ.get("CLANG", "clang"), "--target=arm-none-eabi", "-mcpu=cortex-m0plus",
             "-mthumb", "-std=c11", "-ffreestanding", "-DML63Q25x7", "-DML63Q2557"]
    flags += ["-I" + str(args.cmsis_include)]
    flags += ["-I" + str(path) for path in args.toolchain_include]
    for folder in ("S_PneuTouch", "S_System", "RTE", "S_Driver"):
        flags += ["-I" + str(project / folder)]
    for source in sources:
        subprocess.run([*flags, "-c", str(source), "-o", str(output / (source.stem + ".o"))], check=True)
    print(f"Compiled {len(sources)} pressure/startup Arm objects. Full link/HEX requires LEXIDE.")


if __name__ == "__main__":
    main()
