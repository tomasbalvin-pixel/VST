"""Build the desktop application with PyInstaller.

    python packaging/build.py

Output: dist/PDF Anonymizer/ (Windows, Linux) or dist/PDF Anonymizer.app (macOS).
"""

import os
import sys

import PyInstaller.__main__

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from anonymizer import APP_NAME, __version__  # noqa: E402

args = [
    os.path.join(ROOT, "packaging", "launcher.py"),
    "--name", APP_NAME,
    "--windowed",
    "--onedir",
    "--noconfirm",
    "--clean",
    "--paths", ROOT,
    "--distpath", os.path.join(ROOT, "dist"),
    "--workpath", os.path.join(ROOT, "build"),
    "--specpath", os.path.join(ROOT, "build"),
    "--collect-submodules", "anonymizer",
    # Nothing in the program uses these; leaving them out keeps the bundle small.
    "--exclude-module", "numpy",
    "--exclude-module", "PIL",
    "--exclude-module", "matplotlib",
    "--exclude-module", "pandas",
    "--exclude-module", "lxml",
]
if sys.platform == "darwin":
    args += ["--osx-bundle-identifier", "local.pdf-anonymizer"]

print(f"Building {APP_NAME} {__version__}")
PyInstaller.__main__.run(args)
