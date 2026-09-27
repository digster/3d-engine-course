# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write

A = 100.0 / (1.0 - 100.0)
B = 100.0 * 1.0 / (1.0 - 100.0)
codes = [i / 10.0 for i in range(11)]
dists = [B / (c + A) for c in codes[:-1]] + [100.0]

X0, X1 = 34.0, 614.0
def x_code(c):        return X0 + c * (X1 - X0)
def x_dist100(d):     return X0 + ((d - 1.0) / 99.0) * (X1 - X0)
def x_dist10(d):      return X0 + ((d - 1.0) / 9.0) * (X1 - X0)

w('''
  <h3 id="precision">3.6 Precision, and z-fighting</h3>

  <p>
    The z-buffer is exact in the sense that matters &mdash; the comparison it makes is the right
    comparison, at the right place, for any geometry. What it is not is <em>infinitely precise</em>,
    and the way its precision is distributed is deeply lopsided. This is where z-fighting comes from,
    and it is the one part of this lesson you will still be using in ten years.
  </p>

  <p>
    Start from the relation we already derived, <code>z_n = &minus;A + B/w</code>. Differentiate it to
    ask &ldquo;how much real distance does one step of depth buy me?&rdquo;
  </p>

  <div class="eq">
    <span class="katex-src">\\[ \\frac{dz_n}{dw} = -\\frac{B}{w^2} \\qquad\\Longrightarrow\\qquad \\Delta w = \\Delta z_n \\cdot \\frac{w^2}{-B} = \\Delta z_n \\cdot w^2\\left(\\frac{1}{near}-\\frac{1}{far}\\right) \\]</span>
    <span class="eq-plain">dz_ndc/dw = -B/w²   so   Δw = Δz_ndc * w² * (1/near - 1/far)</span>
  </div>

  <p>
    Read that formula slowly; it contains three separate practical facts.
  </p>

  <p>
    <strong>It is quadratic in distance.</strong> Twice as far away means <em>four times</em> coarser
    depth. Nothing else in a renderer degrades that fast.
  </p>

  <p>
    <strong>Your near plane is the expensive one.</strong> The bracket is
    <code>1/near &minus; 1/far</code>, and for any sane scene <code>1/near</code> dominates it
    completely. Halving <code>near</code> roughly halves your depth precision at every distance,
    everywhere in the scene.
  </p>

  <p>
    <strong>Your far plane is nearly free.</strong> With <code>near = 1</code>, moving <code>far</code>
    from 100 to 1000 changes the bracket from <code>0.99</code> to <code>0.999</code> &mdash; nine
    tenths of one percent worse. Moving <code>near</code> from 1 to 0.1 changes it from
    <code>0.99</code> to <code>9.99</code>: <strong>10.09&times; worse</strong>. If you take one
    practical rule from this lesson, take this one: <em>push your near plane out as far as your game
    can bear, and stop worrying about the far plane.</em>
  </p>

  <p>
    Figure 5 makes the lopsidedness visible. It takes eleven evenly spaced depth values &mdash;
    <code>0.0, 0.1, &hellip; 1.0</code> &mdash; and asks where each one lands in the world.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 320" role="img" aria-labelledby="fig5-t fig5-d">
      <title id="fig5-t">Evenly spaced depth values land at wildly uneven distances</title>
      <desc id="fig5-d">Three horizontal axes. The top axis shows eleven evenly spaced depth values
        from 0 to 1. The middle axis shows distance from the eye, 1 to 100 units, with lines
        connecting each depth value to the distance it represents: ten of the eleven land within the
        first eight percent of the axis and the eleventh is at the far right. The bottom axis zooms
        into distances 1 to 10 and shows the same ten values spread out, revealing that half of the
        entire depth range is spent between 1 and 2 units from the eye.</desc>
''')

# --- row 1: depth codes
w('      <text x="34" y="34" class="sm" font-weight="700">depth value, evenly spaced</text>\n')
w('      <line x1="%.1f" y1="56" x2="%.1f" y2="56" class="ink" stroke-width="1.5"/>\n' % (X0, X1))
for i, c in enumerate(codes):
    x = x_code(c)
    w('      <line x1="%.1f" y1="48" x2="%.1f" y2="64" class="ink" stroke-width="1.5"/>\n' % (x, x))
