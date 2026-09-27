# Lesson 3.6 body — part 2: the theory.
BODY2 = r"""
  <h2 id="theory"><span class="num">3</span>The Theory</h2>

  <h3 id="theory-lambert">3.1 Lambert's cosine law, derived</h3>

  <p>
    Figure 1 has already done the work; this is just writing it down. A beam of light with
    cross-sectional area <code>A</code> carries some power <code>P</code>. It strikes a surface
    whose normal makes an angle <code>θ</code> with the beam. The patch of surface it lands on
    has area <code>A / cos θ</code> — that is elementary trigonometry, the same
    adjacent-over-hypotenuse that gives a ramp its length. So the power arriving per unit area is
  </p>

  <div class="eq">
    <span class="katex-src">\[ E \;=\; \frac{P}{A/\cos\theta} \;=\; \frac{P}{A}\cos\theta \]</span>
    <span class="eq-plain">E = P / (A / cos(theta)) = (P/A) * cos(theta)</span>
  </div>

  <p>
    In prose: <em>the irradiance on a surface is the beam's own intensity times the cosine of the
    angle between the beam and the surface normal.</em> That is Lambert's cosine law, published by
    Johann Heinrich Lambert in 1760, and it is the entire physical content of today's lesson.
  </p>

  <div class="worked">
    <span class="label">Worked example — pushing three angles through by hand</span>
    <p>
      Take a beam of unit width and unit power, so <code>P/A = 1</code>.
    </p>
    <ul>
      <li><strong>θ = 0°</strong> — footprint <code>1 / cos 0° = 1.000</code>, so
          <code>E = 1.000</code>. Head-on: all of it, on the smallest possible patch.</li>
      <li><strong>θ = 30°</strong> — footprint <code>1 / 0.8660 = 1.1547</code>, so
          <code>E = 1/1.1547 = 0.8660</code>. And <code>cos 30° = 0.8660</code> ✓</li>
      <li><strong>θ = 60°</strong> — footprint <code>1 / 0.5 = 2.000</code>, so
          <code>E = 0.500</code>. Exactly half, over exactly twice the area ✓</li>
    </ul>
    <p>
      The harness computes the footprint geometrically and compares it against
      <code>cos θ</code> at ten angles rather than assuming the two agree — because "obviously the
      same thing" is how a factor of <code>cos</code> ends up in the wrong place.
    </p>
  </div>

  <h3 id="theory-dot">3.2 …and why the dot product is already the answer</h3>

  <p>
    We need <code>cos θ</code>, and we have two directions. Lesson 1.7 introduced the dot product
    geometrically — as the length of one vector's shadow on another — and derived
  </p>

  <div class="eq">
    <span class="katex-src">\[ \mathbf{a} \cdot \mathbf{b} = \|\mathbf{a}\|\,\|\mathbf{b}\|\cos\theta \]</span>
    <span class="eq-plain">dot(a, b) = length(a) * length(b) * cos(theta)</span>
  </div>

  <p>
    If both vectors are <strong>unit length</strong>, the two magnitudes are 1 and the dot product
    <em>is</em> the cosine. Nothing new is needed: the quantity Lambert's law asks for is a
    quantity we have been able to compute since Module 1.
  </p>

  <div class="eq">
    <span class="katex-src">\[ E \;=\; \max\!\left(0,\; \mathbf{n} \cdot \mathbf{l}\right) \]</span>
    <span class="eq-plain">E = max(0, dot(n, l)), with n and l both unit length</span>
  </div>

  <p>
    In prose: <em>the diffuse term is the dot product of the unit surface normal with the unit
    vector pointing toward the light, floored at zero.</em>
  </p>

  <div class="callout pitfall">
    <span class="label">Toward the light, not the way the light is going</span>
    <p>
      <code>l</code> is the vector <strong>from the surface to the light</strong>. A directional
      light is usually <em>authored</em> the other way round — as the direction its rays travel,
      because that is what a physical description of sunlight is. Midday sun travels
      <em>downward</em>, so its direction is <code>(0, −1, 0)</code> while the <code>l</code> in
      the formula is <code>(0, +1, 0)</code>.
    </p>
    <p>
      Get the sign wrong and the scene lights from precisely the opposite side. Everything is
      smoothly, plausibly shaded — nothing looks broken — and the light appears to be behind the
      camera when the sun is meant to be in front of it. This is the most common lighting bug
      there is. Our <code>directional_light</code> stores the travel direction and exposes
      <code>to_light()</code>, so the negation has a name and cannot be quietly skipped.
    </p>
  </div>

  <p>
    Both vectors must be normalised, and that is a real requirement rather than tidiness. If
    <code>n</code> has length 1.4, the dot product is 1.4 times the cosine and the surface is 40%
    too bright — a brightness bug with no visible cause, since the geometry is fine and the light
    is fine. §3.5 is about a matrix that changes a normal's length as a matter of course, so this
    is not hypothetical.
  </p>

  <h3 id="theory-clamp">3.3 The clamp is not a detail</h3>

  <p>
    A dot product happily returns negative numbers, and Figure 2 shows exactly where: everywhere
    past the terminator. Physically those surfaces receive <em>no</em> light. Arithmetically they
    receive a negative amount, and if you let that through it gets multiplied by the light colour
    and <strong>subtracted</strong> from the result.
  </p>

  <div class="worked">
    <span class="label">Worked example — what the missing clamp actually does</span>
    <p>
      Take a surface pointing directly away from the light: <code>dot(n, l) = −1</code>. With an
      albedo of 0.8 in the red channel, a white light of intensity 1, and our ambient term of
      0.06:
    </p>
    <p>
      <code>r = 0.8 × (1.0 × 1.0 × −1) + 0.06 = <strong>−0.740</strong></code>
    </p>
    <p>
      Measured, in <code>verify_36.cpp</code> §B. A negative amount of light. It clips to black
      when written to an 8-bit pixel, so on the far side of an object you see… black, which is
      what you wanted anyway. <strong>The bug hides completely on the unlit side.</strong>
    </p>
    <p>
      Where it shows is at the <em>edges</em>. Near the terminator the term passes through small
      negative values that the ambient term can no longer lift above zero, so a band of pixels
      that should be dimly ambient-lit goes to pure black instead — a hard dark rim eating into
      the lit region, with a visibly wrong, too-crisp boundary. Once you know the artifact you can
      name the cause on sight, which is the entire reason for showing it.
    </p>
  </div>

  <h3 id="theory-albedo">3.4 Albedo, and why every multiply is linear</h3>

  <p>
    Lambert gives how much light <em>arrives</em>. What leaves is that times the fraction the
    surface reflects, which is called its <strong>albedo</strong> — a per-channel number in
    <code>[0, 1]</code>. A surface with albedo 0.2 in red and 0.8 in green reflects a fifth of the
    red light and four fifths of the green, which is what "being green" physically is.
  </p>

  <div class="eq">
    <span class="katex-src">\[ L_{\text{out}} \;=\; \rho \cdot \big( I_{\text{light}}\max(0, \mathbf{n}\cdot\mathbf{l}) \;+\; I_{\text{ambient}} \big) \]</span>
    <span class="eq-plain">out = albedo * (light_intensity * max(0, dot(n,l)) + ambient)</span>
  </div>

  <p>
    In prose: <em>reflected light equals the albedo times the sum of the direct term and the
    ambient term.</em> Albedo is a <em>ratio</em>, not a colour you can see — the same surface
    emits nothing in the dark and a great deal under a bright light, and keeping that straight is
    most of what makes lighting code behave.
  </p>

  <div class="callout note">
    <span class="label">The ambient term is a fudge, and the code says so</span>
    <p>
      In a real room the side of an object facing away from the window is not black: light bounces
      off the walls and floor and arrives from everywhere. That is <em>global illumination</em>,
      it is expensive, and it is Module 6's subject. A constant added everywhere is the cheapest
      possible stand-in — it gets "not black" right and everything else wrong, since real bounced
      light varies with position and orientation.
    </p>
    <p>
      It earns its place today for one specific reason: with pure Lambert and no ambient, the
      unlit half of an object is <em>exactly</em> the background colour, and the silhouette
      disappears into it. That would make this lesson's pictures worse at showing the thing the
      lesson is about.
    </p>
  </div>

  <p>
    Now the part that Lesson 1.6 spent a whole lesson preparing. <strong>Every multiplication
    above is a statement about light, so every one of them must happen in linear space.</strong>
    An sRGB-encoded byte is not proportional to light — it is a perceptually-spaced code — so
    multiplying it by 0.5 does not halve the light, it produces something roughly 73% as bright.
    Do lighting in encoded values and every gradient is subtly wrong, every falloff too slow, and
    the terminator sits in the wrong place.
  </p>

  <p>
    So the shading function decodes once, does all its arithmetic in <code>linear_rgb</code>, and
    re-encodes once. That is exactly the discipline <code>face_shade</code> already followed for
    its fake ramp, which is why swapping it out changes the picture's <em>content</em> without
    changing its colour management.
  </p>

  <h3 id="theory-normal-matrix">3.5 The matrix that transforms normals</h3>

  <p>
    Here is where the lesson stops being about light and starts being about geometry — and where
    the obvious answer is wrong.
  </p>

  <p>
    Our normals arrive in <strong>model space</strong>, straight out of the file. The light is in
    <strong>world space</strong>. Something has to carry the normal from one to the other, and the
    obvious candidate is the model matrix <code>M</code>, since that is what carries the
    positions. Lesson 2.7 even taught the rule that makes it look settled: positions are points
    (<code>w = 1</code>, so they translate), normals are directions (<code>w = 0</code>, so they
    do not). Use <code>M</code> and drop the translation. Done.
  </p>

  <p>
    That is correct for a <em>tangent</em> — a vector lying <em>along</em> the surface — and it is
    wrong for a normal. Figure 3 shows why, with a squash.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 700 340" role="img" aria-labelledby="fig3-t fig3-d">
      <title id="fig3-t">A non-uniform scale tilts a normal the wrong way</title>
      <desc id="fig3-d">Two panels. On the left, a surface drawn as a diagonal line with a tangent
        arrow pointing up and to the left along it and a normal arrow pointing up and to the
        right, exactly ninety degrees apart. On the right, the same surface after a non-uniform
        scale has flattened it: the tangent has flattened with the surface as it should, and the
        normal transformed by the same matrix has also flattened, so the angle between them has
        opened to 137.5 degrees instead of 90. A third arrow shows the correctly transformed
        normal, which has instead tilted the other way to stand up more steeply, and is at
        exactly 90 degrees to the tangent.</desc>

      <defs>
        <marker id="f3-t" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-x)"/></marker>
        <marker id="f3-n" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-y)"/></marker>
        <marker id="f3-w" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
                orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--warn-bd)"/></marker>
      </defs>

      <!-- LEFT: before. Tangent up-left, normal up-right, ninety degrees apart. -->
      <g transform="translate(40,30)">
        <text x="130" y="14" class="sm" text-anchor="middle" font-weight="700">before</text>
        <line x1="63" y1="63" x2="197" y2="197" class="ink" stroke-width="2.4"/>
        <line class="ax-x" x1="130" y1="130" x2="66" y2="66" stroke-width="2.2" marker-end="url(#f3-t)"/>
        <line class="ax-y" x1="130" y1="130" x2="194" y2="66" stroke-width="2.2" marker-end="url(#f3-n)"/>
        <text x="52" y="58" class="xs lbl-x" text-anchor="end">t</text>
        <text x="202" y="60" class="xs lbl-y">n</text>
        <path d="M106,106 A34,34 0 0 1 154,106" class="ink-soft" fill="none" stroke-width="1.3"/>
        <text x="130" y="86" class="xs muted" text-anchor="middle">90°</text>
        <text x="130" y="250" class="xs muted" text-anchor="middle">n ⟂ t, as a normal must be</text>
      </g>

      <line x1="352" y1="46" x2="352" y2="290" class="ink-soft" stroke-width="1" stroke-dasharray="4 4"/>

      <!-- RIGHT: after scaling by the slab's (1.8, 0.35, 0.9). Every endpoint below
           is the transformed vector, normalised and drawn at a fixed length — so the
           angles in the picture ARE the angles in the worked example. -->
      <g transform="translate(390,30)">
        <text x="140" y="14" class="sm" text-anchor="middle" font-weight="700">after scaling y by 0.35, z by 0.9</text>
        <line x1="0" y1="96" x2="280" y2="204" class="ink" stroke-width="2.4"/>
        <line class="ax-x" x1="140" y1="150" x2="51" y2="116" stroke-width="2.2" marker-end="url(#f3-t)"/>
        <line x1="140" y1="150" x2="229" y2="116" stroke="var(--warn-bd)" stroke-width="2.4"
              marker-end="url(#f3-w)"/>
        <line class="ax-y" x1="140" y1="150" x2="174" y2="62" stroke-width="2.2" marker-end="url(#f3-n)"/>

        <text x="40" y="102" class="xs lbl-x" text-anchor="end">M·t</text>
        <text x="214" y="140" class="xs t-bad">M·n</text>
        <text x="214" y="158" class="xs t-bad">137.5° — wrong</text>
        <text x="184" y="54" class="xs t-ok">X·n</text>
        <text x="184" y="72" class="xs t-ok">90.0° — right</text>

        <text x="140" y="250" class="xs muted" text-anchor="middle">the surface got flatter, so its normal must stand UP more</text>
      </g>

      <text x="350" y="322" class="sm" text-anchor="middle" font-weight="700">squash the surface and the normal must tilt the OTHER WAY</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 3.</span>
      Why <code>M</code> is the wrong matrix for a normal. Squashing vertically flattens the
      surface, and the tangent flattens with it — correctly, because a tangent <em>is</em> a piece
      of the surface. The normal must do the opposite: a flatter surface has a normal that stands
      up more steeply. Applying <code>M</code> to it flattens it too, leaving it at
      <strong>137.5°</strong> to the tangent instead of 90°. Both angles measured in
      <code>verify_36.cpp</code> §C, on the exact scale our demo's slab uses.
    </figcaption>
  </figure>

  <p>
    So what <em>is</em> the right matrix? Rather than guess, go back to the only thing that
    defines a normal in the first place. A normal is not an arrow someone drew on the surface — it
    is defined by a <strong>relationship</strong>: it is perpendicular to every tangent. Transform
    the surface, and it is that relationship which has to survive.
  </p>

  <p>
    Write it down. Tangents go to <code>M·t</code> — we know that much, because a tangent joins
    two nearby points on the surface and points are what <code>M</code> transforms correctly.
    Suppose normals go to <code>X·n</code> for some matrix <code>X</code> we are looking for. The
    requirement is that perpendicularity is preserved:
  </p>

  <div class="eq">
    <span class="katex-src">\[ (M\mathbf{t}) \cdot (X\mathbf{n}) = 0 \qquad\text{whenever}\qquad \mathbf{t}\cdot\mathbf{n} = 0 \]</span>
    <span class="eq-plain">dot(M*t, X*n) = 0 whenever dot(t, n) = 0</span>
  </div>

  <p>
    Now use the fact that a dot product is a matrix product in disguise:
    <code>a · b = aᵀb</code>. The left-hand side becomes
  </p>

  <div class="eq">
    <span class="katex-src">\[ (M\mathbf{t})^{\mathsf{T}} (X\mathbf{n}) \;=\; \mathbf{t}^{\mathsf{T}} M^{\mathsf{T}} X \,\mathbf{n} \]</span>
    <span class="eq-plain">transpose(M*t) * (X*n) = transpose(t) * transpose(M) * X * n</span>
  </div>

  <p>
    We need that to be zero exactly when <code>tᵀn</code> is zero — that is, we need the middle to
    do nothing at all. Setting <code>Mᵀ X = I</code> gives it, and solving for <code>X</code>:
  </p>

  <div class="eq">
    <span class="katex-src">\[ X \;=\; \left(M^{\mathsf{T}}\right)^{-1} \;=\; \left(M^{-1}\right)^{\mathsf{T}} \]</span>
    <span class="eq-plain">X = inverse(transpose(M)) = transpose(inverse(M))</span>
  </div>

  <p>
    In prose: <em>normals are transformed by the inverse transpose of the model matrix's linear
    part.</em> The two orderings are the same matrix — inverting and transposing commute — so the
    name "inverse transpose" is unambiguous. That is the whole derivation, and it took three lines
    because we started from what a normal <em>is</em> rather than from what looked plausible.
  </p>

  <div class="worked">
    <span class="label">Worked example — the slab's scale, by hand</span>
    <p>
      Our demo's slab is scaled by <code>(1.8, 0.35, 0.9)</code>. Take a surface at 45° in the
      <code>yz</code> plane, so its normal is <code>n = (0, 0.7071, 0.7071)</code> and a tangent
      along it is <code>t = (0, 0.7071, −0.7071)</code>. Check they start perpendicular:
      <code>0.7071×0.7071 + 0.7071×(−0.7071) = 0</code> ✓
    </p>
    <p><strong>The tangent, transformed by M</strong> (correct — a tangent is a piece of the surface):</p>
    <p><code>M·t = (0, 0.35×0.7071, 0.9×(−0.7071)) = (0, 0.2475, −0.6364)</code></p>
    <p><strong>The normal, transformed by M</strong> (the mistake):</p>
    <p><code>M·n = (0, 0.2475, +0.6364)</code></p>
    <p>
      Angle between them: <code>cos φ = (0.2475×0.2475 − 0.6364×0.6364) / (0.6828 × 0.6828)</code>
      <code>= (0.06126 − 0.40500) / 0.46621 = −0.7373</code>, so
      <code>φ = <strong>137.50°</strong></code>. Not 90°. The "normal" is now leaning
      <em>into</em> the surface by 47.5°.
    </p>
    <p>
      <strong>The normal, transformed by the inverse transpose.</strong> For a diagonal matrix the
      inverse is the reciprocal of each entry and the transpose changes nothing, so
      <code>X = diag(1/1.8, 1/0.35, 1/0.9) = diag(0.5556, 2.857, 1.111)</code>:
    </p>
    <p><code>X·n = (0, 2.857×0.7071, 1.111×0.7071) = (0, 2.0203, 0.7857)</code></p>
    <p>
      Angle to the tangent: <code>(0.2475×2.0203 + (−0.6364)×0.7857) = 0.50003 − 0.50002 ≈ 0</code>,
      so <code>φ = <strong>90.00°</strong></code> ✓ — and note it is nowhere near unit length any
      more, which is why the shading function normalises.
    </p>
    <p>
      Swept over 20,000 random (normal, tangent) pairs under this scale: the inverse transpose
      never deviates from 90° by more than <strong>0.00003°</strong>, while the naive transform
      reaches <strong>67.96°</strong> off.
    </p>
  </div>

  <h3 id="theory-hides">3.6 When it does not matter — which is why it ships</h3>

  <p>
    A fair question at this point: if this is so fundamental, how does anyone ever get away
    without it? The answer is that for most transforms the two matrices agree, and it is worth
    knowing exactly which.
  </p>

  <p>
    <strong>A pure rotation.</strong> A rotation matrix is orthonormal, which means its inverse
    <em>is</em> its transpose. So <code>X = (R⁻¹)ᵀ = (Rᵀ)ᵀ = R</code> — the normal matrix is the
    model matrix, exactly. Measured: <code>max |R − normal_matrix(R)| = 5.96e−08</code>, which is
    float noise.
  </p>

  <p>
    <strong>A uniform scale.</strong> For <code>M = sR</code>, the inverse transpose works out to
    <code>(1/s)R</code>. That is a different matrix — but it points every normal in exactly the
    same <em>direction</em> as <code>R</code> does, differing only by a positive scale factor that
    normalisation removes. Measured: <code>0.0000°</code> of difference.
  </p>

  <div class="callout warn">
    <span class="label">So the bug is invisible until something is squashed</span>
    <p>
      Rotation and uniform scale cover an enormous fraction of everything in a typical scene.
      Our own demo is a fair sample: of its three objects, the icosahedron is uniformly scaled and
      shows a worst-case normal tilt of <strong>0.03°</strong> (noise), while the slab
      <code>(1.8, 0.35, 0.9)</code> reaches <strong>67.99°</strong> and the plinth
      <code>(1.2, 0.25, 1.2)</code> reaches <strong>66.46°</strong>.
    </p>
    <p>
      That is the shape of the bug: <strong>the hero object looks perfect and the set dressing is
      wrong.</strong> Nobody reports it, because the thing you are looking at is fine.
    </p>
  </div>

  <p>
    Rendered, the difference is not subtle. Squash the torus into a flattened ring and light it
    both ways, and <strong>97.5% of the object's covered pixels differ</strong>, by up to 135 of
    255 in a channel. The naive version does not look like noise — it looks like a
    <em>correctly lit round tube</em>, with a crisp bright rim and a hard dark band. It is lighting
    the shape the object had <em>before</em> the squash, which is precisely what failing to
    transform the normals means.
  </p>

  <div class="widget">
    <p style="margin-top:0"><strong>Try it.</strong> Squash the surface and watch the two normals
      separate. The tangent always does the right thing; the question is only ever what happens to
      the normal.</p>

    <svg id="w36-svg" viewBox="0 0 560 240" style="width:100%;height:auto" role="img"
         aria-label="Interactive diagram: a surface with a tangent, a naively transformed normal and a correctly transformed normal, responding to a squash slider.">
      <line id="w36-surface" x1="60" y1="200" x2="500" y2="40" stroke="var(--dia-ink)" stroke-width="2.4"/>
      <line id="w36-tan" x1="280" y1="120" x2="380" y2="84" stroke="var(--axis-x)" stroke-width="2.4"/>
      <line id="w36-naive" x1="280" y1="120" x2="316" y2="220" stroke="var(--warn-bd)" stroke-width="2.4"/>
      <line id="w36-good" x1="280" y1="120" x2="316" y2="220" stroke="var(--axis-y)" stroke-width="2.4"/>
      <circle cx="280" cy="120" r="4.5" fill="var(--dia-hi)"/>
      <text x="392" y="80" class="xs lbl-x">M·t  (tangent — always right)</text>
      <text id="w36-lbl-naive" x="0" y="0" class="xs t-bad">M·n</text>
      <text id="w36-lbl-good" x="0" y="0" class="xs t-ok">X·n</text>
    </svg>

    <div class="widget-controls">
      <label>scale y by
        <input id="w36-squash" type="range" min="10" max="200" value="35" step="1">
        <span class="widget-readout" id="w36-squash-v">0.35</span>
      </label>
      <span>naive angle to tangent <span class="widget-readout" id="w36-naive-v">—</span></span>
      <span>correct <span class="widget-readout" id="w36-good-v">—</span></span>
      <span>normals differ by <span class="widget-readout" id="w36-diff-v">—</span></span>
    </div>
  </div>

  <script>
  (function () {
    var svg = document.getElementById('w36-svg');
    if (!svg) { return; }
    var OX = 280, OY = 120, LEN = 105;

    function set(id, x1, y1, x2, y2) {
      var e = document.getElementById(id);
      e.setAttribute('x1', x1); e.setAttribute('y1', y1);
      e.setAttribute('x2', x2); e.setAttribute('y2', y2);
    }
    function label(id, x, y) {
      var e = document.getElementById(id);
      e.setAttribute('x', x); e.setAttribute('y', y);
    }
    function angle(ax, ay, bx, by) {
      var la = Math.hypot(ax, ay), lb = Math.hypot(bx, by);
      var c = (ax * bx + ay * by) / (la * lb);
      c = Math.max(-1, Math.min(1, c));
      return Math.acos(c) * 180 / Math.PI;
    }

    function run() {
      var sy = parseInt(document.getElementById('w36-squash').value, 10) / 100;
      document.getElementById('w36-squash-v').textContent = sy.toFixed(2);

      // Maths space: the horizontal axis is z, the vertical is y. The surface is at
      // 45 degrees, so tangent and normal straddle the vertical: t up-left, n
      // up-right, ninety degrees apart. These are the SAME two vectors as the
      // worked example in the prose above.
      var tz = -0.70710678, ty = 0.70710678;
      var nz =  0.70710678, ny = 0.70710678;

      // The slab's scale. z is fixed at 0.9 and y is the slider, so dragging to 0.35
      // reproduces the worked example exactly.
      var sz = 0.9;
      var Mtx = tz * sz, Mty = ty * sy;      // tangent by M — always correct
      var Mnx = nz * sz, Mny = ny * sy;      // normal by M — the mistake
      var Xnx = nz / sz, Xny = ny / sy;      // normal by the inverse transpose

      // Screen space: +y DOWN, so every y is negated when drawing.
      function draw(id, vx, vy, len) {
        var l = Math.hypot(vx, vy) || 1;
        set(id, OX, OY, OX + vx / l * len, OY - vy / l * len);
        return [OX + vx / l * (len + 22), OY - vy / l * (len + 22)];
      }

      // the surface itself, through the origin along the transformed tangent
      var sl = Math.hypot(Mtx, Mty) || 1;
      set('w36-surface', OX - Mtx / sl * 220, OY + Mty / sl * 220,
                         OX + Mtx / sl * 220, OY - Mty / sl * 220);

      draw('w36-tan', Mtx, Mty, LEN);
      var pn = draw('w36-naive', Mnx, Mny, LEN);
      var pg = draw('w36-good', Xnx, Xny, LEN);
      label('w36-lbl-naive', pn[0] - 12, pn[1] + 4);
      label('w36-lbl-good', pg[0] - 12, pg[1] + 4);

      var an = angle(Mtx, Mty, Mnx, Mny);
      var ag = angle(Mtx, Mty, Xnx, Xny);
      document.getElementById('w36-naive-v').textContent = an.toFixed(1) + '°';
      document.getElementById('w36-good-v').textContent = ag.toFixed(1) + '°';
      document.getElementById('w36-diff-v').textContent =
        angle(Mnx, Mny, Xnx, Xny).toFixed(1) + '°';
    }
    document.getElementById('w36-squash').addEventListener('input', run);
    run();
  })();
  </script>

  <p>
    The horizontal axis is fixed at the slab's <code>z</code> scale of 0.9 and the slider is its
    <code>y</code>, so dragging to <strong>0.35</strong> reproduces the worked example exactly:
    137.5° and 90.0°. Drag instead to <strong>0.90</strong> — making the scale uniform — and the
    two normals coincide, the "differ by" readout falls to 0.0°, and the whole problem vanishes.
    That is §3.6's point in one gesture, and it is why this is a bug you can carry for years.
  </p>

  <h3 id="theory-where">3.7 Where the normal comes from: per-face or per-vertex</h3>

  <p>
    One question remains before code: <em>which</em> normal does a given pixel use?
  </p>

  <p>
    There are two sources available, and they are genuinely different data. A
    <strong>face normal</strong> is computed from the triangle itself —
    <code>cross(b − a, c − a)</code>, which our winding convention makes point outward. It is
    exact for the triangle and knows nothing about its neighbours. A <strong>vertex normal</strong>
    is authored: it comes from the file, and for a curved surface it describes the direction of
    the <em>underlying smooth surface</em> at that point, not of any particular triangle.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 700 300" role="img" aria-labelledby="fig4-t fig4-d">
      <title id="fig4-t">Face normals versus vertex normals on a faceted approximation of a curve</title>
      <desc id="fig4-d">Two panels showing the same polyline approximating a curve. On the left,
        one normal is drawn per segment, each perpendicular to its own flat segment, and the
        brightness strip underneath is a staircase of constant values. On the right, one normal
        is drawn per corner, each following the underlying smooth curve, and the brightness strip
        underneath is a smooth ramp.</desc>

      <!-- LEFT: face normals -->
      <g transform="translate(30,26)">
        <text x="150" y="8" class="sm" text-anchor="middle" font-weight="700">one normal per FACE</text>
        <polyline points="20,150 90,116 160,102 230,116 300,150" class="ink" fill="none" stroke-width="2.4"/>
        <g class="ink-soft" stroke-width="1.8" fill="none">
          <line x1="55" y1="133" x2="47" y2="97"/>
          <line x1="125" y1="109" x2="122" y2="72"/>
          <line x1="195" y1="109" x2="198" y2="72"/>
          <line x1="265" y1="133" x2="273" y2="97"/>
        </g>
        <text x="150" y="186" class="xs muted" text-anchor="middle">each perpendicular to its own flat segment</text>
        <!-- brightness staircase -->
        <g>
          <rect x="20" y="204" width="70" height="22" fill="#8a6a2a"/>
          <rect x="90" y="204" width="70" height="22" fill="#c99a3c"/>
          <rect x="160" y="204" width="70" height="22" fill="#c99a3c"/>
          <rect x="230" y="204" width="70" height="22" fill="#8a6a2a"/>
          <rect x="20" y="204" width="280" height="22" class="ink" fill="none" stroke-width="1.2"/>
        </g>
        <text x="150" y="248" class="xs muted" text-anchor="middle">brightness: a staircase — you can see every facet</text>
      </g>

      <line x1="352" y1="40" x2="352" y2="270" class="ink-soft" stroke-width="1" stroke-dasharray="4 4"/>

      <!-- RIGHT: vertex normals -->
      <g transform="translate(380,26)">
        <text x="150" y="8" class="sm" text-anchor="middle" font-weight="700">one normal per VERTEX</text>
        <polyline points="20,150 90,116 160,102 230,116 300,150" class="ink" fill="none" stroke-width="2.4"/>
        <path d="M20,150 Q160,80 300,150" class="ink-soft" fill="none" stroke-width="1.2" stroke-dasharray="4 3"/>
        <g class="ax-y" stroke-width="1.8" fill="none">
          <line x1="20" y1="150" x2="6" y2="116"/>
          <line x1="90" y1="116" x2="82" y2="80"/>
          <line x1="160" y1="102" x2="160" y2="64"/>
          <line x1="230" y1="116" x2="238" y2="80"/>
          <line x1="300" y1="150" x2="314" y2="116"/>
        </g>
        <text x="150" y="186" class="xs muted" text-anchor="middle">each following the smooth curve the facets approximate</text>
        <!-- smooth ramp -->
        <defs>
          <linearGradient id="w36grad" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stop-color="#7d5f26"/>
            <stop offset="0.5" stop-color="#e0a83c"/>
            <stop offset="1" stop-color="#7d5f26"/>
          </linearGradient>
        </defs>
        <rect x="20" y="204" width="280" height="22" fill="url(#w36grad)"/>
        <rect x="20" y="204" width="280" height="22" class="ink" fill="none" stroke-width="1.2"/>
        <text x="150" y="248" class="xs muted" text-anchor="middle">brightness: a ramp — the facets disappear</text>
      </g>

      <text x="350" y="292" class="sm" text-anchor="middle" font-weight="700">same geometry, same light — the difference is entirely in which normal you ask</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 4.</span>
      The same faceted polyline, lit two ways. Face normals describe the triangles you actually
      have, so brightness is constant across each and the silhouette gives away the tessellation.
      Vertex normals describe the smooth surface the triangles are approximating, so interpolating
      between them produces a ramp and the facets vanish — a lie, but a very useful one, and the
      reason a 2,304-triangle torus can look round.
    </figcaption>
  </figure>

  <div class="callout note">
    <span class="label">This is not yet "flat vs Gouraud vs per-pixel"</span>
    <p>
      Figure 4 is about <em>where the normal comes from</em>. There is a second, independent
      question — <em>where the lighting equation is evaluated</em>: once per face, once per vertex
      with the resulting colour interpolated, or once per pixel with the normal interpolated.
      Those are different axes, they interact, and comparing them properly is Lesson 3.8.
    </p>
    <p>
      Today we evaluate <strong>per vertex</strong> and let Lesson 2.4's existing attribute
      interpolation carry the colour across the triangle — which costs no new rasterizer code at
      all, and gives both of Figure 4's pictures depending only on which normal we hand it.
    </p>
  </div>

  <p>
    And now a payoff from Lesson 3.5 that is worth pausing on. Load <code>cube.obj</code>, shade it
    with face normals, shade it with vertex normals, and compare the two renders:
    <strong>0 pixels differ.</strong> They are bit-identical.
  </p>

  <p>
    The reason is the index problem. A cube's 8 positions became 24 vertices precisely
    <em>because</em> the three faces meeting at each corner disagreed about the normal — so each
    of those 24 vertices carries its own face's normal, and the "per-vertex" normal at a corner
    <em>is</em> the face normal. The same experiment on <code>torus.obj</code>, whose positions
    carry genuinely smooth normals, differs on <strong>5,576 pixels</strong>.
  </p>

  <div class="callout ok">
    <span class="label">The sentence this module has been building toward</span>
    <p>
      <strong>A faceted mesh is faceted because of the vertex split, not because of the shading
      model.</strong> Whether a surface looks smooth or angular was decided by the exporter, in
      the file, before your renderer ever saw it — and Lesson 3.5's de-duplicating map is where
      that decision becomes geometry.
    </p>
  </div>
"""
