import unicodedata


def _fold_char(c: str) -> str:
    base = unicodedata.normalize("NFD", c)[0]
    low = base.lower()
    return low if len(low) == 1 else base


def fold(text: str) -> str:
    """Lower-case and strip diacritics while keeping a 1:1 character mapping.

    ``len(fold(s)) == len(s)`` always holds, so offsets found in the folded text
    are valid offsets in the original text.
    """
    return "".join(_fold_char(c) for c in text)
