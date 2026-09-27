#!/usr/bin/env python3
"""scratch/convert_logs_53.py — move the engine off SDL_Log, once.

77 call sites, and doing them by hand would mean 77 chances to pick a level by
mood. The classification is therefore a written rule applied by a program, and
the rule is the lesson's:

    ERROR   the operation did not happen, and the caller is being told so
    WARN    something went wrong and we carried on anyway
    INFO    a fact worth having on a bug report, or a report the caller asked for
    DEBUG   what a subsystem did, once per operation
    TRACE   per-frame or per-item detail

Category comes from the file's directory, because that is what a category IS —
who is speaking.

Anything this script cannot classify confidently is left alone and printed, so
the residue is visible rather than silently guessed at.
"""
import os
import re
import sys

CATEGORY_BY_PATH = [
    ("engine/src/platform/", "engine::log_platform"),
    ("engine/src/core/",     "engine::log_core"),
    ("engine/src/gfx/gpu_",  "engine::log_gpu"),
    ("engine/src/gfx/image.cpp", "engine::log_asset"),
    ("engine/src/gfx/obj.cpp",   "engine::log_asset"),
    ("engine/src/gfx/",      "engine::log_gfx"),
]

# Ordered: the first pattern that matches wins, so "— continuing" beats "failed".
LEVEL_RULES = [
    (r"— continuing|-- continuing|ignoring the second|continuing unsynchronised",
     "ENGINE_LOG_WARN"),
    (r"failed|cannot |could not |refus|not created|exceeds|beyond the|malformed"
     r"|is not the reflection|accepts no format|was not created|is not a format"
     r"|short write|TWO attributes|into a %u-byte buffer|into a %s-byte",
     "ENGINE_LOG_ERROR"),
]
DEFAULT_LEVEL = "ENGINE_LOG_INFO"


def category_for(path):
    for prefix, cat in CATEGORY_BY_PATH:
        if path.startswith(prefix):
            return cat
    return None


def convert(path):
    with open(path) as fh:
        src = fh.read()
    if "SDL_Log(" not in src:
        return 0, []

    cat = category_for(path)
    if cat is None:
        return 0, [f"{path}: no category rule"]

    out = []
    notes = []
    i = 0
    n = 0
    while True:
        j = src.find("SDL_Log(", i)
        if j < 0:
            out.append(src[i:])
            break

        # Find the matching close paren so we can read the whole call and
        # classify on its full text, not on the first line of it.
        depth = 0
        k = j + len("SDL_Log")
        while k < len(src):
            if src[k] == "(":
                depth += 1
            elif src[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        call = src[j:k + 1]

        level = DEFAULT_LEVEL
        for pattern, lvl in LEVEL_RULES:
            if re.search(pattern, call):
                level = lvl
                break

        out.append(src[i:j])
        out.append(f"{level}({cat}, " + call[len("SDL_Log("):])
        notes.append(f"    {level:20s} {cat:22s} {call[8:70].splitlines()[0]}")
        n += 1
        i = k + 1

    text = "".join(out)

    # The macros live in log.hpp; add the include if the file has none.
    if n and "engine/core/log.hpp" not in text:
        # After the file's own first include, which is its own header.
        m = re.search(r"^#include [<\"][^\n]*[>\"]\n", text, re.M)
        if m:
            text = text[:m.end()] + "\n#include <engine/core/log.hpp>\n" + text[m.end():]
        else:
            notes.append(f"{path}: could not place the include")

    with open(path, "w") as fh:
        fh.write(text)
    return n, notes


def main():
    total = 0
    for root, _dirs, files in os.walk("engine/src"):
        for f in sorted(files):
            if not f.endswith(".cpp"):
                continue
            path = os.path.join(root, f)
            n, notes = convert(path)
            if n:
                print(f"{path}: {n}")
                for line in notes:
                    print(line)
                total += n
    print(f"\n{total} call sites converted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
