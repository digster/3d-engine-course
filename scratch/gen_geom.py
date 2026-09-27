"""Geometry for Lesson 3.1's diagrams, computed rather than eyeballed.

Emits SVG polygon point-lists for the woven triangle (the painter's cycle) so
the over/under crossings are exactly right instead of hand-fudged.
"""
import math, json

def plank(a, b, half_w):
    """Rectangle corners for a plank from a to b, half_w to each side."""
    ax, ay = a; bx, by = b
    dx, dy = bx - ax, by - ay
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    return [(ax + nx * half_w, ay + ny * half_w),
            (bx + nx * half_w, by + ny * half_w),
            (bx - nx * half_w, by - ny * half_w),
            (ax - nx * half_w, ay - ny * half_w)], (ux, uy), (nx, ny)

def slab_lines(mid, n, half_w):
    """The two boundary lines of a plank's width slab: p.n = c."""
    c = mid[0] * n[0] + mid[1] * n[1]
    return [c + half_w, c - half_w]

def overlap_quad(pa, pb):
    """The parallelogram where two width-slabs cross."""
    (mid_a, n_a, hw_a) = pa
    (mid_b, n_b, hw_b) = pb
    ca = slab_lines(mid_a, n_a, hw_a)
    cb = slab_lines(mid_b, n_b, hw_b)
    pts = []
    for i in (0, 1):
        for j in (0, 1):
            # n_a . p = ca[i],  n_b . p = cb[j]
            det = n_a[0] * n_b[1] - n_a[1] * n_b[0]
            x = (ca[i] * n_b[1] - cb[j] * n_a[1]) / det
            y = (n_a[0] * cb[j] - n_b[0] * ca[i]) / det
            pts.append((x, y))
    # order them into a convex quad by angle about their centroid
    cx = sum(p[0] for p in pts) / 4.0
    cy = sum(p[1] for p in pts) / 4.0
    pts.sort(key=lambda p: math.atan2(p[1] - cy, p[0] - cx))
    return pts

def weave(cx, cy, R, half_w, extend):
    """Three planks along an equilateral triangle's sides, with overhang.

    Returns (planks, overlaps) where planks[i] is a point list and overlaps[i]
    is the parallelogram where plank i crosses plank (i+1)%3.
    """
    corners = []
    for i in range(3):
        ang = math.radians(90 + 120 * i)
        corners.append((cx + R * math.cos(ang), cy - R * math.sin(ang)))  # svg y is down
    planks, mids, norms = [], [], []
    for i in range(3):
        a, b = corners[i], corners[(i + 1) % 3]
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy)
        ux, uy = dx / L, dy / L
        ae = (a[0] - ux * extend, a[1] - uy * extend)
        be = (b[0] + ux * extend, b[1] + uy * extend)
        pts, u, n = plank(ae, be, half_w)
        planks.append(pts)
        mids.append(((ae[0] + be[0]) / 2.0, (ae[1] + be[1]) / 2.0))
        norms.append(n)
    overlaps = []
    for i in range(3):
        j = (i + 1) % 3
        overlaps.append(overlap_quad((mids[i], norms[i], half_w),
                                     (mids[j], norms[j], half_w)))
    return corners, planks, overlaps

def fmt(pts):
    return " ".join("%.2f,%.2f" % p for p in pts)

if __name__ == "__main__":
    corners, planks, overlaps = weave(160, 155, 74, 30, 26)
    out = {"corners": corners,
           "planks": [fmt(p) for p in planks],
           "overlaps": [fmt(p) for p in overlaps]}
    print(json.dumps(out, indent=1))
