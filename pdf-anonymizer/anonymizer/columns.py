"""Whole-column redaction in tables.

The user names a column by its heading ("Rodné číslo"). The column is located
from word positions, not from table ruling, so it works the same for ruled
tables, borderless tables and scanned pages read with OCR:

1. Find the heading words on the page. The heading row is every word on the
   same visual line; gaps between words split it into header cells.
2. The column spans from halfway to the previous header cell to halfway to the
   next one.
3. Every row below the heading is redacted inside that span, until the table
   ends: a large vertical gap, or a line of running text that crosses the
   column boundaries.
4. A table that runs to the bottom of the page continues on the next page
   with the same column positions, even if the heading is not repeated.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from statistics import median

import pymupdf

from .textutil import fold

if hasattr(pymupdf, "no_recommend_layout"):
    pymupdf.no_recommend_layout()  # find_tables() would otherwise print an advert to stdout

_TOKEN = re.compile(r"\w+")


@dataclass
class Layout:
    """Column positions of one table, carried to the next page."""
    bounds: list      # (left, right) per header cell: the column's territory
    spans: list       # (left, right) of each header cell's text
    target: int       # index of the column to redact
    rule: str
    continuation: bool = False  # applied on a following page without its heading
    pitch: float | None = None  # row spacing, carried to the next page


def label_prefix(rule: str) -> str:
    p = re.sub(r"[^A-Za-z0-9]+", "_", fold(rule)).strip("_").upper()
    return p or "COLUMN"


def _rows(words, idxs):
    """Group word indices into visual rows, top to bottom."""
    order = sorted(idxs, key=lambda i: (words[i][1] + words[i][3]) / 2)
    rows, current, cy = [], [], None
    for i in order:
        w = words[i]
        c, h = (w[1] + w[3]) / 2, w[3] - w[1]
        if current and abs(c - cy) > 0.5 * h:
            rows.append(current)
            current = []
        current.append(i)
        cy = sum((words[j][1] + words[j][3]) / 2 for j in current) / len(current)
    if current:
        rows.append(current)
    return rows


def word_space(words) -> float:
    """Typical gap between words on a line on this page (font, OCR and generator dependent)."""
    gaps = sorted(b[0] - a[2] for a, b in zip(words, words[1:])
                  if (a[5], a[6]) == (b[5], b[6]) and 0 < b[0] - a[2] < 3 * (a[3] - a[1]))
    height = median(w[3] - w[1] for w in words) if words else 12.0
    # Lower quartile: a table row is often one "line", so column gaps are mixed in.
    # A word space is never much more than 30 % of the text height.
    estimate = gaps[len(gaps) // 4] if len(gaps) >= 5 else 0.25 * height
    return min(estimate, 0.3 * height)


def _segments(words, row, space):
    """Split a row into runs of words separated by no more than a word space."""
    row = sorted(row, key=lambda i: words[i][0])
    cells = [[row[0]]]
    for i in row[1:]:
        prev = words[cells[-1][-1]]
        if words[i][0] - prev[2] > 1.7 * space:
            cells.append([i])
        else:
            cells[-1].append(i)
    return cells


def _find_headings(words, rule):
    """Word-index lists where the rule's words appear consecutively and compactly."""
    want = _TOKEN.findall(fold(rule))
    if not want:
        return []
    stream = [(wi, t) for wi, w in enumerate(words) for t in _TOKEN.findall(fold(w[4]))]
    hits = []
    for s in range(len(stream) - len(want) + 1):
        if [t for _, t in stream[s:s + len(want)]] != want:
            continue
        idxs = sorted({wi for wi, _ in stream[s:s + len(want)]})
        h = max(words[i][3] - words[i][1] for i in idxs)
        y0, y1 = min(words[i][1] for i in idxs), max(words[i][3] for i in idxs)
        if y1 - y0 <= 3.2 * h:  # a heading may wrap onto two lines, not more
            hits.append(idxs)
    return hits


def _layout_for(words, heading, page_width, rule, space):
    h = max(words[i][3] - words[i][1] for i in heading)
    y0, y1 = min(words[i][1] for i in heading), max(words[i][3] for i in heading)
    # Same visual row: the word's centre lies within the heading's extent. (Word boxes
    # of neighbouring rows often overlap, so box overlap would pull in data rows.)
    row = [i for i, w in enumerate(words) if y0 - 0.2 * h <= (w[1] + w[3]) / 2 <= y1 + 0.2 * h]
    cells = _segments(words, row, space)
    # The searched heading is one cell even if its words are spaced widely.
    hit = [k for k, c in enumerate(cells) if set(heading) & set(c)]
    if not hit:
        return None, y1
    cells[hit[0]:hit[-1] + 1] = [[i for c in cells[hit[0]:hit[-1] + 1] for i in c]]
    target = hit[0]
    if len(cells) < 2:
        return None, y1
    cell_tokens = sum(len(_TOKEN.findall(words[i][4])) for i in cells[target])
    if cell_tokens > len(_TOKEN.findall(rule)) + 4:  # the words sit inside running text
        return None, y1
    spans = [(min(words[i][0] for i in c), max(words[i][2] for i in c)) for c in cells]
    bounds = []
    for k, (a, b) in enumerate(spans):
        left = 0.0 if k == 0 else (spans[k - 1][1] + a) / 2
        right = page_width if k == len(spans) - 1 else (b + spans[k + 1][0]) / 2
        bounds.append((left, right))
    return Layout(bounds, spans, target, rule), max(words[i][3] for i in row) - 0.3 * h


