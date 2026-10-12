"""Detection of personal data in plain text.

Every detector receives the text of one PDF page (lines separated by ``\\n``,
words by single spaces) and returns character spans. The engine maps those
spans back to word boxes on the page.

Detection is rule based and deterministic: regular expressions with checksum
validation for structured identifiers, labelled form fields, name heuristics,
and the user's own term lists. No statistical model and no network.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from . import names
from .textutil import fold

# Category id -> (label shown to the user, enabled by default)
CATEGORIES = {
    "PERSON": ("Personal names", True),
    "EMAIL": ("E-mail addresses", True),
    "PHONE": ("Phone numbers", True),
    "ADDRESS": ("Postal addresses", True),
    "BIRTH_DATE": ("Dates of birth", True),
    "NATIONAL_ID": ("Birth numbers / national IDs (rodné číslo, SSN)", True),
    "ID_DOC": ("ID card, passport, insurance and employee numbers", True),
    "BANK": ("Bank accounts, IBAN, payment cards", True),
    "COMPANY_ID": ("Company and tax IDs (IČO, DIČ, VAT)", True),
    "IP": ("IP addresses", True),
    "DATE": ("All other dates", False),
    "CUSTOM": ("Your own terms (always applied)", True),
}

# When findings overlap, the category listed first wins.
PRIORITY = [
    "CUSTOM", "EMAIL", "NATIONAL_ID", "BANK", "ID_DOC", "COMPANY_ID", "PHONE",
    "BIRTH_DATE", "IP", "ADDRESS", "PERSON", "DATE",
]


@dataclass
class Finding:
    category: str
    start: int
    end: int
    # Pseudonym key; findings with the same key get the same replacement label.
    key: str | None = None
    meta: dict = field(default_factory=dict)


@dataclass
class DetectorConfig:
    categories: set = field(default_factory=lambda: {c for c, (_, on) in CATEGORIES.items() if on})
    custom_terms: list = field(default_factory=list)
    allow_terms: list = field(default_factory=list)
    match_word_forms: bool = True


UP = "A-ZÁÄČĎÉĚËÍĽĹŇÓÔÖŐŘŔŠŤÚŮÜŰÝŽĄĆĘŁŃŚŹŻÀÂÆÇÈÊÎÏŒÙÛŸ"
LOW = "a-záäčďéěëíľĺňóôöőřŕšťúůüűýžąćęłńśźżàâæçèêîïœùûÿß"
WORD = f"[{UP}{LOW}]"
CAPWORD = f"[{UP}][{LOW}{UP}]+"

# --------------------------------------------------------------------------- #
# Validators
# --------------------------------------------------------------------------- #


def _digits(s: str) -> str:
    return re.sub(r"\D", "", s)


def luhn_ok(number: str) -> bool:
    d = _digits(number)
    if not 13 <= len(d) <= 19:
        return False
    total = 0
    for i, ch in enumerate(reversed(d)):
        n = int(ch)
        if i % 2:
            n *= 2
            if n > 9:
                n -= 9
        total += n
    return total % 10 == 0


def iban_ok(s: str) -> bool:
    s = re.sub(r"\s", "", s).upper()
    if not 15 <= len(s) <= 34 or not re.fullmatch(r"[A-Z]{2}\d{2}[A-Z0-9]+", s):
        return False
    rearranged = s[4:] + s[:4]
    num = "".join(str(int(c, 36)) for c in rearranged)
    return int(num) % 97 == 1


CZ_BANK_CODES = set("""
0100 0300 0600 0710 0800 2010 2060 2070 2100 2200 2220 2250 2260 2275 2600 2700
3030 3050 3060 3500 4000 4300 5500 5800 6000 6100 6200 6210 6300 6363 6700 6800
7910 7950 7960 7970 7990 8030 8040 8060 8090 8150 8190 8198 8220 8250 8255 8265 8500
""".split())


def cz_account_ok(prefix: str | None, number: str, bank: str) -> bool:
    if bank not in CZ_BANK_CODES:
        return False
    if len(number.lstrip("0")) < 2:
        return False
    weights = [6, 3, 7, 9, 10, 5, 8, 4, 2, 1]
    if sum(int(d) * w for d, w in zip(number.zfill(10), weights)) % 11:
        return False
    if prefix:
        pw = [10, 5, 8, 4, 2, 1]
        if sum(int(d) * w for d, w in zip(prefix.zfill(6), pw)) % 11:
            return False
    return True


def birth_number_ok(s: str) -> bool:
    """Czech/Slovak rodné číslo: date plausibility plus the mod-11 check."""
    d = _digits(s)
    if len(d) not in (9, 10):
        return False
    yy, mm, dd = int(d[0:2]), int(d[2:4]), int(d[4:6])
    if mm > 70:
        mm -= 70
    elif mm > 50:
        mm -= 50
    elif mm > 20:
        mm -= 20
    if not (1 <= mm <= 12 and 1 <= dd <= 31):
        return False
    if len(d) == 9:
        return yy < 54
    n = int(d)
    if n % 11 == 0:
        return True
    return int(d[:9]) % 11 == 10 and d[9] == "0"


def ico_ok(s: str) -> bool:
    d = _digits(s)
    if len(d) != 8:
        return False
    total = sum(int(d[i]) * (8 - i) for i in range(7))
    check = (11 - total % 11) % 10
    return check == int(d[7])


# --------------------------------------------------------------------------- #
# Regular-expression detectors
# --------------------------------------------------------------------------- #

_MONTHS = (
    r"ledna|února|března|dubna|května|června|července|srpna|září|října|listopadu|prosince|"
    r"leden|únor|březen|duben|květen|červen|červenec|srpen|říjen|listopad|prosinec|"
    r"january|february|march|april|may|june|july|august|september|october|november|december|"
    r"jan|feb|mar|apr|jun|jul|aug|sep|sept|oct|nov|dec|"
    r"januar|februar|märz|mai|juni|juli|oktober|dezember"
)
DATE_RE = (
    r"(?:\d{1,2}\. ?\d{1,2}\. ?(?:19|20)\d{2}"
    r"|\d{1,2}/\d{1,2}/(?:19|20)?\d{2}"
    r"|(?:19|20)\d{2}-\d{2}-\d{2}"
    r"|\d{1,2}\.? (?:" + _MONTHS + r")\.? (?:19|20)\d{2}"
    r"|(?:" + _MONTHS + r")\.? \d{1,2},? (?:19|20)\d{2})"
)

_STREET_EN = (
    r"Street|St|Road|Rd|Avenue|Ave|Lane|Ln|Boulevard|Blvd|Drive|Dr|Court|Ct|Way|"
    r"Place|Pl|Square|Sq|Terrace|Close|Crescent|Highway|Hwy"
)

PATTERNS: list = [
    ("EMAIL", re.compile(r"[A-Za-z0-9._%+\-]+@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)*\.[A-Za-z]{2,}"), None),
    # IBAN (mod-97 validated)
    ("BANK", re.compile(r"\b[A-Z]{2}\d{2}(?: ?[A-Z0-9]{4}){2,7}(?: ?[A-Z0-9]{1,3})?\b"), iban_ok),
    # Payment card numbers (Luhn validated)
    ("BANK", re.compile(r"(?<![\d-])[3-6]\d{3}(?:[ -]?\d{2,4}){2,4}(?![\d-])"), luhn_ok),
    # Czech/Slovak birth number with slash, or 10 digits without
    ("NATIONAL_ID", re.compile(r"(?<![\d/])\d{6} ?/ ?\d{3,4}(?![\d/])"), birth_number_ok),
    ("NATIONAL_ID", re.compile(r"(?<![\d/])\d{10}(?![\d/])"), birth_number_ok),
    # US SSN, UK National Insurance number
    ("NATIONAL_ID", re.compile(r"\b\d{3}-\d{2}-\d{4}\b"), None),
    ("NATIONAL_ID", re.compile(r"\b[A-CEGHJ-PR-TW-Z]{2} ?\d{2} ?\d{2} ?\d{2} ?[A-D]\b"), None),
    # VAT / DIČ
    ("COMPANY_ID", re.compile(
        r"\b(?:AT|BE|BG|CY|CZ|DE|DK|EE|EL|ES|FI|FR|HR|HU|IE|IT|LT|LU|LV|MT|NL|PL|PT|RO|SE|SI|SK)"
        r"U?\d{8,12}\b"), None),
    # Czech 3-3-3 grouping (not round amounts, not followed by a currency)
    ("PHONE", re.compile(r"(?<![\d\w,.])[2-7]\d{2} \d{3} \d{3}(?![\d]| ?(?:Kč|CZK|EUR|USD|,-|,\d))"),
     lambda s: not s.endswith("000 000")),
    # North American formats
    ("PHONE", re.compile(r"\(\d{3}\) ?\d{3}-\d{4}\b|\b\d{3}-\d{3}-\d{4}\b"), None),
    ("IP", re.compile(r"\b(?:25[0-5]|2[0-4]\d|1?\d?\d)(?:\.(?:25[0-5]|2[0-4]\d|1?\d?\d)){3}\b"), None),
    # Czech address: [street number,] PSČ City
    ("ADDRESS", re.compile(
        rf"(?:{CAPWORD}(?: {WORD}+\.?){{0,2}} [1-9]\d{{0,4}}(?:/[1-9]\d{{0,4}})?[a-zA-Z]?,? ?)?"
        rf"(?<!\d)\d{{3}} ?\d{{2}},? (?!Kč|CZK|EUR|USD){CAPWORD}(?:[ \-]\d{{1,2}}\b| (?:nad|pod|u) {CAPWORD}"
        rf"| {CAPWORD})?"), None),
    # Czech street with descriptive/orientation number: "Vodičkova 699/30"
    ("ADDRESS", re.compile(rf"\b{CAPWORD}(?: {CAPWORD})? (?!(?:19|20)\d\d/)[1-9]\d{{0,4}}/[1-9]\d{{0,3}}[a-zA-Z]?\b"
                           rf"(?<!/(?:19|20)\d\d)"), None),
    # English street address
    ("ADDRESS", re.compile(rf"\b\d{{1,5}} (?:[A-Z][a-z]+ ){{1,3}}(?:{_STREET_EN})\b\.?"), None),
    # German street address
    ("ADDRESS", re.compile(rf"\b{CAPWORD}(?:straße|strasse|str\.|weg|gasse|platz|allee|ring) \d{{1,4}}[a-z]?\b"), None),
    # UK postcode
    ("ADDRESS", re.compile(r"\b[A-Z]{1,2}\d[A-Z\d]? ?\d[A-Z]{2}\b"), None),
    ("DATE", re.compile(r"\b" + DATE_RE + r"\b", re.IGNORECASE), None),
]

_STREET_LEAD = {fold(w) for w in """Na U V Ve Za Pod Nad Před K Ke Při Mezi Nové Staré Malé Velké
    náměstí nám třída tř nábřeží nábř ulice sídliště Svaté Svatého""".split()}


def _street_trim(s: str) -> int:
    """Offset of the real start of a street name inside an address match.

    Regex matching starts at the leftmost capitalised word, so "Bydlí Vodičkova 12"
    would include the verb. Keep the word before the street only when it is a
    preposition/descriptor (Na Příkopě, náměstí Míru) or a first name (Karla Engliše).
    """
    m = re.match(r"((?:\S+ )*?)(\S+) [1-9]\d{0,4}(?:/\d+)?[a-zA-Z]?,? ?(?:\d{3} ?\d{2}|$|\b)", s)
    if not m or not m.group(1):
        return 0
    before = m.group(1).split()
    keep = 0
    for w in reversed(before):
        k = fold(w.rstrip("."))
        if k in _STREET_LEAD or names.first_name_status(k) is not None or w[:1].isdigit():
            keep += 1
        else:
            break
    drop = before[: len(before) - keep]
    return sum(len(w) + 1 for w in drop)


# Czech account numbers: [prefix-]number/bank code (not "č. 123/2010 Sb.")
_CZ_ACCOUNT = re.compile(r"(?<![\w/-])(?:(\d{1,6})-)?(\d{2,10}) ?/ ?(\d{4})(?![\w/])(?! ?Sb\b)")

# International phone numbers: +420 777 123 456, 0044 20 7946 0958
_PHONE_INTL = re.compile(r"(?<![\w+])(?:\+|00)[1-9]\d{0,2}(?:[ .\-/]?\(?\d{1,4}\)?){2,6}(?!\w)")
_PHONE_GROUP = re.compile(r"[ .\-/]?\(?\d{1,4}\)?$")

# Keyword followed by a date: "nar. 12. 3. 1980", "born 12 March 1980".
_BIRTH_KEYWORD = re.compile(
    r"\b(?:nar\.|narozen[ay]?|narozen[ae] dne|born(?: on)?|geboren(?: am)?|b\.)\s*(?:dne\s*)?(" + DATE_RE + r")",
    re.IGNORECASE,
)

# --------------------------------------------------------------------------- #
# Labelled form fields ("Jméno: Jan Novák")
# --------------------------------------------------------------------------- #

_LABELS = {
    "PERSON": """jméno a příjmení|celé jméno|jméno klienta|jméno zákazníka|jméno pacienta|
        customer name|client name|patient name|employee name|jméno|příjmení|rodné příjmení|name|full name|first name|
        last name|given name|surname|family name|klient|zákazník|pacient|pojištěnec|zaměstnanec|
        žadatel|nájemce|pronajímatel|kupující|prodávající|dlužník|věřitel|zákonný zástupce|
        kontaktní osoba|vypracoval|vypracovala|vyřizuje|odpovědná osoba|zpracoval|schválil|
        customer|client|patient|employee|applicant|tenant|landlord|insured|policyholder|
        contact person|prepared by|approved by|account holder|majitel účtu|vorname|nachname""",
    "BIRTH_DATE": "datum narození|date of birth|birth date|dob|geburtsdatum|narozen|narozena",
    "ADDRESS": """adresa|address|bydliště|trvalé bydliště|trvalý pobyt|místo pobytu|
        korespondenční adresa|doručovací adresa|adresa trvalého pobytu|ulice|street|residence|
        anschrift|místo narození|place of birth|home address""",
    "NATIONAL_ID": """rodné číslo|r\\. ?č\\.|rč|birth number|personal number|social security number|
        ssn|national id|national insurance number|ni number""",
    "ID_DOC": """číslo op|č\\. ?op|číslo občanského průkazu|občanský průkaz|číslo pasu|pas č\\.|
        passport|passport no\\.?|passport number|id card|id number|číslo dokladu|číslo průkazu|
        řidičský průkaz|driving licen[cs]e|driver'?s licen[cs]e|číslo pojištěnce|insurance number|
        osobní číslo|employee id|employee number|číslo zaměstnance|pojistné číslo|číslo pojistky""",
    "PHONE": "telefon|telefonní číslo|tel\\.?|mobil|mobile|phone|fax|tel\\. č\\.",
    "EMAIL": "e-mail|email|mail",
    "BANK": """číslo účtu|č\\. ?ú\\.|bankovní spojení|bankovní účet|účet|iban|account number|
        account no\\.?|bank account""",
    "COMPANY_ID": "ič|ičo|dič|ič dph|vat|vat id|vat no\\.?|tax id|ust-idnr\\.?",
}


def _label_regex() -> re.Pattern:
    parts = []
    for cat, alts in _LABELS.items():
        options = [a.strip() for a in alts.replace("\n", " ").split("|") if a.strip()]
        options = [fold(re.sub(r"\s+", " ", o)) for o in options]
        options.sort(key=len, reverse=True)
        parts.append(f"(?P<{cat}>{'|'.join(options)})")
    # The label must be a whole word and be followed by a colon (or tab).
    return re.compile(r"(?<![\w.])(?:" + "|".join(parts) + r")\s*[:\t]\s*")


_LABEL_RE = _label_regex()
_LINE_PREFIX = re.compile(r"^[\s\-–•*·>\d.)(]*$")


def _labelled_fields(text: str, folded: str) -> list:
    found = []
    lines = _line_spans(text)
    for li, (ls, le) in enumerate(lines):
        line_f = folded[ls:le]
        hits = []
        for m in _LABEL_RE.finditer(line_f):
            accepted = bool(hits) or _LINE_PREFIX.match(line_f[: m.start()]) is not None
            if not accepted:
                prefix = line_f[: m.start()].rstrip()
                accepted = prefix.endswith((",", ";", "|"))
            if accepted:
                hits.append(m)
        for i, m in enumerate(hits):
            cat = m.lastgroup
            vs = ls + m.end()
            ve = ls + hits[i + 1].start() if i + 1 < len(hits) else le
            value = text[vs:ve]
            # Empty value: the data is often on the next line in table layouts.
            if not value.strip() and i + 1 == len(hits) and li + 1 < len(lines):
                ns, ne = lines[li + 1]
                nxt = text[ns:ne]
                if 0 < len(nxt.split()) <= 6 and not _LABEL_RE.match(folded[ns:ne].lstrip()):
                    vs, ve, value = ns, ne, nxt
            stripped = value.strip(" ,;|")
            if not stripped or not re.search(r"\w", stripped):
                continue
            if set(stripped) <= set("_.-– "):  # blank form line "________"
                continue
            off = value.find(stripped)
            found.append(Finding(cat, vs + off, vs + off + len(stripped), meta={"source": "label"}))
    return found


def _line_spans(text: str) -> list:
    spans, start = [], 0
    for line in text.split("\n"):
        spans.append((start, start + len(line)))
        start += len(line) + 1
    return spans


# --------------------------------------------------------------------------- #
# Personal names
# --------------------------------------------------------------------------- #

_TOKEN = re.compile(rf"{WORD}[{UP}{LOW}'’\-]*\.?")


def _is_cap(tok: str) -> bool:
    core = tok.rstrip(".")
    return len(core) >= 2 and core[0].isupper() and core.replace("-", "").replace("'", "").replace("’", "").isalpha()


def _key(tok: str) -> str:
    return fold(tok.rstrip("."))


def _person_names(text: str) -> list:
    found = []
    for ls, le in _line_spans(text):
        line = text[ls:le]
        toks = [(m.start(), m.end(), m.group()) for m in _TOKEN.finditer(line)]
        n = len(toks)

        def gap(i, j):  # text between token i and token j (j = i + 1)
            return line[toks[i][1]:toks[j][0]]

        def namelike(i):
            t = toks[i][2]
            k = _key(t)
            return _is_cap(t) and k not in names.TITLES and k not in names.NAME_STOPWORDS

        def ends_sentence(i):
            return toks[i][2].endswith(".") and len(toks[i][2]) > 3

        used = set()
        for i in range(n):
            if i in used:
                continue
            tok = toks[i][2]
            k = _key(tok)
            span = None
            # 1) Title or salutation followed by capitalised words.
            if k in names.TITLES:
                j = i + 1
                while j < n and _key(toks[j][2]) in names.TITLES and gap(j - 1, j).strip() in ("", ","):
                    j += 1
                end = j
                while end < n and end - j < 3 and namelike(end) and gap(end - 1, end).strip() == "":
                    end += 1
                    if ends_sentence(end - 1):
                        break
                if end > j:
                    span = (j, end - 1)
            # 2) Known first name, extended to neighbouring capitalised words.
            elif _is_cap(tok):
                status = names.first_name_status(k)
                if status is not None:
                    first, last = i, i
                    while (last + 1 < n and last - i < 2 and not ends_sentence(last) and namelike(last + 1)
                           and gap(last, last + 1).strip() == ""):
                        last += 1
                    if i > 0 and namelike(i - 1) and not ends_sentence(i - 1) and gap(i - 1, i).strip() in ("", ","):
                        pk = _key(toks[i - 1][2])
                        if names.looks_like_surname(pk) or names.first_name_status(pk) is not None:
                            first = i - 1
                    if status is False or last > i or first < i:
                        span = (first, last)
                # 3) Surname followed by a first name is handled above; a lone
                #    known surname next to another capitalised word counts too.
                elif names.looks_like_surname(k) and i + 1 < n and namelike(i + 1) \
                        and gap(i, i + 1).strip() == "" and k not in names.NAME_STOPWORDS:
                    nk = _key(toks[i + 1][2])
                    if names.looks_like_surname(nk) or names.first_name_status(nk) is not None:
                        span = (i, i + 1)
            if span:
                a, b = span
                used.update(range(a, b + 1))
                s, e = toks[a][0], toks[b][1]
                while e > s and line[e - 1] in ".,":
                    e -= 1
                found.append(Finding("PERSON", ls + s, ls + e))
    return found


# --------------------------------------------------------------------------- #
# User terms
# --------------------------------------------------------------------------- #

_WTOKEN = re.compile(r"\w+")


def _prepare_terms(terms) -> list:
    out = []
    for t in terms:
        toks = _WTOKEN.findall(fold(t.strip()))
        if toks:
            out.append((t.strip(), toks))
    out.sort(key=lambda x: -sum(len(p) for p in x[1]))
    return out


def _token_matches(word: str, term: str, word_forms: bool) -> bool:
    if word == term:
        return True
    if word_forms and len(term) >= 3 and term.isalpha() and word.startswith(term):
        return len(word) - len(term) <= 4 and word[len(term):].isalpha()
    if word_forms and len(term) >= 4 and term.isalpha() and term[-1] in "aeiouy":
        # Novotná -> Novotné, Svoboda -> Svobodovi
        stem = term[:-1]
        return word.startswith(stem) and 0 < len(word) - len(stem) <= 4 and word[len(stem):].isalpha()
    return False


def _term_matches(folded: str, terms, word_forms: bool, category: str) -> list:
    found = []
    if not terms:
        return found
    words = [(m.start(), m.end(), m.group()) for m in _WTOKEN.finditer(folded)]
    for i in range(len(words)):
        for original, toks in terms:
            if i + len(toks) > len(words):
                continue
            ok = True
            for k, t in enumerate(toks):
                w = words[i + k]
                if k and (w[0] - words[i + k - 1][1] > 3 or "\n" in folded[words[i + k - 1][1]:w[0]]):
                    ok = False
                    break
                if not _token_matches(w[2], t, word_forms):
                    ok = False
                    break
            if ok:
                found.append(Finding(category, words[i][0], words[i + len(toks) - 1][1],
                                     key=f"{category}:{fold(original)}", meta={"term": original}))
                break
    return found


# --------------------------------------------------------------------------- #
# Public entry point
# --------------------------------------------------------------------------- #


def detect(text: str, cfg: DetectorConfig, extra_terms=None) -> list:
    """Return non-overlapping findings for one page of text, sorted by position.

    ``extra_terms`` is a list of (term, key) pairs (used to propagate names found
    elsewhere in the same document); matches inherit the given pseudonym key.
    """
    folded = fold(text)
    cats = set(cfg.categories) | {"CUSTOM"}
    raw: list = []

    for cat, rx, validator in PATTERNS:
        if cat not in cats:
            continue
        for m in rx.finditer(text):
            s = m.group()
            if validator and not validator(s):
                continue
            start = m.start() + (_street_trim(s) if cat == "ADDRESS" else 0)
            raw.append(Finding(cat, start, m.end()))

    if "BANK" in cats:
        for m in _CZ_ACCOUNT.finditer(text):
            if not m.group(1) and len(m.group(2)) < 5:
                continue
            if cz_account_ok(m.group(1), m.group(2), m.group(3)) and not birth_number_ok(m.group(2) + m.group(3)):
                raw.append(Finding("BANK", m.start(), m.end()))

    if "PHONE" in cats:
        for m in _PHONE_INTL.finditer(text):
            s = m.group()
            # A greedy match can swallow a following number; drop trailing groups.
            while len(_digits(s)) > 15:
                s = s[: _PHONE_GROUP.search(s).start()]
            if len(_digits(s)) >= 9:
                raw.append(Finding("PHONE", m.start(), m.start() + len(s)))

    if "BIRTH_DATE" in cats:
        for m in _BIRTH_KEYWORD.finditer(folded):
            raw.append(Finding("BIRTH_DATE", m.start(1), m.end(1)))

    for f in _labelled_fields(text, folded):
        if f.category in cats:
            if f.category == "COMPANY_ID" and re.fullmatch(r"\d{8}", re.sub(r"\s", "", text[f.start:f.end])):
                if not ico_ok(text[f.start:f.end]):
                    continue
            raw.append(f)

    if "PERSON" in cats:
        raw.extend(_person_names(text))
        if extra_terms:
            for term, key in extra_terms:
                for f in _term_matches(folded, _prepare_terms([term]), cfg.match_word_forms, "PERSON"):
                    f.key = key
                    f.meta["source"] = "propagated"
                    raw.append(f)

    raw.extend(_term_matches(folded, _prepare_terms(cfg.custom_terms), cfg.match_word_forms, "CUSTOM"))

    raw = [f for f in (_apply_allowlist(f, text, folded, cfg.allow_terms) for f in raw) if f]
    return _resolve_overlaps(raw)


def _apply_allowlist(f: Finding, text: str, folded: str, allow_terms) -> Finding | None:
    if not allow_terms or f.category == "CUSTOM":
        return f
    allow = {" ".join(_WTOKEN.findall(fold(a))) for a in allow_terms if a.strip()}
    allow_tokens = {a for a in allow if " " not in a}
    span = folded[f.start:f.end]
    if " ".join(_WTOKEN.findall(span)) in allow:
        return None
    toks = list(_WTOKEN.finditer(span))
    while toks and toks[0].group() in allow_tokens:
        toks.pop(0)
    while toks and toks[-1].group() in allow_tokens:
        toks.pop()
    if not toks:
        return None
    s, e = f.start + toks[0].start(), f.start + toks[-1].end()
    if (s, e) != (f.start, f.end):
        f = Finding(f.category, s, e, f.key, dict(f.meta))
    return f


def _resolve_overlaps(findings: list) -> list:
    rank = {c: i for i, c in enumerate(PRIORITY)}
    findings.sort(key=lambda f: (rank.get(f.category, 99), -(f.end - f.start), f.start))
    taken: list = []
    result = []
    for f in findings:
        # Cut away parts already claimed by a higher-priority finding and keep the
        # rest, so "Klient: Jan Novák, nar. 1. 1. 1980" redacts both the date and
        # the name around it.
        pieces = [(f.start, f.end)]
        for s, e in taken:
            nxt = []
            for a, b in pieces:
                if b <= s or e <= a:
                    nxt.append((a, b))
                    continue
                if a < s:
                    nxt.append((a, s))
                if e < b:
                    nxt.append((e, b))
            pieces = nxt
        for a, b in pieces:
            if b - a < 2 and (a, b) != (f.start, f.end):
                continue
            g = f if (a, b) == (f.start, f.end) else Finding(f.category, a, b, f.key, dict(f.meta))
            taken.append((a, b))
            result.append(g)
    result.sort(key=lambda f: f.start)
    return result
