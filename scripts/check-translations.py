#!/usr/bin/env python3
"""Check that every user-visible QML string is translated in po/ru.po.

No third-party modules. The QML extraction is regex-based so it also catches
`i18n.tr()` calls that xgettext might miss; xgettext is still run to prove the
template generation step works.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_po(path):
    entries = {}
    msgid = None
    msgstr = None
    mode = None

    def unquote(value):
        return json.loads(value)

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            if msgid is not None:
                entries[msgid] = msgstr or ""
            msgid = None
            msgstr = None
            mode = None
            continue
        if line.startswith("#"):
            continue
        if line.startswith("msgid "):
            if msgid is not None:
                entries[msgid] = msgstr or ""
            msgid = unquote(line[6:].strip())
            msgstr = ""
            mode = "id"
        elif line.startswith("msgstr "):
            msgstr = unquote(line[7:].strip())
            mode = "str"
        elif line.startswith('"'):
            text = unquote(line)
            if mode == "id":
                msgid += text
            elif mode == "str":
                msgstr += text

    if msgid is not None:
        entries[msgid] = msgstr or ""
    return entries


def qml_strings(root):
    pattern = re.compile(r'i18n\.tr\("((?:[^"\\]|\\.)*)"\)')
    strings = set()
    for path in sorted((root / "qml").rglob("*.qml")):
        text = path.read_text(encoding="utf-8")
        for match in pattern.finditer(text):
            strings.add(json.loads('"' + match.group(1) + '"'))
    return strings


def run_xgettext(root):
    sources = [str(path) for path in sorted((root / "qml").rglob("*.qml"))]
    with tempfile.TemporaryDirectory() as tmp:
        pot = Path(tmp) / "check.pot"
        result = subprocess.run(
            [
                "xgettext",
                "--from-code=UTF-8",
                "--language=JavaScript",
                "--keyword=tr",
                "--keyword=tr:1,2",
                "--keyword=i18n.tr",
                "--keyword=i18n.tr:1,2",
                "--keyword=_",
                "-o",
                str(pot),
                *sources,
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(result.stderr, file=sys.stderr)
            return False
    return True


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    po_path = root / "po" / "ru.po"

    if not po_path.exists():
        print(f"missing translation file: {po_path}", file=sys.stderr)
        return 1

    if not run_xgettext(root):
        return 1

    check = subprocess.run(
        ["msgfmt", "-c", "-o", os.devnull, str(po_path)],
        capture_output=True,
        text=True,
    )
    if check.returncode != 0:
        print(check.stderr, file=sys.stderr)
        return 1

    entries = parse_po(po_path)
    expected = qml_strings(root) | {
        "Battery Inspector",
        "Detailed battery telemetry for Ubuntu Touch",
    }

    missing = sorted(text for text in expected if text not in entries)
    untranslated = sorted(
        text for text in expected if text in entries and not entries[text].strip()
    )

    if missing or untranslated:
        for text in missing:
            print(f"missing: {text!r}")
        for text in untranslated:
            print(f"untranslated: {text!r}")
        return 1

    print(
        f"translations OK: {len(qml_strings(root))} UI strings + "
        f"2 desktop strings"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
