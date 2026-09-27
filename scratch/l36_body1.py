# Lesson 3.6 body — part 1: header, objectives, TOC, the problem, intuition.
BODY1 = r"""
  <div class="lesson-head">
    <div class="eyebrow">Module 3 — Software Rasterizer II: Depth, Light, Texture · Lesson 3.6</div>
    <h1>Normals and Lambert's Cosine Law</h1>
    <p class="deck">
      Everything on screen has been coloured by a lookup table indexed on triangle number. Today
      a real light arrives, and surfaces start responding to which way they face — which turns
      out to require a matrix that is <em>not</em> the model matrix.
    </p>

    <div class="meta">
      <dl>
        <dt>Time</dt>
        <dd>≈ 5 hours</dd>

        <dt>Prereqs</dt>
        <dd>
          <a href="03-05-obj-loader.html">3.5 — A Hand-Rolled OBJ Loader</a>,
          <a href="01-07-vectors-2d.html">1.7 — Vectors, Geometrically</a>,
          <a href="01-06-colour.html">1.6 — Colour, and the Trap in the Middle</a>,
          <a href="02-05-matrices.html">2.5 — Matrices as Basis Transforms</a>
        </dd>

        <dt>Files</dt>
        <dd>
          <code>src/gfx/light.hpp</code>, <code>src/math/mat4.hpp</code>,
          <code>src/main.cpp</code>
        </dd>

        <dt>Milestone</dt>
        <dd>
          A torus that looks like an object rather than a pattern — lit by a light you can swing
          with <kbd>A</kbd>/<kbd>D</kbd>, and correct on squashed geometry that the obvious
          implementation gets 97.5% wrong.
        </dd>
      </dl>
    </div>
  </div>

  <section class="objectives" aria-labelledby="obj-h">
    <h2 id="obj-h">By the end of this lesson you will be able to…</h2>
    <ul>
      <li>…derive Lambert's cosine law from a picture of a spreading beam, without looking up a
          formula.</li>
      <li>…say exactly why <code>max(0, ·)</code> is in the shading term, and describe the
          artifact you get without it.</li>
      <li>…explain why a normal is not transformed by the model matrix, and <em>derive</em> the
          inverse transpose from the one property that defines a normal.</li>
      <li>…predict when that distinction is invisible — and why that makes it a bug that
          ships.</li>
      <li>…distinguish where a normal comes from (per-face or per-vertex) from where lighting is
          evaluated, and say which of those Lesson 3.8 is about.</li>
      <li>…keep every lighting multiply in linear light, and say what goes wrong otherwise.</li>
    </ul>
  </section>

  <details class="toc" open>
    <summary>Contents</summary>
    <ol>
      <li><a href="#problem">The Problem</a></li>
      <li><a href="#intuition">Building Intuition</a>
        <ol>
          <li><a href="#intuition-beam">Light does not get dimmer, it gets spread</a></li>
          <li><a href="#intuition-terminator">The terminator, and what lies beyond it</a></li>
        </ol>
      </li>
      <li><a href="#theory">The Theory</a>
        <ol>
          <li><a href="#theory-lambert">Lambert's cosine law, derived</a></li>
          <li><a href="#theory-dot">…and why the dot product is already the answer</a></li>
          <li><a href="#theory-clamp">The clamp is not a detail</a></li>
          <li><a href="#theory-albedo">Albedo, and why every multiply is linear</a></li>
          <li><a href="#theory-normal-matrix">The matrix that transforms normals</a></li>
          <li><a href="#theory-hides">When it does not matter — which is why it ships</a></li>
          <li><a href="#theory-where">Where the normal comes from: per-face or per-vertex</a></li>
        </ol>
      </li>
      <li><a href="#implementation">Implementation</a>
        <ol>
          <li><a href="#impl-light">A light, and a shading function</a></li>
          <li><a href="#impl-matrix"><code>normal_matrix</code>, and where it lives</a></li>
          <li><a href="#impl-vertex">Shading per vertex, in world space</a></li>
          <li><a href="#impl-fallback">Meshes with no normals</a></li>
          <li><a href="#impl-demo">The three modes, and two comparisons</a></li>
        </ol>
      </li>
      <li><a href="#listings">Complete Code Listings</a></li>
      <li><a href="#build">Build &amp; Run</a></li>
      <li><a href="#pitfalls">Common Pitfalls &amp; Debugging</a></li>
      <li><a href="#exercises">Exercises</a></li>
      <li><a href="#recap">Recap &amp; Next</a></li>
      <li><a href="#reading">Further Reading</a></li>
    </ol>
  </details>

  <h2 id="problem"><span class="num">1</span>The Problem</h2>

  <p>
    Here is the function that has coloured every solid surface in this course since Lesson 3.1.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp — the fake we have been living with</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">[[nodiscard]] Uint32 face_shade(Uint32 base, std::size_t face)
{
    constexpr float k_steps[5] = {1.00f, 0.84f, 0.70f, 0.57f, 0.45f};
    const float k = k_steps[face % 5];
    // …scale `base` by k, in linear light…
}</code></pre>
  </figure>

  <p>
    Read what it takes as input: a colour, and <strong>the index of the triangle</strong>. Not a
    normal. Not a light. Not a position. The brightness of a surface in our engine currently
    depends on nothing except where it happened to sit in an index buffer.
  </p>

  <p>
    It has been useful — it is the only reason adjacent faces of the cube were ever
    distinguishable — and it has one property that gives it away completely:
  </p>

  <div class="callout warn">
    <span class="label">The failure mode — and it is a strange one, because nothing looks broken</span>
    <p>
      <strong>Spin the object and the shading does not move.</strong> A face that was the third
      brightest step stays the third brightest step forever, because <code>face % 5</code> does
      not change when the object turns. Real surfaces do the opposite: turn a book towards a
      window and the cover brightens.
    </p>
    <p>
      That is the tell for fake lighting, and it is worth learning to see, because the symptom is
      not "wrong colours" — every individual frame looks plausible. The symptom is
      <em>the absence of a relationship</em> between the geometry and the light. Once you notice
      it you cannot unnotice it, and you will spot it in other people's screenshots.
    </p>
  </div>

  <p>
    There is a second, quieter cost. Look at the torus from Lesson 3.5 under that palette
    (Figure 3 shows the three pictures side by side later on): 2,304 triangles, each assigned one
    of five brightnesses by its index, produces a <em>checkerboard of noise</em> laid over a
    shape. You can see the silhouette and nothing else — no sense of which parts bulge toward you
    and which fall away. The palette is not merely fake, it is actively hiding the geometry that
    Lesson 3.5 worked so hard to load.
  </p>

  <p>
    And there is an unspent asset. Lesson 3.5's loader read a <code>vn</code> for every vertex of
    every model and put them in <code>mesh::normals</code>, and the header said plainly at the
    time that <em>nothing reads them yet</em>. 1,150 normals in <code>torus.obj</code>, parsed,
    de-duplicated, index-matched, and unused. This lesson is where they do something.
  </p>

  <div class="callout note">
    <span class="label">What this lesson is not</span>
    <p>
      It is not "how to make things look good". Lambert is the <em>diffuse</em> term only — it
      describes a perfectly matte surface, like chalk or unfinished paper, and it is deliberately
      the entire model today. No highlights (that is 3.7), no shadows cast by other objects (that
      is Module 6), no bounced light (Module 6 as well, and the constant we add in its place is
      labelled a fudge in the code). Building the simplest honest model first is what makes the
      next three lessons' additions <em>legible</em> rather than magical.
    </p>
  </div>

  <h2 id="intuition"><span class="num">2</span>Building Intuition</h2>

  <h3 id="intuition-beam">2.1 Light does not get dimmer, it gets spread</h3>

  <p>
    Everyone knows a surface facing a light is brighter than one at an angle. The question is
    <em>why</em>, and the usual answer — "less light hits it" — is wrong in a way that matters.
  </p>

  <p>
    Take a torch and point it straight down at a table. It makes a bright circle. Now tilt the
    torch. The circle becomes an ellipse — <strong>larger</strong> than the circle was. Not one
    photon has been lost; the torch is emitting exactly what it emitted before. The same light is
    now spread over more table.
  </p>

  <p>
    That is the whole of Lambert's law, and it contains no physics beyond arithmetic:
    <strong>brightness is power per unit area, and tilting the surface increases the area without
    changing the power.</strong> Figure 1 is that sentence as a picture.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 700 360" role="img" aria-labelledby="fig1-t fig1-d">
      <title id="fig1-t">A beam of fixed width spreads over a larger footprint when the surface tilts</title>
      <desc id="fig1-d">Two panels. On the left a vertical beam of width one strikes a horizontal
        surface and covers a footprint of width one. On the right the same beam of width one
        strikes a surface tilted by an angle theta, marked between the surface normal and the
        beam, and covers a footprint of width one over cosine theta, which is drawn visibly
        wider. Labels state that the power is the same in both cases so the power per unit area
        falls by cosine theta.</desc>

      <defs>
        <marker id="f1-ray" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/>
        </marker>
        <marker id="f1-n" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-y)"/>
        </marker>
      </defs>

      <!-- LEFT: head-on -->
      <g transform="translate(30,20)">
        <text x="140" y="12" class="sm" text-anchor="middle" font-weight="700">head-on</text>

        <!-- the beam -->
        <rect x="95" y="36" width="90" height="150" fill="var(--warn-bg)" opacity="0.55"/>
        <g class="hi" stroke-width="1.6" fill="none" marker-end="url(#f1-ray)">
          <line x1="110" y1="40" x2="110" y2="180"/>
          <line x1="140" y1="40" x2="140" y2="180"/>
          <line x1="170" y1="40" x2="170" y2="180"/>
        </g>

        <!-- surface -->
        <line x1="40" y1="190" x2="240" y2="190" class="ink" stroke-width="2.4"/>
        <line class="ax-y" x1="140" y1="190" x2="140" y2="130" stroke-width="2" marker-end="url(#f1-n)"/>
        <text x="148" y="140" class="xs lbl-y">n</text>

        <!-- footprint -->
        <line x1="95" y1="204" x2="185" y2="204" class="ink-soft" stroke-width="1.4"/>
        <line x1="95" y1="199" x2="95" y2="209" class="ink-soft" stroke-width="1.4"/>
        <line x1="185" y1="199" x2="185" y2="209" class="ink-soft" stroke-width="1.4"/>
        <text x="140" y="224" class="sm mono" text-anchor="middle">1.00</text>
        <text x="140" y="244" class="xs muted" text-anchor="middle">footprint</text>
        <text x="140" y="296" class="sm" text-anchor="middle" font-weight="700">power / area = 1.00</text>
      </g>

      <line x1="350" y1="40" x2="350" y2="280" class="ink-soft" stroke-width="1" stroke-dasharray="4 4"/>

      <!-- RIGHT: tilted 60 degrees.
           The surface runs along (0.5, -0.866) in screen coordinates — 60 degrees
           from horizontal — so its normal is 60 degrees from the vertical beam and
           the footprint is exactly 1/cos(60) = 2 beam-widths. Every number in this
           panel is read off those coordinates, not asserted next to them. -->
      <g transform="translate(380,20)">
        <text x="150" y="12" class="sm" text-anchor="middle" font-weight="700">tilted by θ = 60°</text>

        <!-- the same beam, the same width, shaded down to where it lands -->
        <polygon points="95,36 185,36 185,112 95,268" fill="var(--warn-bg)" opacity="0.55"/>
        <g class="hi" stroke-width="1.6" fill="none" marker-end="url(#f1-ray)">
          <line x1="110" y1="40" x2="110" y2="228"/>
          <line x1="140" y1="40" x2="140" y2="176"/>
          <line x1="170" y1="40" x2="170" y2="124"/>
        </g>

        <line x1="85" y1="285" x2="195" y2="95" class="ink" stroke-width="2.4"/>
        <!-- the normal: perpendicular to the surface, pointing up-left -->
        <line class="ax-y" x1="140" y1="190" x2="88" y2="160" stroke-width="2" marker-end="url(#f1-n)"/>
        <text x="70" y="154" class="xs lbl-y">n</text>
        <!-- the angle between the normal and the beam -->
        <path d="M140,145 A45,45 0 0 0 101,167" class="ink-soft" fill="none" stroke-width="1.3"/>
        <text x="118" y="158" class="xs muted">θ</text>

        <text x="206" y="206" class="sm mono">footprint 2.00</text>
        <text x="206" y="224" class="xs muted">= 1 / cos 60°</text>
        <text x="150" y="296" class="sm" text-anchor="middle" font-weight="700">power / area = 0.50</text>
      </g>

      <text x="350" y="344" class="sm" text-anchor="middle" font-weight="700">same beam, same power — twice the footprint, half the brightness</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 1.</span>
      Lambert's law, before any algebra. A beam of fixed cross-section striking a surface tilted
      by <code>θ</code> from the normal covers a footprint <code>1/cos θ</code> times as wide, so
      the power arriving per unit area falls by <code>cos θ</code>. At 60° the footprint is
      exactly twice as wide and the surface is exactly half as bright — measured in
      <code>verify_36.cpp</code> §A, which computes the footprint geometrically and checks it
      against the cosine rather than assuming they agree.
    </figcaption>
  </figure>

  <p>
    Notice what the angle is measured <em>from</em>. It is the angle between the incoming light
    and the surface's <strong>normal</strong> — not the surface itself. That is not a convention
    someone picked; it falls out of the picture. Head-on means the beam is parallel to the normal
    and the footprint is smallest; graze the surface and the normal is nearly perpendicular to
    the beam and the footprint runs away to infinity.
  </p>

  <p>
    And that is why a renderer needs normals at all. The normal is not decoration — it is the
    single quantity that determines how a surface meets light.
  </p>

  <h3 id="intuition-terminator">2.2 The terminator, and what lies beyond it</h3>

  <p>
    Follow the tilt further. At 90° the surface is edge-on to the light: the footprint is
    infinite, the brightness is zero, and the beam is skimming past. Tilt beyond that and the
    surface has turned <em>away</em> — it is now facing into its own shadow, and the light cannot
    reach it at all.
  </p>

  <p>
    The line where that happens has a name borrowed from astronomy: the <strong>terminator</strong>,
    the boundary between the lit and unlit sides of a body. On the moon it is the edge of the
    crescent. Figure 2 walks a normal around a curved surface and shows what the cosine does as
    it crosses.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 700 400" role="img" aria-labelledby="fig2-t fig2-d">
      <title id="fig2-t">The cosine around a curved surface, and the terminator where it reaches zero</title>
      <desc id="fig2-d">A circle representing a curved surface in cross-section, with parallel
        light arriving from the upper left and travelling down and to the right. Normals are
        drawn outward at five points around it, each labelled with the value of the dot product
        of that normal with the direction to the light: 1.00 where the surface faces the light
        squarely, 0.80, then 0.10 just before the terminator, then negative values of minus 0.59
        and minus 0.98 on the far side. A dashed line perpendicular to the light marks the
        terminator, and the half of the disc beyond it is shaded dark to show it is unlit. A note
        says the negative values must be clamped to zero, because a surface cannot receive a
        negative amount of light.</desc>

      <defs>
        <marker id="f2-n" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="5.5" markerHeight="5.5"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink-soft)"/>
        </marker>
        <marker id="f2-l" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/>
        </marker>
      </defs>

      <!-- Parallel light, travelling along (0.6, 0.8). Every number below is read
           off that direction rather than asserted beside it. -->
      <g class="hi" stroke-width="1.5" fill="none" marker-end="url(#f2-l)">
        <line x1="40" y1="20" x2="112" y2="116"/>
        <line x1="100" y1="10" x2="172" y2="106"/>
        <line x1="160" y1="4" x2="232" y2="100"/>
      </g>
      <text x="44" y="140" class="sm t-hi" font-weight="700">light</text>

      <circle cx="300" cy="180" r="90" class="fill-soft ink" stroke-width="2"/>
      <!-- The UNLIT half: everything beyond the terminator, i.e. the semicircle
           centred on the light's direction of travel. -->
      <path d="M372,126 A90,90 0 0 1 228,234 L300,180 Z" fill="var(--bg-sunken)" opacity="0.92"/>

      <!-- The terminator is PERPENDICULAR to the light, through the centre. -->
      <line x1="372" y1="126" x2="228" y2="234" class="ink-soft" stroke-width="1.6" stroke-dasharray="5 4"/>
      <text x="214" y="256" class="xs muted" text-anchor="end">terminator</text>

      <!-- Outward normals at five sample points. -->
      <g class="ink-soft" stroke-width="1.6" fill="none" marker-end="url(#f2-n)">
        <line x1="246" y1="108" x2="218" y2="71"/>
        <line x1="300" y1="90"  x2="300" y2="44"/>
        <line x1="366" y1="119" x2="400" y2="88"/>
        <line x1="389" y1="180" x2="434" y2="180"/>
        <line x1="366" y1="241" x2="400" y2="272"/>
      </g>

      <text x="196" y="78"  class="mono xs" text-anchor="end">1.00</text>
      <text x="300" y="34"  class="mono xs" text-anchor="middle">0.80</text>
      <text x="408" y="84"  class="mono xs">0.10</text>
      <text x="442" y="184" class="mono xs">−0.59</text>
      <text x="408" y="282" class="mono xs">−0.98</text>

      <text x="30" y="308" class="sm" font-weight="700">dot(n, l)</text>
      <text x="30" y="326" class="xs muted">n and l both unit,</text>
      <text x="30" y="340" class="xs muted">so this IS cos θ</text>

      <text x="380" y="352" class="sm" text-anchor="middle" font-weight="700">past the terminator the cosine is NEGATIVE — and must be clamped to 0</text>
      <text x="380" y="372" class="xs muted" text-anchor="middle">a surface cannot receive a negative amount of light</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 2.</span>
      The same law, walked around a curved surface. Where the normal points straight at the light
      the cosine is 1; it falls smoothly to 0 at the terminator, where the surface is edge-on. On
      the far side the arithmetic keeps going and produces <em>negative</em> numbers — which is
      the point at which the formula stops describing anything physical, and the reason
      <code>lambert()</code> clamps. §3.3 shows exactly what the unclamped version does to a
      picture.
    </figcaption>
  </figure>

  <p>
    Two things to take from Figure 2 before any code. First, the falloff is <em>smooth</em> — that
    gradient across a curved surface is the entire reason a lit sphere reads as a sphere, and the
    debug palette has none of it. Second, the negative region is real arithmetic that has to be
    dealt with deliberately, because a dot product does not know it has stopped being physical.
  </p>
"""
