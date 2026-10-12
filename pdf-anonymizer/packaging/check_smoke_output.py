"""Verify the smoke-test output of the built application."""

import os
import sys

import pymupdf

out = sys.argv[1]
pdf = os.path.join(out, "document_001.pdf")
assert os.path.exists(pdf), f"missing {pdf}: {os.listdir(out) if os.path.isdir(out) else 'no output dir'}"
text = pymupdf.open(pdf)[0].get_text()
for secret in ("Novák", "jan.novak", "777 123 456"):
    assert secret not in text, f"{secret!r} still present"
assert os.path.exists(os.path.join(out, "anonymization_report.csv"))
print("Smoke test passed")
