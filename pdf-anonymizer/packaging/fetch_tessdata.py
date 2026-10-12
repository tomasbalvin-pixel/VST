"""Download the OCR language data bundled with the application (build time only).

    python packaging/fetch_tessdata.py

Files come from the tesseract-ocr/tessdata_fast 4.1.0 release (Apache-2.0) and
are checked against fixed SHA-256 hashes.
"""

import hashlib
import os
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(ROOT, "tessdata")
URL = "https://github.com/tesseract-ocr/tessdata_fast/raw/4.1.0/{}.traineddata"
FILES = {
    "ces": "934bcaf97ef3348413263331131c9fa7f55f30db333c711929c124fb635f7e1b",
    "slk": "fbcc400a9c74c6a13d922fcb1211b655d1b165387b675ed75cd2dbd756b974a5",
    "eng": "7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2",
    "deu": "19d219bbb6672c869d20a9636c6816a81eb9a71796cb93ebe0cb1530e2cdb22d",
}


def sha256(path: str) -> str:
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


def main() -> None:
    os.makedirs(DEST, exist_ok=True)
    for lang, digest in FILES.items():
        path = os.path.join(DEST, f"{lang}.traineddata")
        if os.path.exists(path) and sha256(path) == digest:
            print(f"{lang}: present")
            continue
        print(f"{lang}: downloading")
        urllib.request.urlretrieve(URL.format(lang), path)
        actual = sha256(path)
        if actual != digest:
            os.remove(path)
            raise SystemExit(f"{lang}: checksum mismatch ({actual})")
    print(f"OCR language data ready in {DEST}")


if __name__ == "__main__":
    main()
