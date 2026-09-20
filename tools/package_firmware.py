"""Package the active PneutouchAi LEXIDE project (without build outputs)."""
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config
from tools.generate_config import header


def package():
    project = ROOT / config.FIRMWARE_DIR
    if (project / "S_PneuTouch/pneu_config.h").read_text(encoding="utf-8") != header():
        raise ValueError("config.py changed; run generate_config.py again")
    destination = ROOT / "build" / f"{config.PROJECT_NAME}.zip"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for source in sorted(project.rglob("*")):
            relative = source.relative_to(project)
            if not source.is_file() or relative.parts[0] in ("Debug", "Release", ".metadata"):
                continue
            if "__pycache__" in relative.parts: continue
            archive.write(source, Path(config.PROJECT_NAME) / relative)
    print(destination)
    return destination


if __name__ == "__main__":
    try: package()
    except (OSError, ValueError) as error:
        raise SystemExit(f"Package failed: {error}")
