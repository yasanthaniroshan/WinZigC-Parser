#!/usr/bin/env python3
"""End-to-end VM test driver.

Compiles a WinZigC source with `winzigc`, runs the emitted stack-machine
assembly through `scripts/machine.py`, and compares stdout against an expected
output file. Used by CTest (see CMakeLists.txt) to exercise the frame-pointer
calling convention at runtime -- including recursion and nested live calls,
which the per-scope absolute-address scheme could not support.

`winzigc` always writes `output.asm` into its current working directory, so we
run it inside a throwaway temp dir to stay isolated and parallel-safe.
"""
import argparse
import os
import pathlib
import subprocess
import sys
import tempfile


def _normalize(text):
    return "\n".join(line.rstrip() for line in text.strip().splitlines())


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

    with tempfile.TemporaryDirectory() as tmp:
        compiled = subprocess.run(
            [os.path.abspath(args.winzigc), os.path.abspath(args.source)],
            cwd=tmp, capture_output=True, text=True)
        if compiled.returncode != 0:
            sys.stderr.write("winzigc failed (%d):\n%s\n%s\n"
                             % (compiled.returncode, compiled.stdout, compiled.stderr))
            return 1

        asm = os.path.join(tmp, "output.asm")
        if not os.path.exists(asm):
            sys.stderr.write("winzigc produced no output.asm in %s\n" % tmp)
            return 1

        run = subprocess.run(
            [sys.executable, os.path.abspath(args.machine), asm],
            input=stdin_data, capture_output=True, text=True)
        if run.returncode != 0:
            sys.stderr.write("machine.py failed (%d):\n%s\n%s\n"
                             % (run.returncode, run.stdout, run.stderr))
            return 1
        got = run.stdout

    if _normalize(got) != _normalize(expected):
        sys.stderr.write("OUTPUT MISMATCH for %s\n--- expected ---\n%s\n--- got ---\n%s\n"
                         % (args.source, expected, got))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
