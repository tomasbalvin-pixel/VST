"""Create a PDF with personal data, used to smoke-test the built application.

Page 1 has a text layer, page 2 is an image-only scan of page 1 (needs OCR).
"""

import sys

import pymupdf

doc = pymupdf.open()
page = doc.new_page()
page.insert_htmlbox(pymupdf.Rect(50, 50, 550, 300),
                    "Jméno: Jan Novák<br>E-mail: jan.novak@example.cz<br>Tel.: +420 777 123 456")
pix = page.get_pixmap(dpi=200)
scan = doc.new_page()
scan.insert_image(scan.rect, pixmap=pix)
doc.save(sys.argv[1])
