"""Command-line interface: python -m anonymizer.cli INPUT... -o OUTPUT"""

from __future__ import annotations

import argparse
import os
import sys

from . import APP_NAME, __version__
from .detectors import CATEGORIES, DetectorConfig
from .engine import Options, analyze, collect_pdfs, run_batch


def _read_list(path):
    if not path:
        return []
    with open(path, encoding="utf-8-sig") as fh:
        return [ln.strip() for ln in fh if ln.strip()]


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="pdf-anonymizer", description=f"{APP_NAME} {__version__} (offline)")
    p.add_argument("inputs", nargs="+", help="PDF files or folders")
    p.add_argument("-o", "--output", help="output folder (required unless --preview)")
    p.add_argument("--terms", help="text file with terms to always anonymise, one per line")
    p.add_argument("--allow", help="text file with terms never to anonymise, one per line")
    p.add_argument("--categories", help="comma-separated categories (default: the standard set). "
                   "Available: " + ", ".join(c for c in CATEGORIES if c != "CUSTOM"))
    p.add_argument("--exact", action="store_true", help="match custom terms exactly (no inflected forms)")
    p.add_argument("--style", choices=["label", "black"], default="label")
    p.add_argument("--remove-images", action="store_true")
    p.add_argument("--keep-names", action="store_true", help="keep original file names (+ _anonymized)")
    p.add_argument("--no-pdf", action="store_true")
    p.add_argument("--no-text", action="store_true")
    p.add_argument("--preview", action="store_true", help="print findings, write nothing")
    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    cfg = DetectorConfig(custom_terms=_read_list(args.terms), allow_terms=_read_list(args.allow),
                         match_word_forms=not args.exact)
    if args.categories:
        cfg.categories = {c.strip().upper() for c in args.categories.split(",") if c.strip()}
        unknown = cfg.categories - set(CATEGORIES)
        if unknown:
            print(f"Unknown categories: {', '.join(sorted(unknown))}", file=sys.stderr)
            return 2
    files = collect_pdfs(args.inputs)
    if args.output:  # do not re-process earlier results stored inside an input folder
        out_abs = os.path.abspath(args.output) + os.sep
        files = [f for f in files if not os.path.abspath(f).startswith(out_abs)]
    if not files:
        print("No PDF files found.", file=sys.stderr)
        return 2

    if args.preview:
        for f in files:
            a = analyze(f, cfg)
            print(f"== {f}")
            if a.error:
                print(f"   error: {a.error}")
            for w in a.warnings:
                print(f"   warning: {w}")
            for page, cat, text in a.items():
                print(f"   p{page:<4} {cat:<12} {text}")
        return 0

    if not args.output:
        print("--output is required", file=sys.stderr)
        return 2
    opts = Options(detector=cfg, style=args.style, remove_images=args.remove_images,
                   rename_files=not args.keep_names, write_pdf=not args.no_pdf, write_text=not args.no_text)
    results = run_batch(files, args.output, opts,
                        progress=lambda i, n, p: p and print(f"[{i + 1}/{n}] {p}", file=sys.stderr))
    for r in results:
        print(f"{r.status:6} {r.source} -> {r.output_pdf or r.output_text}")
        for w in ([r.error] if r.error else []) + r.warnings:
            print(f"       {w}")
        for page, cat, text in r.residues:
            print(f"       still visible on page {page}: {cat} '{text}'")
    return 1 if any(r.status == "FAILED" for r in results) else 0


if __name__ == "__main__":
    from . import offline
    offline.enforce()
    sys.exit(main())