w('      <text x="%.1f" y="80" class="xs mono muted" text-anchor="middle">0.0</text>\n' % x_code(0.0))
w('      <text x="%.1f" y="80" class="xs mono muted" text-anchor="middle">0.5</text>\n' % x_code(0.5))
w('      <text x="%.1f" y="80" class="xs mono muted" text-anchor="middle">1.0</text>\n' % x_code(1.0))

# --- connectors
w('      <g class="ink-soft" stroke-width="1">\n')
for c, d in zip(codes, dists):
    w('        <line x1="%.1f" y1="66" x2="%.1f" y2="150" stroke-dasharray="2 3"/>\n'
      % (x_code(c), x_dist100(d)))
w('      </g>\n')

# --- row 2: distance 1..100
w('      <text x="34" y="140" class="sm" font-weight="700">where they land: distance from the eye (1 to 100 units)</text>\n')
w('      <line x1="%.1f" y1="162" x2="%.1f" y2="162" class="ink" stroke-width="1.5"/>\n' % (X0, X1))
for c, d in zip(codes, dists):
    x = x_dist100(d)
    w('      <line x1="%.1f" y1="154" x2="%.1f" y2="170" class="hi" stroke-width="1.5"/>\n' % (x, x))
w('      <text x="%.1f" y="186" class="xs mono muted">1</text>\n' % X0)
w('      <text x="%.1f" y="186" class="xs mono muted" text-anchor="end">100</text>\n' % X1)
w('      <text x="200" y="204" class="xs t-bad">ten of the eleven are inside the first 8% of the axis</text>\n')

# --- row 3: zoom 1..10
w('      <text x="34" y="242" class="sm" font-weight="700">the same ten, zoomed: distance 1 to 10</text>\n')
w('      <line x1="%.1f" y1="264" x2="%.1f" y2="264" class="ink" stroke-width="1.5"/>\n' % (X0, X1))
for c, d in zip(codes[:-1], dists[:-1]):
    x = x_dist10(d)
    w('      <line x1="%.1f" y1="256" x2="%.1f" y2="272" class="hi" stroke-width="1.5"/>\n' % (x, x))
w('      <text x="%.1f" y="288" class="xs mono muted">1</text>\n' % X0)
w('      <text x="%.1f" y="288" class="xs mono muted" text-anchor="end">10</text>\n' % X1)
w('      <line x1="%.1f" y1="250" x2="%.1f" y2="250" class="ax-y" stroke-width="3"/>\n'
  % (x_dist10(1.0), x_dist10(dists[5])))
w('      <text x="%.1f" y="244" class="xs t-ok" text-anchor="middle">half the depth range lives in here</text>\n'
  % ((x_dist10(1.0) + x_dist10(dists[5])) / 2.0 + 66))
