#!/usr/bin/env python3
"""scratch/figs_510.py — Lesson 5.10's diagrams.

Same rules as 5.5's: computed coordinates, no fill="..." on any <text> (CSS wins
over presentation attributes on text), every line kept well inside the viewBox,
and no right-anchored label near x=0.

Filenames are numbered by PAGE ORDER, which is the trap 5.3 fell into.
Row labels start well right of any column, which is the trap 5.4 fell into.
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
    for suffix, colour in (("b", RED), ("g", GREEN)):
        out.append(
            f'    <marker id="e-{suffix}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="{colour}"/></marker>')
    out.append("  </defs>")
    return "\n".join(out)


def svg(uid, w, h, title, desc, body):
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{uid}-t {uid}-d">\n'
            f'  <title id="{uid}-t">{esc(title)}</title>\n'
            f'  <desc id="{uid}-d">{esc(desc)}</desc>\n'
            f'{markers(uid)}\n' + "\n".join(body) + "\n</svg>\n")


def box(x, y, w, h, colour=None, dash=None, rx=3, width=1.2, opacity=None):
    fill = 'class="fill-soft grid"' if colour is None else f'fill="{colour}" stroke="{colour}"'
    d = f' stroke-dasharray="{dash}"' if dash else ""
    o = f' fill-opacity="{opacity}"' if opacity is not None else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {fill}'
            f' stroke-width="{width}"{d}{o}/>')


def hollow(x, y, w, h, colour, dash=None, rx=3, width=1.3):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="none"'
            f' stroke="{colour}" stroke-width="{width}"{d}/>')


def label(x, y, text, cls="sm", anchor="middle"):
    return f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">{esc(text)}</text>'


def arrow(x1, y1, x2, y2, uid, kind="s", width=1.2, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    cls = {"s": "ink-soft", "i": "ink", "h": "hi", "b": "ink", "g": "ink"}[kind]
    stroke = ""
    if kind == "b":
        cls = ""
        stroke = f' stroke="{RED}"'
    if kind == "g":
        cls = ""
        stroke = f' stroke="{GREEN}"'
    c = f' class="{cls}"' if cls else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"{c}{stroke}'
            f' stroke-width="{width}"{d} marker-end="url(#e-{kind}-{uid})"/>')


def rule(x1, y1, x2, y2, cls="grid", width=1.0, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}"'
            f' stroke-width="{width}"{d}/>')






# ===========================================================================
# Figure 1 — the indirection, and what it buys  (§1)
# ===========================================================================
def fig_indirection():
    uid = "l510f1"
    W, H = 720, 400
    b = [label(20, 20, "One level of indirection, and the four things it buys.", "sm", "start")]

    b.append(label(24, 48, "TODAY — the keycode IS the decision", "xs t-bad", "start"))
    b.append(box(60, 60, 300, 24, colour=GREY, opacity=0.14))
    b.append(label(210, 76, "case SDL_SCANCODE_C: ride_camera();", "xs mono"))
    losses = ["cannot be rebound", "is about ONE device",
              "cannot be recorded", "says the wrong thing"]
    for i, t in enumerate(losses):
        b.append(rule(370, 72 + i * 0, 396, 72, "ink-soft", 1.0) if i == 0 else "")
        b.append(box(410, 52 + i * 22, 250, 18, colour=RED, opacity=0.10, dash="3 3"))
        b.append(label(535, 65 + i * 22, t, "xs muted"))
    b = [x for x in b if x]
    b.append(arrow(366, 72, 404, 72, uid, "b", 1.2))

    b.append(rule(24, 154, 696, 154, "grid", 1.2))

    b.append(label(24, 176, "LESSON 5.10 — a NAME in the middle", "xs t-ok", "start"))

    stages = [("an ACTION", AMBER, '"ride_camera"', 'a name the game means'),
              ("a BINDING", BLUE, "C  ->  +1", "one signal, mapped onto it"),
              ("a VALUE", GREEN, "1.0  held  pressed", "published once per frame")]
    for i, (title, colour, mid, note) in enumerate(stages):
        x = 40 + i * 224
        b.append(box(x, 192, 200, 20, colour=colour, opacity=0.24))
        b.append(label(x + 100, 206, title, "xs mono"))
        b.append(box(x, 214, 200, 20, colour=colour, opacity=0.10))
        b.append(label(x + 100, 228, mid, "xs mono"))
        b.append(label(x + 100, 248, note, "xs muted"))
        if i < 2:
            b.append(arrow(x + 202, 218, x + 222, 218, uid, "h", 1.2))

    gains = ["a settings screen can change it",
             "a keyboard, a mouse, a gamepad — same action",
             "a replay records the INTENTION, not the key",
             "the code says what it does"]
    for i, t in enumerate(gains):
        b.append(box(60, 272 + i * 22, 600, 18, colour=GREEN, opacity=0.10))
        b.append(label(360, 285 + i * 22, t, "xs muted"))

    b.append(label(24, 382, "None of the four is a performance argument, and this lesson "
                            "measures nothing. It is a design lesson and says so.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "A hard-coded keycode compared with an action, a binding and a value",
               "Above, a switch on a scancode and the four things it cannot do. Below, the same "
               "decision expressed as a named action, a binding that maps a signal onto it, and "
               "a value published once per frame.", b)


# ===========================================================================
# Figure 2 — one mechanism, three jobs  (§3)
# ===========================================================================
def fig_mechanism():
    uid = "l510f2"
    W, H = 720, 380
    b = [label(20, 20, "Every binding contributes a SIGNED value. An action's value is the sum. "
                       "That is the whole rule.", "sm", "start")]

    rows = [
        ("jump", AMBER, [("Space", "+1")], "0 or 1", "ask held() / pressed()", 
         "a button"),
        ("steer", BLUE, [("A", "-1"), ("D", "+1")], "-1, 0 or +1", "ask value()",
         "an axis from two keys — and holding BOTH gives exactly 0"),
        ("look_x", GREEN, [("mouse dx", "x 0.5")], "unbounded", "ask value()",
         "an axis from a continuous source"),
    ]
    y = 56
    for name, colour, binds, rng, ask, note in rows:
        b.append(box(50, y, 110, 22, colour=colour, opacity=0.26))
        b.append(label(105, y + 15, name, "xs mono"))
        x = 190
        for k, sc in binds:
            b.append(box(x, y, 96, 22, colour=colour, opacity=0.14))
            b.append(label(x + 48, y + 15, f"{k} {sc}", "xs mono"))
            x += 106
        b.append(arrow(x + 2, y + 11, x + 24, y + 11, uid, "h", 1.2))
        b.append(box(x + 28, y, 108, 22, colour=colour, opacity=0.20))
        b.append(label(x + 82, y + 15, rng, "xs mono"))
        b.append(label(x + 148, y + 15, ask, "xs muted", "start"))
        b.append(label(50, y + 38, note, "xs muted", "start"))
        y += 64

    b.append(rule(24, 254, 696, 254, "grid", 1.2))

    b.append(label(24, 276, "THERE IS NO “BUTTON MODE” AND “AXIS MODE” TO CHOOSE BETWEEN,",
                   "xs t-hi", "start"))
    b.append(label(24, 292, "because the second is what the first already is once you allow a "
                            "scale. A design with two kinds would have needed", "xs muted",
                   "start"))
    b.append(label(24, 306, "a rule for “what does A and D held together mean”. This one does "
                            "not: -1 + 1 = 0, and nothing anywhere says so.", "xs muted",
                   "start"))

    b.append(box(60, 326, 600, 22, colour=GREY, opacity=0.10))
    b.append(label(360, 341, "held(a)  =  |value(a)| >= 0.5", "xs mono"))
    b.append(label(24, 370, "The threshold is what makes an analog source have to be PUSHED "
                            "rather than brushed. A key contributes 1, so it clears it easily.",
                   "xs muted", "start"))

    return svg(uid, W, H, "One binding rule covering buttons, key axes and analog axes",
               "Three actions: jump bound to one key, steer bound to two keys with opposite "
               "scales so they cancel, and look_x bound to a scaled mouse delta. All three use "
               "the same rule: sum the bindings' signed contributions.", b)


# ===========================================================================
# Figure 3 — edges come from the level  (§4)
# ===========================================================================
def fig_edges():
    uid = "l510f3"
    W, H = 720, 400
    b = [label(20, 20, "“jump” bound to Space AND the left mouse button. Five frames.",
               "sm", "start")]

    frames = ["idle", "Space\ndown", "…and\nLMB down", "Space\nup", "LMB\nup"]
    space = [0, 1, 1, 0, 0]
    lmb = [0, 0, 1, 1, 0]
    level = [0, 1, 1, 1, 0]

    x0, colw = 130, 108
    for i, f in enumerate(frames):
        cx = x0 + i * colw
        for j, part in enumerate(f.split("\n")):
            b.append(label(cx + 44, 46 + j * 12, part, "xs muted"))

    def strip(y, vals, colour, name):
        b.append(label(122, y + 15, name, "xs mono muted", "end"))
        for i, v in enumerate(vals):
            cx = x0 + i * colw
            b.append(box(cx, y, 88, 22, colour=colour if v else GREY,
                         opacity=0.42 if v else 0.10))
            b.append(label(cx + 44, y + 15, "down" if v else "up", "xs mono"))

    strip(78, space, BLUE, "Space")
    strip(106, lmb, PURPLE, "LMB")
    strip(140, level, GREEN, "action level")

    # the edges
    b.append(label(122, 187, "edges", "xs mono muted", "end"))
    marks = ["", "PRESSED", "—", "—", "RELEASED"]
    for i, mk in enumerate(marks):
        cx = x0 + i * colw
        if mk in ("", "—"):
            b.append(label(cx + 44, 187, mk or "", "xs muted"))
        else:
            b.append(box(cx, 174, 88, 20, colour=GREEN, opacity=0.28))
            b.append(label(cx + 44, 188, mk, "xs mono"))

    b.append(rule(24, 210, 696, 210, "grid", 1.2))

    b.append(label(24, 232, "ONE rising edge and ONE falling edge for the whole sequence, and "
                            "that is the correct answer.", "xs t-ok", "start"))
    b.append(label(24, 252, "Frame 3: the mouse goes down while the action is ALREADY active, "
                            "so there is no second press.", "xs muted", "start"))
    b.append(label(24, 266, "Frame 4: Space comes up but the action is STILL active through the "
                            "mouse, so there is no release.", "xs muted", "start"))

    b.append(rule(24, 286, 696, 286, "grid", 1.0, "3 3"))
    b.append(label(24, 306, "DERIVE EDGES FROM THE BINDINGS INSTEAD AND YOU GET TWO OF EACH —",
                   "xs t-bad", "start"))
    for i, (lab, colour) in enumerate((("PRESSED", RED), ("PRESSED", RED),
                                       ("RELEASED", RED), ("RELEASED", RED))):
        cx = x0 + (i + 1) * colw
        b.append(box(cx, 318, 88, 20, colour=colour, opacity=0.26))
        b.append(label(cx + 44, 332, lab, "xs mono"))
    b.append(label(24, 358, "…and the player jumps twice for one press. The bug is invisible "
                            "until an action has more than one binding, which is why",
                   "xs muted", "start"))
    b.append(label(24, 372, "verify_510 §C binds two and the rest of the harness would not "
                            "have caught it.", "xs muted", "start"))
    b.append(label(24, 394, "AN EDGE IS A CHANGE IN THE ACTION, NOT A CHANGE IN A SIGNAL.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "Edges derived from an action's level rather than from its bindings",
               "Five frames in which a jump action bound to both a key and a mouse button is "
               "activated by one, then the other, then released in turn. The level stays true "
               "throughout, so there is exactly one press edge and one release edge.", b)


# ===========================================================================
# Figure 4 — where the hooks are  (§5)
# ===========================================================================
def fig_frame():
    uid = "l510f4"
    W, H = 720, 348
    b = [label(20, 20, "One frame, and the gap Lesson 5.10 had to fill.", "sm", "start")]

    stages = [("drain events", GREY, "on_event, per event"),
              ("tick clock", GREY, ""),
              ("publish input", BLUE, "input::update()"),
              ("MAP ACTIONS", AMBER, "on_input()   <- new"),
              ("N fixed steps", GREEN, "on_fixed_step, 0..N times"),
              ("render", PURPLE, "on_frame(alpha)")]
    y = 52
    for i, (name, colour, note) in enumerate(stages):
        b.append(box(60, y, 190, 24, colour=colour, opacity=0.26 if name.isupper() else 0.16))
        b.append(label(155, y + 16, name, "xs mono"))
        if note:
            b.append(label(266, y + 16, note, "xs muted", "start"))
        if i < len(stages) - 1:
            b.append(arrow(155, y + 26, 155, y + 40, uid, "s", 1.0))
        y += 42

    b.append(hollow(56, 172, 198, 30, AMBER, dash="4 3", width=1.5))

    b.append(rule(24, 306, 696, 306, "grid", 1.0, "3 3"))
    b.append(label(470, 180, "the ONLY moment at which", "xs t-hi", "start"))
    b.append(label(470, 193, "an action map may be updated", "xs muted", "start"))

    b.append(label(470, 232, "pressed() is valid HERE", "xs t-ok", "start"))
    b.append(label(470, 245, "(runs exactly once)", "xs muted", "start"))
    b.append(label(470, 268, "pressed() is NOT valid here", "xs t-bad", "start"))
    b.append(label(470, 281, "use consume_pressed()", "xs muted", "start"))
    b.append(arrow(462, 176, 440, 184, uid, "h", 1.1))
    b.append(arrow(462, 228, 440, 184, uid, "g", 1.1))
    b.append(arrow(462, 264, 440, 226, uid, "b", 1.1))

    # Two lines. The one-line version was 69 units past the viewBox — a spill the
    # geometry check catches and the eye does not, because SVG simply draws it.
    b.append(label(24, 322, "Before 5.10 nothing ran exactly once, after input was refreshed "
                            "and before any step.", "xs muted", "start"))
    b.append(label(24, 334, "A map updated in on_frame would leave every step reading LAST "
                            "frame's actions.", "xs muted", "start"))

    return svg(uid, W, H, "The frame's stages and the hook added for the action map",
               "The frame runs event drain, clock tick, input publish, the new on_input hook, "
               "zero or more fixed steps, and render. on_input is the only point at which input "
               "is fresh and no step has consumed it.", b)


# ===========================================================================
# Figure 5 — the two ways a step loses an edge  (§5)
# ===========================================================================
def fig_queue():
    uid = "l510f5"
    W, H = 720, 400
    b = [label(20, 20, "Why a fixed-timestep engine needs a second kind of edge.",
               "sm", "start")]

    def scenario(y, title, steps, verdict, colour, note):
        b.append(label(24, y, title, "xs t-hi", "start"))
        b.append(box(60, y + 12, 120, 22, colour=BLUE, opacity=0.24))
        b.append(label(120, y + 27, "one press", "xs mono"))
        b.append(arrow(182, y + 23, 206, y + 23, uid, "s", 1.1))
        for i in range(steps):
            b.append(box(212 + i * 88, y + 12, 80, 22, colour=GREEN, opacity=0.20))
            b.append(label(252 + i * 88, y + 27, f"step {i + 1}", "xs mono"))
        if steps == 0:
            b.append(box(212, y + 12, 168, 22, colour=GREY, opacity=0.10, dash="3 3"))
            b.append(label(296, y + 27, "no step ran", "xs muted"))
        b.append(box(452, y + 12, 216, 22, colour=colour, opacity=0.30))
        b.append(label(560, y + 27, verdict, "xs mono"))
        b.append(label(60, y + 50, note, "xs muted", "start"))

    b.append(label(24, 46, "READING pressed() INSIDE THE STEP", "xs t-bad", "start"))
    scenario(64, "a TWO-step frame", 2, "jumps TWICE", RED,
             "the edge is true for the whole frame, and the frame ran the step twice")
    scenario(130, "a ZERO-step frame", 0, "jump LOST", RED,
             "the edge came and went during a frame in which nothing was listening")

    b.append(rule(24, 196, 696, 196, "grid", 1.2))

    b.append(label(24, 218, "READING consume_pressed() INSTEAD", "xs t-ok", "start"))
    scenario(236, "a TWO-step frame", 2, "jumps ONCE", GREEN,
             "the first step takes the queued press; the second finds the queue empty")
    scenario(302, "a ZERO-step frame", 0, "queued, not lost", GREEN,
             "the press waits in the queue until a step actually runs")

    b.append(label(24, 372, "The queue is FOUR deep and presses past that are dropped, which is "
                            "a decision: an unbounded queue replays a burst of jumps",
                   "xs muted", "start"))
    b.append(label(24, 386, "after the player has stopped asking, and that feels worse than "
                            "losing them. A fighting game would raise the number and call it "
                            "design.", "xs muted", "start"))

    return svg(uid, W, H, "A frame-scoped edge compared with a queued one across variable steps",
               "Reading a frame edge inside the fixed step double-fires on a two-step frame and "
               "loses the press on a zero-step frame; consuming a queued press fires exactly "
               "once in both cases.", b)


FIGS = {
    "l510_fig1.svg": fig_indirection,   # §1
    "l510_fig2.svg": fig_mechanism,     # §3
    "l510_fig3.svg": fig_edges,         # §4
    "l510_fig4.svg": fig_frame,         # §5
    "l510_fig5.svg": fig_queue,         # §5
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
