"""Offline OCR for scanned pages.

MuPDF contains the Tesseract OCR engine; only the language data files
(``*.traineddata``) are needed. They are bundled with the application, so OCR
runs locally like everything else.
"""

from __future__ import annotations

import os
import sys


LANGUAGES = ("ces", "slk", "eng", "deu")
DPI = 300


def _ascii_path(path: str) -> str:
    # Tesseract cannot open non-ASCII paths on Windows (C:\Users\Tomáš\...); the
    # 8.3 short form of the same folder is plain ASCII.
    if sys.platform != "win32" or path.isascii():
        return path
    import ctypes
    buf = ctypes.create_unicode_buffer(1024)
    if ctypes.windll.kernel32.GetShortPathNameW(path, buf, len(buf)):
        return buf.value
    return path


def tessdata_dir() -> str | None:
    candidates = [os.environ.get("PDF_ANONYMIZER_TESSDATA", "")]
    if getattr(sys, "frozen", False):
        candidates.append(os.path.join(getattr(sys, "_MEIPASS", os.path.dirname(sys.executable)), "tessdata"))
    candidates.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tessdata"))
    for c in candidates:
        if c and os.path.isfile(os.path.join(c, "eng.traineddata")):
            return _ascii_path(c)
    return None


def languages(folder: str | None = None) -> str:
    folder = folder or tessdata_dir()
    if not folder:
        return ""
    return "+".join(lang for lang in LANGUAGES if os.path.isfile(os.path.join(folder, f"{lang}.traineddata")))


def available() -> bool:
    return bool(languages())


def page_words(page, flags: int) -> list:
    """Word tuples (same format as page.get_text("words")) read by OCR from the page image."""
    folder = tessdata_dir()
    if not folder:
        raise RuntimeError("OCR language data not found")
    textpage = page.get_textpage_ocr(flags=flags, language=languages(folder), dpi=DPI, full=True,
                                     tessdata=folder)
    return page.get_text("words", textpage=textpage, flags=flags)


def page_text(page) -> str:
    folder = tessdata_dir()
    textpage = page.get_textpage_ocr(language=languages(folder), dpi=DPI, full=True, tessdata=folder)
    return page.get_text("text", textpage=textpage)