w('''    </svg>
    <figcaption><span class="fignum">Figure 5.</span> Depth precision with <code>near&nbsp;=&nbsp;1</code>,
      <code>far&nbsp;=&nbsp;100</code>. Eleven evenly spaced depth values, and the distances they
      actually mean. The value <code>0.5</code> &mdash; the exact middle of the buffer&rsquo;s range
      &mdash; sits at 1.98 units, so <strong>half of your entire depth buffer is spent on the first
      one percent of your view distance</strong>, and the last tenth of it has to cover everything
      from 9.17 units to the horizon. This is not a defect to be fixed; it is the direct consequence
      of dividing by <em>w</em>, which is also the thing that makes perspective look right.</figcaption>
  </figure>

  <p>
    Now put a real format on it. A GPU depth attachment is a texture with a format, and SDL_GPU offers
    <code>SDL_GPU_TEXTUREFORMAT_D16_UNORM</code>, <code>D24_UNORM</code> and <code>D32_FLOAT</code>
    among others. A 16-bit unorm buffer has 65,535 evenly spaced codes between 0 and 1, so
    <code>&Delta;z_n = 1/65535</code>, and the formula above turns that into real units:
  </p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">Distance represented by one depth code, near = 1, far = 100</caption>
      <thead>
        <tr><th>Distance from eye</th><th>D16_UNORM</th><th>D24_UNORM</th><th>What that means</th></tr>
      </thead>
      <tbody>
        <tr><td>2 units</td><td><code>0.000060</code></td><td><code>0.00000023</code></td><td>sub-millimetre; nothing will ever fight</td></tr>
        <tr><td>10 units</td><td><code>0.0015</code></td><td><code>0.0000059</code></td><td>1.5&nbsp;mm at 16 bits</td></tr>
        <tr><td>50 units</td><td><code>0.038</code></td><td><code>0.00015</code></td><td>4&nbsp;cm &mdash; decals start to flicker</td></tr>
        <tr><td>90 units</td><td><code>0.12</code></td><td><code>0.00048</code></td><td>12&nbsp;cm at 16 bits; a floor and a rug merge</td></tr>
      </tbody>
    </table>
  </div>

  <p>
    Two surfaces closer together than one code <em>store the same number</em>. The comparison
    <code>z &lt; stored</code> then fails, the second surface loses, and which one you see depends
    entirely on which was drawn first. As the camera moves, the rounding tips back and forth from
    pixel to pixel, and you get the shimmering interleaved mess everyone recognises on sight:
    <strong>z-fighting</strong>.
  </p>

  <div class="callout warn">
    <span class="label">Reproduce it on demand</span>
    <p>
      The demo&rsquo;s fourth scene is two large panels one millimetre apart. Press <kbd>C</kbd> until
      you reach it and <kbd>B</kbd> to change the depth format. At <code>D32_FLOAT</code> and
      <code>D24_UNORM</code> the near panel wins every one of the 875 covered pixels. At
      <code>D16_UNORM</code> it loses <strong>478 of them &mdash; 54.6%</strong>, in bands that crawl
      as you orbit. The arithmetic predicts it: at that distance one 16-bit code spans 0.00208 units
      and the gap is 0.001, so the two panels round to the same code slightly less than half the time.
      Measured, not asserted &mdash; the numbers come from
      <code>scratch/verify_31_render.cpp</code>.
    </p>
  </div>

  <p>
    The widget below is the formula, made draggable. It is worth two minutes: pull the near plane
    down and watch the whole scene lose precision at once.
  </p>

  <div class="widget">
    <strong>Depth precision explorer</strong>
    <svg id="pz-svg" viewBox="0 0 620 210" role="img" aria-labelledby="w2-t w2-d">
      <title id="w2-t">Interactive depth precision plot</title>
      <desc id="w2-d">A plot of how much real distance one depth code spans, against distance from
        the eye, for the chosen near plane, far plane and depth format. Sliders change each.</desc>
      <line x1="46" y1="170" x2="600" y2="170" stroke="var(--dia-ink)" stroke-width="1.5"/>
      <line x1="46" y1="16" x2="46" y2="170" stroke="var(--dia-ink)" stroke-width="1.5"/>
      <polyline id="pz-curve" points="" fill="none" stroke="var(--dia-hi)" stroke-width="2.5"/>
      <text x="320" y="196" class="xs" fill="var(--dia-ink-soft)" text-anchor="middle" font-family="var(--font-ui)" font-size="10">distance from the eye &rarr;</text>
      <text id="pz-ymax" x="42" y="20" class="xs" fill="var(--dia-ink-soft)" text-anchor="end" font-family="var(--font-mono)" font-size="10">&nbsp;</text>
      <text id="pz-xmax" x="600" y="188" class="xs" fill="var(--dia-ink-soft)" text-anchor="end" font-family="var(--font-mono)" font-size="10">&nbsp;</text>
      <text x="52" y="180" class="xs" fill="var(--dia-ink-soft)" font-family="var(--font-mono)" font-size="10">0</text>
    </svg>
    <div class="widget-controls">
      <label for="pz-near">near</label>
      <input id="pz-near" type="range" min="1" max="300" step="1" value="100">
      <label for="pz-far">far</label>
      <input id="pz-far" type="range" min="50" max="2000" step="10" value="100">
      <label for="pz-bits">bits</label>
      <input id="pz-bits" type="range" min="16" max="24" step="8" value="16">
      <span class="widget-readout" id="pz-read">&nbsp;</span>
    </div>
  </div>

  <script>
  /* Page-specific widget for Lesson 3.1 — OUTSIDE the SHARED-SCRIPT markers so
     apply-shared.py never touches it. Plots Δw = Δz * w² * (1/near - 1/far). */
  (function () {
    "use strict";
    var nearEl = document.getElementById("pz-near");
    var farEl = document.getElementById("pz-far");
    var bitsEl = document.getElementById("pz-bits");
    var curve = document.getElementById("pz-curve");
    var read = document.getElementById("pz-read");
    var ymax = document.getElementById("pz-ymax");
    var xmax = document.getElementById("pz-xmax");
    if (!nearEl || !curve) { return; }

    function draw() {
      var near = nearEl.value / 100.0;          /* 0.01 .. 3.00 */
      var far = +farEl.value;
      if (far < near * 4) { far = near * 4; }
      var bits = +bitsEl.value;
      var dz = 1.0 / (Math.pow(2, bits) - 1);
      var bracket = 1.0 / near - 1.0 / far;

      /* Sample across the visible range and find the peak for scaling. */
      var pts = [], peak = 0, i, w, dw;
      for (i = 0; i <= 120; i++) {
        w = near + (far - near) * (i / 120);
        dw = dz * w * w * bracket;
        pts.push([w, dw]);
        if (dw > peak) { peak = dw; }
      }
      var s = pts.map(function (p) {
        var x = 46 + (p[0] - near) / (far - near) * 554;
        var y = 170 - (peak > 0 ? p[1] / peak : 0) * 152;
        return x.toFixed(1) + "," + y.toFixed(1);
      }).join(" ");
      curve.setAttribute("points", s);

      var mid = dz * Math.pow(far * 0.1, 2) * bracket;
      ymax.textContent = peak.toFixed(peak < 0.1 ? 5 : 3);
      xmax.textContent = far.toFixed(0);
      read.textContent = "near " + near.toFixed(2) + "  far " + far.toFixed(0) +
        "  D" + bits + "   one code = " + mid.toPrecision(3) +
        " units at " + (far * 0.1).toFixed(1) + " units";
    }
    [nearEl, farEl, bitsEl].forEach(function (el) { el.addEventListener("input", draw); });
    draw();
  })();
  </script>

  <div class="callout note">
    <span class="label">What professionals do about it</span>
    <p>
      Three things, in order of how often they are the answer. <strong>Move the near plane out</strong>
      &mdash; free, and usually enough. <strong>Use a 24- or 32-bit depth format</strong> &mdash; costs
      bandwidth, and note from the table that D24 is roughly 256&times; finer than D16 everywhere.
      And <strong>reversed-Z</strong>: map near to 1 and far to 0, clear to 0, and test with
      <code>&gt;</code>. That sounds like it should change nothing, and with a floating-point depth
      buffer it changes everything &mdash; float has its own precision crowded near zero, which
      cancels almost exactly against 1/<em>w</em>&rsquo;s crowding near the near plane. It is close to
      free and it is what modern engines do. Exercise 3.1.4 has you build it.
    </p>
  </div>

  <h3 id="cost">3.7 What it costs</h3>

  <p>
    <strong>Memory:</strong> one float per pixel. At our 320&times;180 framebuffer that is 230 KB; at
    1920&times;1080 with 32-bit depth it is 8.3 MB. That was an outrageous price in 1974 and is
    a rounding error now, which is the entire reason the technique won.
  </p>

  <p>
    <strong>Time:</strong> per covered pixel, one multiply-add-multiply-add to interpolate the depth,
    one load, one compare, and a conditional store. Compare that against the painter&rsquo;s
    algorithm&rsquo;s per-frame sort of the whole scene and the z-buffer is not merely more correct,
    it is usually faster too.
  </p>

  <p>
    And there is a bonus hiding in the code we are about to write. Look at the order of operations:
    the depth test runs <em>before</em> the colour is computed, so a pixel that loses never pays for
    its colour at all. With flat colours that saves almost nothing. With Module 6&rsquo;s PBR shading
    it saves an enormous amount, and it is why hardware has a dedicated
    <strong>early-Z</strong> stage that rejects fragments before the fragment shader ever runs &mdash;
    and why drawing roughly front-to-back, even though the z-buffer does not require any order, is one
    of the cheapest optimisations in real-time rendering. Exercise 3.1.3 measures it.
  </p>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 4 appended:", len(out.getvalue()), "chars")
