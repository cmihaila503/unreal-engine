#!/usr/bin/env python3
"""loc_lint — finds text the player reads that can't be translated, and broken localization keys.

Usage:  python Tools/loc_lint.py Source/Murdar_GameDev [--strict]

Reports:
  RAW   FText::FromString(TEXT("...")) / FText::FromString(FString::Printf(TEXT("..."))) whose literal has words
        (letters and a space, or any Romanian diacritic): the player reads it, the translator never sees it.
  DIAC  a Romanian diacritic inside a TEXT("...") that isn't the argument of NSLOCTEXT/LOCTEXT (log lines in
        Romanian are fine to flag: they usually mean a display string took a detour through FString).
  DUP   the same (namespace, key) with two different source texts: one of them is lost at gather time.
Exit code: 1 when --strict and anything was reported.
Lines containing `loc-ok` are skipped (for the rare intended case).
"""
import os
import re
import sys

DIACRITICS = "ăâîșțşţĂÂÎȘȚŞŢ"
STR = r'"((?:[^"\\]|\\.)*)"'
RE_RAW = re.compile(r'FText::FromString\s*\(\s*(?:FString::Printf\s*\(\s*)?TEXT\s*\(\s*' + STR)
RE_TEXT = re.compile(r'TEXT\s*\(\s*' + STR + r'\s*\)')
RE_NSLOC = re.compile(r'NSLOCTEXT\s*\(\s*' + STR + r'\s*,\s*' + STR + r'\s*,\s*' + STR + r'\s*\)')
RE_LOC = re.compile(r'(?<!NS)LOCTEXT\s*\(\s*' + STR + r'\s*,\s*' + STR + r'\s*\)')
RE_NS_DEFINE = re.compile(r'#define\s+LOCTEXT_NAMESPACE\s+' + STR)


def has_words(s):
    """Something a person reads: a diacritic, or two letters separated by a space."""
    if any(c in DIACRITICS for c in s):
        return True
    return re.search(r'[A-Za-z]{2,}\s+[A-Za-z]{2,}', s) is not None


def lint_text(path, text):
    findings = []
    keys = []  # (namespace, key, source, line)
    namespace = None
    for n, line in enumerate(text.splitlines(), 1):
        if 'loc-ok' in line:
            continue
        m = RE_NS_DEFINE.search(line)
        if m:
            namespace = m.group(1)
        for m in RE_RAW.finditer(line):
            if has_words(m.group(1)):
                findings.append(('RAW', path, n, m.group(1)))
        loc_spans = [m.span() for m in RE_NSLOC.finditer(line)] + [m.span() for m in RE_LOC.finditer(line)]
        for m in RE_TEXT.finditer(line):
            inside = any(a <= m.start() < b for a, b in loc_spans)
            if not inside and any(c in DIACRITICS for c in m.group(1)) and not RE_RAW.search(line):
                findings.append(('DIAC', path, n, m.group(1)))
        for m in RE_NSLOC.finditer(line):
            keys.append((m.group(1), m.group(2), m.group(3), (path, n)))
        for m in RE_LOC.finditer(line):
            keys.append((namespace or '?', m.group(1), m.group(2), (path, n)))
    return findings, keys


def find_dups(keys):
    seen = {}
    dups = []
    for ns, key, src, where in keys:
        if (ns, key) in seen and seen[(ns, key)][0] != src:
            dups.append(('DUP', where[0], where[1], f'{ns}:{key} = "{src}" vs "{seen[(ns, key)][0]}" ({seen[(ns, key)][1][0]}:{seen[(ns, key)][1][1]})'))
        else:
            seen.setdefault((ns, key), (src, where))
    return dups


def lint_tree(root):
    findings, keys = [], []
    for base, _, files in os.walk(root):
        for f in files:
            if f.endswith(('.h', '.cpp')):
                p = os.path.join(base, f)
                with open(p, encoding='utf-8', errors='replace') as fh:
                    fs, ks = lint_text(p, fh.read())
                findings += fs
                keys += ks
    return findings + find_dups(keys)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    strict = '--strict' in argv
    results = lint_tree(argv[1])
    for kind, path, line, what in results:
        print(f'{kind:4} {path}:{line}  {what}')
    print(f'\n{len(results)} finding(s)')
    return 1 if strict and results else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
