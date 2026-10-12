# PDF Anonymizer

A desktop program for Windows and macOS that finds personal data in PDF files and removes it. It runs entirely on your computer. The program never connects to the network: at start-up it disables networking for its own process, so even a library that tried to send data would fail. Your original files are only read, never changed. Results go to a folder you choose.

## Install

Download the installer from the latest successful run of the **PDF Anonymizer** workflow (GitHub → Actions → PDF Anonymizer → Artifacts), or from a release if one has been published.

**Windows.** Run `PDF-Anonymizer-Setup-x.y.z.exe`. It installs for your user account only (into `%LOCALAPPDATA%\Programs`), so administrator rights are not needed. It adds a Start-menu entry and an uninstaller, the same way Claude Desktop installs. The installer is not code-signed, so SmartScreen will say "Windows protected your PC". Click *More info → Run anyway*. If your company's IT blocks unsigned programs (AppLocker or Intune policies), they have to allow it, or use the portable `.zip` if policy permits. The portable `.zip` contains the same program and needs no installation.

**macOS (Apple Silicon).** Open `PDF-Anonymizer-x.y.z-macOS.dmg` and drag the app to *Applications*. The app is not notarised. On first launch, right-click it and choose *Open*, or allow it under *System Settings → Privacy & Security*.

## Use

1. **Add files…** or **Add folder…** (subfolders are included).
2. Choose an output folder. The default is `anonymized` next to the first file.
3. Under *What to anonymise*, tick the categories you want. Under *Your terms*, list anything that must always go, such as client names, company names or project code names, and anything that is being removed wrongly.
4. Click **Preview findings…** to see every detection before anything is written. Select false positives and click *Never anonymise selected*.
5. Click **Anonymise**.

For each input, the output folder receives:

| File | Content |
|---|---|
| `document_001.pdf` | The PDF with each finding destroyed and covered by a black box labelled `PERSON_1`, `EMAIL_2` and so on. The same value always gets the same label across the whole batch. |
| `document_001.txt` | The extracted text with the same labels, convenient for analysis. |
| `anonymization_report.csv` | Counts per category and status (`OK`, `CHECK`, `FAILED`) per file. It contains no original values. |

Output files are renamed to `document_001…` by default, because file names often contain names. The log window and the preview show original values on screen only; they are never written to disk. No key that maps labels back to real values is created.

## What is detected

| Category | Method |
|---|---|
| Personal names | About 530 common Czech, Slovak, English, German and Polish first names, with Czech declined forms (Tomáše, Tomášovi). Titles and salutations (Ing., MUDr., pan, paní, Mr., Dear). Czech female surnames ending in -ová. Labelled fields (Jméno:, Klient:, Name:). A surname found once is removed everywhere else in the same document. |
| E-mail, phone | Pattern matching: international, Czech 3-3-3 and North American formats. |
| Addresses | Czech street + number + PSČ + city, "Vodičkova 699/30", English and German street formats, UK postcodes, and labelled fields (Adresa:, Bydliště:, Address:). |
| Dates of birth | Labelled (Datum narození:, DOB:) or introduced by *nar.*, *born*. Other dates are optional and off by default. |
| National IDs | Czech/Slovak rodné číslo (checksum and date validated), US SSN, UK NI number. |
| ID documents | Labelled ID card, passport, driving licence, insurance and employee numbers. |
| Bank | IBAN (mod-97 validated), Czech account numbers (checksum plus a list of real bank codes), payment cards (Luhn validated). |
| Company and tax IDs | IČO (checksum), DIČ, EU VAT numbers. These matter for sole traders, whose company ID identifies a person. |
| IP addresses | IPv4. |
| Your terms | Every term you list, optionally with inflected forms (Novák → Nováka, Novákovi). |

## How the PDF is cleaned

- Form-field values and comments are first flattened into the page, so they are scanned like normal text.
- Each finding is removed from the page content with MuPDF's redaction. The characters are deleted, and image pixels under the box are blanked, not just covered.
- Metadata (author, title, XMP), bookmarks, attachments, embedded files, JavaScript, links, form fields, annotations and thumbnails are removed.
- The file is rewritten from scratch with garbage collection, so no earlier version of a page survives inside it.
- The written file is then read back. If any detected value can still be extracted, the file is marked `CHECK` and the log shows what and where.
- Optionally, all images are removed: photos, scanned signatures, logos.

## Limits

These limits matter for how far you can trust the output.

- **Scanned PDFs without a text layer cannot be read.** The program has no OCR. Such pages are reported as `CHECK`. Either run OCR first (for example *Adobe Acrobat → Scan & OCR*), or enable *Remove all images*, which blanks those pages. Scans that already have an OCR text layer are handled, and the image under each finding is blanked too.
- **Detection is rule-based, not a language model.** It is predictable and fully offline, but it misses some data. It will not catch a name that is not in its lists, has no title or label, and appears in running text. It cannot judge indirect identifiers like "the only female engineer in the Brno office". Use the preview, put known names into *Your terms*, and spot-check results before you share anything. Over-redaction is deliberate: when in doubt, it removes.
- Handwritten signatures drawn as vector graphics are not detected. Use *Remove all images* for scanned signatures.
- Password-protected PDFs must be unlocked first.

## Building from source

Requires Python 3.10+.

```
pip install -r requirements-dev.txt
python -m pytest tests                # 44 tests
python -m anonymizer                  # run the GUI from source
python packaging/build.py             # build dist/PDF Anonymizer (PyInstaller)
```

The Windows installer is built with [Inno Setup](https://jrsoftware.org/isinfo.php) from `packaging/windows/installer.iss`. GitHub Actions (`.github/workflows/pdf-anonymizer.yml`) does all of this on every push to `pdf-anonymizer/` and uploads the Windows installer, the Windows portable zip and the macOS disk image as artifacts. Pushing a tag `pdf-anonymizer-v1.0.0` also publishes them as a GitHub release.

There is also a command-line mode, using the same executable or `python -m anonymizer`:

```
python -m anonymizer INPUT.pdf FOLDER/ -o OUTPUT/ [--terms names.txt] [--allow keep.txt]
                     [--categories PERSON,EMAIL,...] [--style black] [--remove-images]
                     [--keep-names] [--preview]
```

## Licence note

PDF processing uses [PyMuPDF](https://pymupdf.readthedocs.io/), which is licensed under the AGPL-3.0. Using the program internally is fine. Distributing it to third parties brings AGPL obligations, or requires a commercial PyMuPDF licence from Artifex.
