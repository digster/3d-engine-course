#!/usr/bin/env python3
"""scratch/figs_52.py — Lesson 5.2's diagrams.

Coordinates are computed rather than typed, so a box and the text inside it
cannot drift apart. Everything is theme-driven: no `fill="..."` on any <text>
(course.css's `figure.dia svg text` rule always beats a presentation attribute),
and shapes use the class vocabulary from docs/_template/README.md §7.
"""
import os

OUT = "scratch"

# The five accent colours the course's diagrams already use, so a reader who has
# followed the fold-out figures in 5.1 sees the same vocabulary.
AMBER = "#f0961e"
BLUE = "#5082e6"
GREEN = "#5ac878"
RED = "#e05c5c"
PURPLE = "#9b7ede"


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def markers(uid):
    out = ["  <defs>"]
    for suffix, var in (("i", "--dia-ink"), ("s", "--dia-ink-soft"), ("h", "--dia-hi")):
        out.append(
            f'    <marker id="e-{suffix}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="var({var})"/></marker>')
    # A literal colour rather than a theme token, because this arrowhead means
    # "the shape we are forbidding" in both light and dark — the same reason
    # `t-inv` is a fixed white.
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
            f'{markers(uid)}\n'
            + "\n".join(body) + "\n</svg>\n")


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
# Figure 1 — the fork
# ===========================================================================
def fig_fork():
    uid = "l52f1"
    W, H = 720, 400
    b = [label(20, 20, "The same six phases. The only difference is who says "
                       "`while`.", "sm", "start")]

    panels = [
        (20, "LIBRARY — your program calls the engine", "main() { while (running) { … } }",
         BLUE, True),
        (380, "FRAMEWORK — the engine calls your program", "SDL_EnterAppMainCallbacks(…)",
         AMBER, False),
    ]

    phases = ["drain events", "tick clock + input", "N x fixed step",
              "render", "blit + overlay", "present"]

    for px, heading, owner, colour, yours in panels:
        pw = 320
        b.append(label(px, 46, heading, "sm", "start"))

        # The OUTER box is the loop's owner. Whose box the `while` is drawn
        # inside is the entire content of this figure.
        b.append(box(px, 58, pw, 300, dash="4 3"))
        b.append(f'<rect x="{px}" y="58" width="{pw}" height="22" rx="3"'
                 f' fill="{colour}" stroke="{colour}" stroke-width="1.2"/>')
        b.append(label(px + pw / 2, 73, owner, "xs mono t-inv"))
        b.append(label(px + pw / 2, 94,
                       "your code" if yours else "SDL3, inside the library",
                       "xs muted"))

        for i, name in enumerate(phases):
            y = 106 + i * 38
            b.append(box(px + 26, y, pw - 52, 28))
            b.append(label(px + pw / 2, y + 18, name, "xs"))
            if i:
                b.append(arrow(px + pw / 2, y - 10, px + pw / 2, y - 2, uid, "s", 1.0))

        b.append(label(px + pw / 2, 350,
                       "the `while` is in THIS box" if yours
                       else "the `while` is in SDL's box", "xs t-hi"))

    b.append(f'<line x1="360" y1="58" x2="360" y2="358" class="grid" stroke-width="1"/>')
    b.append(label(20, 380, "Both are correct SDL3. The right-hand one is the only "
                            "one that also works where the OS owns the loop — "
                            "the web, and mobile.", "xs", "start"))
    b.append(label(20, 394, "engine::platform is the left arrangement; engine::app is "
                            "the right one, and it is BUILT ON the left (Figure 5).",
                   "xs muted", "start"))
    return svg(uid, W, H, "Library versus framework",
               "Two panels showing the same six frame phases. On the left the loop "
               "lives in your main(); on the right it lives inside SDL, which calls "
               "your four callbacks.", b)


