"""PDF processing: text extraction, redaction, sanitising and verification.

Redaction is destructive. The characters under each redaction box are removed
from the page content stream (and image pixels under the box are blanked), not
merely covered, and the file is rewritten with garbage collection so no old
object survives. Metadata, bookmarks, attachments, JavaScript, links, form
fields and annotations are removed as well.
"""

from __future__ import annotations

import csv
import os
import re
from collections import Counter
from dataclasses import dataclass, field

import pymupdf

from . import names, ocr
from .detectors import CATEGORIES, DetectorConfig, Finding, detect
from .textutil import fold

_WORD_FLAGS = pymupdf.TEXTFLAGS_WORDS
_UNCLIPPED_FLAGS = pymupdf.TEXTFLAGS_WORDS & ~pymupdf.TEXT_MEDIABOX_CLIP


@dataclass
class Options:
    detector: DetectorConfig = field(default_factory=DetectorConfig)
    style: str = "label"          # "label": black box with PERSON_1 etc.; "black": plain box
    remove_images: bool = False   # remove every image (photos, signatures, logos, scans)
    rename_files: bool = True     # document_001.pdf instead of the original file name
    write_pdf: bool = True
    write_text: bool = True


class Pseudonyms:
    """Stable replacement labels: the same value always gets the same label."""

    def __init__(self):
        self._labels: dict = {}
        self._counts: Counter = Counter()

    def label(self, category: str, key: str) -> str:
        if key not in self._labels:
            self._counts[category] += 1
            self._labels[key] = f"{category}_{self._counts[category]}"
        return self._labels[key]


@dataclass
class PageData:
    number: int
    words: list            # PyMuPDF word tuples
    cuts: list             # rectangle that removes the word's glyphs
    boxes: list            # rectangle drawn as the visible black box
    text: str
    word_spans: list       # (start, end) of each word in text
    findings: list = field(default_factory=list)
    word_owner: list = field(default_factory=list)  # finding index per word or None


@dataclass
class DocumentAnalysis:
    path: str
    pages: list = field(default_factory=list)
    warnings: list = field(default_factory=list)
    scanned_pages: list = field(default_factory=list)  # pages with images but no readable text
    ocr_pages: list = field(default_factory=list)      # pages read with OCR
    error: str | None = None

    def items(self):
        """(page number, category, original text) for every finding."""
        for p in self.pages:
            for f in p.findings:
                yield p.number, f.category, p.text[f.start:f.end]


@dataclass
class DocumentResult:
    source: str
    output_pdf: str | None = None
    output_text: str | None = None
    pages: int = 0
    counts: Counter = field(default_factory=Counter)
    warnings: list = field(default_factory=list)
    # Detected values still found in the output. Shown on screen, never written to disk.
    residues: list = field(default_factory=list)
    ocr_pages: list = field(default_factory=list)
    error: str | None = None

    @property
    def status(self) -> str:
        if self.error:
            return "FAILED"
        if self.residues or self.warnings:
            return "CHECK"
        return "OK"


# --------------------------------------------------------------------------- #
# Analysis
# --------------------------------------------------------------------------- #


def _normalise(s: str) -> str:
    return re.sub(r"[\W_]+", "", fold(s))


def _key_for(f: Finding, text: str) -> str:
    return f.key or f"{f.category}:{_normalise(text[f.start:f.end])}"


def _redaction_boxes(page, words):
    """Per word: (cut rectangle, visible box).

    MuPDF removes every glyph whose font box (ascender to descender) touches a
    redaction rectangle, and those boxes overlap between tightly spaced lines.
    Cutting with a thin band through the middle of a horizontal line removes all
    glyphs of the word without touching the lines above and below. Rotated text
    and scanned pages with an OCR layer use the full box: on scans it must also
    blank the image pixels of the word.
    """
    full = [pymupdf.Rect(w[:4]) for w in words]
    page_area = abs(page.rect)
    if any(abs(pymupdf.Rect(i["bbox"]) & page.rect) > 0.5 * page_area for i in page.get_image_info()):
        return full, full
    blocks = page.get_text("dict", flags=_WORD_FLAGS)["blocks"]
    cuts, boxes = [], []
    for w, f in zip(words, full):
        try:
            direction = blocks[w[5]]["lines"][w[6]]["dir"]
        except (IndexError, KeyError):
            direction = None
        if direction is None or abs(direction[1]) > 1e-3 or direction[0] <= 0:
            cuts.append(f)
            boxes.append(f)
            continue
        cy, half = (f.y0 + f.y1) / 2, max(0.5, f.height * 0.12)
        cuts.append(pymupdf.Rect(f.x0, cy - half, f.x1, cy + half))
        inset = f.height * 0.12
        boxes.append(pymupdf.Rect(f.x0, f.y0 + inset, f.x1, f.y1 - inset))
    return cuts, boxes


