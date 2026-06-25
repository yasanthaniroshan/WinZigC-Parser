#!/usr/bin/env python3
"""End-to-end optimization test driver.

Compiles a WinZigC source at every optimization level (-O O0, O1, O2), runs each
through the stack machine (scripts/machine.py), and verifies that every level's
output matches the expected file. Since O0 is the unoptimized reference, this
makes the suite fail if any optimization at O1 (dead-code elimination) or O2
(constant propagation + folding, on top of O1) ever changes a program's
observable behavior. winzigc always writes output.asm into its working
directory, so each compile runs in a throwaway temp dir.
"""
import argparse
import os
import pathlib
import subprocess
import sys
import tempfile


def _normalize(text):
    return "\n".join(line.rstrip() for line in text.strip().splitlines())


def _compile_and_run(winzigc, machine, source, level, stdin_data):
    """Compile `source` at the given -O level and run it. Returns (stdout, error)."""
    with tempfile.TemporaryDirectory() as tmp:
        compiled = subprocess.run(
            [os.path.abspath(winzigc), os.path.abspath(source), "-O", level],
            cwd=tmp, capture_output=True, text=True)
        if compiled.returncode != 0:
            return None, "winzigc -O %s failed (%d):\n%s\n%s" % (
                level, compiled.returncode, compiled.stdout, compiled.stderr)

        asm = os.path.join(tmp, "output.asm")
        if not os.path.exists(asm):
            return None, "winzigc -O %s produced no output.asm" % level

        run = subprocess.run(
            [sys.executable, os.path.abspath(machine), asm],
            input=stdin_data, capture_output=True, text=True)
        if run.returncode != 0:
            return None, "machine.py failed (%d) for -O %s:\n%s\n%s" % (
                run.returncode, level, run.stdout, run.stderr)
        return run.stdout, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--winzigc", required=True, help="path to the winzigc binary")
    ap.add_argument("--machine", required=True, help="path to scripts/machine.py")
    ap.add_argument("--source", required=True, help="the .winzig program to compile")
    ap.add_argument("--expected", required=True, help="file with the expected stdout")
    ap.add_argument("--stdin", default=None, help="optional file piped to the program")
    args = ap.parse_args()

    expected = pathlib.Path(args.expected).read_text()
    stdin_data = ""
    if args.stdin and os.path.exists(args.stdin):
        stdin_data = pathlib.Path(args.stdin).read_text()

    exp = _normalize(expected)
    ok = True
    for level in ("O0", "O1", "O2"):
        out, err = _compile_and_run(args.winzigc, args.machine, args.source, level, stdin_data)
        if err:
            sys.stderr.write(err + "\n")
            ok = False
            continue
        if _normalize(out) != exp:
            sys.stderr.write("%s OUTPUT MISMATCH for %s\n--- expected ---\n%s\n--- got ---\n%s\n"
                             % (level, args.source, expected, out))
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
