#!/usr/bin/env python3
"""End-to-end optimization test driver.

Compiles a WinZigC source at BOTH -O O0 and -O O1, runs each through the
stack machine (scripts/machine.py), and verifies three things:

  1. the O0 (unoptimized, reference) output matches the expected file,
  2. the O1 (optimized) output matches the expected file,
  3. O0 and O1 produce identical output.

Check (3) is the important one: it makes the suite fail if any optimization
(dead-variable elimination, constant folding, frame compaction, ...) ever
changes a program's observable behavior. winzigc always writes output.asm into
its working directory, so each compile runs in a throwaway temp dir.
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

    out0, err0 = _compile_and_run(args.winzigc, args.machine, args.source, "O0", stdin_data)
    if err0:
        sys.stderr.write(err0 + "\n")
        return 1
    out1, err1 = _compile_and_run(args.winzigc, args.machine, args.source, "O1", stdin_data)
    if err1:
        sys.stderr.write(err1 + "\n")
        return 1

    exp, n0, n1 = _normalize(expected), _normalize(out0), _normalize(out1)
    ok = True
    if n0 != exp:
        sys.stderr.write("O0 OUTPUT MISMATCH for %s\n--- expected ---\n%s\n--- got ---\n%s\n"
                         % (args.source, expected, out0))
        ok = False
    if n1 != exp:
        sys.stderr.write("O1 OUTPUT MISMATCH for %s\n--- expected ---\n%s\n--- got ---\n%s\n"
                         % (args.source, expected, out1))
        ok = False
    if n0 != n1:
        sys.stderr.write("OPTIMIZATION CHANGED BEHAVIOR for %s (O0 != O1)\n--- O0 ---\n%s\n--- O1 ---\n%s\n"
                         % (args.source, out0, out1))
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