def _scan_rows(words, layout, start_y, prev_center, page_height, space, table_bottom=None):
    """Redact the target column below start_y. Returns (row groups, continues_on_next_page)."""
    below = [i for i, w in enumerate(words) if (w[1] + w[3]) / 2 > start_y]
    groups, pitches, started = [], [], False
    if layout.pitch:
        pitches.append(layout.pitch)
    last_center = None
    for row in _rows(words, below):
        center = sum((words[i][1] + words[i][3]) / 2 for i in row) / len(row)
        if table_bottom is not None and center > table_bottom:
            return groups, False
        word_cols = {i: _column_of(words, [i], layout) for i in row}
        cols = set(word_cols.values())
        # Running text: one run of words reaching under several headings. A long cell
        # value may touch its neighbour, so tables with 3+ columns need three.
        need = min(3, len(layout.spans))
        crossing = any(sum(1 for a, b in layout.spans
                           if min(words[seg[-1]][2], b) - max(words[seg[0]][0], a) > 2) >= need
                       for seg in _segments(words, row, space))
        if not started:
            if crossing or (layout.continuation and len(cols) < 2):
                if layout.continuation and table_bottom is None:
                    continue  # letterhead above the continued table
                return groups, False
            started = True
        elif prev_center is not None:
            d = center - prev_center
            ref = median(pitches) if pitches else d
            # Running text crosses column boundaries; a large gap ends the table;
            # a lone single-column line spaced like a new paragraph is not a row.
            if crossing or d > 2.0 * ref or (len(cols) == 1 and len(layout.bounds) >= 3 and d > 1.1 * ref):
                layout.pitch = median(pitches) if pitches else layout.pitch
                return groups, False
            pitches.append(d)
        prev_center = last_center = center
        cell = sorted((i for i, k in word_cols.items() if k == layout.target), key=lambda i: words[i][0])
        if cell:
            groups.append(sorted(cell, key=lambda i: words[i][0]))
    layout.pitch = median(pitches) if pitches else layout.pitch
    return groups, last_center is not None and last_center > 0.75 * page_height


def _column_of(words, seg, layout) -> int:
    center = (words[seg[0]][0] + words[seg[-1]][2]) / 2
    for k, (a, b) in enumerate(layout.bounds):
        if a <= center < b:
            return k
    return len(layout.bounds) - 1


def _ruled_tables(page):
    try:
        return [t.bbox for t in page.find_tables().tables]
    except Exception:  # noqa: BLE001 - table detection is only a refinement
        return []


def column_groups(page, words, rules, carried):
    """Word groups to redact on one page.

    Returns ([(rule, [word indices of one cell])...], layouts to carry to the next page).
    """
    out, carry = [], []
    width, height = page.rect.width, page.rect.height
    space = word_space(words)
    tables = None
    for rule in rules:
        found_heading = False
        for heading in _find_headings(words, rule):
            layout, header_bottom = _layout_for(words, heading, width, rule, space)
            if not layout:
                continue
            found_heading = True
            if tables is None:
                tables = _ruled_tables(page)
            hx = (words[heading[0]][0] + words[heading[0]][2]) / 2
            bottom = next((b[3] for b in tables if b[0] <= hx <= b[2] and b[1] - 2 <= header_bottom <= b[3] + 2),
                          None)
            header_center = sum((words[i][1] + words[i][3]) / 2 for i in heading) / len(heading)
            groups, cont = _scan_rows(words, layout, header_bottom, header_center, height, space, bottom)
            out.extend((rule, g) for g in groups)
            if cont:
                carry.append(layout)
        if not found_heading:
            for layout in carried:
                if layout.rule != rule:
                    continue
                if tables is None:
                    tables = _ruled_tables(page)
                cx = sum(layout.bounds[layout.target]) / 2
                table = next((b for b in tables if b[0] <= cx <= b[2]), None)
                cont_layout = Layout(layout.bounds, layout.spans, layout.target, rule, continuation=True,
                                     pitch=layout.pitch)
                groups, cont = _scan_rows(words, cont_layout, table[1] - 1 if table else -1.0, None, height,
                                          space, table[3] if table else None)
                out.extend((rule, g) for g in groups)
                if cont:
                    carry.append(Layout(layout.bounds, layout.spans, layout.target, rule, pitch=cont_layout.pitch))
    return out, carry
