"""Run portable C driver and PC receiver tests on Mac/Linux with no vendor SDK."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.generate_config import header

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = "src/pneutouch-solist/S_PneuTouch/"
STORE = ("model_store", "crc16")
SELECT = ("feature_select", "fast_log")
# Firmware sources linked into each host test (hx710b.c is linked into all of them).
C_TESTS = {
    "hx710b": (),
    "acquisition": ("acquisition",),
    "pressure_app": ("acquisition", "pressure_app", "app_buttons", "app_record", "store_protocol", "pressure_features",
                     "uart_text"),
    "app_buttons": ("acquisition", "pressure_app", "app_buttons", "app_record", "store_protocol", "pressure_features",
                    "uart_text") + STORE,
    "pressure_features": ("pressure_features",),
    "ai_validation": ("ai_validation", "store_protocol", "uart_text") + STORE,
    "pressure_model": ("pressure_model", "model_tune", "model_screen") + SELECT + STORE,
    "model_tune": ("pressure_model", "model_tune", "model_screen") + SELECT + STORE,
    "pressure_display": ("pressure_display",),
    "model_store": STORE,
    "feature_select": SELECT,
    "fast_log": ("fast_log",),
    "soft_spi": (),  # tests/test_soft_spi.c includes S_Bsp/SoftSpi.c with a fake Port 8
}


def main():
    # Windows may briefly keep a just-executed binary locked during cleanup.
    with tempfile.TemporaryDirectory(prefix="pneutouch-test-", ignore_cleanup_errors=True) as directory:
        (Path(directory) / "pneu_config.h").write_text(header())
        for name, modules in C_TESTS.items():
            executable = Path(directory) / f"test_{name}{'.exe' if os.name == 'nt' else ''}"
            sources = [FIRMWARE + "hx710b.c", f"tests/test_{name}.c"]
            sources += [FIRMWARE + module + ".c" for module in modules]
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-Itests/ai_stubs", "-Isrc/pneutouch-solist/S_Driver", "-Isrc/pneutouch-solist/S_Bsp",
                "-Isrc/pneutouch-solist/S_PneuTouch", "-I" + directory, *sources,
                "-o", str(executable), "-lm"], cwd=ROOT, check=True)
            subprocess.run([str(executable)], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"],
                   cwd=ROOT, check=True)
    subprocess.run([os.environ.get("NODE", "node"), "--test", *map(str, sorted((ROOT / "tests").glob("*.test.mjs")))],
                   cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
