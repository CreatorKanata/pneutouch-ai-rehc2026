"""Compile all firmware translation units for Cortex-M0+; this is not a HEX build."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    project = ROOT / "src/pneutouch-solist-ai"
    sources = sorted((project / "Source").glob("*.c"))
    sources += sorted((project / "vendor/RTE").glob("*.c"))
    sources += sorted((project / "vendor/S_Driver").glob("*.c"))
    if not (project / "generated/vendor-manifest.json").exists():
        raise SystemExit("Run python3 tools/prepare_vendor.py first")
    output = ROOT / "build/arm-check"
    output.mkdir(parents=True, exist_ok=True)
    flags = [os.environ.get("CLANG", "clang"), "--target=arm-none-eabi", "-mcpu=cortex-m0plus",
             "-mthumb", "-std=c11", "-ffreestanding", "-DML63Q25x7", "-DML63Q2557"]
    for folder in ("Source", "generated", "vendor/RTE", "vendor/S_Driver", "vendor/CMSIS"):
        flags += ["-I" + str(project / folder)]
    for source in sources:
        subprocess.run([*flags, "-c", str(source), "-o", str(output / (source.stem + ".o"))], check=True)
    print(f"Compiled {len(sources)} Arm objects. Final link/HEX still requires Windows LEXIDE.")


if __name__ == "__main__":
    main()
