"""Check reproducible DT dependency selection and both LEXIDE build configurations."""
from pathlib import Path
import json
import re
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile
import config
from tools.prepare_vendor import DRIVER_FILES, prepare

ROOT = Path(__file__).resolve().parents[1]


class ProjectTests(unittest.TestCase):
    def test_debug_and_release_use_phase0_sources(self):
        project = ROOT / "src/pneutouch-solist-ai"
        self.assertEqual(ET.parse(project / ".project").findtext("name"), config.PROJECT_NAME)
        configs = ET.parse(project / ".cproject").findall(".//configuration")
        self.assertEqual({c.get("name") for c in configs}, {"Debug", "Release"})
        option_ids = [o.get("id") for c in configs for o in c.findall(".//option")]
        self.assertEqual(len(option_ids), len(set(option_ids)))
        for c in configs:
            values = [v.get("value", "") for v in c.findall(".//listOptionValue")]
            self.assertIn("ML63Q2557", values)
            self.assertIn("../ML63Q25x7_lccarm.ld", values)
            self.assertFalse(any(".lib" in v or "AIVibration" in v for v in values))
            self.assertTrue(c.findall(".//option[@name='Generate HEX file'][@value='true']"))

    def test_linker_entry_resolves_generated_layout_in_both_builds(self):
        project = ROOT / "src/pneutouch-solist-ai"
        configs = ET.parse(project / ".cproject").findall(".//configuration")
        for c in configs:
            with self.subTest(configuration=c.get("name")):
                working = project / c.get("name")
                scripts = c.findall(".//option[@name='Script files']/listOptionValue")
                self.assertEqual(len(scripts), 1)
                entry = (working / scripts[0].get("value")).resolve()
                self.assertTrue(entry.is_file())
                # Only delegate: a second MEMORY/SECTIONS block would redefine layout.
                tokens = re.sub(r"/\*.*?\*/", "", entry.read_text(), flags=re.S).split()
                self.assertEqual(len(tokens), 2)
                self.assertEqual(tokens[0], "INCLUDE")
                self.assertEqual((working / tokens[1]).resolve(),
                                 project / "generated/ML63Q25x7_lccarm.ld")

    def test_prepare_ignores_other_sdk_projects_and_records_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / config.VENDOR_PROJECT
            for folder in ("S_Driver", "RTE"):
                (source / folder).mkdir(parents=True, exist_ok=True)
            for name in DRIVER_FILES: (source / "S_Driver" / name).write_text("/* vendor notice */\n")
            for name in ("ML63Q25x7.h", "system_ML63Q25x7.h", "system_ML63Q25x7.c", "startup_ML63Q25x7.c"):
                (source / "RTE" / name).write_text("/* vendor notice */\n")
            (source / ".cproject").write_text("<cproject />")
            (source / "ML63Q25x7_lccarm.ld").write_text("__STACK_SIZE = 0x100;\n")
            (root / ".cproject").write_text("unrelated RB project")
            with zipfile.ZipFile(root / config.CMSIS_PACK, "w") as archive:
                archive.writestr("CMSIS/Core/Include/core_cm0plus.h", "/* CMSIS */")
                archive.writestr("LICENSE.txt", "license")
            output = prepare(root, root / "output")
            manifest = json.loads((output / "generated/vendor-manifest.json").read_text())
            self.assertIn("vendor/S_Driver/wdt.c", manifest["generated_sha256"])
            self.assertEqual((output / "vendor/CMSIS/LICENSE.txt").read_text(), "license")

    def test_missing_dependency_fails_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output"
            with self.assertRaises(ValueError): prepare(Path(directory), output)
            self.assertFalse(output.exists())


if __name__ == "__main__": unittest.main()
