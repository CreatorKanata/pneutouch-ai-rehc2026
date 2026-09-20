"""Prepare ignored vendor dependencies for the checked-in LEXIDE project.

Vendor files retain their notices and stay in ignored vendor/, not in Git.
Run again after changing config.py. Source/ and IDE metadata are never overwritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config

DRIVER_FILES = (
    "mcu.h", "rdwr_reg.h", "clock.h", "smpl_common.h", "smpl_common.c",
    "wdt.h", "wdt.c", "uartf1.h", "uartf_common.h",
    "codeoption.h", "codeoption_config.h", "codeoption.c",
)


def header():
    if config.SAMPLE_RATE_HZ not in (10, 40):
        raise ValueError("HX710B pressure mode supports only 10 or 40 SPS")
    if (config.CPU_HZ, config.UART_BAUD, config.UART_DIVISOR, config.UART_ADJUST) != (
        48_000_000, 115_200, 0x19, 0x19
    ):
        raise ValueError("Changing clock/baud requires a verified ROHM UART configuration")
    if not 1 <= config.CLOCK_HALF_US <= 10:
        raise ValueError("SCK timing must leave margin below the 50 us high limit")
    if not 400 <= config.SETTLE_MS < config.SENSOR_TIMEOUT_MS < 0x80000000:
        raise ValueError("Use >=400 ms settling and a longer finite sensor timeout")
    if not 0 < config.STACK_BYTES <= 8192 or config.STACK_BYTES % 8:
        raise ValueError("Stack must fit the 16 KiB SRAM and be 8-byte aligned")
    values = {key: getattr(config, key) for key in (
        "CPU_HZ", "UART_BAUD", "UART_DIVISOR", "UART_ADJUST", "SAMPLE_RATE_HZ",
        "SENSOR_TIMEOUT_MS", "SETTLE_MS", "CLOCK_HALF_US",
    )}
    values["HX_PULSES"] = 27 if config.SAMPLE_RATE_HZ == 40 else 25
    return ("/* Generated from config.py. Edit the source configuration. */\n"
            "#ifndef PNEU_CONFIG_H\n#define PNEU_CONFIG_H\n" +
            "".join(f"#define PNEU_{key} {value}U\n" for key, value in values.items()) +
            "#endif\n")


def prepare(reference_root, output):
    generated_header = header()  # Validate before writing anything.
    source = reference_root / config.VENDOR_PROJECT
    pack = reference_root / config.CMSIS_PACK
    required = [source / ".cproject", pack, source / "ML63Q25x7_lccarm.ld"]
    required += [source / "S_Driver" / name for name in DRIVER_FILES]
    rte_files = ("ML63Q25x7.h", "system_ML63Q25x7.h", "system_ML63Q25x7.c",
                 "startup_ML63Q25x7.c")
    required += [source / "RTE" / name for name in rte_files]
    for path in required:
        if not path.is_file():
            raise ValueError(f"Missing DT dependency: {path}")
    generated = output / "generated"
    vendor = output / "vendor"
    generated.mkdir(parents=True, exist_ok=True)
    for folder in ("RTE", "S_Driver", "CMSIS"):
        (vendor / folder).mkdir(parents=True, exist_ok=True)
    (generated / "pneu_config.h").write_text(generated_header)
    for name in DRIVER_FILES:
        shutil.copy2(source / "S_Driver" / name, vendor / "S_Driver" / name)
    for name in rte_files:
        shutil.copy2(source / "RTE" / name, vendor / "RTE" / name)
    with zipfile.ZipFile(pack) as archive:
        prefix = "CMSIS/Core/Include/"
        for member in archive.namelist():
            if Path(member).is_absolute() or ".." in Path(member).parts:
                raise ValueError("Invalid CMSIS archive path")
            if member.startswith(prefix) and member.endswith(".h"):
                target = vendor / "CMSIS" / member[len(prefix):]
                if ".." in target.parts:
                    raise ValueError("Invalid CMSIS archive path")
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(member))
        # Preserve the pack's license alongside extracted headers when present.
        for member in archive.namelist():
            if Path(member).name.lower() in ("license.txt", "license"):
                target = vendor / "CMSIS" / member
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(member))
    linker = (source / "ML63Q25x7_lccarm.ld").read_bytes()
    linker, count = re.subn(rb"__STACK_SIZE = 0x[0-9A-Fa-f]+;",
                          f"__STACK_SIZE = 0x{config.STACK_BYTES:08X};".encode(), linker)
    if count != 1:
        raise ValueError("Vendor linker script stack definition changed")
    (generated / "ML63Q25x7_lccarm.ld").write_bytes(linker)
    manifest = {
        "project": config.VENDOR_PROJECT,
        "source_sha256": {str(path.relative_to(reference_root)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in required},
        "generated_sha256": {str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in sorted(output.rglob("*")) if path.is_file()
                             and ("vendor" in path.parts or path.parent == generated)
                             and path.name != "vendor-manifest.json"},
    }
    (generated / "vendor-manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--references", type=Path, default=ROOT / "references")
    args = parser.parse_args()
    try:
        print(prepare(args.references.resolve(), ROOT / "src/pneutouch-solist-ai"))
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Prepare failed: {error}\n")


if __name__ == "__main__":
    main()