# ===========================================================================
# Figure 2 — one frame, under the callbacks
# ===========================================================================
def fig_frame():
    uid = "l52f2"
    W, H = 720, 386
    b = [label(20, 20, "One frame under SDL_MAIN_USE_CALLBACKS, top to bottom",
               "sm", "start")]

    # ---- band 1: what SDL does ------------------------------------------
    lane_sdl = 50
    b.append(label(20, lane_sdl - 8, "SDL3 owns the schedule", "xs muted", "start"))
    sdl_steps = [(20, 150, "SDL_PumpEvents()", GREEN),
                 (180, 230, "dispatch each queued event", GREEN),
                 (418, 282, "SDL_AppIterate(appstate)", AMBER)]
    for x, w, name, colour in sdl_steps:
        b.append(box(x, lane_sdl, w, 30, colour=colour))
        b.append(label(x + w / 2, lane_sdl + 19, name, "xs mono t-inv"))

    # ---- band 2: the one hook that is per-EVENT -------------------------
    lane_ev = 116
    b.append(arrow(295, lane_sdl + 32, 295, lane_ev - 4, uid, "s", 1.0))
    b.append(box(180, lane_ev, 230, 30, colour=BLUE))
    b.append(label(295, lane_ev + 19, "on_event(e)   x N", "xs mono t-inv"))
    b.append(label(424, lane_ev + 12, "once per event, before the frame",
                   "xs muted", "start"))
    b.append(label(424, lane_ev + 26, "it belongs to", "xs muted", "start"))

    # ---- band 3: everything inside one iterate --------------------------
    lane_it = 196
    b.append(arrow(654, lane_sdl + 32, 654, lane_it - 26, uid, "s", 1.0))
    b.append(label(20, lane_it - 12, "one call to SDL_AppIterate expands to:",
                   "xs muted", "start"))

    phases = [("begin_frame", "clock + input"),
              ("on_fixed_step", "x N  (0..8)"),
              ("on_frame", "alpha"),
              ("blit_framebuffer", "our pixels"),
              ("on_overlay + present", "HUD, then vsync")]
    pw, gap = 130.0, 7.0
    for i, (name, sub) in enumerate(phases):
        x = 20 + i * (pw + gap)
        b.append(box(x, lane_it, pw, 44, colour=BLUE))
        b.append(label(x + pw / 2, lane_it + 19, name, "xs mono t-inv"))
        b.append(label(x + pw / 2, lane_it + 33, sub, "xs t-inv"))

    b.append(f'<line x1="20" y1="{lane_it + 58}" x2="698" y2="{lane_it + 58}"'
             f' class="grid" stroke-width="1"/>')

    y = lane_it + 78
    notes = [
        ("Every queued event is delivered BEFORE the frame it belongs to,", "xs t-hi"),
        ("so Lesson 1.2's “drain, then update()” contract still holds — "
         "SDL enforces it now instead of your loop.", "xs muted"),
        ("Verified in SDL 3.4.12: SDL_IterateMainCallbacks() calls SDL_PumpEvents(),",
         "xs"),
        ("then SDL_DispatchMainCallbackEvents(), then the iterate callback "
         "— src/main/SDL_main_callbacks.c.", "xs muted"),
        ("The ordering is not a promise in a doc comment; it is three lines "
         "in that order.", "xs muted"),
    ]
    for i, (t, cls) in enumerate(notes):
        b.append(label(20, y + i * 16, t, cls, "start"))

    b.append(label(20, y + len(notes) * 16 + 6,
                   "Nothing about Lesson 1.4's loop changed. What changed is who "
                   "owns the `while`.", "xs t-hi", "start"))
    return svg(uid, W, H, "One frame under the SDL callbacks",
               "Three bands: SDL pumps and dispatches events then calls iterate; "
               "on_event runs once per event; and one iterate expands into "
               "begin_frame, the fixed steps, on_frame, the blit, on_overlay and "
               "present.", b)


