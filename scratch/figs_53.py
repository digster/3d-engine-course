#!/usr/bin/env python3
"""scratch/figs_53.py — Lesson 5.3's diagrams.

Same rules as 5.2's: computed coordinates, no fill="..." on any <text>, every
`xs` line kept under ~110 characters so it cannot run past a 720-unit viewBox,
and no right-anchored label near x=0.
"""
import os

OUT = "scratch"

AMBER = "#f0961e"
BLUE = "#5082e6"
GREEN = "#5ac878"
RED = "#e05c5c"
PURPLE = "#9b7ede"
GREY = "#8f918a"


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def markers(uid):
    out = ["  <defs>"]
    for suffix, var in (("i", "--dia-ink"), ("s", "--dia-ink-soft"), ("h", "--dia-hi")):
        out.append(
            f'    <marker id="e-{suffix}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="var({var})"/></marker>')
    out.append(
        f'    <marker id="e-b-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
        f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
        f'<path d="M0,0 L10,5 L0,10 z" fill="{RED}"/></marker>')
    out.append("  </defs>")
    return "\n".join(out)


def svg(uid, w, h, title, desc, body):
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{uid}-t {uid}-d">\n'
            f'  <title id="{uid}-t">{esc(title)}</title>\n'
            f'  <desc id="{uid}-d">{esc(desc)}</desc>\n'
            f'{markers(uid)}\n' + "\n".join(body) + "\n</svg>\n")


def box(x, y, w, h, colour=None, dash=None, rx=3):
    fill = 'class="fill-soft grid"' if colour is None else f'fill="{colour}" stroke="{colour}"'
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {fill}'
            f' stroke-width="1.2"{d}/>')


def label(x, y, text, cls="sm", anchor="middle"):
    return f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">{esc(text)}</text>'


def arrow(x1, y1, x2, y2, uid, kind="s", width=1.2, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    cls = {"s": "ink-soft", "i": "ink", "h": "hi"}[kind]
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}"'
            f' stroke-width="{width}"{d} marker-end="url(#e-{kind}-{uid})"/>')


# ===========================================================================
# Figure 1 — one voice, then several
# ===========================================================================
def fig_one_voice():
    uid = "l53f1"
    W, H = 720, 404
    b = [label(20, 20, "197 logging calls. Before, every one of them was the same "
                       "statement.", "sm", "start")]

    # Before: everything funnels into one point.
    b.append(label(20, 46, "BEFORE", "xs t-hi", "start"))
    sources = ["platform", "gpu device", "shaders", "pipelines", "textures", "assets"]
    for i, name in enumerate(sources):
        y = 62 + i * 22
        b.append(box(20, y, 96, 17))
        b.append(label(68, y + 12, name, "xs"))
        b.append(arrow(120, y + 9, 178, 118, uid, "s", 1.0))

    b.append(box(182, 100, 150, 36, colour=GREY))
    b.append(label(257, 116, "SDL_Log(...)", "xs mono t-inv"))
    b.append(label(257, 129, "APPLICATION / info", "xs t-inv"))
    b.append(label(344, 112, "one category,", "xs muted", "start"))
    b.append(label(344, 126, "one level, no filter.", "xs muted", "start"))
    b.append(label(344, 140, "The only choices were", "xs t-bad", "start"))
    b.append(label(344, 154, "“all of it” and “none of it”.", "xs t-bad", "start"))

    # After: a grid.
    b.append(f'<line x1="20" y1="196" x2="700" y2="196" class="grid" stroke-width="1"/>')
    b.append(label(20, 216, "AFTER — a category says WHO is speaking; a level says "
                            "HOW MUCH IT MATTERS", "xs t-hi", "start"))

    cats = [("core", 7), ("platform", 14), ("gfx", 0), ("gpu", 56), ("asset", 8)]
    levels = [("error", 54, RED), ("info", 25, BLUE), ("warn", 6, AMBER),
              ("debug", 4, PURPLE), ("critical", 2, RED), ("trace", 1, GREEN)]

    b.append(label(20, 244, "by category", "xs muted", "start"))
    for i, (name, n) in enumerate(cats):
        y = 256 + i * 20
        b.append(label(20, y + 11, name, "xs mono", "start"))
        w = n * 3.4
        if w > 0:
            b.append(f'<rect x="96" y="{y}" width="{w}" height="13" rx="2" fill="{BLUE}"/>')
        b.append(label(96 + w + 6, y + 11, str(n), "xs mono", "start"))


    b.append(label(392, 244, "by level", "xs muted", "start"))
    for i, (name, n, colour) in enumerate(levels):
        y = 256 + i * 20
        b.append(label(392, y + 11, name, "xs mono", "start"))
        w = n * 3.4
        if w > 0:
            b.append(f'<rect x="452" y="{y}" width="{w}" height="13" rx="2" fill="{colour}"/>')
        b.append(label(452 + w + 6, y + 11, str(n), "xs mono", "start"))

    b.append(label(20, 380, "gfx is zero because the CPU rasterizer never logs — a fact "
                            "about it, not about the category.", "xs muted", "start"))
    b.append(label(20, 394, "The engine now defaults to silence and the demo does not. "
                            "That took no filtering code — see Figure 3.",
                   "xs muted", "start"))
    return svg(uid, W, H, "One voice, then several",
               "Before: six subsystems all funnelling into a single SDL_Log at "
               "application/info. After: two bar charts, the 92 engine calls "
               "broken down by category and by level.", b)


