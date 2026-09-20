"""Generate/check the active PneutouchAi sensor configuration from config.py."""
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config


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



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if the tracked header is stale")
    args = parser.parse_args()
    destination = ROOT / config.FIRMWARE_DIR / "S_PneuTouch/pneu_config.h"
    expected = header()
    if args.check:
        if not destination.is_file() or destination.read_text(encoding="utf-8") != expected:
            parser.exit(1, "Firmware config is stale; run python tools/generate_config.py\n")
        print("PneutouchAi configuration is current")
    else:
        destination.write_text(expected, encoding="utf-8")
        print(destination)


if __name__ == "__main__":
    main()