# ===========================================================================
# Figure 3 — lifecycle, and where each surface stops
# ===========================================================================
def fig_lifecycle():
    uid = "l52f3"
    W, H = 720, 372
    b = [label(20, 20, "Created downwards, destroyed upwards — and the three "
                       "surfaces are three stopping points", "sm", "start")]

    rungs = [("SDL_Init(flags)", "video only when the surface needs it"),
             ("SDL_CreateWindow", "one window, and nobody owns it yet"),
             ("SDL_CreateRenderer", "CLAIMS the window (Lesson 4.2)"),
             ("SDL_CreateTexture", "streaming, ARGB8888, framebuffer-sized")]

    top = 56
    for i, (call, note) in enumerate(rungs):
        y = top + i * 56
        b.append(box(170, y, 210, 34))
        b.append(label(275, y + 21, call, "xs mono"))
        b.append(label(396, y + 21, note, "xs muted", "start"))
        if i:
            b.append(arrow(240, y - 20, 240, y - 4, uid, "i", 1.4))
            b.append(arrow(310, y + 4, 310, y - 20, uid, "s", 1.4))

    b.append(label(206, top - 26, "create", "xs", "middle"))
    b.append(label(330, top - 26, "destroy", "xs muted", "middle"))

    # Where each surface stops.
    stops = [("headless", 0, GREEN, "no window at all — and no SDL_INIT_VIDEO,"
                                    " so it runs on a build server"),
             ("gpu", 1, PURPLE, "a window claimed by NOBODY, ready for "
                                "gpu_device::create"),
             ("renderer", 3, BLUE, "the whole ladder: Module 1's presentation path")]
    for name, idx, colour, note in stops:
        y = top + idx * 56 + 34
        b.append(f'<line x1="90" y1="{y + 6}" x2="164" y2="{y + 6}" stroke="{colour}"'
                 f' stroke-width="2" stroke-dasharray="3 2"/>')
        b.append(f'<circle cx="164" cy="{y + 6}" r="4" fill="{colour}"/>')
        b.append(label(20, y + 10, name, "xs mono", "start"))

    b.append(f'<line x1="20" y1="296" x2="700" y2="296" class="grid" stroke-width="1"/>')
    for i, (name, _idx, colour, note) in enumerate(stops):
        y = 316 + i * 16
        b.append(f'<rect x="20" y="{y - 8}" width="8" height="8" fill="{colour}"/>')
        b.append(label(36, y, f"{name}: {note}", "xs muted", "start"))
    return svg(uid, W, H, "The lifecycle ladder and the three surfaces",
               "Four creation steps drawn as a ladder with a downward create arrow "
               "and an upward destroy arrow, and three dashed markers showing where "
               "the headless, gpu and renderer surfaces each stop.", b)