def _page_data(page, number: int, ocr_words=None) -> PageData:
    words = page.get_text("words", flags=_WORD_FLAGS) if ocr_words is None else ocr_words
    parts, spans, pos = [], [], 0
    prev_line = None
    for w in words:
        line_id = (w[5], w[6])
        if prev_line is not None:
            sep = " " if line_id == prev_line else "\n"
            parts.append(sep)
            pos += 1
        spans.append((pos, pos + len(w[4])))
        parts.append(w[4])
        pos += len(w[4])
        prev_line = line_id
    if ocr_words is not None:
        # Full word boxes: the redaction must blank the image pixels of each word.
        cuts = boxes = [pymupdf.Rect(w[:4]) for w in words]
    else:
        cuts, boxes = _redaction_boxes(page, words) if words else ([], [])
    return PageData(number, words, cuts, boxes, "".join(parts), spans)


def _propagation_terms(pages) -> list:
    """Surnames found anywhere in the document, to catch later lone mentions."""
    terms = {}
    for p in pages:
        for f in p.findings:
            if f.category != "PERSON":
                continue
            for tok in re.findall(r"[^\W\d_][\w'’-]*", p.text[f.start:f.end]):
                k = fold(tok)
                if (len(tok) >= 3 and tok[0].isupper() and names.first_name_status(k) is None
                        and k not in names.TITLES and k not in names.NAME_STOPWORDS):
                    terms.setdefault(k, (tok, f.key))
    return list(terms.values())


def _assign_words(page: PageData) -> None:
    owner = [None] * len(page.words)
    for fi, f in enumerate(page.findings):
        for wi, (s, e) in enumerate(page.word_spans):
            if s < f.end and f.start < e and owner[wi] is None:
                owner[wi] = fi
    page.word_owner = owner


def _known_key(f: Finding, text: str, extra) -> str | None:
    """Pseudonym key of a propagated surname if this PERSON finding is just that surname."""
    keys = set()
    for tok in re.findall(r"[^\W\d_][\w'’-]*", text[f.start:f.end]):
        k = fold(tok)
        if k in names.TITLES or names.first_name_status(k) is not None:
            continue
        match = [key for term, key in extra if _same_word(k, fold(term))]
        if not match:
            return None
        keys.add(match[0])
    return keys.pop() if len(keys) == 1 else None


def _same_word(word: str, term: str) -> bool:
    if word == term:
        return True
    stem = term[:-1] if term[-1:] in "aeiouy" else term
    return len(stem) >= 3 and word.startswith(stem) and len(word) - len(stem) <= 4


def _needs_ocr(page, words) -> bool:
    """Scanned page (image, little or no text) or a text layer that does not decode."""
    images = page.get_image_info()
    if not words:
        return bool(images)
    page_area = abs(page.rect)
    if len(words) < 10 and any(abs(pymupdf.Rect(i["bbox"]) & page.rect) > 0.5 * page_area for i in images):
        return True
    # Fonts without a Unicode mapping extract as U+FFFD or control characters.
    garbled = sum(1 for w in words if "\ufffd" in w[4] or not any(c.isalnum() for c in w[4]))
    return garbled > 0.3 * len(words)


