import pytest

from anonymizer.detectors import (DetectorConfig, birth_number_ok, cz_account_ok, detect, iban_ok,
                                  ico_ok, luhn_ok)


def found(text, **cfg):
    return [(f.category, text[f.start:f.end]) for f in detect(text, DetectorConfig(**cfg))]


def test_validators():
    assert iban_ok("CZ65 0800 0000 1920 0014 5399")
    assert iban_ok("GB82WEST12345698765432")
    assert not iban_ok("CZ65 0800 0000 1920 0014 5398")
    assert luhn_ok("4111 1111 1111 1111")
    assert not luhn_ok("4111 1111 1111 1112")
    assert birth_number_ok("800312/1236")
    assert not birth_number_ok("801312/1234")  # month 13
    assert ico_ok("25596641")
    assert not ico_ok("25596642")
    assert cz_account_ok("19", "2000145399", "0800")
    assert not cz_account_ok(None, "2000145399", "9999")


@pytest.mark.parametrize("text,expected", [
    ("Jméno: Jan Novák", ("PERSON", "Jan Novák")),
    ("Kontakt: jan.novak@example.cz", ("EMAIL", "jan.novak@example.cz")),
    ("Volejte +420 777 123 456 kdykoliv", ("PHONE", "+420 777 123 456")),
    ("Mobil 777 123 456", ("PHONE", "777 123 456")),
    ("Rodné číslo: 800312/1236", ("NATIONAL_ID", "800312/1236")),
    ("Účet 19-2000145399/0800", ("BANK", "19-2000145399/0800")),
    ("IBAN CZ65 0800 0000 1920 0014 5399", ("BANK", "CZ65 0800 0000 1920 0014 5399")),
    ("Bydlí Vodičkova 699/30, 110 00 Praha 1.", ("ADDRESS", "Vodičkova 699/30, 110 00 Praha 1")),
    ("Jan Novák, nar. 12. 3. 1980", ("BIRTH_DATE", "12. 3. 1980")),
    ("Date of birth: 4 July 1976", ("BIRTH_DATE", "4 July 1976")),
    ("DIČ CZ25596641", ("COMPANY_ID", "CZ25596641")),
    ("IČO: 25596641", ("COMPANY_ID", "25596641")),
    ("Server 192.168.1.10 odpověděl", ("IP", "192.168.1.10")),
    ("Dear Mr. John Smith,", ("PERSON", "John Smith")),
    ("Předáno panu Tomášovi Dvořákovi.", ("PERSON", "Tomášovi Dvořákovi")),
    ("se dohodl s paní Svobodovou", ("PERSON", "Svobodovou")),
    ("Prof. Ing. Karel Veselý, CSc.", ("PERSON", "Karel Veselý")),
    ("Card 4111 1111 1111 1111", ("BANK", "4111 1111 1111 1111")),
    ("SSN 123-45-6789", ("NATIONAL_ID", "123-45-6789")),
    ("Lives at 12 Baker Street", ("ADDRESS", "12 Baker Street")),
])
def test_detects(text, expected):
    assert expected in found(text)


@pytest.mark.parametrize("text", [
    "Daně z příjmů zaplatí kupující.",
    "Mark the box below.",
    "Podle zákona č. 123/2010 Sb. platí",
    "Celková cena 250 000 000 Kč.",
    "Smlouva je uzavřena na dobu neurčitou.",
    "Faktura 2024/0815 je splatná 14 dní.",
    "Jan 2024 report",
])
def test_no_false_positives(text):
    assert found(text) == []


def test_dates_only_when_enabled():
    text = "Smlouva ze dne 5. 6. 2024"
    assert found(text) == []
    cats = DetectorConfig().categories | {"DATE"}
    assert ("DATE", "5. 6. 2024") in found(text, categories=cats)


def test_label_value_on_next_line_and_multiple_labels():
    text = "Jméno:\nPetr Hrubý\nTelefon: 603 123 456 E-mail: p@x.cz"
    res = found(text)
    assert ("PERSON", "Petr Hrubý") in res
    assert ("PHONE", "603 123 456") in res
    assert ("EMAIL", "p@x.cz") in res


def test_label_not_matched_inside_other_word_or_mid_line():
    assert found("Product name: Widget 3000") == []


def test_custom_terms_with_word_forms():
    text = "Projekt Fénix předán firmě Acme. Fénixu se daří."
    res = found(text, custom_terms=["Fénix", "Acme"])
    assert ("CUSTOM", "Fénix") in res
    assert ("CUSTOM", "Fénixu") in res
    assert ("CUSTOM", "Acme") in res
    exact = found(text, custom_terms=["Fénix"], match_word_forms=False)
    assert ("CUSTOM", "Fénixu") not in exact


def test_allowlist_trims_and_drops():
    text = "Jméno: Jan Novák Praha"
    res = found(text, allow_terms=["Praha"])
    assert ("PERSON", "Jan Novák") in res
    assert found("Ing. Petr Pavel", allow_terms=["Petr Pavel"]) == []


def test_overlap_keeps_both_parts():
    res = found("Klient: Jan Novák, nar. 1. 2. 1975")
    cats = {c for c, _ in res}
    assert {"PERSON", "BIRTH_DATE"} <= cats