# ===========================================================================
# Figure 4 — the measurement
# ===========================================================================
def fig_boilerplate():
    uid = "l52f4"
    W, H = 720, 380
    b = [label(20, 20, "Lifecycle SDL calls a demo has to make for itself",
               "sm", "start")]
    b.append(label(20, 36, "SDL_Init, SDL_Quit, CreateWindow/Renderer/Texture and "
                           "their three Destroys, SetRenderVSync,", "xs muted", "start"))
    b.append(label(20, 48, "SetTextureScaleMode, Lock/UnlockTexture, PollEvent, "
                           "RenderClear/Texture/Present, SetAppMetadata, GetVersion.",
                   "xs muted", "start"))

    # `None` means "this program did not exist", which is a different statement
    # from zero and must not be drawn as a bar of any length.
    rows = [("hello_cube", 17, 0), ("pong", None, 0), ("sandbox", 31, 3)]
    scale = 15.0
    x0 = 130
    top = 88
    for i, (name, before, after) in enumerate(rows):
        y = top + i * 62
        b.append(label(x0 - 12, y + 14, name, "xs mono", "end"))
        b.append(label(x0 - 12, y + 40, "", "xs", "end"))

        for j, (val, colour, tag) in enumerate(((before, RED, "before"),
                                                (after, GREEN, "after"))):
            yy = y + j * 24
            if val is None:
                b.append(label(x0, yy + 12, "—   did not exist yet", "xs muted", "start"))
                continue
            # Zero gets a zero-length bar and its label alone. A minimum width
            # would draw "none" as "a few", which is the one thing this chart
            # exists to distinguish.
            w = val * scale
            if w > 0:
                b.append(f'<rect x="{x0}" y="{yy}" width="{w}" height="16" rx="2"'
                         f' fill="{colour}"/>')
            b.append(label(x0 + w + 8, yy + 12, f"{val}   {tag}", "xs mono", "start"))

    b.append(label(x0 - 12, top + 3 * 62 + 2, "pong was a branch inside sandbox's "
                                              "[Tab] switch, not a program.",
                   "xs muted", "start"))

    b.append(f'<line x1="20" y1="{top + 3 * 62 + 18}" x2="700" y2="{top + 3 * 62 + 18}"'
             f' class="grid" stroke-width="1"/>')
    yb = top + 3 * 62 + 40
    b.append(label(20, yb, "48 across the three demos, before. 3 after — and all "
                           "three are SDL_PollEvent, in sandbox, on purpose.",
                   "xs t-hi", "start"))
    b.append(label(20, yb + 16, "The layer that removed 45 of them is 428 lines of "
                                "code across five files. It is not free; it is paid "
                                "for once.", "xs muted", "start"))
    b.append(label(20, yb + 32, "Counted by scratch/measure_52.py, which strips "
                                "comments before it counts:", "xs muted", "start"))
    b.append(label(20, yb + 46, "the first run reported a sentence ABOUT SDL_Init "
                                "as a call to it.", "xs muted", "start"))
    return svg(uid, W, H, "Boilerplate removed, measured",
               "Paired bars per demo showing lifecycle SDL calls before and after: "
               "hello_cube 17 to 0, pong 0, sandbox 31 to 3.", b)


# ===========================================================================
# Figure 5 — ownership across the C boundary
# ===========================================================================
def fig_ownership():
    uid = "l52f5"
    W, H = 720, 252
    b = [label(20, 20, "One C++ object, handed through a C API and back",
               "sm", "start")]

    steps = [(20, 150, "make_unique<my_app>", "ENGINE_MAIN", BLUE),
             (190, 130, "release()", "we let go", AMBER),
             (350, 150, "SDL holds a void*", "for the program's life", RED),
             (540, 160, "unique_ptr(ptr)", "app_runner::quit", GREEN)]
    y = 58
    for i, (x, w, name, sub, colour) in enumerate(steps):
        b.append(box(x, y, w, 44, colour=colour))
        b.append(label(x + w / 2, y + 19, name, "xs mono t-inv"))
        b.append(label(x + w / 2, y + 33, sub, "xs t-inv"))
        if i:
            px, pw = steps[i - 1][0], steps[i - 1][1]
            b.append(arrow(px + pw + 3, y + 22, x - 4, y + 22, uid, "i", 1.4))

    b.append(f'<rect x="350" y="{y + 54}" width="180" height="3" fill="{RED}"/>')
    b.append(label(350, y + 76, "unowned — and this window contains",
                   "xs t-bad", "start"))
    b.append(label(350, y + 90, "NO BRANCHES AT ALL.", "xs t-bad", "start"))

    b.append(f'<line x1="20" y1="172" x2="700" y2="172" class="grid" stroke-width="1"/>')
    lines = [
        "app_runner::init publishes the pointer BEFORE anything can fail, which is "
        "the part that looks wrong and is not.",
        "The alternative — publish only on success — means a failed start "
        "must destroy the instance itself, and SDL then calls",
        "SDL_AppQuit(nullptr) with no way to tell “never built” from "
        "“already cleaned up”. One owner, one teardown path.",
        "This is the one place in the engine where a raw pointer owns something, "
        "and it owns it for exactly as long as C is holding it.",
    ]
    for i, t in enumerate(lines):
        b.append(label(20, 194 + i * 16, t, "xs" if i == 0 else "xs muted", "start"))
    return svg(uid, W, H, "Ownership across the C boundary",
               "Four steps: make_unique in the macro, release, SDL holding a raw "
               "void pointer for the life of the program, and app_runner::quit "
               "wrapping it back into a unique_ptr.", b)


