# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write
AMB, TEA, VIO = "#E0A83C", "#3CB8A8", "#A070D8"

w('''
  <p>
    <strong>Failure three: two triangles can pass through each other.</strong> This is the deepest of
    the three, and the one that ends the argument. When two triangles intersect, the correct answer
    changes <em>halfway across one of them</em>. On the left of the intersection line A is nearer; on
    the right, B is. Figure&nbsp;2 shows it from above.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 300" role="img" aria-labelledby="fig2-t fig2-d">
      <title id="fig2-t">Two intersecting panels seen from above, and the resulting screen strip</title>
      <desc id="fig2-d">A top-down view: the eye is at the bottom, two panels cross in an X above it.
        To the left of the crossing point the amber panel is nearer the eye; to the right the teal
        panel is nearer. Below, a strip representing one row of screen pixels is amber on the left
        half and teal on the right half, with a marker showing where the crossing projects.</desc>

      <!-- top-down view -->
      <text x="30" y="26" class="sm" font-weight="700">seen from above</text>
      <text x="30" y="42" class="xs muted">the eye is at the bottom, looking up the page</text>

      <!-- sight lines -->
      <g class="grid" stroke-width="1">
        <path d="M 200 250 L 60 60"/>
        <path d="M 200 250 L 130 55"/>
        <path d="M 200 250 L 200 52"/>
        <path d="M 200 250 L 270 55"/>
        <path d="M 200 250 L 340 60"/>
      </g>

      <!-- the two panels, crossing -->
      <line x1="70" y1="70" x2="330" y2="170" stroke="''' + AMB + '''" stroke-width="7" stroke-linecap="round"/>
      <line x1="70" y1="170" x2="330" y2="70" stroke="''' + TEA + '''" stroke-width="7" stroke-linecap="round"/>
      <circle cx="200" cy="120" r="4.5" class="ink" fill="var(--dia-bg)" stroke-width="2"/>

      <!-- the eye -->
      <circle cx="200" cy="250" r="7" class="ink" fill="var(--dia-fill)" stroke-width="1.5"/>
      <text x="212" y="255" class="xs muted">eye</text>

      <text x="66" y="196" class="xs t-hi" text-anchor="middle">amber nearer</text>
      <text x="334" y="196" class="xs t-hi" text-anchor="middle">teal nearer</text>
      <text x="200" y="106" class="xs muted" text-anchor="middle">they cross here</text>

      <!-- resulting screen row -->
      <g transform="translate(420,60)">
        <text x="0" y="-24" class="sm" font-weight="700">one row of pixels</text>
        <rect x="0" y="0" width="100" height="34" fill="''' + AMB + '''" fill-opacity="0.92"/>
        <rect x="100" y="0" width="100" height="34" fill="''' + TEA + '''" fill-opacity="0.92"/>
        <rect x="0" y="0" width="200" height="34" fill="none" class="ink" stroke-width="1"/>
        <line x1="100" y1="-8" x2="100" y2="46" class="hi" stroke-width="2" stroke-dasharray="3 3"/>
        <text x="100" y="62" class="xs t-hi" text-anchor="middle">the crossing</text>
        <text x="0" y="98" class="xs muted">The correct answer is amber for</text>
        <text x="0" y="112" class="xs muted">half these pixels and teal for the</text>
        <text x="0" y="126" class="xs muted">other half. &ldquo;Which triangle is in</text>
        <text x="0" y="140" class="xs muted">front&rdquo; has no answer at all &mdash; only</text>
        <text x="0" y="154" class="xs muted">&ldquo;which triangle is in front <tspan font-style="italic">here</tspan>&rdquo;</text>
        <text x="0" y="168" class="xs muted">does.</text>
      </g>
    </svg>
    <figcaption><span class="fignum">Figure 2.</span> Intersecting panels, from above. This is the
      failure that cannot be argued with. The cycle in Figure 1 at least has no correct order;
      here a correct order exists <em>per pixel</em> and is simply not expressible per triangle. No
      sorting algorithm can produce it, because sorting produces one answer per object and the
      question has one answer per pixel.</figcaption>
  </figure>

  <p>
    Read those three failures together and they say the same thing three ways. <strong>Visibility is
    a per-pixel question, and sorting answers a per-object one.</strong> Every repair to the
    painter&rsquo;s algorithm &mdash; splitting long triangles, detecting cycles and splitting to
    break them, splitting at intersections &mdash; is the same repair: cut the geometry up until each
    piece is small enough that the per-object answer happens to be the per-pixel answer. Newell&rsquo;s
    algorithm does exactly that, and it is expensive, fiddly, and produces a triangle count you cannot
    predict.
  </p>

  <p>
    There is also a practical objection that has nothing to do with correctness. A sort is
    <em>global</em>: every triangle must be gathered up and compared against every other before
    anything can be drawn. That is <span class="badge">O(n log n)</span> per frame, it grows with the
    scene, it cannot start until the last triangle has been transformed, and it is exactly the shape
    of work a GPU &mdash; which wants to chew through triangles independently and in any order &mdash;
    is worst at.
  </p>

  <p>
    So we want something that decides visibility per pixel, needs no ordering, and lets each triangle
    be drawn without knowing the others exist. Remarkably, that thing is one float per pixel.
  </p>

  <!-- ================= 2. BUILDING INTUITION ================= -->
  <h2 id="intuition"><span class="num">2</span>Building Intuition</h2>

  <p>
    Forget triangles for a moment and think about a single pixel. That pixel is a tiny window onto
    the scene, and out through it goes a ray from the eye. Everything the scene contains along that
    ray is a candidate for what you see; what you actually see is <strong>whichever candidate the ray
    reaches first</strong>.
  </p>

  <p>
    That is the whole idea, and notice what it does <em>not</em> require. It does not require knowing
    the other pixels. It does not require knowing how many candidates there will be. It does not
    require them to arrive in any particular order. If you are told about candidates one at a time
    and you keep a note of &ldquo;the nearest one so far&rdquo;, then after the last one your note
    holds the right answer &mdash; and it held the right answer for the candidates seen so far at
    every point along the way.
  </p>

  <p>
    Keeping a running minimum is the oldest trick in programming, and it is order-independent by
    construction. The z-buffer is that trick, run a hundred thousand times in parallel &mdash; once
    per pixel. Alongside the colour buffer we keep a second buffer, the same size, holding one number
    per pixel: <em>how far away is the thing whose colour is currently sitting there</em>. Figure 3
    is the picture to carry around.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 300" role="img" aria-labelledby="fig3-t fig3-d">
      <title id="fig3-t">One pixel, one ray, two candidate surfaces</title>
      <desc id="fig3-d">A ray leaves the eye through one pixel of the screen and meets two surfaces
        at different distances. The nearer hit is marked as the winner. To the right, two grids
        labelled colour buffer and depth buffer show the same pixel highlighted in each: the colour
        buffer holds a colour, the depth buffer holds the number 0.31.</desc>

      <!-- eye -->
      <circle cx="46" cy="150" r="8" class="ink" fill="var(--dia-fill)" stroke-width="1.5"/>
      <text x="30" y="178" class="xs muted">eye</text>

      <!-- screen plane -->
      <line x1="120" y1="60" x2="120" y2="240" class="ink" stroke-width="2"/>
      <text x="96" y="52" class="xs muted" text-anchor="middle">screen</text>
      <rect x="114" y="140" width="12" height="12" fill="var(--dia-hi)" fill-opacity="0.85"/>
      <text x="128" y="136" class="xs t-hi">this pixel</text>

      <!-- the ray -->
      <defs>
        <marker id="f3arrow" viewBox="0 0 10 10" refX="9" refY="5"
                markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M 0 0 L 10 5 L 0 10 z" fill="var(--dia-hi)"/>
        </marker>
      </defs>
      <path d="M 46 150 L 350 172" class="hi" stroke-width="2" fill="none" marker-end="url(#f3arrow)"/>

      <!-- two surfaces -->
      <line x1="196" y1="96" x2="220" y2="230" stroke="''' + TEA + '''" stroke-width="6" stroke-linecap="round"/>
      <line x1="286" y1="90" x2="300" y2="228" stroke="''' + AMB + '''" stroke-width="6" stroke-linecap="round"/>
      <circle cx="208" cy="161" r="5" class="ink" fill="var(--dia-bg)" stroke-width="2"/>
      <circle cx="293" cy="167" r="5" class="ink" fill="var(--dia-bg)" stroke-width="2"/>
      <text x="208" y="252" class="xs t-ok" text-anchor="middle">hit at 0.31</text>
      <text x="293" y="272" class="xs muted" text-anchor="middle">hit at 0.58</text>
      <text x="208" y="266" class="xs t-ok" text-anchor="middle">WINS</text>

      <!-- the two buffers -->
      <g transform="translate(410,50)">
        <text x="0" y="-16" class="sm" font-weight="700">colour buffer</text>
        <g class="grid" stroke-width="1">
          <rect x="0" y="0" width="100" height="60" fill="none"/>
          <path d="M 25 0 V 60 M 50 0 V 60 M 75 0 V 60 M 0 20 H 100 M 0 40 H 100"/>
        </g>
        <rect x="25" y="20" width="25" height="20" fill="''' + TEA + '''" fill-opacity="0.92"/>

        <text x="0" y="106" class="sm" font-weight="700">depth buffer</text>
        <g class="grid" stroke-width="1">
          <rect x="0" y="120" width="100" height="60" fill="none"/>
          <path d="M 25 120 V 180 M 50 120 V 180 M 75 120 V 180 M 0 140 H 100 M 0 160 H 100"/>
        </g>
        <rect x="25" y="140" width="25" height="20" fill="var(--dia-hi)" fill-opacity="0.2"/>
        <text x="37" y="154" class="xs mono t-hi" text-anchor="middle">.31</text>
        <text x="120" y="34" class="xs muted">same width,</text>
        <text x="120" y="48" class="xs muted">same height,</text>
        <text x="120" y="62" class="xs muted">one entry per pixel</text>
        <text x="120" y="146" class="xs muted">a float, not a colour:</text>
        <text x="120" y="160" class="xs muted">0 at the near plane,</text>
        <text x="120" y="174" class="xs muted">1 at the far plane</text>
      </g>
    </svg>
    <figcaption><span class="fignum">Figure 3.</span> The depth buffer is a second sheet of paper the
      same size as the first. Where the colour buffer records <em>what</em> you see at a pixel, the
      depth buffer records <em>how far away it was</em> &mdash; which is exactly the information you
      need to decide whether the next thing to arrive should replace it.</figcaption>
  </figure>

  <p>
    We are not going to trace rays; we are going to keep rasterizing triangles, exactly as Lesson 2.2
    taught. But the accounting is the same. When a triangle covers a pixel, it is announcing a
    candidate. We work out how far away <em>that triangle</em> is <em>at that pixel</em>, compare it
    with the note, and keep the nearer one.
  </p>

  <div class="callout note">
    <span class="label">Where this comes from</span>
    <p>
      Ed Catmull described the z-buffer in his 1974 doctoral thesis at Utah, and Wolfgang Stra&szlig;er
      arrived at the same idea independently in his thesis the same year. At the time it was
      considered extravagant &mdash; a full extra buffer of memory, when memory was the most expensive
      thing in the building. Catmull himself noted the cost. Fifty years later every GPU on earth has
      one in hardware, with dedicated compression and early-rejection circuitry built around it. It is
      the clearest example in graphics of a brute-force answer winning because it is
      <em>order-independent</em>, and therefore parallel.
    </p>
  </div>

  <!-- ================= 3. THE THEORY ================= -->
  <h2 id="theory"><span class="num">3</span>The Theory</h2>

  <h3 id="algorithm">3.1 The algorithm, stated</h3>

  <p>
    In full, and it really is this short:
  </p>

  <figure class="listing">
    <figcaption><span class="path">the z-buffer, in words</span></figcaption>
    <pre><code>clear every depth to FAR
clear every pixel to the background colour

for each triangle, in any order whatsoever:
    for each pixel the triangle covers:
        z = the triangle&#39;s depth AT THIS PIXEL
        if z &lt; depth[pixel]:
            depth[pixel]  = z
            colour[pixel] = the triangle&#39;s colour at this pixel</code></pre>
  </figure>

  <p>
    Three properties are worth naming out loud, because each one is a thing the painter&rsquo;s
    algorithm cannot do.
  </p>

  <p>
    <strong>It is order-independent.</strong> Swap any two triangles in the loop and the final image is
    identical, because <code>min</code> does not care about order. That is what makes it safe to hand
    triangles to a GPU in whatever order they finish being transformed &mdash; and, in Module 8, safe
    to rasterize them on several threads.
  </p>

  <p>
    <strong>It is local.</strong> A triangle needs to know nothing about any other triangle. There is
    no gathering phase, no global sort, no <span class="badge">O(n log n)</span>. Cost grows with the
    number of <em>pixels covered</em>, not with the square or the log of the triangle count.
  </p>

  <p>
    <strong>It handles all three failures without noticing them.</strong> Intersecting triangles?
    Each pixel is decided on its own, so the intersection line appears for free &mdash; no one ever
    computes it. Cycles? There is no ordering to be cyclic. Long triangles? Depth is evaluated per
    pixel, so nothing is ever averaged away.
  </p>

  <h3 id="clear">3.2 What to clear it to</h3>

  <p>
    Our depth convention is fixed and comes from SDL_GPU (<a href="../conventions.html#ndc">Conventions
    &sect;4</a>): device depth runs <code>0</code> at the near plane to <code>1</code> at the far
    plane, and <em>larger means further away</em>. So &ldquo;nothing has been drawn here yet&rdquo;
    must be represented by the largest possible depth, <code>1.0</code>, so that the first triangle to
    arrive beats it.
  </p>

  <div class="callout warn">
    <span class="label">Clear to zero and you get a black screen</span>
    <p>
      Zero is the <em>near</em> plane &mdash; the closest anything can possibly be. A depth buffer
      full of zeros is a claim that the entire screen is already covered by something pressed against
      the camera lens, and every triangle you submit loses its test. Not &ldquo;most&rdquo;: every
      single one. The symptom is a perfectly clean background and no geometry at all, which reads like
      a broken transform rather than a broken clear, and sends people looking in entirely the wrong
      file. Exercise 3.1.1 asks you to produce it deliberately, once, so you never have to diagnose
      it slowly.
    </p>
  </div>

  <p>
    And the clear must happen <strong>every frame</strong>, before anything is drawn. Skip it and last
    frame&rsquo;s depths survive into this one, so this frame&rsquo;s geometry is tested against
    surfaces that are no longer on screen. The image does not break all at once &mdash; it accumulates
    holes, as though the scene were slowly eating itself.
  </p>

  <h3 id="which-depth">3.3 Which depth do we store?</h3>

  <p>
    Here is the question that makes this lesson more than an afternoon&rsquo;s coding, and it has a
    genuinely surprising answer.
  </p>

  <p>
    We have two plausible candidates for &ldquo;how far away is this pixel&rdquo;. There is
    <strong>view-space z</strong> &mdash; a real distance in world units, negative in front of the
    camera, the thing your intuition means by depth. And there is <strong>device depth</strong> &mdash;
    the <code>[0,1]</code> value that comes out of the projection matrix and the perspective divide,
    which Lesson 2.11&rsquo;s viewport transform has been computing and throwing away for two lessons.
  </p>

  <p>
    View-space z looks like the obvious choice. It is a physical quantity, it is what a human means by
    depth, and the numbers are readable. It is also <strong>wrong</strong>, and not by a little.
  </p>

  <p>
    The reason is not about depth at all &mdash; it is about interpolation. We do not have the depth
    at every pixel; we have it at the three corners, and we get the interior by barycentric
    interpolation, which is the machinery Lesson 2.3 built and 2.4 generalised. Barycentric
    interpolation computes the unique <strong>affine</strong> function of the pixel position that
    agrees with the three corner values. If the quantity you are interpolating genuinely is an affine
    function of the pixel position, the answer is exact. If it is not, the answer is a straight line
    drawn under a curve, and the gap between them is your error.
  </p>

  <p>
    So the question &ldquo;which depth&rdquo; becomes a precise one: <em>which of these two quantities
    is an affine function of the position on the screen?</em> The next section answers it, and the
    answer is the one that looks less physical.
  </p>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 2 appended:", len(out.getvalue()), "chars")
