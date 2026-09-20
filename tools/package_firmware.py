"""Package the complete LEXIDE source project, including local vendor dependencies."""
from pathlib import Path
import hashlib
import json
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config
from tools.prepare_vendor import header


def package():
    project = ROOT / "src/pneutouch-solist-ai"
    manifest = json.loads((project / "generated/vendor-manifest.json").read_text())
    if (project / "generated/pneu_config.h").read_text() != header():
        raise ValueError("config.py changed; run prepare_vendor.py again")
    for relative, expected in manifest["generated_sha256"].items():
        if hashlib.sha256((project / relative).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Dependency changed: {relative}; inspect and prepare again")
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
        raise SystemExit(f"Package failed; prepare dependencies first: {error}")