def analyze_open_document(doc, path: str, cfg: DetectorConfig, warnings=(), log=None) -> DocumentAnalysis:
    result = DocumentAnalysis(path, warnings=list(warnings))
    for i, page in enumerate(doc):
        pdata = _page_data(page, i + 1)
        if _needs_ocr(page, pdata.words):
            if ocr.available():
                if log:
                    log(f"    reading page {i + 1}/{doc.page_count} with OCR")
                try:
                    pdata = _page_data(page, i + 1, ocr_words=ocr.page_words(page, _WORD_FLAGS))
                    result.ocr_pages.append(i + 1)
                except Exception as exc:  # noqa: BLE001 - reported to the user
                    result.warnings.append(f"Page {i + 1}: OCR failed ({exc}).")
                    result.scanned_pages.append(i + 1)
            else:
                result.scanned_pages.append(i + 1)
        elif len(page.get_text("words", flags=_UNCLIPPED_FLAGS)) > len(pdata.words):
            result.warnings.append(f"Page {i + 1} contains text outside the visible page area.")
        result.pages.append(pdata)

    # Pass 1: direct detection.
    for p in result.pages:
        p.findings = detect(p.text, cfg)
        for f in p.findings:
            f.key = _key_for(f, p.text)
    # Pass 2: repeat with surnames found in pass 1, so "Novák" alone on page 5 is
    # caught after "Ing. Jan Novák" appeared on page 1.
    extra = _propagation_terms(result.pages)
    if extra and "PERSON" in cfg.categories:
        for p in result.pages:
            p.findings = detect(p.text, cfg, extra_terms=extra)
            for f in p.findings:
                if f.category == "PERSON" and not f.key:
                    f.key = _known_key(f, p.text, extra)
                f.key = _key_for(f, p.text)
    for p in result.pages:
        _assign_words(p)
    return result


def open_pdf(path: str):
    """Open and flatten a PDF. Returns (document, warnings)."""
    doc = pymupdf.open(path)
    if doc.needs_pass:
        doc.close()
        raise ValueError("The PDF is password protected. Remove the password first.")
    if not doc.is_pdf:
        doc.close()
        raise ValueError("Not a PDF file.")
    fields = []
    for page in doc:
        for w in page.widgets() or []:
            if isinstance(w.field_value, str) and w.field_value.strip() not in ("", "Off"):
                fields.append((page.number, w.field_value))
    # Burn form-field values and comments into the page so they are read and
    # redacted like normal text.
    doc.bake(annots=True, widgets=True)
    warnings = []
    for pno, value in fields:
        if _normalise(value) not in _normalise(doc[pno].get_text("text")):
            warnings.append(f"Page {pno + 1}: a form field value could not be read after flattening; "
                            "check that page manually.")
    return doc, warnings


def analyze(path: str, cfg: DetectorConfig, log=None) -> DocumentAnalysis:
    try:
        doc, warnings = open_pdf(path)
    except Exception as exc:  # noqa: BLE001 - reported to the user
        return DocumentAnalysis(path, error=str(exc))
    try:
        result = analyze_open_document(doc, path, cfg, warnings, log)
        result.warnings.extend(_scan_warnings(result.scanned_pages, removed=False))
        return result
    finally:
        doc.close()


def _scan_warnings(pages, removed: bool) -> list:
    if not pages:
        return []
    listed = ", ".join(map(str, pages))
    if removed:
        return [f"Page(s) {listed} had no text layer (scanned images); the images were removed, "
                "so these pages are blank."]
    return [f"Page(s) {listed} have no readable text (scanned images) and OCR is not available. "
            "Their content cannot be read or redacted; enable 'Remove all images' or check them manually."]


# --------------------------------------------------------------------------- #
# Output
# --------------------------------------------------------------------------- #


def _runs(page: PageData):
    """Group consecutive words of the same finding on the same line."""
    run = None
    for wi, owner in enumerate(page.word_owner):
        w = page.words[wi]
        line_id = (w[5], w[6])
        if owner is None:
            if run:
                yield run
                run = None
            continue
        if run and run["owner"] == owner and run["line"] == line_id:
            run["words"].append(wi)
        else:
            if run:
                yield run
            run = {"owner": owner, "line": line_id, "words": [wi]}
    if run:
        yield run


def _anonymised_text(analysis: DocumentAnalysis, labels) -> str:
    out = []
    for p in analysis.pages:
        out.append(f"--- Page {p.number} ---")
        lines: list = []
        current_line, buf = None, []
        wi = 0
        while wi < len(p.words):
            w = p.words[wi]
            line_id = (w[5], w[6])
            if line_id != current_line:
                if buf:
                    lines.append(" ".join(buf))
                buf, current_line = [], line_id
            owner = p.word_owner[wi]
            if owner is None:
                buf.append(w[4])
                wi += 1
                continue
            f = p.findings[owner]
            first = wi
            while wi + 1 < len(p.words) and p.word_owner[wi + 1] == owner and \
                    (p.words[wi + 1][5], p.words[wi + 1][6]) == line_id:
                wi += 1
            # Keep punctuation that lies outside the detected value: "(Novák)," -> "([PERSON_1]),"
            s0, _ = p.word_spans[first]
            _, e1 = p.word_spans[wi]
            prefix = p.text[s0:max(s0, f.start)]
            suffix = p.text[min(e1, f.end):e1]
            prefix = prefix if not re.search(r"\w", prefix) else ""
            suffix = suffix if not re.search(r"\w", suffix) else ""
            buf.append(f"{prefix}[{labels[(p.number, owner)]}]{suffix}")
            wi += 1
        if buf:
            lines.append(" ".join(buf))
        out.extend(lines)
        out.append("")
    return "\n".join(out)


