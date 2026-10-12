"""Create a small PDF with personal data, used to smoke-test the built application."""

import sys

import pymupdf

doc = pymupdf.open()
page = doc.new_page()
page.insert_htmlbox(pymupdf.Rect(50, 50, 550, 300),
                    "Jméno: Jan Novák<br>E-mail: jan.novak@example.cz<br>Tel.: +420 777 123 456")
doc.save(sys.argv[1])
