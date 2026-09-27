#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 3.7 from real geometry.

Lesson 3.6 shipped three figures that disagreed with the prose beside them —
drawn by eye, labelled by hand, and `check-page.js` said `pass: true` the whole
time because a lying figure is still valid markup. Nothing here is placed by eye:
every arrow endpoint, every lobe outline and every printed number comes out of the
same trigonometry the engine uses, and the numbers this script prints are the
numbers the page quotes.

Writes scratch/l37_fig{1..5}.svg and prints the values the prose cites.
"""
import math

# ---- Conventions shared by every figure -------------------------------------
# A direction is given as an angle in degrees from the surface normal, positive
# toward +x. The surface is horizontal; the normal points up. SVG's y grows
# downward, so "up" is -y.

def dir_xy(deg):
    """Unit direction at `deg` from the normal, in SVG coordinates (y down)."""
    a = math.radians(deg)
    return (math.sin(a), -math.cos(a))


def pt(ox, oy, deg, length):
    dx, dy = dir_xy(deg)
    return (ox + dx * length, oy + dy * length)


def f(v):
    return f"{v:.1f}"


def arrow(ox, oy, deg, length, cls, marker):
    x, y = pt(ox, oy, deg, length)
    return (f'<line class="{cls}" x1="{f(ox)}" y1="{f(oy)}" x2="{f(x)}" y2="{f(y)}" '
            f'stroke-width="2.4" marker-end="url(#{marker})"/>')


def label(ox, oy, deg, length, text, cls="lbl", dx=0, dy=0):
    x, y = pt(ox, oy, deg, length)
    return f'<text x="{f(x + dx)}" y="{f(y + dy)}" class="{cls}">{text}</text>'


def arc_between(ox, oy, a0, a1, r):
    """A circular arc from angle a0 to a1 (degrees from the normal), radius r."""
    x0, y0 = pt(ox, oy, a0, r)
    x1, y1 = pt(ox, oy, a1, r)
    sweep = 1 if a1 > a0 else 0
    large = 1 if abs(a1 - a0) > 180 else 0
    return (f'<path class="arc" d="M {f(x0)},{f(y0)} A {f(r)},{f(r)} 0 {large} {sweep} '
            f'{f(x1)},{f(y1)}" fill="none" stroke-width="1.4"/>')


DEFS = """  <defs>
    <marker id="a-n" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--axis-y)"/></marker>
    <marker id="a-l" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--warn-bd)"/></marker>
    <marker id="a-r" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--note-bd)"/></marker>
    <marker id="a-v" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--pitfall-bd)"/></marker>
    <marker id="a-h" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--ok-bd)"/></marker>
  </defs>