# ===========================================================================
# Figure 2 — the levels, and who logs
# ===========================================================================
def fig_levels():
    uid = "l53f2"
    W, H = 720, 434
    b = [label(20, 20, "A level is a promise about what the message means",
               "sm", "start")]

    rows = [
        ("critical", RED, "the program cannot continue", "2"),
        ("error", RED, "the operation did not happen, and the caller is being told so", "54"),
        ("warn", AMBER, "something went wrong and we carried on anyway", "6"),
        ("info", BLUE, "a fact worth having on a bug report", "25"),
        ("debug", PURPLE, "what a subsystem did, once per operation", "4"),
        ("trace", GREEN, "per-frame or per-item detail", "1"),
    ]
    top = 52
    # A 26-unit gap after `error`, so the default-cutoff caption has somewhere to
    # live that is not on top of a band.
    gap_after = 1
    for i, (name, colour, meaning, count) in enumerate(rows):
        y = top + i * 30 + (26 if i > gap_after else 0)
        b.append(f'<rect x="20" y="{y}" width="86" height="22" rx="3"'
                 f' fill="{colour}" stroke="{colour}"/>')
        b.append(label(63, y + 15, name, "xs mono t-inv"))
        b.append(label(118, y + 15, meaning, "xs", "start"))
        b.append(label(690, y + 15, count, "xs mono muted", "end"))

    # The default cutoff, drawn in the gap so it touches nothing.
    cut = top + 2 * 30 + 8
    b.append(f'<line x1="14" y1="{cut}" x2="700" y2="{cut}" stroke="{RED}"'
             f' stroke-width="1.6" stroke-dasharray="5 3"/>')
    b.append(label(20, cut + 18, "everything below this line is silent by default "
                                 "— `--log gpu=debug` raises it", "xs t-bad", "start"))

    b.append(f'<line x1="20" y1="274" x2="700" y2="274" class="grid" stroke-width="1"/>')
    b.append(label(20, 294, "AND THE RULE THAT DECIDES WHO SAYS IT", "xs t-hi", "start"))

    notes = [
        "Log the failure ONCE, at the deepest point that knows WHY — it has the path,",
        "the size, the SDL_GetError() string. Callers propagate silently and add only",
        "the CONSEQUENCE, which is the thing they know and the callee does not.",
        "",
        "The failure this avoids: five stack levels each logging “failed”, so one",
        "problem produces five lines and the most informative one is buried.",
        "The cost, stated honestly: a caller that RECOVERS still gets an error line",
        "it did not deserve. Code that is expected to fail logs at debug and says so.",
    ]
    for i, t in enumerate(notes):
        cls = "xs" if i in (0, 4) else "xs muted"
        b.append(label(20, 314 + i * 14, t, cls, "start"))
    return svg(uid, W, H, "The six levels and the ownership rule",
               "Six coloured level bands from critical down to trace with their "
               "meanings and call counts, a dashed line showing that everything "
               "below error is silent by default, and the rule that a failure is "
               "logged once at the deepest point that knows why.", b)


