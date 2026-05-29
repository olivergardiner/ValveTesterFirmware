"""
make_arduino_project.py
Creates a self-contained Arduino project folder from the PlatformIO source files.
The output folder is excluded from git via .gitignore.

Usage (from the project root):
    python make_arduino_project.py
"""

import re
import shutil
from pathlib import Path

SKETCH_NAME = "ValveTesterFirmware"

project_root = Path(__file__).parent
out_dir      = project_root / "arduino_project" / SKETCH_NAME

# ── 1. Create output folder ───────────────────────────────────────────────────
if out_dir.exists():
    shutil.rmtree(out_dir)
out_dir.mkdir(parents=True)
print(f"Created: {out_dir}")

# ── 2. Copy header files from include\ ────────────────────────────────────────
for h in (project_root / "include").glob("*.h"):
    shutil.copy(h, out_dir / h.name)
    print(f"  Copied header: {h.name}")

# ── 3. Copy non-main .cpp files from src\ ─────────────────────────────────────
for cpp in (project_root / "src").glob("*.cpp"):
    if cpp.name != "main.cpp":
        shutil.copy(cpp, out_dir / cpp.name)
        print(f"  Copied source: {cpp.name}")

# ── 4. Convert main.cpp → <SketchName>.ino ────────────────────────────────────
# Arduino implicitly includes Arduino.h, so strip the PlatformIO-specific include.
main_cpp = (project_root / "src" / "main.cpp").read_text(encoding="utf-8")
ino_content = re.sub(r'^\s*#include\s+<Arduino\.h>\r?\n', '', main_cpp, flags=re.MULTILINE)
ino_file = out_dir / f"{SKETCH_NAME}.ino"
ino_file.write_text(ino_content, encoding="utf-8")
print(f"  Created sketch: {ino_file.name}")

# ── 5. Ensure arduino_project/ is in .gitignore ───────────────────────────────
gitignore = project_root / ".gitignore"
entry = "arduino_project/"
existing = gitignore.read_text(encoding="utf-8") if gitignore.exists() else ""
if entry not in existing.splitlines():
    with gitignore.open("a", encoding="utf-8") as f:
        f.write(f"\n{entry}\n")
    print(f"Added '{entry}' to .gitignore")
else:
    print(f"'{entry}' already in .gitignore")

print(f"\nDone. Open {ino_file} in the Arduino IDE.")