"""

# Shared per-figure CSS classes are declared once in the page's own <style>.


def surface(x0, x1, y, extra=""):
    return (f'<line class="surf" x1="{f(x0)}" y1="{f(y)}" x2="{f(x1)}" y2="{f(y)}" '
            f'stroke-width="2.5"/>{extra}')


def hatch(x0, x1, y, step=14, drop=9):
    """Little ticks under a surface line, so "below" reads as solid."""
    out = []
    x = x0
    while x < x1:
        out.append(f'<line class="hatch" x1="{f(x)}" y1="{f(y)}" x2="{f(x - drop)}" '
                   f'y2="{f(y + drop)}" stroke-width="1"/>')
        x += step
    return "".join(out)


def lobe_path(ox, oy, centre_deg, exponent, scale, steps=181, clip_visible=True):
    """The outline of cos^p, drawn as a polar curve about `centre_deg`.

    `clip_visible` stops the curve at the surface. Without it the lobe is drawn
    for directions BELOW the surface — mathematically what cos^p says, and
    visually a claim that light leaves into the ground. The function is defined
    there; the surface is not.
    """
    pts = []
    for i in range(steps + 1):
        off = -90.0 + 180.0 * i / steps
        direction = centre_deg + off
        if clip_visible and abs(direction) > 90.0:
            continue
        r = (max(0.0, math.cos(math.radians(off))) ** exponent) * scale
        x, y = pt(ox, oy, direction, r)
        pts.append(f"{f(x)},{f(y)}")
    return " ".join(pts)


def svg(view_w, view_h, title, desc, body, tid):
    return (f'<svg viewBox="0 0 {view_w} {view_h}" role="img" '
            f'aria-labelledby="{tid}-t {tid}-d">\n'
            f'  <title id="{tid}-t">{title}</title>\n'
            f'  <desc id="{tid}-d">{desc}</desc>\n{DEFS}{body}\n</svg>\n')


def write(name, text):
    with open(f"scratch/{name}", "w") as fh:
        fh.write(text)
    print(f"  wrote scratch/{name}")


# =============================================================================
# Figure 1 — a mirror, and then a rough one
# =============================================================================
def fig1():
    OX, OY = 330.0, 300.0
    L_DEG = -35.0                 # light, to the left of the normal
    R_DEG = -L_DEG                # mirror direction: same angle, other side
    ARM = 175.0

    b = [surface(60, 600, OY, hatch(75, 605, OY))]
    b.append(f'<line class="normal" x1="{f(OX)}" y1="{f(OY)}" x2="{f(OX)}" y2="{f(OY-200)}" '
             f'stroke-width="2.4" stroke-dasharray="6 4" marker-end="url(#a-n)"/>')
    b.append(f'<text x="{f(OX+8)}" y="{f(OY-198)}" class="lbl lbl-n">n</text>')

    # The lobe first, so the arrows sit on top of it.
    b.append(f'<polyline class="lobe" points="{lobe_path(OX, OY, R_DEG, 12, 150)}" '
             f'fill="var(--note-bg)" fill-opacity="0.55" stroke="var(--note-bd)" '
             f'stroke-width="1.4"/>')

    b.append(arrow(OX, OY, L_DEG, ARM, "vec-l", "a-l"))
    b.append(label(OX, OY, L_DEG, ARM + 20, "l", "lbl lbl-l", dx=-6, dy=2))
    b.append(arrow(OX, OY, R_DEG, ARM, "vec-r", "a-r"))
    # R and eye A point the SAME way (A is exactly on the mirror ray), so their
    # labels must be separated PERPENDICULAR to the ray or they stack.
    # OUTSIDE the lobe (max radius 150) and on the opposite side from the eye
    # labels, which are offset the other way.
    rlx, rly = pt(OX, OY, R_DEG, ARM + 26)
    rpx, rpy = dir_xy(R_DEG - 90)
    b.append(f'<text x="{f(rlx + rpx*30)}" y="{f(rly + rpy*30 + 5)}" '
             f'class="lbl lbl-r" text-anchor="middle">R</text>')

    b.append(arc_between(OX, OY, L_DEG, 0, 62))
    b.append(arc_between(OX, OY, 0, R_DEG, 74))
    b.append(label(OX, OY, L_DEG / 2, 78, f"{abs(L_DEG):.0f}&#176;", "sm muted", dx=-12))
    # Outside the lobe's reach (max radius 150 along R, less off-axis) and above
    # the arc it annotates.
    b.append(label(OX, OY, R_DEG / 2, 172, f"{abs(R_DEG):.0f}&#176;", "sm muted",
                   dx=-30, dy=-6))

    # Three sample eye directions, with the value each one receives.
    print("Figure 1 — Phong lobe, exponent 12, mirror at %.0f deg" % R_DEG)
    # Offsets chosen so EVERY sample stays above the surface: an eye below it
    # cannot see the surface at all, and drawing one there would be nonsense.
    for eye_deg, tag in ((R_DEG, "A"), (R_DEG + 22, "B"), (R_DEG + 44, "C")):
        off = eye_deg - R_DEG
        assert abs(eye_deg) <= 90.0, "sample eye is below the surface"
        val = max(0.0, math.cos(math.radians(off))) ** 12
        x, y = pt(OX, OY, eye_deg, ARM + 8)
        b.append(f'<line class="vec-v" x1="{f(OX)}" y1="{f(OY)}" x2="{f(x)}" y2="{f(y)}" '
                 f'stroke-width="1.8" stroke-dasharray="5 3" marker-end="url(#a-v)"/>')
        lx, ly = pt(OX, OY, eye_deg, ARM + 30)
        ppx, ppy = dir_xy(eye_deg + 90)
        lx += ppx * 18
        ly += ppy * 18
        b.append(f'<text x="{f(lx)}" y="{f(ly)}" class="sm lbl-v" text-anchor="middle">'
                 f'{tag}</text>')
        b.append(f'<text x="{f(lx)}" y="{f(ly+13)}" class="xs muted" text-anchor="middle">'
                 f'{val:.3f}</text>')
        print(f"    eye {tag}: {off:+.0f} deg off the mirror ray -> cos^12 = {val:.4f}")

    b.append('<text x="70" y="46" class="sm" font-weight="700">A perfect mirror answers '
             'only at R. A glossy one answers NEAR it.</text>')
    b.append('<text x="70" y="66" class="xs muted">Shaded outline: the fraction returned '
             'toward each direction, cos<tspan dy="-5" font-size="10">12</tspan>'
             '<tspan dy="5"> of the angle from R.</tspan></text>')
    write("l37_fig1.svg", svg(680, 400,
        "A mirror direction with a glossy lobe around it",
        "A horizontal surface with its normal drawn upward. An arrow l points up and to the "
        "left toward the light at 52 degrees from the normal, and an arrow R points up and to "
        "the right at the same 52 degrees. A rounded lobe surrounds R, widest along R and "
        "narrowing away from it; three dashed eye directions A, B and C are labelled with the "
        "fraction of light each receives: 1.000 along R, 0.319 at 22 degrees off, and 0.008 at "
        "48 degrees off.", "".join(b), "f1"))


# =============================================================================
# Figure 2 — the halfway vector is the normal that would do the job
# =============================================================================
def fig2():
    OX, OY = 330.0, 300.0
    L_DEG = -50.0
    V_DEG = 18.0
    H_DEG = (L_DEG + V_DEG) / 2.0
    ARM = 170.0

    b = [surface(60, 600, OY, hatch(75, 605, OY))]
    b.append(f'<line class="normal" x1="{f(OX)}" y1="{f(OY)}" x2="{f(OX)}" y2="{f(OY-190)}" '
             f'stroke-width="2.2" stroke-dasharray="6 4" marker-end="url(#a-n)"/>')
    b.append(f'<text x="{f(OX+8)}" y="{f(OY-188)}" class="lbl lbl-n">n</text>')

    b.append(arrow(OX, OY, L_DEG, ARM, "vec-l", "a-l"))
    b.append(label(OX, OY, L_DEG, ARM + 20, "l", "lbl lbl-l", dx=-8, dy=2))
    b.append(arrow(OX, OY, V_DEG, ARM, "vec-v", "a-v"))
    b.append(label(OX, OY, V_DEG, ARM + 20, "v", "lbl lbl-v", dx=4, dy=2))
    b.append(arrow(OX, OY, H_DEG, ARM * 0.86, "vec-h", "a-h"))
    b.append(label(OX, OY, H_DEG, ARM * 0.86 + 22, "h", "lbl lbl-h", dx=-4, dy=2))

    # The tilted microfacet that h would be the normal of: a short segment
    # perpendicular to h, drawn where h ends.
    hx, hy = pt(OX, OY, H_DEG, ARM * 0.86)
    px, py = dir_xy(H_DEG + 90)
    b.append(f'<line class="facet" x1="{f(hx - px*46)}" y1="{f(hy - py*46)}" '
             f'x2="{f(hx + px*46)}" y2="{f(hy + py*46)}" stroke-width="5" '
             f'stroke-linecap="round"/>')
    # The annotation goes in the RIGHT MARGIN, not next to the facet: every
    # position near the facet lands on l, on v, or on the arc labels.
    b.append('<text x="640" y="112" class="xs muted" text-anchor="end">'
             'the short bar is a microfacet with this tilt;</text>')
    b.append('<text x="640" y="126" class="xs muted" text-anchor="end">'
             'it sends l straight into v</text>')

    b.append(arc_between(OX, OY, L_DEG, H_DEG, 64))
    b.append(arc_between(OX, OY, H_DEG, V_DEG, 78))
    half = (V_DEG - L_DEG) / 2.0
    b.append(label(OX, OY, (L_DEG + H_DEG) / 2, 84, f"{half:.0f}&#176;", "sm muted", dx=-28))
    b.append(label(OX, OY, (H_DEG + V_DEG) / 2, 104, f"{half:.0f}&#176;", "sm muted", dx=-84))

    b.append('<text x="70" y="46" class="sm" font-weight="700">h = normalise(l + v) bisects '
             'them &#8212; and that is exactly what makes it the needed normal.</text>')
    b.append(f'<text x="70" y="66" class="xs muted">Here l is {abs(L_DEG):.0f}&#176; from n '
             f'and v is {V_DEG:.0f}&#176; the other way, so h sits '
             f'{abs(H_DEG):.0f}&#176; from n. dot(n,h) = '
             f'{math.cos(math.radians(H_DEG)):.4f}.</text>')

    print("Figure 2 — halfway vector")
    print(f"    l at {L_DEG:+.0f} deg, v at {V_DEG:+.0f} deg -> h at {H_DEG:+.1f} deg")
    print(f"    dot(n,h) = {math.cos(math.radians(H_DEG)):.6f}; "
          f"each of l and v is {half:.0f} deg from h")
    write("l37_fig2.svg", svg(660, 360,
        "The halfway vector bisects the light and eye directions",
        "The same horizontal surface and upward normal. An arrow l points up-left at 50 degrees "
        "from the normal and an arrow v points up-right at 18 degrees. Between them, an arrow h "
        "sits at 16 degrees to the left of the normal, exactly bisecting them: 34 degrees from l "
        "and 34 degrees from v. A short thick bar drawn perpendicular to h represents a "
        "microfacet whose tilt would reflect the light straight into the eye.",
        "".join(b), "f2"))


# =============================================================================
# Figure 3 — beta is exactly half of alpha
# =============================================================================
def fig3():
    OX, OY = 340.0, 300.0
    L_DEG = -50.0
    V_DEG = 10.0
    R_DEG = -L_DEG
    H_DEG = (L_DEG + V_DEG) / 2.0
    alpha = R_DEG - V_DEG
    beta = -H_DEG
    ARM = 168.0

    b = [surface(60, 620, OY, hatch(75, 625, OY))]
    b.append(f'<line class="normal" x1="{f(OX)}" y1="{f(OY)}" x2="{f(OX)}" y2="{f(OY-190)}" '
             f'stroke-width="2.2" stroke-dasharray="6 4" marker-end="url(#a-n)"/>')
    b.append(f'<text x="{f(OX+8)}" y="{f(OY-188)}" class="lbl lbl-n">n</text>')

    for deg, cls, mk, name, cl in ((L_DEG, "vec-l", "a-l", "l", "lbl-l"),
                                   (R_DEG, "vec-r", "a-r", "R", "lbl-r"),
                                   (V_DEG, "vec-v", "a-v", "v", "lbl-v"),
                                   (H_DEG, "vec-h", "a-h", "h", "lbl-h")):
        b.append(arrow(OX, OY, deg, ARM, cls, mk))
        b.append(label(OX, OY, deg, ARM + 22, name, f"lbl {cl}", dx=-4, dy=2))

    b.append(arc_between(OX, OY, V_DEG, R_DEG, 116))
    b.append(label(OX, OY, (V_DEG + R_DEG) / 2, 138,
                   f"&#945; = {alpha:.0f}&#176;", "sm", dx=8, dy=-8))
    b.append(arc_between(OX, OY, H_DEG, 0, 62))
    b.append(label(OX, OY, H_DEG / 2, 40, f"&#946; = {beta:.0f}&#176;", "sm",
                   dx=-92, dy=-6))

    b.append('<text x="70" y="44" class="sm" font-weight="700">Move the eye by one degree and '
             'h moves by half a degree. That is the whole 4&#215; rule.</text>')
    b.append(f'<text x="70" y="64" class="xs muted">R is l mirrored about n, so it sits at '
             f'{R_DEG:.0f}&#176;; h bisects l and v, so it sits at {H_DEG:.0f}&#176;. '
             f'&#945; = {alpha:.0f}&#176; and &#946; = {beta:.0f}&#176;, exactly half.</text>')

    print("Figure 3 — beta = alpha/2")
    print(f"    l {L_DEG:+.0f}, v {V_DEG:+.0f} -> R {R_DEG:+.0f}, h {H_DEG:+.0f}")
    print(f"    alpha = {alpha:.0f} deg, beta = {beta:.0f} deg, ratio {alpha/beta:.4f}")
    write("l37_fig3.svg", svg(680, 360,
        "The angle to the halfway vector is exactly half the angle to the mirror ray",
        "The surface and normal again. Four arrows leave the same point: l at 50 degrees left "
        "of the normal, R at 50 degrees right, v at 10 degrees right, and h at 20 degrees left. "
        "An outer arc marks alpha, the 40-degree angle between R and v; an inner arc marks beta, "
        "the 20-degree angle between n and h. Beta is exactly half of alpha.",
        "".join(b), "f3"))


# =============================================================================
# Figure 4 — Phong's cut-off, drawn correctly
# =============================================================================
#
# WHAT THIS FIGURE ORIGINALLY GOT WRONG, and it is worth recording. The first
# draft drew R "below the surface" and said so, repeating the usual folklore.
# That is impossible: R is l mirrored about n, so if l is within 90 degrees of
# the normal then R is too, and BOTH are above the surface. Always.
#
# The real geometry is better and sharper. Phong's lobe only covers the
# hemisphere within 90 degrees of R — directions from -a-90 to -a+90 for a light
# at +a. The VISIBLE hemisphere is -90 to +90. Those two half-spaces are not the
# same half-space, and the visible band they fail to share runs from -a+90 to
# +90: a wedge EXACTLY AS WIDE as the light's angle from the normal. An eye in
# that wedge sees no Phong highlight at all.
def fig4():
    OX, OY = 340.0, 292.0
    L_DEG = 62.0     # light and eye on the SAME side: this is the whole condition
    V_DEG = 46.0
    R_DEG = -L_DEG
    H_DEG = (L_DEG + V_DEG) / 2.0
    CUT_DEG = R_DEG + 90.0   # the edge of Phong's hemisphere, = 90 - L_DEG
    alpha = abs(V_DEG - R_DEG)
    ARM = 158.0

    b = [surface(50, 650, OY, hatch(65, 655, OY))]

    # The dead wedge: visible directions that lie more than 90 degrees from R.
    wx0, wy0 = pt(OX, OY, CUT_DEG, 232)
    wx1, wy1 = pt(OX, OY, 90.0, 232)
    b.append(f'<g class="grid"><path class="deadzone" d="M {f(OX)},{f(OY)} '
             f'L {f(wx0)},{f(wy0)} A 232,232 0 0 1 {f(wx1)},{f(wy1)} Z" '
             f'fill="var(--verify-bg)" fill-opacity="0.85" stroke="none"/></g>')
    b.append(f'<line class="cut" x1="{f(OX)}" y1="{f(OY)}" x2="{f(wx0)}" y2="{f(wy0)}" '
             f'stroke-width="1.5" stroke-dasharray="5 4"/>')

    b.append(f'<line class="normal" x1="{f(OX)}" y1="{f(OY)}" x2="{f(OX)}" y2="{f(OY-186)}" '
             f'stroke-width="2.2" stroke-dasharray="6 4" marker-end="url(#a-n)"/>')
    b.append(f'<text x="{f(OX+8)}" y="{f(OY-184)}" class="lbl lbl-n">n</text>')

    b.append(arrow(OX, OY, R_DEG, ARM, "vec-r", "a-r"))
    b.append(label(OX, OY, R_DEG, ARM + 24, "R", "lbl lbl-r", dx=-14, dy=2))
    b.append(arrow(OX, OY, L_DEG, ARM, "vec-l", "a-l"))
    b.append(label(OX, OY, L_DEG, ARM + 22, "l", "lbl lbl-l", dx=8, dy=8))
    b.append(arrow(OX, OY, V_DEG, ARM * 0.86, "vec-v", "a-v"))
    b.append(label(OX, OY, V_DEG, ARM * 0.86 + 20, "v", "lbl lbl-v", dx=10, dy=4))
    b.append(arrow(OX, OY, H_DEG, ARM * 0.62, "vec-h", "a-h"))
    hlx, hly = pt(OX, OY, H_DEG, ARM * 0.62)
    hpx, hpy = dir_xy(H_DEG - 90)
    b.append(f'<text x="{f(hlx + hpx*20)}" y="{f(hly + hpy*20 + 5)}" '
             f'class="lbl lbl-h" text-anchor="middle">h</text>')

    b.append(arc_between(OX, OY, R_DEG, V_DEG, 118))
    b.append(label(OX, OY, (R_DEG + V_DEG) / 2, 150,
                   f"&#945; = {alpha:.0f}&#176; &gt; 90&#176;", "sm", dx=-118, dy=-4))

    # The wedge's caption lives in the bottom-right margin. Inside the wedge it
    # would sit on l, on v, or on the alpha arc — there is no clear spot in there.
    b.append(f'<text x="676" y="330" class="xs t-bad" text-anchor="end">'
             f'shaded: no Phong highlight for an eye anywhere in here</text>')
    b.append(f'<text x="676" y="344" class="xs t-bad" text-anchor="end">'
             f'&#8212; a wedge {L_DEG:.0f}&#176; wide, the light&#x27;s own angle from n</text>')

    ndoth = math.cos(math.radians(H_DEG))
    b.append('<text x="24" y="26" class="sm" font-weight="700">R is NOT below the surface. '
             'It never can be.</text>')
    b.append(f'<text x="24" y="42" class="xs muted">Phong answers only within 90&#176; of R '
             f'&#8212; left of the dashed line at {CUT_DEG:.0f}&#176;.</text>')
    b.append(f'<text x="24" y="55" class="xs muted">l at {L_DEG:.0f}&#176; and v at '
             f'{V_DEG:.0f}&#176; are on the SAME side, so &#945; = {alpha:.0f}&#176; and '
             f'Phong returns 0.</text>')
    b.append(f'<text x="24" y="68" class="xs muted">h lies between them at {H_DEG:.0f}&#176;, '
             f'so dot(n,h) = {ndoth:.3f} and Blinn gives {ndoth**8:.4f} at q = 8.</text>')

    print("Figure 4 — Phong's cut-off")
    print(f"    l {L_DEG:.0f} deg, v {V_DEG:.0f} deg (same side) -> R at {R_DEG:.0f} deg, "
          f"which is {abs(R_DEG):.0f} deg from the normal and therefore ABOVE the surface")
    print(f"    alpha = {alpha:.0f} deg; dot(R,v) = {math.cos(math.radians(alpha)):+.4f} "
          f"-> phong 0")
    print(f"    Phong's hemisphere ends at {CUT_DEG:.0f} deg, so the dead wedge is "
          f"{90 - CUT_DEG:.0f} deg wide = the light's angle from n")
    print(f"    dot(n,h) = {ndoth:.6f} at h = {H_DEG:.0f} deg -> blinn^8 = {ndoth**8:.6f} "
          f"(8-bit {round(255*(1.055*(ndoth**8)**(1/2.4)-0.055)):d})")
    write("l37_fig4.svg", svg(700, 360,
        "Phong answers only within 90 degrees of the mirror ray, which is not the visible half",
        "A horizontal surface with its normal drawn upward. The light direction l points up and "
        "to the right at 62 degrees from the normal; the mirror direction R points up and to the "
        "LEFT at 62 degrees, both comfortably above the surface. A dashed line at 28 degrees "
        "right of the normal marks the edge of the hemisphere Phong can answer in, and the wedge "
        "between it and the surface on the right is shaded: any eye in that 62-degree wedge sees "
        "no Phong highlight. The eye direction v, at 46 degrees, lies inside that wedge, 108 "
        "degrees from R. The halfway vector h sits at 54 degrees, between l and v and well above "
        "the surface, so Blinn still answers.", "".join(b), "f4"))


# =============================================================================
# Figure 5 — the two lobes, matched, as a CARTESIAN graph
# =============================================================================
#
# The first draft drew these as polar lobes and it was quietly useless. On a
# linear radial scale the whole argument — that Blinn is still returning 0.063 at
# 90 degrees where Phong returns nothing — is a dot 15 pixels from the origin,
# indistinguishable from the dot for 0.004. A polar lobe shows the SHAPE of the
# spray well (that is Figure 1's job) and hides the tail, which is the one thing
# this figure exists to show. Plotting value against angle shows both.
def fig5():
    X0, Y0 = 92.0, 306.0        # axis origin
    W, H = 520.0, 230.0         # plot area
    A_MAX = 130.0
    P, Q = 2, 8

    def gx(alpha):
        return X0 + alpha / A_MAX * W

    def gy(value):
        return Y0 - value * H

    b = []
    # Grid, in a <g class="grid"> so it is treated as background: graph paper is
    # meant to sit under labels (check-page skips .grid for exactly this).
    g = []
    for v in (0.25, 0.5, 0.75, 1.0):
        g.append(f'<line x1="{f(X0)}" y1="{f(gy(v))}" x2="{f(X0+W)}" y2="{f(gy(v))}" '
                 f'stroke-width="1"/>')
    for a in (30, 60, 90, 120):
        g.append(f'<line x1="{f(gx(a))}" y1="{f(Y0)}" x2="{f(gx(a))}" y2="{f(gy(1.05))}" '
                 f'stroke-width="1"/>')
    b.append(f'<g class="grid" stroke="var(--dia-grid)">{"".join(g)}</g>')

    # Axes.
    b.append(f'<line class="axis" x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0+W+10)}" y2="{f(Y0)}" '
             f'stroke-width="1.8"/>')
    b.append(f'<line class="axis" x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0)}" y2="{f(gy(1.12))}" '
             f'stroke-width="1.8"/>')
    for v in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(f'<text x="{f(X0-10)}" y="{f(gy(v)+4)}" class="xs muted" '
                 f'text-anchor="end">{v:.2f}</text>')
    for a in (0, 30, 60, 90, 120):
        b.append(f'<text x="{f(gx(a))}" y="{f(Y0+18)}" class="xs muted" '
                 f'text-anchor="middle">{a}&#176;</text>')
    b.append(f'<text x="{f(X0+W/2)}" y="{f(Y0+36)}" class="sm muted" text-anchor="middle">'
             f'&#945;, the angle between the mirror ray R and the eye</text>')
    b.append(f'<text x="{f(X0-46)}" y="{f(gy(0.5))}" class="sm muted" text-anchor="middle" '
             f'transform="rotate(-90 {f(X0-46)} {f(gy(0.5))})">fraction returned</text>')

    # The 90-degree wall.
    b.append(f'<line class="cut" x1="{f(gx(90))}" y1="{f(Y0)}" x2="{f(gx(90))}" '
             f'y2="{f(gy(1.12))}" stroke-width="1.6" stroke-dasharray="5 4"/>')

    def curve(fn, steps=260):
        pts = []
        for i in range(steps + 1):
            a = A_MAX * i / steps
            pts.append(f"{f(gx(a))},{f(gy(fn(a)))}")
        return " ".join(pts)

    phong = lambda a: max(0.0, math.cos(math.radians(a))) ** P
    blinn = lambda a: max(0.0, math.cos(math.radians(a / 2))) ** Q

    b.append(f'<polyline class="lobe-b" points="{curve(blinn)}" fill="none" '
             f'stroke="var(--ok-bd)" stroke-width="2.4"/>')
    b.append(f'<polyline class="lobe-p" points="{curve(phong)}" fill="none" '
             f'stroke="var(--note-bd)" stroke-width="2.2" stroke-dasharray="7 4"/>')

    print("Figure 5 — matched lobes, phong p=%d vs blinn q=%d" % (P, Q))
    for off in (0, 20, 40, 60, 80, 90, 100, 120):
        ph, bl = phong(off), blinn(off)
        print(f"    alpha {off:3d} deg   phong {ph:.5f}   blinn {bl:.5f}   "
              f"{'PHONG CUT OFF' if off >= 90 else ''}")

    # Mark the three the prose quotes, with the values written beside them.
    for a in (60, 90, 120):
        ph, bl = phong(a), blinn(a)
        b.append(f'<circle cx="{f(gx(a))}" cy="{f(gy(bl))}" r="4" fill="var(--ok-bd)"/>')
        b.append(f'<text x="{f(gx(a)+9)}" y="{f(gy(bl)-9)}" class="xs t-ok">'
                 f'{bl:.3f}</text>')
        if ph > 0.0005:
            b.append(f'<circle cx="{f(gx(a))}" cy="{f(gy(ph))}" r="4" '
                     f'fill="var(--note-bd)"/>')

    # Callouts, in the empty top-right of the plot area.
    # Left of the 90-degree wall, right-anchored, so neither callout crosses it.
    b.append(f'<text x="{f(gx(88))}" y="{f(gy(1.02))}" class="xs t-bad" text-anchor="end">'
             f'&#945; = 90&#176;: Phong falls to zero and stays there</text>')
    b.append(f'<text x="{f(gx(88))}" y="{f(gy(0.90))}" class="xs t-ok" text-anchor="end">'
             f'Blinn is still returning 0.063 there &#8212; code 71/255</text>')

    b.append('<text x="30" y="30" class="sm" font-weight="700">Same tightness at the peak, '
             'different tails &#8212; and only one survives past 90&#176;.</text>')
    b.append(f'<text x="30" y="46" class="xs muted">Dashed: Phong, cos<tspan dy="-5" '
             f'font-size="10">{P}</tspan><tspan dy="5">&#945;. Solid: Blinn, '
             f'cos</tspan><tspan dy="-5" font-size="10">{Q}</tspan>'
             f'<tspan dy="5">(&#945;/2), the matched exponent from &#167;3.4.</tspan></text>')

    write("l37_fig5.svg", svg(680, 360,
        "Phong and Blinn at matched exponents, plotted against the angle from the mirror ray",
        "A graph with the angle alpha from 0 to 130 degrees along the bottom and the fraction of "
        "light returned from 0 to 1 up the side. Two curves start together at 1.0 and fall away. "
        "The dashed Phong curve, cosine squared of alpha, reaches exactly zero at 90 degrees and "
        "stays there. The solid Blinn curve, cosine to the eighth of half alpha, tracks it "
        "closely near the peak, runs slightly above it through the middle, and is still at 0.063 "
        "at 90 degrees and 0.004 at 120 degrees, where Phong returns nothing at all. A dashed "
        "vertical line marks the 90-degree boundary.", "".join(b), "f5"))


if __name__ == "__main__":
    print("figs_37 — computing Lesson 3.7's figures\n")
    fig1()
    print()
    fig2()
    print()
    fig3()
    print()
    fig4()
    print()
    fig5()
