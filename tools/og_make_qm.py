#!/usr/bin/env python3
"""Compile the fork's Deep `.ts` translation sources into the shipped `.qm`.

The fork keeps its interface copy in `Telegram/Resources/qrc/deep/*.ts` and
ships the compiled `*.qm` files through `Telegram/Resources/qrc/deep.qrc`.
Qt Creator would normally re-run `lrelease` on every save, but the fork's build
on the CI image only consumes the `.qm` files, so this script pins the step
down and makes it reproducible outside an IDE: it finds a `lrelease` binary,
compiles every `.ts` next to it, and reports the result with its size and its
16-byte QM header.

Usage
-----
    python3 tools/og_make_qm.py                     # compile the default dir
    python3 tools/og_make_qm.py --check             # fail if a .qm is stale
    python3 tools/og_make_qm.py --lrelease PATH     # use this lrelease
    python3 tools/og_make_qm.py --verbose           # show lrelease output
    python3 tools/og_make_qm.py Telegram/Resources/qrc/deep

The script itself needs nothing but the Python standard library: `lrelease` is
searched on `PATH` first and then in the usual Qt / PySide / PyQt install
locations, or handed in explicitly with `--lrelease`.

Exit status is 0 when every `.ts` compiled (and, with `--check`, when every
`.qm` already matches what would be generated), 1 otherwise.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TS_DIR = REPO_ROOT / "Telegram" / "Resources" / "qrc" / "deep"

# Names the Qt / PySide installers use for the same tool. `lrelease` first so a
# real Qt install wins over a Python wheel.
LRELEASE_NAMES = (
    "lrelease",
    "lrelease6",
    "lrelease-qt6",
    "lrelease5",
    "lrelease-qt5",
    "pyside6-lrelease",
    "pyside2-lrelease",
)

# Locations an installer uses that are not necessarily on PATH. Globs are
# expanded at runtime; the Windows entries only match on Windows.
LRELEASE_GLOBS = (
    "/usr/local/opt/qt*/bin/lrelease",
    "/opt/homebrew/opt/qt*/bin/lrelease",
    "/usr/lib/qt6/bin/lrelease",
    "/usr/lib/x86_64-linux-gnu/qt6/bin/lrelease",
    "~/Qt/*/gcc_64/bin/lrelease",
    "~/Qt/*/macos/bin/lrelease",
    "~/.local/share/pyside6*/lrelease",
    "C:/Qt/*/*/bin/lrelease.exe",
)

def find_lrelease(explicit):
    """Return the path of a usable lrelease, or None."""
    if explicit:
        path = Path(explicit).expanduser()
        if path.is_file():
            return path
        found = shutil.which(explicit)
        if found:
            return Path(found)
        sys.stderr.write("error: no lrelease at {}\n".format(explicit))
        return None

    for name in LRELEASE_NAMES:
        found = shutil.which(name)
        if found:
            return Path(found)

    for pattern in LRELEASE_GLOBS:
        expanded = str(Path(pattern).expanduser())
        if not Path(expanded).is_absolute():
            continue
        for candidate in sorted(Path("/").glob(expanded.lstrip("/"))):
            if candidate.is_file():
                return candidate
    return None


def read_header(path, size=16):
    with open(path, "rb") as handle:
        return handle.read(size).hex()


def compile_one(lrelease, ts_path, qm_path, verbose):
    """Run lrelease for one .ts; return (ok, message)."""
    command = [str(lrelease), str(ts_path), "-qm", str(qm_path)]
    result = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    output = (result.stdout or "").strip()
    if verbose and output:
        print("    " + output.replace("\n", "\n    "))
    if result.returncode != 0:
        return False, "lrelease failed ({}): {}".format(
            result.returncode, output or "no output"
        )
    if not qm_path.is_file():
        return False, "lrelease produced no {}".format(qm_path.name)
    return True, output


def format_report(ts_path, qm_path):
    size = qm_path.stat().st_size
    return "{} -> {} ({} B, head {})".format(
        ts_path.name, qm_path.name, size, read_header(qm_path)
    )


def main():
    parser = argparse.ArgumentParser(
        description="Compile the Deep .ts sources into the shipped .qm files."
    )
    parser.add_argument(
        "directory",
        nargs="?",
        default=str(DEFAULT_TS_DIR),
        help="directory holding the .ts sources (default: %(default)s)",
    )
    parser.add_argument(
        "--lrelease",
        default=None,
        help="path to lrelease (default: search PATH and the usual Qt locations)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compile into a temporary directory and fail if a committed .qm "
        "differs from the fresh build",
    )
    parser.add_argument("--verbose", action="store_true", help="show lrelease output")
    args = parser.parse_args()

    ts_dir = Path(args.directory).expanduser().resolve()
    if not ts_dir.is_dir():
        sys.stderr.write("error: {} is not a directory\n".format(ts_dir))
        return 1

    sources = sorted(ts_dir.glob("*.ts"))
    if not sources:
        sys.stderr.write("error: no .ts file in {}\n".format(ts_dir))
        return 1

    lrelease = find_lrelease(args.lrelease)
    if lrelease is None:
        sys.stderr.write(
            "error: no lrelease found. Install Qt (or PySide6) so that one of\n"
            "       {} is on PATH, or pass --lrelease PATH.\n".format(
                ", ".join(LRELEASE_NAMES)
            )
        )
        return 1

    print("lrelease: {}".format(lrelease))
    print("sources : {}".format(ts_dir))

    failures = 0
    stale = 0
    with tempfile.TemporaryDirectory(prefix="og_make_qm.") as tmp:
        for ts_path in sources:
            fresh = Path(tmp) / (ts_path.stem + ".qm")
            ok, message = compile_one(lrelease, ts_path, fresh, args.verbose)
            if not ok:
                print("FAIL {}".format(ts_path.name))
                print("     " + message)
                failures += 1
                continue

            committed = ts_dir / (ts_path.stem + ".qm")
            if args.check:
                if not committed.is_file():
                    print("MISSING {}".format(committed.name))
                    stale += 1
                    continue
                if committed.read_bytes() != fresh.read_bytes():
                    print("STALE   {}".format(committed.name))
                    stale += 1
                    continue
                print("OK      {} ({} B, head {})".format(
                    committed.name, committed.stat().st_size, read_header(committed)
                ))
                continue

            shutil.copyfile(fresh, committed)
            print("WROTE   " + format_report(ts_path, committed))

    if failures or stale:
        print("done: {} failed, {} stale, {} source(s)".format(
            failures, stale, len(sources)
        ))
        return 1
    print("done: {} source(s) OK".format(len(sources)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
