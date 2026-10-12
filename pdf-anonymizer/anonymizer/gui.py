"""Desktop interface (Tkinter, bundled with Python; no extra GUI dependencies)."""

from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from . import APP_NAME, __version__, ocr, offline
from .detectors import CATEGORIES, DetectorConfig
from .engine import Options, analyze, collect_pdfs, run_batch


def config_path() -> str:
    if sys.platform == "win32":
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
    elif sys.platform == "darwin":
        base = os.path.expanduser("~/Library/Application Support")
    else:
        base = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(base, APP_NAME, "settings.json")


def open_folder(path: str) -> None:
    if sys.platform == "win32":
        os.startfile(path)  # noqa: S606 - local folder only
    elif sys.platform == "darwin":
        subprocess.run(["open", path], check=False)
    else:
        subprocess.run(["xdg-open", path], check=False)


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.queue: queue.Queue = queue.Queue()
        self.worker: threading.Thread | None = None
        self.cancel = threading.Event()
        self.files: list = []

        root.title(f"{APP_NAME} {__version__}")
        root.minsize(760, 640)
        self._build()
        self._load_settings()
        root.protocol("WM_DELETE_WINDOW", self._on_close)
        root.after(100, self._poll)

    # ------------------------------------------------------------------ UI

    def _build(self) -> None:
        pad = {"padx": 8, "pady": 4}
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        status = "Offline: network access is blocked for this program." if offline.is_enforced() \
            else "Warning: offline guard not active."
        status += f"   Scanned pages: read with OCR ({ocr.languages()})." if ocr.available() \
            else "   OCR not available: scanned pages cannot be read."
        ttk.Label(main, text=status, foreground="#1a7f37" if offline.is_enforced() else "#b42318") \
            .pack(anchor="w", **pad)

        # Files
        files = ttk.LabelFrame(main, text="1. PDF files to anonymise")
        files.pack(fill="both", expand=True, **pad)
        lb_frame = ttk.Frame(files)
        lb_frame.pack(side="left", fill="both", expand=True, padx=6, pady=6)
        self.listbox = tk.Listbox(lb_frame, selectmode="extended", height=7)
        sb = ttk.Scrollbar(lb_frame, orient="vertical", command=self.listbox.yview)
        self.listbox.configure(yscrollcommand=sb.set)
        self.listbox.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        btns = ttk.Frame(files)
        btns.pack(side="right", fill="y", padx=6, pady=6)
        ttk.Button(btns, text="Add files…", command=self._add_files).pack(fill="x", pady=2)
        ttk.Button(btns, text="Add folder…", command=self._add_folder).pack(fill="x", pady=2)
        ttk.Button(btns, text="Remove selected", command=self._remove_selected).pack(fill="x", pady=2)
        ttk.Button(btns, text="Clear", command=self._clear).pack(fill="x", pady=2)

        # Output folder
        out = ttk.LabelFrame(main, text="2. Output folder (originals are never changed)")
        out.pack(fill="x", **pad)
        self.out_var = tk.StringVar()
        ttk.Entry(out, textvariable=self.out_var).pack(side="left", fill="x", expand=True, padx=6, pady=6)
        ttk.Button(out, text="Browse…", command=self._browse_out).pack(side="right", padx=6, pady=6)

        # Settings
        nb = ttk.Notebook(main)
        nb.pack(fill="both", expand=True, **pad)

        cat_tab = ttk.Frame(nb, padding=8)
        nb.add(cat_tab, text="3. What to anonymise")
        self.cat_vars = {}
        for i, (cat, (label, default)) in enumerate(CATEGORIES.items()):
            if cat == "CUSTOM":
                continue
            v = tk.BooleanVar(value=default)
            self.cat_vars[cat] = v
            ttk.Checkbutton(cat_tab, text=label, variable=v).grid(row=i // 2, column=i % 2, sticky="w",
                                                                  padx=4, pady=2)

        terms_tab = ttk.Frame(nb, padding=8)
        nb.add(terms_tab, text="Your terms")
        terms_tab.columnconfigure(0, weight=1)
        terms_tab.columnconfigure(1, weight=1)
        terms_tab.rowconfigure(1, weight=1)
        ttk.Label(terms_tab, text="Always anonymise (one per line):\nnames, companies, project codes…") \
            .grid(row=0, column=0, sticky="w")
        ttk.Label(terms_tab, text="Never anonymise (one per line):\nwords wrongly detected") \
            .grid(row=0, column=1, sticky="w")
        self.terms_text = tk.Text(terms_tab, height=6, width=30, undo=True)
        self.allow_text = tk.Text(terms_tab, height=6, width=30, undo=True)
        self.terms_text.grid(row=1, column=0, sticky="nsew", padx=(0, 4))
        self.allow_text.grid(row=1, column=1, sticky="nsew", padx=(4, 0))
        ttk.Button(terms_tab, text="Load list from file…",
                   command=lambda: self._load_list(self.terms_text)).grid(row=2, column=0, sticky="w", pady=4)
        ttk.Button(terms_tab, text="Load list from file…",
                   command=lambda: self._load_list(self.allow_text)).grid(row=2, column=1, sticky="w", pady=4)
        self.forms_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(terms_tab, text="Also match inflected forms (Novák → Nováka, Novákovi)",
                        variable=self.forms_var).grid(row=3, column=0, columnspan=2, sticky="w")

        out_tab = ttk.Frame(nb, padding=8)
        nb.add(out_tab, text="Output options")
        self.style_var = tk.StringVar(value="label")
        ttk.Radiobutton(out_tab, text="Black box with a label (PERSON_1, EMAIL_2…) — same value, same label",
                        value="label", variable=self.style_var).pack(anchor="w")
        ttk.Radiobutton(out_tab, text="Plain black box", value="black", variable=self.style_var).pack(anchor="w")
        self.images_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(out_tab, text="Remove all images (photos, signatures, logos, scanned pages)",
                        variable=self.images_var).pack(anchor="w", pady=(8, 0))
        self.rename_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(out_tab, text="Rename output files to document_001, document_002… "
                                      "(file names often contain names)",
                        variable=self.rename_var).pack(anchor="w")
        self.pdf_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(out_tab, text="Write anonymised PDF", variable=self.pdf_var).pack(anchor="w")
        self.txt_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(out_tab, text="Write anonymised plain text (.txt)", variable=self.txt_var).pack(anchor="w")

        # Actions
        act = ttk.Frame(main)
        act.pack(fill="x", **pad)
        self.preview_btn = ttk.Button(act, text="Preview findings…", command=self._preview)
        self.preview_btn.pack(side="left")
        self.run_btn = ttk.Button(act, text="Anonymise", command=self._run)
        self.run_btn.pack(side="left", padx=6)
        self.cancel_btn = ttk.Button(act, text="Cancel", command=self.cancel.set, state="disabled")
        self.cancel_btn.pack(side="left")
        self.open_btn = ttk.Button(act, text="Open output folder",
                                   command=lambda: open_folder(self.out_var.get()), state="disabled")
        self.open_btn.pack(side="right")
        self.progress = ttk.Progressbar(main, mode="determinate")
        self.progress.pack(fill="x", **pad)

        log_frame = ttk.LabelFrame(main, text="Log (shown on screen only, never saved)")
        log_frame.pack(fill="both", expand=True, **pad)
        self.log = tk.Text(log_frame, height=8, state="disabled", wrap="word")
        lsb = ttk.Scrollbar(log_frame, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=lsb.set)
        self.log.pack(side="left", fill="both", expand=True)
        lsb.pack(side="right", fill="y")

    # ------------------------------------------------------------ helpers

    def _write(self, line: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _set_files(self, files) -> None:
        seen = set()
        self.files = [f for f in files if not (f in seen or seen.add(f))]
        self.listbox.delete(0, "end")
        for f in self.files:
            self.listbox.insert("end", f)

    def _add_files(self) -> None:
        picked = filedialog.askopenfilenames(title="Select PDF files", filetypes=[("PDF files", "*.pdf")])
        self._set_files(self.files + collect_pdfs(picked))
        self._default_out()

    def _add_folder(self) -> None:
        folder = filedialog.askdirectory(title="Select a folder with PDF files")
        if folder:
            self._set_files(self.files + collect_pdfs([folder]))
            self._default_out()

    def _default_out(self) -> None:
        if self.files and not self.out_var.get():
            self.out_var.set(os.path.join(os.path.dirname(self.files[0]), "anonymized"))

    def _remove_selected(self) -> None:
        sel = set(self.listbox.curselection())
        self._set_files([f for i, f in enumerate(self.files) if i not in sel])

    def _clear(self) -> None:
        self._set_files([])

    def _browse_out(self) -> None:
        folder = filedialog.askdirectory(title="Select output folder")
        if folder:
            self.out_var.set(folder)

    def _load_list(self, widget: tk.Text) -> None:
        path = filedialog.askopenfilename(title="Select a text file (one term per line)",
                                          filetypes=[("Text files", "*.txt *.csv"), ("All files", "*.*")])
        if path:
            with open(path, encoding="utf-8-sig", errors="replace") as fh:
                widget.insert("end", ("\n" if widget.get("1.0", "end").strip() else "") + fh.read().strip())

    @staticmethod
    def _lines(widget: tk.Text) -> list:
        return [ln.strip() for ln in widget.get("1.0", "end").splitlines() if ln.strip()]

    def _options(self) -> Options:
        cats = {c for c, v in self.cat_vars.items() if v.get()} | {"CUSTOM"}
        det = DetectorConfig(categories=cats, custom_terms=self._lines(self.terms_text),
                             allow_terms=self._lines(self.allow_text), match_word_forms=self.forms_var.get())
        return Options(detector=det, style=self.style_var.get(), remove_images=self.images_var.get(),
                       rename_files=self.rename_var.get(), write_pdf=self.pdf_var.get(),
                       write_text=self.txt_var.get())

    def _busy(self, busy: bool) -> None:
        state = "disabled" if busy else "normal"
        self.run_btn.configure(state=state)
        self.preview_btn.configure(state=state)
        self.cancel_btn.configure(state="normal" if busy else "disabled")

    def _check_inputs(self, need_out: bool) -> bool:
        if not self.files:
            messagebox.showinfo(APP_NAME, "Add at least one PDF file first.")
            return False
        if need_out and not self.out_var.get().strip():
            messagebox.showinfo(APP_NAME, "Choose an output folder.")
            return False
        return True

    # ------------------------------------------------------------ actions

    def _run(self) -> None:
        if not self._check_inputs(need_out=True):
            return
        opts = self._options()
        if not (opts.write_pdf or opts.write_text):
            messagebox.showinfo(APP_NAME, "Select at least one output (PDF or text).")
            return
        self._save_settings()
        self.cancel.clear()
        self._busy(True)
        files, out = list(self.files), self.out_var.get().strip()
        self._write(f"Anonymising {len(files)} file(s) into {out}")

        def work():
            def progress(i, n, path):
                self.queue.put(("progress", i, n, path))
            try:
                results = run_batch(files, out, opts, progress=progress, cancelled=self.cancel.is_set,
                                    log=lambda m: self.queue.put(("log", m)))
                self.queue.put(("done", results, out))
            except Exception as exc:  # noqa: BLE001
                self.queue.put(("error", f"{type(exc).__name__}: {exc}"))

        self.worker = threading.Thread(target=work, daemon=True)
        self.worker.start()

    def _preview(self) -> None:
        if not self._check_inputs(need_out=False):
            return
        self.cancel.clear()
        self._busy(True)
        files, cfg = list(self.files), self._options().detector

        def work():
            analyses = []
            for i, f in enumerate(files):
                if self.cancel.is_set():
                    break
                self.queue.put(("progress", i, len(files), f))
                analyses.append(analyze(f, cfg, log=lambda m: self.queue.put(("log", m))))
            self.queue.put(("progress", len(analyses), len(files), None))
            self.queue.put(("preview", analyses))

        self.worker = threading.Thread(target=work, daemon=True)
        self.worker.start()

    def _poll(self) -> None:
        try:
            while True:
                msg = self.queue.get_nowait()
                kind = msg[0]
                if kind == "progress":
                    _, i, n, path = msg
                    self.progress.configure(maximum=max(n, 1), value=i)
                    if path:
                        self._write(f"[{i + 1}/{n}] {os.path.basename(path)}")
                elif kind == "log":
                    self._write(msg[1])
                elif kind == "done":
                    self._busy(False)
                    self._report(msg[1], msg[2])
                elif kind == "preview":
                    self._busy(False)
                    PreviewWindow(self, msg[1])
                elif kind == "error":
                    self._busy(False)
                    self._write("ERROR: " + msg[1])
                    messagebox.showerror(APP_NAME, msg[1])
        except queue.Empty:
            pass
        self.root.after(100, self._poll)

    def _report(self, results, out: str) -> None:
        ok = sum(r.status == "OK" for r in results)
        check = sum(r.status == "CHECK" for r in results)
        failed = sum(r.status == "FAILED" for r in results)
        for r in results:
            name = os.path.basename(r.source)
            found = ", ".join(f"{c}: {n}" for c, n in sorted(r.counts.items())) or "nothing found"
            self._write(f"{r.status:6} {name} → {os.path.basename(r.output_pdf or r.output_text or '-')}"
                        f" ({found})")
            if r.ocr_pages:
                self._write(f"       read with OCR: page(s) {', '.join(map(str, r.ocr_pages))}")
            if r.error:
                self._write(f"       error: {r.error}")
            for w in r.warnings:
                self._write(f"       warning: {w}")
            for page, cat, text in r.residues:
                self._write(f"       still visible on page {page}: {cat} '{text}'")
        self._write(f"Finished: {ok} OK, {check} to check, {failed} failed. Report: anonymization_report.csv")
        self.open_btn.configure(state="normal")
        if self.cancel.is_set():
            self._write("Cancelled by user.")
        summary = f"{ok} file(s) OK, {check} need a manual check, {failed} failed.\n\nOutput: {out}"
        if check or failed:
            summary += "\n\nSee the log for details. Always review the output before sharing it."
        messagebox.showinfo(APP_NAME, summary)

    # ------------------------------------------------------------ settings

    def _load_settings(self) -> None:
        try:
            with open(config_path(), encoding="utf-8") as fh:
                s = json.load(fh)
        except (OSError, ValueError):
            return
        for c, v in s.get("categories", {}).items():
            if c in self.cat_vars:
                self.cat_vars[c].set(bool(v))
        self.style_var.set(s.get("style", "label"))
        self.images_var.set(s.get("remove_images", False))
        self.rename_var.set(s.get("rename_files", True))
        self.pdf_var.set(s.get("write_pdf", True))
        self.txt_var.set(s.get("write_text", True))
        self.forms_var.set(s.get("match_word_forms", True))
        if s.get("allow_terms"):
            self.allow_text.insert("1.0", "\n".join(s["allow_terms"]))

    def _save_settings(self) -> None:
        # The "always anonymise" list is deliberately not saved: it usually holds
        # the very names that must not be left lying around.
        s = {
            "categories": {c: v.get() for c, v in self.cat_vars.items()},
            "style": self.style_var.get(),
            "remove_images": self.images_var.get(),
            "rename_files": self.rename_var.get(),
            "write_pdf": self.pdf_var.get(),
            "write_text": self.txt_var.get(),
            "match_word_forms": self.forms_var.get(),
            "allow_terms": self._lines(self.allow_text),
        }
        try:
            os.makedirs(os.path.dirname(config_path()), exist_ok=True)
            with open(config_path(), "w", encoding="utf-8") as fh:
                json.dump(s, fh, indent=2, ensure_ascii=False)
        except OSError:
            pass

    def _on_close(self) -> None:
        self.cancel.set()
        self._save_settings()
        self.root.destroy()


class PreviewWindow:
    """Lists every finding so the user can spot misses and false positives."""

    def __init__(self, app: App, analyses):
        self.app = app
        win = tk.Toplevel(app.root)
        win.title("Findings preview (not saved anywhere)")
        win.geometry("820x520")
        ttk.Label(win, padding=6, wraplength=780, text=(
            "Everything below will be anonymised. Select wrongly detected rows and click "
            "'Never anonymise selected'. Anything missing can be added under 'Your terms'.")).pack(anchor="w")
        frame = ttk.Frame(win)
        frame.pack(fill="both", expand=True, padx=6)
        cols = ("file", "page", "category", "text")
        self.tree = ttk.Treeview(frame, columns=cols, show="headings", selectmode="extended")
        for c, w in zip(cols, (220, 50, 110, 400)):
            self.tree.heading(c, text=c.capitalize())
            self.tree.column(c, width=w, stretch=c == "text")
        sb = ttk.Scrollbar(frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

        total = 0
        notes = []
        for a in analyses:
            name = os.path.basename(a.path)
            if a.error:
                notes.append(f"{name}: {a.error}")
                continue
            notes.extend(f"{name}: {w}" for w in a.warnings)
            for page, cat, text in a.items():
                self.tree.insert("", "end", values=(name, page, cat, text))
                total += 1
        bottom = ttk.Frame(win, padding=6)
        bottom.pack(fill="x")
        ttk.Label(bottom, text=f"{total} finding(s) in {len(analyses)} file(s)").pack(side="left")
        ttk.Button(bottom, text="Close", command=win.destroy).pack(side="right")
        ttk.Button(bottom, text="Never anonymise selected", command=self._allow).pack(side="right", padx=6)
        if notes:
            box = tk.Text(win, height=min(6, len(notes) + 1), wrap="word")
            box.insert("1.0", "\n".join(notes))
            box.configure(state="disabled")
            box.pack(fill="x", padx=6, pady=(0, 6))

    def _allow(self) -> None:
        texts = {self.tree.item(i, "values")[3] for i in self.tree.selection()}
        if not texts:
            return
        existing = set(App._lines(self.app.allow_text))
        new = [t for t in sorted(texts) if t not in existing]
        if new:
            sep = "\n" if existing else ""
            self.app.allow_text.insert("end", sep + "\n".join(new))
        for i in self.tree.selection():
            self.tree.delete(i)


def main() -> None:
    root = tk.Tk()
    try:
        ttk.Style().theme_use("vista" if sys.platform == "win32" else ttk.Style().theme_use())
    except tk.TclError:
        pass
    App(root)
    root.mainloop()
