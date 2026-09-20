"""Run portable C driver and PC receiver tests on Mac/Linux with no vendor SDK."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.prepare_vendor import header

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="pneutouch-test-") as directory:
        (Path(directory) / "pneu_config.h").write_text(header())
        for name in ("hx710b", "acquisition"):
            executable = Path(directory) / f"test_{name}"
            sources = ["src/pneutouch-solist-ai/Source/hx710b.c", f"tests/test_{name}.c"]
            if name == "acquisition": sources.append("src/pneutouch-solist-ai/Source/acquisition.c")
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-Isrc/pneutouch-solist-ai/Source", "-I" + directory, *sources,
                "-o", str(executable)], cwd=ROOT, check=True)
            subprocess.run([str(executable)], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"],
                   cwd=ROOT, check=True)
    subprocess.run([os.environ.get("NODE", "node"), "--test", *map(str, sorted((ROOT / "tests").glob("*.test.mjs")))],
                   cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