# ===========================================================================
# Figure 6 — the layering rule
# ===========================================================================
def fig_layers():
    uid = "l52f6"
    W, H = 720, 340
    b = [label(20, 20, "app is built ON platform — never beside it",
               "sm", "start")]

    tiers = [
        (58, [("hello_cube", 90), ("pong", 60), ("sandbox", 80)], None),
        (128, [("engine::app", 150)], AMBER),
        (198, [("engine::platform", 170)], BLUE),
        (268, [("SDL3::SDL3", 140)], None),
    ]
    centres = {}
    for y, items, colour in tiers:
        total = sum(w for _n, w in items) + 40 * (len(items) - 1)
        x = 250 - total / 2 + 60
        for name, w in items:
            b.append(box(x, y, w, 34, colour=colour))
            cls = "xs mono t-inv" if colour else "xs mono"
            b.append(label(x + w / 2, y + 21, name, cls))
            centres[name] = (x + w / 2, y, y + 34)
            x += w + 40

    for src, dst in (("hello_cube", "engine::app"), ("pong", "engine::app"),
                     ("engine::app", "engine::platform"),
                     ("engine::platform", "SDL3::SDL3")):
        sx, _sy, sb = centres[src]
        dx, dt, _db = centres[dst]
        b.append(arrow(sx, sb + 3, dx, dt - 5, uid, "s", 1.2))

    # sandbox skips app entirely, which is the point.
    sx, _sy, sb = centres["sandbox"]
    dx, dt, _db = centres["engine::platform"]
    b.append(arrow(sx, sb + 3, dx + 60, dt - 5, uid, "h", 1.4))
    b.append(label(452, 116, "sandbox skips engine::app", "xs t-hi", "start"))
    b.append(label(452, 130, "and keeps its own main().", "xs muted", "start"))
    b.append(label(452, 144, "That has to stay possible,", "xs muted", "start"))
    b.append(label(452, 158, "or the library is a lie.", "xs muted", "start"))

    # The forbidden arrow: app reaching PAST platform, straight to SDL. It must
    # actually arrive at SDL3 — an arrow into empty space depicts nothing — so it
    # routes around platform's left edge and ends inside the SDL3 box.
    b.append(f'<path d="M 232 148 L 132 214 L 236 280" fill="none" stroke="{RED}"'
             f' stroke-width="1.4" stroke-dasharray="4 3"'
             f' marker-end="url(#e-b-{uid})"/>')
    b.append(f'<line x1="112" y1="200" x2="140" y2="228" stroke="{RED}" stroke-width="2.2"/>')
    b.append(f'<line x1="140" y1="200" x2="112" y2="228" stroke="{RED}" stroke-width="2.2"/>')
    b.append(label(20, 252, "app reaching around", "xs t-bad", "start"))
    b.append(label(20, 266, "platform, straight to SDL", "xs t-bad", "start"))

    b.append(f'<line x1="20" y1="308" x2="700" y2="308" class="grid" stroke-width="1"/>')
    b.append(label(20, 328, "verify_52 §E renders six frames down each path and "
                            "compares the framebuffers: byte-identical. The framework "
                            "is the library, called differently.", "xs", "start"))
    return svg(uid, W, H, "The layering rule",
               "A four-tier dependency diagram: demos above engine::app, above "
               "engine::platform, above SDL3, with sandbox bypassing app and a "
               "crossed-out arrow for app reaching around platform to SDL.", b)


FIGS = {
    "l52_fig1.svg": fig_fork,
    "l52_fig2.svg": fig_frame,
    "l52_fig3.svg": fig_lifecycle,
    "l52_fig4.svg": fig_boilerplate,
    "l52_fig5.svg": fig_ownership,
    "l52_fig6.svg": fig_layers,
}


def main():
    for name, fn in FIGS.items():
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
