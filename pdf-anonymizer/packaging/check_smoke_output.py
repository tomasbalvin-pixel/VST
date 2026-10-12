"""Verify the smoke-test output of the built application (run from source tree)."""

import os
import sys

import pymupdf

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from anonymizer import ocr  # noqa: E402

out = sys.argv[1]
pdf = os.path.join(out, "document_001.pdf")
assert os.path.exists(pdf), f"missing {pdf}: {os.listdir(out) if os.path.isdir(out) else 'no output dir'}"
doc = pymupdf.open(pdf)
assert doc.page_count == 2
secrets = ("Novák", "jan.novak", "777 123 456")
for secret in secrets:
    assert secret not in doc[0].get_text(), f"{secret!r} still in text layer"
seen = ocr.page_text(doc[1])
for secret in secrets:
    assert secret not in seen, f"{secret!r} still visible on the scanned page"
txt = open(os.path.join(out, "document_001.txt"), encoding="utf-8").read()
assert txt.count("[PERSON_1]") == 2, txt  # the scanned page was read by OCR in the built app
assert os.path.exists(os.path.join(out, "anonymization_report.csv"))
print("Smoke test passed")
