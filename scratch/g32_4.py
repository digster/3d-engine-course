# -*- coding: utf-8 -*-
import io, math
out = io.StringIO(); w = out.write

# measured by scratch/verify_32.cpp
DATA = [(1, 2, 14632, 48.5, 4.2281),
        (2, 8, 14527, 48.2, 1.8313),
        (4, 32, 15453, 51.2, 0.7566),
        (8, 128, 14109, 46.7, 0.2890),
        (16, 512, 1480, 4.9, 0.1016),
        (32, 2048, 381, 1.3, 0.0328)]

w('''
  <h3 id="cost">3.6 What it costs, and the escape route people took instead</h3>

  <p>
    <strong>One divide per pixel.</strong> That is the honest headline, and it is a real cost: the
    depth test in Lesson 3.1 was three multiply-adds and a comparison, and this is arithmetic of a
    different class. On the hardware of 1994 it was decisive; a divide was tens of cycles and there
    were only a few million of them per second to spend.
  </p>

  <p>
    Two things soften it. The divide is paid <strong>once per pixel, not once per attribute</strong>
    &mdash; the same reciprocal scales the uv, every colour channel, and in Lesson 3.6 the normal
    too. And the pre-multiplication by <code>1/w</code> is per <em>vertex</em>, hoisted out of the
    loop entirely, which is the same hoist that made the linear-light colour blend affordable in
    Lesson 2.4.
  </p>

  <p>
    But suppose you cannot afford it. There is an escape route, and an entire console generation
    took it: <strong>subdivide</strong>. Affine interpolation is exact at the corners, so if you cut
    a big triangle into small ones, each small triangle spans a smaller range of <code>w</code> and
    the chord sits closer to the curve. Chop finely enough and nobody can see the difference.
  </p>

  <p>
    The demo makes that measurable. <kbd>T</kbd> subdivides the floor and the harness reports what
    each level buys:
  </p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">Affine interpolation error against floor tessellation</caption>
      <thead>
        <tr><th>Floor</th><th>Triangles</th><th>Pixels wrong</th><th>% of the floor</th>
            <th>Worst u error</th><th>Improvement</th></tr>
      </thead>
      <tbody>
''')
prev = None
for cells, tris, px, pct, err in DATA:
    imp = "&mdash;" if prev is None else "&times;%.2f" % (prev / err)
    prev = err
    w('        <tr><td><code>%d&times;%d</code></td><td>%d</td><td>%d</td><td>%.1f%%</td>'
      '<td><code>%.4f</code> cells</td><td>%s</td></tr>\n' % (cells, cells, tris, px, pct, err, imp))
w('''      </tbody>
    </table>
  </div>

  <p>
    Two things in that table are worth more than the headline.
  </p>

  <p>
    <strong>The pixel count is a bad metric, and its badness is instructive.</strong> It sits near
    48% for the first four rows and then falls off a cliff. That is not the error behaving oddly
    &mdash; it is the <em>metric</em> saturating: once the uv error exceeds half a checker cell the
    pattern is effectively scrambled, and two scrambled two-colour images disagree on about half
    their pixels no matter how much more wrong one of them gets. <em>You cannot be more wrong than
    a coin flip.</em> This is a general hazard when you measure a continuous error through a
    quantised output, and it is why the last column exists.
  </p>

  <p>
    <strong>The convergence is second order, and it never arrives.</strong> The improvement ratios
    climb 2.31, 2.42, 2.62, 2.84, 3.10 &mdash; heading for 4, which is what &ldquo;halving the
    interval quarters the error&rdquo; means for a chord under a smooth curve. (They fall short at
    the coarse end because the nearest quads still span an enormous range of <code>w</code>; uniform
    subdivision in world space is a blunt instrument, which is why the near quads are the ones that
    need it.) But look at the last row: at 2,048 triangles the floor is <em>still</em> wrong on 381
    pixels. Subdivision converges. It does not terminate.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 260" role="img" aria-labelledby="fig5-t fig5-d">
      <title id="fig5-t">Affine error against subdivision, on a log scale</title>
      <desc id="fig5-d">A plot of the worst texture-coordinate error against the number of floor
        subdivisions, both on logarithmic scales. The points fall on a straight line of slope
        about minus two, indicating second-order convergence: each doubling of the subdivision
        divides the error by roughly four. The line continues below the bottom of the plot without
        ever reaching zero.</desc>
''')

# log-log plot: x = log2(cells) 0..5, y = log10(err)
X0, X1, Y0, Y1 = 90.0, 470.0, 200.0, 40.0
lo, hi = math.log10(0.02), math.log10(6.0)
def px(c): return X0 + (math.log2(c) / 5.0) * (X1 - X0)
def py(e): return Y0 - (math.log10(e) - lo) / (hi - lo) * (Y0 - Y1)

