# Lesson 3.6 body — part 3: implementation, listings, build, pitfalls, exercises, recap.
BODY3 = r"""
  <h2 id="implementation"><span class="num">4</span>Implementation</h2>

  <p>
    One new header, two new functions in the maths library, and a change to how the demo assigns
    vertex colours. The rasterizer is not touched at all, and §4.3 explains why that is the
    interesting part rather than a coincidence.
  </p>

  <h3 id="impl-light">4.1 A light, and a shading function</h3>

  <!--EXC:directional_light-->

  <p>
    The comment shouts about the sign because that bug is worth one paragraph of noise forever.
    Everything else in the struct is a consequence of §3.4: the colour is
    <code>linear_rgb</code> because it is about to be multiplied, and <code>intensity</code> is
    separate from the colour so hue and brightness can be adjusted independently.
  </p>

  <!--EXC:lambert-->

  <p>
    Three lines, and every one of them was derived. The <code>max</code> is §3.3's clamp; the
    "both must be unit" precondition is §3.2's; and the fact that this <em>is</em> the cosine is
    Lesson 1.7's dot product, unchanged.
  </p>

  <!--EXC:shade-->

  <p>
    The normalisation inside <code>shade</code> is defensive on purpose. After §3.5's inverse
    transpose a normal is emphatically <em>not</em> unit length — the worked example produced one
    of length 2.16 — and after interpolation across a triangle it will not be either, since the
    average of two unit vectors is shorter than either. A caller who forgets gets a brightness
    scaled by the normal's length: a lighting bug with no lighting cause.
  </p>

  <div class="callout cpp">
    <span class="label">C++ in place — designated initialisers and default member initialisers</span>
    <p>
      Every member of <code>directional_light</code> and <code>lighting</code> has a default in
      the declaration, so <code>engine::lighting lights;</code> is immediately a valid, sensible
      light rather than a struct full of garbage. Combined with C++20's designated initialisers
      (<code>{.intensity = 2.0f}</code>) you get named, order-independent, partial construction
      with no constructor written by hand.
    </p>
    <p>
      The rule to keep: <strong>a default member initialiser should be the value that is correct
      when you have not thought about it.</strong> Lesson 3.4 broke that rule once, deliberately,
      for <code>fill_style::cull</code> — because no cull mode is universally correct, so the
      default is the <em>safe</em> one instead. Knowing which of the two you are choosing is the
      whole point.
    </p>
  </div>

  <h3 id="impl-matrix">4.2 <code>normal_matrix</code>, and where it lives</h3>

  <p>
    §3.5's derivation, as two functions in <code>mat4.hpp</code>:
  </p>

  <!--EXC:normal_matrix-->

  <p>
    It goes in the maths library rather than in the renderer because it is a statement about
    matrices, not about lighting — normal mapping (Module 6) and physics (Module 7) will both want
    it. And it needs no new machinery: <code>mat3</code> has had <code>inverse</code> and
    <code>transpose</code> since Lesson 2.5, where the general 3×3 inverse was built via cofactors
    and the adjugate.
  </p>

  <div class="callout note">
    <span class="label">Why the 3×3 inverse is enough, when <code>mat4</code> has none</span>
    <p>
      <code>mat4.hpp</code> says explicitly that it carries no general 4×4 inverse, on the grounds
      that nothing has needed one and the view matrix's inverse can be written down directly from
      its structure. That still holds here: a normal has no position, so the translation column is
      irrelevant, and <code>linear_of</code> drops it before we invert anything. The lesson that
      generalises: <strong>when you think you need a general inverse, check whether the problem
      actually lives in a subspace where a cheaper one exists.</strong> It usually does.
    </p>
  </div>

  <h3 id="impl-vertex">4.3 Shading per vertex, in world space</h3>

  <p>
    Here is the change in <code>collect_triangles</code>, and the first thing to notice is where
    it is <em>not</em>: the rasterizer is untouched.
  </p>

  <!--EXC:per_vertex-->

  <p>
    Two decisions worth defending.
  </p>

  <p>
    <strong>Once per vertex, not once per triangle corner.</strong> The icosahedron's 12 vertices
    are used by 20 triangles, so shading per vertex is 12 evaluations instead of 60 — the identical
    argument indexed geometry made in Lesson 2.12, now applied to lighting. This is the entire
    performance case for vertex lighting, and it is why an era of hardware did it this way.
  </p>

  <p>
    <strong>In world space, not view space.</strong> A light's direction is authored in world
    space, so shading there means it does not have to be re-derived every time the camera moves.
    It also makes a testable claim available: orbit the camera and the shading must <em>not</em>
    change, because Lambert does not depend on where you are standing. Measured in
    <code>verify_36.cpp</code> §F — the brightest lit pixel is the same value from two different
    camera angles, while swinging the light changes 6,104 pixels.
  </p>

  <div class="callout ok">
    <span class="label">The rasterizer did not change, and that is the point</span>
    <p>
      <code>fill_style</code> gained no field. <code>fill_triangle</code> gained no branch. The
      lighting result is a vertex colour, and interpolating vertex colours across a triangle is
      something the rasterizer has done since Lesson 2.4.
    </p>
    <p>
      That is not luck — it is the vertex/fragment split, arriving on its own. Lighting is a
      <em>vertex-stage</em> computation whose output feeds the fragment stage, and the reason it
      fits so neatly is that our pipeline already had that boundary without naming it. Module 4
      makes it literal by giving each stage its own shader; the thing to notice today is that
      <code>fill_style</code> is starting to look like a place where lighting <em>cannot</em> go,
      which is what will eventually make a material system necessary rather than tidy.
    </p>
  </div>

  <h3 id="impl-fallback">4.4 Meshes with no normals</h3>

  <p>
    Three of our meshes carry normals; the rest do not. <code>cube_mesh()</code>,
    <code>quad_mesh()</code>, <code>icosahedron_mesh()</code> and Lesson 3.2's generated floor were
    all authored before normals existed, and <code>quirks.obj</code> has faces with no
    <code>vn</code> at all. <code>mesh::normal_at</code> returns the zero vector for all of them.
  </p>

  <p>
    The fallback is the face normal, computed from the triangle's own edges:
  </p>

  <!--EXC:face_normal-->

  <p>
    Zero is a usable sentinel here for the reason <code>normal_at</code> documented back in Lesson
    3.5: a zero vector has no direction, so it can never be mistaken for a real normal, and it
    survives a matrix multiply as zero. No second flag has to travel alongside the data.
  </p>

  <p>
    Note that the face normal goes through <strong>the same normal matrix</strong> as everything
    else. A face normal is a normal; it obeys §3.5's rule exactly like an authored one, and
    transforming it with <code>M</code> instead would reintroduce the whole bug on precisely the
    meshes that have no authored normals to check against.
  </p>

  <div class="callout note">
    <span class="label">What we are <em>not</em> doing: generating smooth normals</span>
    <p>
      The fallback gives a mesh with no normals flat shading, which is correct and honest but
      means such a mesh can never look smooth. Generating <em>smooth</em> normals means averaging
      the face normals around each vertex, weighted by area — which needs to know which faces
      touch which vertex, i.e. the adjacency that <code>validate()</code> already builds in Lesson
      3.5. Reusing that rather than rebuilding it is Exercise 3.6.4.
    </p>
  </div>

  <h3 id="impl-demo">4.5 The three modes, and two comparisons</h3>

  <!--EXC:shade_mode-->

  <p>
    The debug palette stays, behind <kbd>G</kbd>. That is the eighth time this engine has kept a
    wrong thing on a key — after <code>draw_line_naive</code> (2.1), Pong's unswept collision
    (1.8), the encoded blend space (2.4), the <code>w</code> toggles (2.7), the composition orders
    (2.8), affine interpolation (3.2), the near-plane modes (3.3) and the forward-axis facing test
    (3.4) — and the first time the wrong thing was ever the <em>default</em>. Flipping between the
    palette and Lambert while the object spins is the fastest way to see what "the shading does not
    respond to the geometry" means.
  </p>

  <p>
    And the normal matrix gets the same treatment, with a number attached:
  </p>

  <!--EXC:normal_toggle-->

  <p>
    The HUD reports two things: the worst-case angle by which the naive transform would tilt a
    normal in this scene, and the pixel count between the two renders. The second is guarded on
    the first being non-zero — with no non-uniform scale present the two matrices agree to the
    last bit, so there is nothing to render twice. <strong>That guard is the lesson</strong>: it
    is a machine-checkable statement of exactly when the bug can bite.
  </p>

  <h2 id="listings"><span class="num">5</span>Complete Code Listings</h2>

  <p>Every file this lesson touched, in full. Nothing elided.</p>

  <figure class="tbl">
    <div class="tbl-scroll">
      <table class="manifest">
        <thead><tr><th>Path</th><th>Status</th><th>Purpose</th></tr></thead>
        <tbody>
          <tr><td>src/gfx/light.hpp</td><td><span class="badge new">new</span></td>
              <td>The directional light, Lambert's law, and the shading function.</td></tr>
          <tr><td>src/math/mat4.hpp</td><td><span class="badge mod">modified</span></td>
              <td><code>linear_of</code> and <code>normal_matrix</code> — the inverse transpose, derived in the header.</td></tr>
          <tr><td>src/main.cpp</td><td><span class="badge mod">modified</span></td>
              <td>Per-vertex shading in world space, three shading modes on <kbd>G</kbd>, the normal-matrix toggle on <kbd>J</kbd>, a light on <kbd>A</kbd>/<kbd>D</kbd>.</td></tr>
        </tbody>
      </table>
    </div>
    <figcaption><span class="fignum">Manifest.</span> Files touched in this lesson. <code>CMakeLists.txt</code> is unchanged — <code>light.hpp</code> is header-only.</figcaption>
  </figure>

  <!--LISTINGS-->

  <h2 id="build"><span class="num">6</span>Build &amp; Run</h2>

  <figure class="listing shell">
    <figcaption><span class="path">all platforms</span><span class="lang" data-lang="bash">shell</span></figcaption>
    <pre><code class="lang-bash">cmake -S . -B build
cmake --build build

# macOS / Linux
./build/engine
# Windows
.\build\Debug\engine.exe</code></pre>
  </figure>

  <h3>What you should see</h3>

  <p>
    The scene now opens <strong>lit</strong>. Press <kbd>L</kbd> to jump to the torus, and the
    first thing to do is press <kbd>G</kbd> twice to walk the three modes in order:
  </p>

  <ul>
    <li><strong>debug palette (3.1)</strong> — a checkerboard of five brightnesses scattered over
      a shape. You can find the silhouette and nothing else.</li>
    <li><strong>LAMBERT, flat</strong> — the torus becomes an <em>object</em>. It has a lit side, a
      dark side and a terminator between them, and you can see which parts bulge toward the light.
      Every one of its 2,304 facets is individually visible, because each has one constant
      brightness.</li>
    <li><strong>LAMBERT, per-vertex</strong> — the facets vanish and the surface becomes smooth.
      Same geometry, same light; the only change is that each vertex now uses the normal the file
      authored rather than the one its triangle implies.</li>
  </ul>

  <p>
    Then hold <kbd>A</kbd> or <kbd>D</kbd>. The light swings, the terminator sweeps across the
    surface, and the shading follows it. Now use the arrow keys to orbit the camera instead:
    <strong>the shading does not change at all.</strong> That is not a bug — Lambert is
    view-independent, and 3.7's specular term will be the first thing in this engine that
    responds to where you are standing.
  </p>

  <p>
    Press <kbd>C</kbd> to return to the solids scene, and <kbd>J</kbd> to break the normal matrix:
  </p>

  <div class="callout ok">
    <span class="label">Checkpoint</span>
    <p>
      On the solids scene with per-vertex Lambert, the HUD's bottom line reads
      <code>[G] LAMBERT, per-vertex [J] inv-transpose  tilt 68.0 deg  dif 0 px</code>. Press
      <kbd>J</kbd>: the label turns red and reads <code>NAIVE M</code>, and
      <code>dif</code> jumps to a few thousand. Watch <em>which</em> objects change — the
      icosahedron is uniformly scaled and is pixel-identical either way; the slab and the plinth
      are not, and their shading visibly detaches from their shape.
    </p>
    <p>
      That is the bug in miniature. The hero object looks perfect in both, which is exactly why
      nobody catches it.
    </p>
  </div>

  <h2 id="pitfalls"><span class="num">7</span>Common Pitfalls &amp; Debugging</h2>

  <div class="pitfalls">
    <details class="pitfall-item">
      <summary>The scene is lit from the wrong side — <span class="sym">everything looks fine, but backwards</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>Shading is smooth and plausible, but the lit side of every object is the side facing
            <em>away</em> from where you put the light.</dd>
          <dt>Cause</dt>
          <dd>The sign of <code>l</code>. The cosine law needs the vector <em>toward</em> the
            light; a directional light is usually authored as the direction its rays
            <em>travel</em>. The two are negatives of each other.</dd>
          <dt>Fix</dt>
          <dd>Ask for it by name — <code>light.to_light()</code> — rather than remembering to
            negate. To confirm, set the light to travel straight down
            <code>(0, −1, 0)</code> and check that the <em>tops</em> of objects are bright.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>A hard black rim eats into the lit side — <span class="sym">too crisp to be a shadow</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>Near the terminator, a band of pixels goes to pure black with a sharp edge, rather
            than fading into the ambient level.</dd>
          <dt>Cause</dt>
          <dd>The missing <code>max(0, ·)</code>. Past the terminator the dot product is negative,
            so the direct term <em>subtracts</em> light and cancels the ambient term before
            clipping to black (§3.3: a measured <code>−0.740</code> in red).</dd>
          <dt>Fix</dt>
          <dd>Clamp the dot product, not the final colour. Clamping at the end hides it — the
            rim becomes ambient-coloured instead of black — while the light is still being
            subtracted.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Squashed objects are lit as though they were not — <span class="sym">the hero object looks perfect</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>A flattened or stretched object has shading that belongs to its <em>unsquashed</em>
            shape: a flattened ring lit like a round tube, with a rim and a hard dark band that do
            not follow its actual silhouette. Everything uniformly scaled looks right.</dd>
          <dt>Cause</dt>
          <dd>Transforming normals with the model matrix instead of its inverse transpose (§3.5).
            Measured: 97.5% of the object's covered pixels differ, by up to 135/255 in a channel.</dd>
          <dt>Fix</dt>
          <dd><code>normal_matrix(model)</code>. To confirm you have the right one, take any
            tangent, transform it with <code>M</code> and the normal with your candidate, and
            check the dot product is still zero — that is the definition, and it is a two-line
            test.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Everything is too bright, or brightness varies with scale — <span class="sym">no obvious cause</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>Surfaces are uniformly over- or under-lit, and scaling an object changes how bright
            it is even though its orientation has not changed.</dd>
          <dt>Cause</dt>
          <dd>An un-normalised normal. <code>dot(n, l)</code> returns <code>|n| cos θ</code>, so a
            normal of length 2.16 (which the inverse transpose readily produces — see the worked
            example) makes the surface 116% too bright.</dd>
          <dt>Fix</dt>
          <dd>Normalise after transforming, not before. Normalising in model space and then
            applying the matrix does not help — the matrix is what changed the length.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Lighting looks washed out and the falloff is too slow — <span class="sym">gradients are wrong, colours are not</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>Lit surfaces are plausible at the extremes but the mid-tones are too bright, and the
            terminator is in the wrong place — too far around the object.</dd>
          <dt>Cause</dt>
          <dd>Multiplying sRGB-encoded values instead of linear ones (§3.4). Halving an encoded
            byte gives roughly 73% of the light, not 50%, so every falloff is compressed.</dd>
          <dt>Fix</dt>
          <dd>Decode once, do all arithmetic in <code>linear_rgb</code>, encode once. Test with a
            single light at intensity 1 on a white surface at 60°: it must read
            <code>0.5</code> in <em>linear</em> terms, which is about <strong>188</strong> as an
            sRGB byte — not 128. If you get 128, you are shading in the wrong space.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Loaded models are black — <span class="sym">but the hand-typed ones are fine</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>A mesh from a file renders as a silhouette in the ambient colour, with no shading at
            all, while <code>cube_mesh()</code> and friends light correctly.</dd>
          <dt>Cause</dt>
          <dd>The mesh has no normals — either the file had no <code>vn</code> statements, or the
            loader dropped them. <code>normal_at</code> returns the zero vector, which normalises
            to zero and gives <code>dot(n, l) = 0</code> everywhere: exactly ambient.</dd>
          <dt>Fix</dt>
          <dd>Fall back to the face normal (§4.4). The diagnostic is in the loader's own report —
            <code>normals</code> is the count of <code>vn</code> statements read, and a zero there
            explains the picture completely.</dd>
        </dl>
      </div>
    </details>
  </div>

  <h2 id="exercises"><span class="num">8</span>Exercises</h2>

  <div class="exercise">
    <h3><span class="xnum">3.6.1</span>Predict the terminator<span class="difficulty">warm-up</span></h3>
    <p>
      Without running anything: with the light travelling along <code>(0, −1, 0)</code>, which
      parts of the torus are exactly at the terminator? Give the condition on the surface normal,
      then predict what fraction of the visible surface is lit when you look straight down at it.
      Check by pressing <kbd>A</kbd>/<kbd>D</kbd> until the light is overhead.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        The terminator is where <code>dot(n, l) = 0</code>, i.e. where the normal is perpendicular
        to <code>l</code>. For a light straight down, that is every point whose normal is
        horizontal — on a torus, two whole circles.
      </p>
    </details>
    <details>
      <summary>Solution</summary>
      <p>
        The outer equator and the inner equator: the two circles where the tube's normal points
        horizontally outward and horizontally inward. Everything above them is lit, everything
        below is not, so from directly overhead you see exactly the lit half — but note the inner
        wall of the hole is <em>also</em> partly lit on its far side, which is a hint that a
        torus's self-shadowing is not something Lambert models at all. (Nothing in this engine
        casts shadows yet; that is Module 6.)
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.6.2</span>Break the normal matrix on purpose<span class="difficulty">guided</span></h3>
    <p>
      Give the slab a scale of <code>(1, 0.05, 1)</code> — nearly a sheet of paper — and compare
      <kbd>J</kbd>'s two modes. Then work out, on paper, the angle by which the naive transform
      tilts a normal that started at 45°, and check it against the HUD's <code>tilt</code> readout.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        For a diagonal scale the two transforms are <code>diag(1, s, 1)</code> and
        <code>diag(1, 1/s, 1)</code>. Take <code>n = (0, 0.7071, 0.7071)</code> and compute the
        angle of each result from the <code>z</code> axis.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        With <code>s = 0.05</code>: <code>M·n = (0, 0.0354, 0.7071)</code>, which is 2.9° from the
        <code>z</code> axis, while <code>X·n = (0, 14.14, 0.7071)</code>, which is 87.1°. The two
        normals are <strong>84.2°</strong> apart — nearly perpendicular. The naive version lights a
        near-flat sheet as though it were still a 45° wedge, which is the most extreme form of
        "lighting the shape it used to be".
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.6.3</span>A second light<span class="difficulty">medium</span></h3>
    <p>
      Add a second directional light — a dim, cool "fill" from roughly the opposite side — and sum
      its contribution. Then answer with a measurement: does adding the fill light ever make a
      pixel brighter than the sum of both lights' intensities? Should it be able to?
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Lighting is <em>additive</em>: each light's contribution is computed independently and
        summed, because photons from different sources do not interact. That means
        <code>shade()</code> wants a loop, and the ambient term should be added once rather than
        once per light — a mistake that is easy to make and looks like "the fill light is too
        strong".
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        No pixel can exceed the sum of the intensities, because each term is bounded by its own
        light's intensity (the cosine is at most 1) — which makes it a checkable invariant, and a
        good one to assert in a test the way <code>verify_36.cpp</code> §G does for one light. The
        design question worth sitting with is where the loop lives: putting it inside
        <code>shade()</code> means the function takes a span of lights, which is the shape Module
        6 needs anyway once lights have positions and ranges.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.6.4</span>Generate the normals a file forgot<span class="difficulty">medium</span></h3>
    <p>
      Write <code>generate_normals(mesh_data&amp;, bool smooth)</code>. For <code>smooth</code>,
      each vertex's normal is the <strong>area-weighted average</strong> of the face normals of
      every triangle that uses it. Run it on a mesh loaded with no <code>vn</code> statements and
      confirm it now shades like <code>torus.obj</code> rather than flat.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Do not normalise the face normals before accumulating them. <code>cross(b−a, c−a)</code>
        has length equal to twice the triangle's area, so simply summing the raw cross products
        <em>is</em> the area weighting — big triangles get more say, which is what you want. And
        note you need to know which faces touch a vertex, which is the adjacency
        <code>validate()</code> already walks in Lesson 3.5.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        The subtlety is which vertices to average <em>across</em>. Averaging by vertex index gives
        nothing on a mesh whose seams are already split — every vertex belongs to one smoothing
        region already. Averaging by welded <em>position</em> smooths across the seam, which is
        right for a uv seam and wrong for a genuine crease: it would round off the edges of a cube.
        Real exporters solve this with smoothing groups (OBJ's <code>s</code> statement, which we
        skip) or an angle threshold. Picking one and saying why is the exercise.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.6.5</span>Where does the energy go?<span class="difficulty">open</span></h3>
    <p>
      A Lambertian surface with albedo 1 reflects <em>all</em> the light that reaches it. Our
      <code>shade()</code> returns <code>albedo × intensity × cos θ</code> — but a real diffuse
      surface scatters that light over a whole hemisphere. Find out what the correct constant is,
      why it is <code>1/π</code>, and what would have to change elsewhere for the picture to look
      the same after you insert it.
    </p>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Integrating a constant radiance over the hemisphere with the cosine weighting gives
        <code>π</code>, so a physically normalised Lambertian BRDF is <code>albedo/π</code> — and
        every image in this lesson would go dark by that factor. Real renderers absorb the
        <code>π</code> into the light's units instead, which is why "intensity" in a game engine is
        rarely in watts. This is the first crack in the model that Module 6's radiometry lesson
        opens properly, and the honest summary for today is: our numbers are
        <em>self-consistent</em> rather than physical, and the difference is one constant and a
        change of units.
      </p>
    </details>
  </div>

  <h2 id="recap"><span class="num">9</span>Recap &amp; Next</h2>

  <div class="recap">
    <h2>What the engine can do now that it could not before</h2>
    <ul>
      <li><strong>Light.</strong> A directional light and Lambert's cosine law, derived from a
        spreading beam rather than quoted — with the clamp, the linear-space discipline, and the
        toward-the-light sign convention all made explicit rather than inherited.</li>
      <li><strong>Correct normals under any transform.</strong> <code>normal_matrix</code>, derived
        from the one property that defines a normal, together with a measured account of exactly
        when it matters (non-uniform scale: up to 68° of tilt and 97.5% of pixels) and when it
        provably does not (rotation, uniform scale).</li>
      <li><strong>Two normal sources, one evaluation point.</strong> Per-face and per-vertex
        normals both shade correctly, with the face normal as the fallback for geometry that
        carries none — and the measurement that <code>cube.obj</code> cannot tell them apart,
        because Lesson 3.5's split already decided the answer.</li>
      <li><strong>A rasterizer that did not have to change.</strong> Lighting produced vertex
        colours, and interpolating vertex colours is Lesson 2.4's job. The vertex/fragment
        boundary showed up on its own.</li>
    </ul>
    <p>
      <strong>Next:</strong> <a href="03-07-specular-blinn-phong.html">3.7 — Specular and
      Blinn-Phong</a>. Everything today is view-independent: orbit the camera and nothing changes,
      because a matte surface scatters light equally in all directions. Real surfaces are not
      matte. A highlight is light that bounced off in a <em>preferred</em> direction — which means
      the shading finally has to know where you are standing, and the model has to grow a second
      term. We will also be honest about what Blinn-Phong gets wrong, because that debt is what
      Module 6's microfacet theory is paid to settle.
    </p>
  </div>

  <h2 id="reading"><span class="num">10</span>Further Reading</h2>

  <ul class="reading">
    <li><span class="src">Scratchapixel</span> <em>Introduction to Shading</em> — the same cosine
      law, derived the same way, with a more careful treatment of solid angle than we needed
      today.</li>
    <li><span class="src">Real-Time Rendering</span> Chapter 5, <em>Shading Basics</em> — and
      §9.9 for where Lambert sits inside the general BRDF framework that Module 6 builds.</li>
    <li><span class="src">The classic</span> Eric Lengyel, <em>The Mathematics of 3D Game
      Programming</em>, §4.5 — the inverse-transpose derivation done in the same
      preserve-the-relationship style, generalised to any linear transform.</li>
    <li><span class="src">Primary source</span> J. H. Lambert, <em>Photometria</em> (1760). Worth
      knowing that the law predates every piece of technology in this course by two centuries, and
      was derived by measuring candles against each other by eye.</li>
    <li><span class="src">Ahead</span> Naty Hoffman's <em>Physics and Math of Shading</em> course
      notes (SIGGRAPH) — the clearest short account of why <code>albedo/π</code> is the physically
      normalised diffuse term, which Exercise 3.6.5 sets up and Module 6 pays off.</li>
  </ul>
"""