def _remove_images(page) -> int:
    infos = page.get_image_info()
    for info in infos:
        page.add_redact_annot(pymupdf.Rect(info["bbox"]), fill=False, cross_out=False)
    if infos:
        page.apply_redactions(images=pymupdf.PDF_REDACT_IMAGE_REMOVE,
                              graphics=pymupdf.PDF_REDACT_LINE_ART_NONE,
                              text=pymupdf.PDF_REDACT_TEXT_NONE)
    return len(infos)


def _redact_page(page, pdata: PageData, labels, style: str) -> None:
    visible = []
    for run in _runs(pdata):
        cut, box = pymupdf.Rect(), pymupdf.Rect()
        for wi in run["words"]:
            cut |= pdata.cuts[wi]
            box |= pdata.boxes[wi]
        page.add_redact_annot(cut, fill=False, cross_out=False)
        visible.append((box, labels[(pdata.number, run["owner"])] if style == "label" else None))
    if not visible:
        return
    page.apply_redactions(images=pymupdf.PDF_REDACT_IMAGE_PIXELS,
                          graphics=pymupdf.PDF_REDACT_LINE_ART_NONE,
                          text=pymupdf.PDF_REDACT_TEXT_REMOVE)
    for box, label in visible:
        page.draw_rect(box, color=None, fill=(0, 0, 0), overlay=True)
        if label:
            size = min(8.0, box.height * 0.7)
            size = min(size, size * (box.width - 2) / max(1.0, pymupdf.get_text_length(label, "helv", size)))
            if size < 3.0:
                continue
            page.insert_text(pymupdf.Point(box.x0 + 1, (box.y0 + box.y1) / 2 + size * 0.35), label,
                             fontname="helv", fontsize=size, color=(1, 1, 1))
    page.clean_contents()


def _sanitise(doc) -> None:
    doc.set_toc([])
    doc.scrub(attached_files=True, clean_pages=True, embedded_files=True, hidden_text=False,
              javascript=True, metadata=True, redactions=False, remove_links=True,
              reset_fields=True, reset_responses=True, thumbnails=True, xml_metadata=True)
    for name in list(doc.embfile_names()):
        doc.embfile_del(name)
    for page in doc:
        for annot in list(page.annots() or []):
            page.delete_annot(annot)
        for widget in list(page.widgets() or []):
            page.delete_widget(widget)
    doc.set_metadata({})
    doc.del_xml_metadata()


def _verify(path: str, analysis: DocumentAnalysis) -> list:
    """Re-read the written file and report detected values that are still present.

    Pages that were read with OCR are OCR'd again, so the check covers what is
    visible in the page image, not only the text layer.
    """
    residues = []
    with pymupdf.open(path) as out:
        for p in analysis.pages:
            if p.number > out.page_count or not p.findings:
                continue
            page = out[p.number - 1]
            page_text = page.get_text("text")
            if p.number in analysis.ocr_pages:
                page_text += "\n" + ocr.page_text(page)
            words = {fold(w) for w in re.findall(r"\w+", page_text)}
            compact = _normalise(page_text)
            for f in p.findings:
                original = p.text[f.start:f.end]
                tokens = [fold(t) for t in re.findall(r"\w+", original)]
                if any(ch.isdigit() for ch in original):
                    norm = _normalise(original)
                    leaked = len(norm) >= 5 and norm in compact
                else:
                    leaked = any(len(t) >= 3 and t in words and names.first_name_status(t) is None
                                 and t not in names.TITLES for t in tokens)
                if leaked:
                    residues.append((p.number, f.category, original))
    return residues