# ===========================================================================
# Figure 3 — the free payoff
# ===========================================================================
def fig_defaults():
    uid = "l53f3"
    # H must clear the last note line at y = 252 + 6*14 = 336.
    W, H = 720, 352
    b = [label(20, 20, "SDL's own defaults do the filtering for us",
               "sm", "start")]
    b.append(label(20, 38, "src/SDL_log.c, and the documented value of the "
                           "SDL_LOGGING hint:  app=info, assert=warn, test=verbose, *=error",
                   "xs muted", "start"))

    b.append(box(20, 62, 300, 26, colour=BLUE))
    b.append(label(170, 79, "SDL_LOG_CATEGORY_APPLICATION", "xs mono t-inv"))
    b.append(label(332, 79, "info  — a demo's SDL_Log prints", "xs", "start"))

    b.append(label(20, 112, "SDL's own categories", "xs muted", "start"))
    sdl_cats = ["ERROR", "ASSERT", "SYSTEM", "AUDIO", "VIDEO", "RENDER", "INPUT", "GPU"]
    for i, name in enumerate(sdl_cats):
        x = 20 + (i % 4) * 76
        y = 122 + (i // 4) * 24
        b.append(box(x, y, 70, 18))
        b.append(label(x + 35, y + 13, name, "xs mono muted"))

    b.append(f'<line x1="336" y1="112" x2="336" y2="176" class="grid" stroke-width="1"/>')
    b.append(label(348, 112, "SDL_LOG_CATEGORY_CUSTOM — and everything above it",
                   "xs muted", "start"))
    ours = ["core", "platform", "gfx", "gpu", "asset"]
    for i, name in enumerate(ours):
        x = 348 + (i % 3) * 76
        y = 122 + (i // 3) * 24
        b.append(box(x, y, 70, 18, colour=AMBER))
        b.append(label(x + 35, y + 13, name, "xs mono t-inv"))

    b.append(f'<rect x="20" y="188" width="678" height="26" rx="3" fill="{RED}"/>')
    b.append(label(359, 205, "every category SDL does not know about  →  error",
                   "xs mono t-inv"))

    b.append(f'<line x1="20" y1="232" x2="700" y2="232" class="grid" stroke-width="1"/>')
    notes = [
        "So moving the engine's messages off APPLICATION and onto our own categories",
        "makes the engine quiet by default. No configuration, no filter code, no wrapper.",
        "",
        "That is the strongest argument for using SDL's logger rather than wrapping it:",
        "a wrapper would have had to REIMPLEMENT this, and would have got a different answer.",
        "The engine's own diagnostics are one flag away: `--log platform=info`.",
        "That flag works on every program, because platform::start reads it.",
    ]
    for i, t in enumerate(notes):
        cls = "xs t-hi" if i == 0 else ("xs" if i == 3 else "xs muted")
        b.append(label(20, 252 + i * 14, t, cls, "start"))
    return svg(uid, W, H, "Where the default filtering comes from",
               "SDL's category list split into SDL's own categories and the five "
               "the engine adds above SDL_LOG_CATEGORY_CUSTOM, with a red band "
               "showing that everything SDL does not know defaults to error.", b)


# ===========================================================================
# Figure 4 — error or assertion
# ===========================================================================
def fig_error_or_assert():
    uid = "l53f4"
    W, H = 720, 356
    b = [label(20, 20, "One question tells you which of the two you are holding",
               "sm", "start")]

    b.append(box(180, 44, 360, 34, colour=BLUE))
    b.append(label(360, 65, "Could a CORRECT program, on a WORKING machine, hit this?",
                   "xs t-inv"))

    b.append(arrow(280, 82, 190, 116, uid, "s", 1.3))
    b.append(arrow(440, 82, 530, 116, uid, "s", 1.3))
    # Well clear of the arrows: at y=96 the left arrow is at x~243 and the right
    # one at x~477, so a label centred on either would sit on the stroke.
    b.append(label(198, 100, "yes", "xs mono t-hi"))
    b.append(label(528, 100, "no", "xs mono t-hi"))

    b.append(box(30, 120, 320, 30, colour=AMBER))
    b.append(label(190, 140, "AN ERROR — the world did it to you", "xs t-inv"))
    b.append(box(370, 120, 320, 30, colour=PURPLE))
    b.append(label(530, 140, "AN ASSERTION — your own code is wrong", "xs t-inv"))

    left = [
        "a file is missing",
        "a GPU refuses a format",
        "the user typed nonsense",
        "",
        "→ RETURN a failure the caller can act on",
        "→ survives into release: the world stays hostile",
        "→ engine::image_report, gpu_report, obj_report",
    ]
    right = [
        "an index is out of range",
        "a pointer that cannot be null is",
        "a function called before start()",
        "",
        "→ STOP: nothing sensible can be done, because",
        "   the code that would do it was written by the",
        "   same person who was already wrong",
    ]
    for i, t in enumerate(left):
        cls = "xs" if t.startswith("→") else "xs muted"
        b.append(label(30, 172 + i * 15, t, cls, "start"))
    for i, t in enumerate(right):
        cls = "xs" if t.startswith("→") else "xs muted"
        b.append(label(370, 172 + i * 15, t, cls, "start"))

    b.append(f'<line x1="20" y1="288" x2="700" y2="288" class="grid" stroke-width="1"/>')
    b.append(label(20, 306, "and if it IS an assertion, three macros and one rule each:",
                   "xs t-hi", "start"))
    macros = [
        ("ENGINE_ASSERT", "debug only. The default. Condition must have no side effects."),
        ("ENGINE_CHECK", "every build. Only where continuing is worse than stopping."),
        ("ENGINE_VERIFY", "the expression ALWAYS runs; the check is debug only."),
    ]
    for i, (name, meaning) in enumerate(macros):
        y = 320 + i * 14
        b.append(label(20, y, name, "xs mono t-hi", "start"))
        b.append(label(150, y, meaning, "xs muted", "start"))
    return svg(uid, W, H, "Error or assertion",
               "A decision box asking whether a correct program on a working "
               "machine could hit this, branching to error on the left and "
               "assertion on the right, with examples and the three assertion "
               "macros beneath.", b)


# ===========================================================================
# Figure 5 — the build matrix, measured
# ===========================================================================
def fig_build_matrix():
    uid = "l53f5"
    W, H = 720, 384
    b = [label(20, 20, "What survives which build — measured, not assumed",
               "sm", "start")]
    b.append(label(20, 38, "One translation unit, compiled four ways; each function's "
                           "code size read out of the object file.", "xs muted", "start"))

    cols = ["-O0", "-O0 -DNDEBUG", "-O2", "-O2 -DNDEBUG"]
    rows = [
        ("empty function", [20, 20, 4, 4], None),
        ("ENGINE_LOG_TRACE", [80, 28, 44, 4], "gated on NDEBUG (ours)"),
        ("ENGINE_LOG_DEBUG", [80, 28, 44, 4], "gated on NDEBUG (ours)"),
        ("ENGINE_LOG_INFO", [80, 80, 44, 44], "never compiled out"),
        ("ENGINE_ASSERT", [148, 148, 4, 4], "gated on __OPTIMIZE__ (SDL)"),
        ("ENGINE_CHECK", [148, 148, 100, 100], "survives everything"),
        ("ENGINE_VERIFY", [180, 180, 16, 16], "expression always runs"),
    ]
    base = [20, 20, 4, 4]

    x0, colw = 172, 96
    for j, c in enumerate(cols):
        b.append(label(x0 + j * colw + colw / 2, 66, c, "xs mono"))
    b.append(f'<line x1="20" y1="72" x2="700" y2="72" class="grid" stroke-width="1"/>')

    for i, (name, vals, note) in enumerate(rows):
        y = 78 + i * 26
        b.append(label(20, y + 16, name, "xs mono", "start"))
        for j, v in enumerate(vals):
            gone = (v == base[j])
            cx = x0 + j * colw
            colour = GREEN if gone and i > 0 else None
            b.append(box(cx + 6, y, colw - 12, 20, colour=colour))
            cls = "xs mono t-inv" if colour else "xs mono"
            b.append(label(cx + colw / 2, y + 14, f"{v} B", cls))
        if note:
            b.append(label(x0 + 4 * colw + 8, y + 14, note, "xs muted", "start"))

    b.append(f'<line x1="20" y1="266" x2="700" y2="266" class="grid" stroke-width="1"/>')
    b.append(label(20, 284, "GREEN = identical to an empty function: nothing was emitted "
                            "at all.", "xs t-hi", "start"))
    notes = [
        "READ COLUMN 3. `-O2` with no -DNDEBUG kills ENGINE_ASSERT — SDL decides its level from",
        "__OPTIMIZE__, not from NDEBUG — while our trace and debug logging is still fully compiled in.",
        "Column 2 is the exact inverse: live assertions, dead trace logging.",
        "Two gates, two different switches, and neither configuration is the one you would guess.",
        "That mismatch is also a real bug this lesson shipped and then fixed: ENGINE_VERIFY was",
        "guarded on NDEBUG, so at -O2 it compiled to 4 bytes and stopped evaluating its expression.",
    ]
    for i, t in enumerate(notes):
        cls = "xs" if i in (0, 4) else "xs muted"
        b.append(label(20, 302 + i * 13, t, cls, "start"))
    return svg(uid, W, H, "The build matrix, measured",
               "A table of seven macros against four build configurations, giving "
               "the bytes of code each emits, with the cells that emitted nothing "
               "highlighted.", b)


# ===========================================================================
# Figure 6 — the shape the engine already converged on
# ===========================================================================
def fig_report():
    uid = "l53f6"
    # H must clear the last note line at y = 262 + 7*14 = 360.
    W, H = 720, 378
    b = [label(20, 20, "Three subsystems, arrived at independently, the same shape",
               "sm", "start")]

    panels = [
        (20, "obj_report", "3.5", GREEN, ["obj_status status", "int line",
                                          "positions / uvs / normals", "faces / ngons",
                                          "vertices / triangles", "split_vertices",
                                          "bool ok()"]),
        (250, "gpu_report", "4.2", BLUE, ["gpu_status status", "const char* driver",
                                          "asked / granted formats", "swapchain_format",
                                          "present modes", "frames_in_flight",
                                          "bool ok()"]),
        (480, "image_report", "5.3", AMBER, ["image_status status", "int width, height",
                                             "source_channels", "size_t bytes",
                                             "size_t file_bytes", "",
                                             "bool ok()"]),
    ]
    for x, name, lesson, colour, fields in panels:
        b.append(box(x, 46, 210, 30, colour=colour))
        b.append(label(x + 105, 66, f"{name}   ({lesson})", "xs mono t-inv"))
        for i, f in enumerate(fields):
            if not f:
                continue
            y = 92 + i * 20
            cls = "xs mono t-hi" if (i == 0 or f.startswith("bool")) else "xs mono muted"
            b.append(label(x + 8, y, f, cls, "start"))

    b.append(f'<line x1="20" y1="242" x2="700" y2="242" class="grid" stroke-width="1"/>')
    notes = [
        "A STATUS that names the failure, the FACTS you ask for next, and an ok().",
        "The first two were written a module apart by nobody trying to match the other.",
        "",
        "So 5.3 does not invent an error type. It NAMES the one the engine kept",
        "reinventing, and converts the third case — which was the odd one out, a bare",
        "image_status with the interesting numbers thrown away.",
        "(std::expected is C++23; a hand-rolled result<T> is exercise 9.5, with the",
        "argument for why the codebase does not need one yet.)",
    ]
    for i, t in enumerate(notes):
        cls = "xs t-hi" if i == 0 else ("xs" if i == 3 else "xs muted")
        b.append(label(20, 262 + i * 14, t, cls, "start"))
    return svg(uid, W, H, "The report shape, converged on three times",
               "Three side-by-side field lists — obj_report, gpu_report and "
               "image_report — each beginning with a status enum, continuing with "
               "diagnostic facts, and ending with a bool ok().", b)


FIGS = {
    "l53_fig1.svg": fig_one_voice,
    "l53_fig2.svg": fig_levels,
    "l53_fig3.svg": fig_defaults,
    "l53_fig4.svg": fig_error_or_assert,
    # NUMBERED BY PAGE ORDER, not by the order they were written. The report
    # figure appears in §3.4 and the build matrix in §4.5, so they are 5 and 6
    # respectively — and the filename says so, because a filename that disagrees
    # with the figure number is how the two got swapped in the first place.
    "l53_fig5.svg": fig_report,
    "l53_fig6.svg": fig_build_matrix,
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