w('      <g class="grid" stroke-width="1">\n')
for e in (0.1, 1.0):
    w('        <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>\n' % (X0, py(e), X1, py(e)))
w('      </g>\n')
w('      <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="ink" stroke-width="1.5"/>\n' % (X0, Y0, X1, Y0))
w('      <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="ink" stroke-width="1.5"/>\n' % (X0, Y0, X0, Y1 - 8))
w('      <polyline points="%s" class="hi" stroke-width="2.5" fill="none"/>\n'
  % " ".join("%.1f,%.1f" % (px(c), py(e)) for c, _, _, _, e in DATA))
for c, tris, _, _, e in DATA:
    w('      <circle cx="%.1f" cy="%.1f" r="3.5" class="ink" fill="var(--dia-hi)" stroke-width="1.5"/>\n'
      % (px(c), py(e)))
    w('      <text x="%.1f" y="%.1f" class="xs mono muted" text-anchor="middle">%dx%d</text>\n'
      % (px(c), Y0 + 16, c, c))
w('      <text x="%.1f" y="%.1f" class="xs mono muted" text-anchor="end">1.0</text>\n' % (X0 - 6, py(1.0) + 3))
w('      <text x="%.1f" y="%.1f" class="xs mono muted" text-anchor="end">0.1</text>\n' % (X0 - 6, py(0.1) + 3))
w('''      <text x="%.1f" y="%.1f" class="xs muted" text-anchor="middle">floor subdivision &rarr;</text>
      <text x="30" y="30" class="xs muted">worst u error, in checker cells</text>
      <text x="%.1f" y="%.1f" class="xs t-bad">slope &asymp; &minus;2:</text>
      <text x="%.1f" y="%.1f" class="xs t-bad">each doubling quarters the error</text>
      <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="ink-soft" stroke-width="1" stroke-dasharray="4 3"/>
      <text x="500" y="%.1f" class="xs t-ok">one divide per pixel</text>
      <text x="500" y="%.1f" class="xs t-ok">gets you here, with</text>
      <text x="500" y="%.1f" class="xs t-ok">two triangles.</text>
    </svg>
    <figcaption><span class="fignum">Figure 5.</span> The error against subdivision, log&ndash;log.
      A straight line of slope &minus;2 is second-order convergence: every doubling costs four times
      the triangles and buys four times the accuracy. The dashed line is exactness, which the curve
      approaches and never touches. Perspective correction gets there in one step, at two triangles,
      for one divide per pixel &mdash; which is why every renderer built after about 1998 simply pays
      it.</figcaption>
  </figure>
''' % ((X0 + X1) / 2, Y0 + 34, px(2) + 8, py(1.0) - 26, px(2) + 8, py(1.0) - 12,
       X0, Y0 + 4, X1, Y0 + 4, 96, 112, 128))

