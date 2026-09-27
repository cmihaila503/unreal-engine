#!/usr/bin/env python3
"""run_rule_tests — compiles and runs every pure rule test in Handoff/*/Tests (and the Python ones).

Usage:  python Tools/run_rule_tests.py [Handoff dir] [--filter Name]

Each C++ test's opening comment holds its own build command ("// g++ -std=c++17 ... -I../Source/... file.cpp -o x && ./x");
the -I paths are taken from there (relative to the test's folder). Compiler: $CXX, else g++, clang++, or MSVC cl
(from a Developer Command Prompt). Warnings are errors. Exit code 1 if anything fails.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile


def compiler():
    for c in [os.environ.get('CXX'), 'g++', 'clang++', 'cl']:
        if c and shutil.which(c):
            return c
    return None


def includes_of(test_path):
    # The build command is in the file's opening comment (first few lines).
    with open(test_path, encoding='utf-8', errors='replace') as f:
        head = ''.join(f.readline() for _ in range(4))
    return re.findall(r'-I\s*(\S+)', head)


def build_cmd(cxx, src, incs, exe):
    if os.path.basename(cxx).lower().startswith('cl'):
        return [cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/utf-8'] + ['/I' + i for i in incs] + [src, '/Fe' + exe]
    return [cxx, '-std=c++17', '-Wall', '-Wextra', '-Wshadow', '-Werror'] + ['-I' + i for i in incs] + [src, '-o', exe]


def main(argv):
    root = argv[1] if len(argv) > 1 and not argv[1].startswith('--') else os.path.join(os.path.dirname(__file__), '..', '..')
    only = argv[argv.index('--filter') + 1] if '--filter' in argv else None
    cxx = compiler()
    if not cxx:
        print('No C++ compiler found (set CXX, or run from a Developer Command Prompt for cl).')
        return 1
    failed, passed = [], 0
    tmp = tempfile.mkdtemp(prefix='murdar_rules_')
    for system in sorted(os.listdir(root)):
        tests = os.path.join(root, system, 'Tests')
        if not os.path.isdir(tests) or (only and only.lower() not in system.lower()):
            continue
        for f in sorted(os.listdir(tests)):
            src = os.path.join(tests, f)
            if f.endswith('.cpp'):
                exe = os.path.join(tmp, system + '_' + f[:-4] + ('.exe' if os.name == 'nt' else ''))
                incs = includes_of(src)
                b = subprocess.run(build_cmd(cxx, f, incs, exe), cwd=tests, capture_output=True, text=True)
                if b.returncode != 0:
                    failed.append((system, f, 'BUILD', (b.stdout + b.stderr)[-2000:]))
                    continue
                r = subprocess.run([exe], cwd=tests, capture_output=True, text=True)
                last = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ''
                print(f'{system:18} {f:32} {last}')
                if r.returncode != 0:
                    failed.append((system, f, 'RUN', r.stdout[-2000:]))
                else:
                    passed += 1
            elif f.startswith('test_') and f.endswith('.py'):
                r = subprocess.run([sys.executable, '-m', 'unittest', '-q', f[:-3]], cwd=tests, capture_output=True, text=True)
                print(f'{system:18} {f:32} {"OK" if r.returncode == 0 else "FAILED"}')
                if r.returncode != 0:
                    failed.append((system, f, 'RUN', r.stderr[-2000:]))
                else:
                    passed += 1
    print(f'\n{passed} test file(s) passed, {len(failed)} failed')
    for system, f, stage, out in failed:
        print(f'\n--- {system}/{f} ({stage}) ---\n{out}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
