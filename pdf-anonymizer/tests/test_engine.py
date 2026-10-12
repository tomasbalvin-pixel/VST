import csv
import os
import subprocess
import sys

import pymupdf
import pytest

from anonymizer.detectors import DetectorConfig
from anonymizer.engine import Options, analyze, run_batch

SECRETS = ["Novák", "Svobodov", "777 123 456", "800312/1236", "Vodičkova", "jan.novak@example.cz"]

BODY = """Jméno: Jan Novák<br>
Rodné číslo: 800312/1236<br>
Adresa: Vodičkova 699/30, 110 00 Praha 1<br>
Tel.: +420 777 123 456, e-mail: jan.novak@example.cz<br>
Dne 5. 6. 2024 se pan Novák dohodl s paní Svobodovou.<br>
Smlouva je uzavřena na dobu neurčitou."""


def make_pdf(path, body=BODY, pages=1):
    doc = pymupdf.open()
    for _ in range(pages):
        page = doc.new_page()
        page.insert_htmlbox(pymupdf.Rect(50, 50, 550, 400), body)
    doc.set_metadata({"author": "Jan Novák", "title": "Smlouva Novák", "subject": "Novák"})
    doc.set_toc([[1, "Smlouva Jan Novák", 1]])
    doc.embfile_add("notes.txt", b"Jan Novak private notes")
    doc[0].insert_link({"kind": pymupdf.LINK_URI, "from": pymupdf.Rect(50, 50, 200, 70),
                        "uri": "mailto:jan.novak@example.cz"})
    doc.save(path)
    doc.close()
    return path


@pytest.fixture
def pdf(tmp_path):
    return make_pdf(str(tmp_path / "Smlouva_Jan_Novak.pdf"))


def test_redacts_text_physically(pdf, tmp_path):
    out = tmp_path / "out"
    [r] = run_batch([pdf], str(out), Options())
    assert r.status == "OK", (r.warnings, r.error)
    assert os.path.basename(r.output_pdf) == "document_001.pdf"

    doc = pymupdf.open(r.output_pdf)
    text = doc[0].get_text()
    for s in SECRETS:
        assert s not in text
    assert "Smlouva je uzavřena na dobu neurčitou." in text   # ordinary text survives
    assert "Dne 5. 6. 2024 se pan" in text and "dohodl s paní" in text  # neighbours intact
    assert "PERSON_1" in text                                 # pseudonym label drawn

    raw = open(r.output_pdf, "rb").read()
    assert b"jan.novak" not in raw and b"Novak" not in raw
    assert not any(v for k, v in doc.metadata.items() if k not in ("format", "encryption"))
    assert doc.get_toc() == []
    assert doc.embfile_count() == 0
    assert doc[0].get_links() == []


def test_text_output_and_consistent_labels(pdf, tmp_path):
    [r] = run_batch([pdf], str(tmp_path / "out"), Options())
    txt = open(r.output_text, encoding="utf-8").read()
    for s in SECRETS:
        assert s not in txt
    assert "Jméno: [PERSON_1]" in txt
    assert "pan [PERSON_1] dohodl" in txt        # surname alone maps to the same person
    assert "[PERSON_2]." in txt                  # punctuation outside the value is kept


def test_labels_consistent_across_files(tmp_path):
    a = make_pdf(str(tmp_path / "a.pdf"))
    b = make_pdf(str(tmp_path / "b.pdf"))
    ra, rb = run_batch([a, b], str(tmp_path / "out"), Options())
    ta = open(ra.output_text, encoding="utf-8").read()
    tb = open(rb.output_text, encoding="utf-8").read()
    assert ta == tb


def test_original_untouched_and_never_overwritten(tmp_path):
    src = make_pdf(str(tmp_path / "document_001.pdf"))
    before = open(src, "rb").read()
    [r] = run_batch([src], str(tmp_path), Options())
    assert open(src, "rb").read() == before
    assert os.path.abspath(r.output_pdf) != os.path.abspath(src)


def test_report_contains_no_original_values(pdf, tmp_path):
    out = tmp_path / "out"
    run_batch([pdf], str(out), Options())
    report = open(out / "anonymization_report.csv", encoding="utf-8-sig").read()
    for s in SECRETS + ["Smlouva_Jan_Novak"]:
        assert s not in report
    rows = list(csv.DictReader(report.splitlines()))
    assert rows[0]["status"] == "OK" and int(rows[0]["PERSON"]) >= 2


def test_custom_and_allow_terms(tmp_path):
    src = make_pdf(str(tmp_path / "x.pdf"), body="Projekt Fénix pro Acme. Kontakt: recepce Praha.")
    det = DetectorConfig(custom_terms=["Fénix", "Acme"], allow_terms=["Praha"])
    [r] = run_batch([src], str(tmp_path / "out"), Options(detector=det))
    text = pymupdf.open(r.output_pdf)[0].get_text()
    assert "Fénix" not in text and "Acme" not in text and "Praha" in text


def test_scanned_page_is_flagged(tmp_path):
    pix = pymupdf.Pixmap(pymupdf.csRGB, pymupdf.IRect(0, 0, 20, 20), False)
    doc = pymupdf.open()
    page = doc.new_page()
    page.insert_image(page.rect, pixmap=pix)
    path = str(tmp_path / "scan.pdf")
    doc.save(path)
    [r] = run_batch([path], str(tmp_path / "out"), Options())
    assert r.status == "CHECK" and "no text layer" in r.warnings[0]
    [r] = run_batch([path], str(tmp_path / "out2"), Options(remove_images=True))
    assert pymupdf.open(r.output_pdf)[0].get_image_info() == []


def test_form_fields_are_redacted(tmp_path):
    doc = pymupdf.open()
    page = doc.new_page()
    page.insert_text((50, 60), "Formular", fontsize=11)
    w = pymupdf.Widget()
    w.field_name, w.field_type = "jmeno", pymupdf.PDF_WIDGET_TYPE_TEXT
    w.rect, w.text_fontsize, w.field_value = pymupdf.Rect(50, 80, 300, 100), 11, "Eva Dvorakova"
    page.add_widget(w)
    path = str(tmp_path / "form.pdf")
    doc.save(path)
    [r] = run_batch([path], str(tmp_path / "out"), Options(detector=DetectorConfig(custom_terms=["Dvorakova"])))
    out = pymupdf.open(r.output_pdf)
    assert "Dvorakova" not in out[0].get_text()
    assert list(out[0].widgets()) == []


def test_password_protected_pdf_fails_cleanly(tmp_path):
    doc = pymupdf.open()
    doc.new_page().insert_text((50, 60), "secret")
    path = str(tmp_path / "locked.pdf")
    doc.save(path, encryption=pymupdf.PDF_ENCRYPT_AES_256, user_pw="pw", owner_pw="pw")
    [r] = run_batch([path], str(tmp_path / "out"), Options())
    assert r.status == "FAILED" and "password" in r.error
    assert analyze(path, DetectorConfig()).error


def test_offline_guard_blocks_network():
    code = ("from anonymizer import offline; offline.enforce(); import socket, urllib.request\n"
            "try:\n    urllib.request.urlopen('http://example.com', timeout=2)\nexcept OSError as e:\n"
            "    print('BLOCKED' if 'offline' in str(e) or 'disabled' in str(e) else 'OTHER', e)\n")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = subprocess.run([sys.executable, "-c", code], cwd=root, capture_output=True, text=True, timeout=60)
    assert "BLOCKED" in out.stdout, out.stdout + out.stderr