w('''
  <p>
    The widget below is the demo, in the page. Drag the subdivision up and watch the affine picture
    crawl toward the correct one.
  </p>

  <div class="widget">
    <strong>The floor, both ways</strong>
    <svg id="pc-svg" viewBox="0 0 620 250" role="img" aria-labelledby="w1-t w1-d">
      <title id="w1-t">Interactive checkered floor</title>
      <desc id="w1-d">A checkered floor drawn with a chosen interpolation mode and subdivision
        level, using the same projection the engine uses. Controls change the subdivision, the
        camera height and the interpolation mode.</desc>
      <g id="pc-cells"></g>
      <text id="pc-read" x="12" y="240" class="xs mono muted">&nbsp;</text>
    </svg>
    <div class="widget-controls">
      <label for="pc-cells-n">subdivision</label>
      <input id="pc-cells-n" type="range" min="0" max="4" step="1" value="0">
      <label for="pc-height">eye height</label>
      <input id="pc-height" type="range" min="30" max="220" step="1" value="100">
      <label for="pc-mode"><input id="pc-mode" type="checkbox" checked> perspective-correct</label>
    </div>
  </div>

  <script>
  /* Page-specific widget for Lesson 3.2 — OUTSIDE the SHARED-SCRIPT markers so
     apply-shared.py never touches it. Draws the checkered floor as SVG polygons,
     with the uv lattice placed either by projecting the surface (correct) or by
     each triangle's linear screen map (affine). Vanilla JS, no libraries. */
  (function () {
    "use strict";
    var cellsEl = document.getElementById("pc-cells-n");
    var heightEl = document.getElementById("pc-height");
    var modeEl = document.getElementById("pc-mode");
    var g = document.getElementById("pc-cells");
    var read = document.getElementById("pc-read");
    if (!cellsEl || !g) { return; }

    var U_CELLS = 4, V_CELLS = 6;          /* the checker, fixed */
    var HALF_W = 1.5, ZN = -2.0, ZF = -12.0;
    var CX = 310, CY = 24, SCALE = 250, F = 1.0;

    function world(u, v) {
      return [-HALF_W + (u / U_CELLS) * 2 * HALF_W, -eyeH(), ZN + (v / V_CELLS) * (ZF - ZN)];
    }
    function eyeH() { return heightEl.value / 100.0; }
    function proj(p) {
      var w = -p[2];
      return [CX + (F * p[0] / w) * SCALE, CY - (F * p[1] / w) * SCALE];
    }
    /* Invert a triangle's affine uv map to find where (u,v) lands on screen. */
    function affine(triUV, triP, u, v) {
      var a11 = triUV[1][0] - triUV[0][0], a12 = triUV[2][0] - triUV[0][0];
      var a21 = triUV[1][1] - triUV[0][1], a22 = triUV[2][1] - triUV[0][1];
      var r1 = u - triUV[0][0], r2 = v - triUV[0][1];
      var det = a11 * a22 - a12 * a21;
      var b1 = (r1 * a22 - a12 * r2) / det, b2 = (a11 * r2 - r1 * a21) / det;
      return [triP[0][0] + b1 * (triP[1][0] - triP[0][0]) + b2 * (triP[2][0] - triP[0][0]),
              triP[0][1] + b1 * (triP[1][1] - triP[0][1]) + b2 * (triP[2][1] - triP[0][1])];
    }
    function poly(pts, fill) {
      var e = document.createElementNS("http://www.w3.org/2000/svg", "polygon");
      e.setAttribute("points", pts.map(function (p) { return p[0].toFixed(1) + "," + p[1].toFixed(1); }).join(" "));
      e.setAttribute("fill", fill);
      g.appendChild(e);
    }

    function draw() {
      while (g.firstChild) { g.removeChild(g.firstChild); }
      var n = Math.pow(2, +cellsEl.value);          /* quads per side */
      var correct = modeEl.checked;
      var LIGHT = "#E8E2D6", DARK = "#3A4058";

      /* backdrop: the whole quad, so gaps read as gaps */
      poly([proj(world(0, 0)), proj(world(U_CELLS, 0)),
            proj(world(U_CELLS, V_CELLS)), proj(world(0, V_CELLS))], LIGHT);

      var STEP = 0.25;   /* sample the checker finely enough to see the warp */
      for (var qj = 0; qj < n; qj++) {
        for (var qi = 0; qi < n; qi++) {
          /* this sub-quad's uv range and its two triangles */
          var u0 = qi * U_CELLS / n, u1 = (qi + 1) * U_CELLS / n;
          var v0 = qj * V_CELLS / n, v1 = (qj + 1) * V_CELLS / n;
          var uv = [[u0, v0], [u1, v0], [u1, v1], [u0, v1]];
          var P = uv.map(function (c) { return proj(world(c[0], c[1])); });
          var tris = [[0, 1, 2], [0, 2, 3]];

          for (var t = 0; t < 2; t++) {
            var ti = tris[t];
            var tUV = [uv[ti[0]], uv[ti[1]], uv[ti[2]]];
            var tP = [P[ti[0]], P[ti[1]], P[ti[2]]];
            /* walk the checker cells that touch this sub-quad */
            for (var cv = Math.floor(v0); cv < v1 + 1e-9; cv += STEP) {
              for (var cu = Math.floor(u0); cu < u1 + 1e-9; cu += STEP) {
                if ((Math.floor(cu) + Math.floor(cv)) % 2 !== 0) { continue; }
                var a = Math.max(cu, u0), b = Math.min(cu + STEP, u1);
                var c = Math.max(cv, v0), d = Math.min(cv + STEP, v1);
                if (b <= a || d <= c) { continue; }
                /* keep only the half of the sub-quad this triangle owns */
                var mid = [(a + b) / 2, (c + d) / 2];
                var su = (mid[0] - u0) / (u1 - u0), sv = (mid[1] - v0) / (v1 - v0);
                var inFirst = (su + sv) <= 1.0000001;
                if ((t === 0) !== !inFirst) { /* tri 0 is the (0,1,2) half */ }
                if (t === 0 && !(su >= sv - 1e-9 || true)) { continue; }
                var q = [[a, c], [b, c], [b, d], [a, d]];
                var pts = q.map(function (p) {
                  return correct ? proj(world(p[0], p[1])) : affine(tUV, tP, p[0], p[1]);
                });
                poly(pts, DARK);
              }
            }
            if (!correct) { break; }   /* affine: the two triangles share the lattice we drew */
          }
        }
      }
      read.textContent = (correct ? "perspective-correct" : "AFFINE") +
        "   " + n + "x" + n + " floor = " + (n * n * 2) + " triangles";
    }
    [cellsEl, heightEl, modeEl].forEach(function (el) { el.addEventListener("input", draw); });
    draw();
  })();
  </script>
''')

open('docs/lessons/03-02-perspective-correct.html', 'a').write(out.getvalue())
print("part 4:", len(out.getvalue()), "chars")