def anonymize(path: str, out_dir: str, stem: str, options: Options, pseudonyms: Pseudonyms,
              log=None) -> DocumentResult:
    result = DocumentResult(path)
    src = os.path.normcase(os.path.abspath(path))
    while src in {os.path.normcase(os.path.abspath(os.path.join(out_dir, stem + ext))) for ext in (".pdf", ".txt")}:
        stem += "_anonymized"  # never overwrite an original
    try:
        doc, warnings = open_pdf(path)
    except Exception as exc:  # noqa: BLE001 - reported to the user
        result.error = str(exc).replace(path, "(input file)")
        return result
    try:
        analysis = analyze_open_document(doc, path, options.detector, warnings, log)
        result.pages = len(analysis.pages)
        result.ocr_pages = analysis.ocr_pages
        result.warnings.extend(analysis.warnings)
        result.warnings.extend(_scan_warnings(analysis.scanned_pages,
                                              removed=options.remove_images and options.write_pdf))

        labels = {}
        for p in analysis.pages:
            for fi, f in enumerate(p.findings):
                labels[(p.number, fi)] = pseudonyms.label(f.category, f.key)
                result.counts[f.category] += 1
        if not any(p.findings for p in analysis.pages) and not analysis.scanned_pages:
            result.warnings.append("No personal data was detected in this file. Check it, and add any "
                                   "names it contains under 'Your terms'.")

        if options.write_text:
            result.output_text = os.path.join(out_dir, stem + ".txt")
            with open(result.output_text, "w", encoding="utf-8") as fh:
                fh.write(_anonymised_text(analysis, labels))

        if options.write_pdf:
            for page, pdata in zip(doc, analysis.pages):
                if options.remove_images:
                    result.counts["IMAGE"] += _remove_images(page)
                _redact_page(page, pdata, labels, options.style)
            _sanitise(doc)
            result.output_pdf = os.path.join(out_dir, stem + ".pdf")
            doc.save(result.output_pdf, garbage=4, deflate=True, clean=True,
                     preserve_metadata=0, encryption=pymupdf.PDF_ENCRYPT_NONE)
            result.residues = _verify(result.output_pdf, analysis)
            if result.residues:
                result.warnings.append(
                    f"{len(result.residues)} detected value(s) may still be readable in the output; "
                    "review the PDF before using it.")
    except Exception as exc:  # noqa: BLE001 - reported to the user
        result.error = f"{type(exc).__name__}: {exc}"
    finally:
        doc.close()
    return result


def collect_pdfs(paths) -> list:
    out = []
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                out.extend(os.path.join(root, f) for f in sorted(files) if f.lower().endswith(".pdf"))
        elif p.lower().endswith(".pdf"):
            out.append(p)
    return out


def output_stems(paths, rename: bool) -> list:
    stems, seen = [], Counter()
    for i, p in enumerate(paths, 1):
        base = f"document_{i:03d}" if rename else os.path.splitext(os.path.basename(p))[0] + "_anonymized"
        seen[base] += 1
        stems.append(base if seen[base] == 1 else f"{base}_{seen[base]}")
    return stems


def run_batch(paths, out_dir: str, options: Options, progress=None, cancelled=lambda: False, log=None) -> list:
    """Anonymise ``paths`` into ``out_dir`` and write a report without any original values."""
    os.makedirs(out_dir, exist_ok=True)
    pseudonyms = Pseudonyms()
    results = []
    for i, (path, stem) in enumerate(zip(paths, output_stems(paths, options.rename_files))):
        if cancelled():
            break
        if progress:
            progress(i, len(paths), path)
        results.append(anonymize(path, out_dir, stem, options, pseudonyms, log))
    if progress:
        progress(len(results), len(paths), None)
    write_report(results, out_dir, options)
    return results


def write_report(results, out_dir: str, options: Options) -> str:
    path = os.path.join(out_dir, "anonymization_report.csv")
    cats = [c for c in CATEGORIES if c in options.detector.categories or c == "CUSTOM"]
    if options.remove_images:
        cats.append("IMAGE")
    with open(path, "w", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh)
        w.writerow(["output file", "status", "pages"] + cats + ["warnings"])
        for r in results:
            name = os.path.basename(r.output_pdf or r.output_text or "") or "(not written)"
            if not options.rename_files:
                name = name or os.path.basename(r.source)
            w.writerow([name, r.status, r.pages] + [r.counts.get(c, 0) for c in cats]
                       + [" | ".join(([r.error] if r.error else []) + r.warnings)])
    return path
