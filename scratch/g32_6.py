# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write

w('''
  <!-- ================= 6. BUILD & RUN ================= -->
  <h2 id="build"><span class="num">6</span>Build &amp; Run</h2>

  <figure class="listing shell">
    <figcaption>
      <span class="path">all platforms</span>
      <span class="lang" data-lang="bash">shell</span>
    </figcaption>
    <pre><code class="lang-bash">cmake -S . -B build
cmake --build build

# macOS / Linux
./build/engine
# Windows
.\\build\\Debug\\engine.exe</code></pre>
  </figure>

  <p>
    No new source files this lesson, so nothing changes in <code>CMakeLists.txt</code>.
  </p>

  <h3>What you should see</h3>

  <p>
    Press <kbd>C</kbd> until the scene reads <code>FLOOR (checker to horizon)</code>. The camera
    drops to eye level and a checkerboard runs away to a vanishing point. Then:
  </p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">New demo controls</caption>
      <thead><tr><th>Key</th><th>Does</th><th>What to look for</th></tr></thead>
      <tbody>
        <tr><td><kbd>I</kbd></td><td>affine &harr; perspective-correct</td>
            <td>The whole lesson, on one key. The floor stops being a floor.</td></tr>
        <tr><td><kbd>T</kbd></td><td>subdivides 1&rarr;2&rarr;4&rarr;8&rarr;16&rarr;1</td>
            <td>With <kbd>I</kbd> on affine, watch the error shrink &mdash; and refuse to vanish.</td></tr>
        <tr><td><kbd>Y</kbd></td><td>frame throttle</td>
            <td>Moved from <kbd>T</kbd>, which this lesson took.</td></tr>
      </tbody>
    </table>
  </div>

  <div class="callout ok">
    <span class="label">Checkpoint &mdash; five things to verify</span>
    <ol>
      <li>
        <strong>The correct floor is correct at 1&times;1.</strong> Two triangles for the whole
        plane, and the checkerboard is exact. That is the headline: no subdivision, no cost beyond
        one divide per pixel.
      </li>
      <li>
        <strong>The affine floor kinks along the diagonal.</strong> Look at where each quad is split.
        The pattern is <em>continuous</em> across that line &mdash; both triangles agree along a
        shared edge, because affine interpolation along an edge depends only on its two endpoints
        &mdash; but the gradient jumps, so the checker lines visibly bend there.
      </li>
      <li>
        <strong>The counter reads about 14,600 px at 1&times;1</strong> and stays near 48% until the
        floor is subdivided 16&times;16, when it collapses to roughly 5%. If it never falls, check
        that your uvs come from the world position and not the grid index.
      </li>
      <li>
        <strong>Nothing else changed.</strong> Press <kbd>C</kbd> back round to the Module 2 solids
        and the Gouraud triangle demo on <kbd>Tab</kbd>: both must look exactly as they did. Their
        <code>inv_w</code> is 1, so the correction is the identity.
      </li>
      <li>
        <strong>Orbit far enough and the floor vanishes.</strong> That is not this lesson's bug: a
        triangle with any vertex behind the near plane is dropped whole, and fixing it by
        <em>clipping</em> rather than dropping is Lesson 3.3. You are looking at its motivation.
      </li>
    </ol>
  </div>

  <figure class="listing shell">
    <figcaption>
      <span class="path">expected HUD &mdash; floor scene, affine, 1&times;1</span>
      <span class="lang" data-lang="bash">text</span>
    </figcaption>
    <pre><code>SCENE   rotation_z(t)                t = +0.60
[O] model matrix = T * R * S   (correct)
[F] Z-BUFFER             [C] scene = FLOOR (checker to horizon)
[I] AFFINE (wrong)  [T]  1x1  floor (2 tris)   affine vs correct: 14632 px</code></pre>
  </figure>

  <!-- ================= 7. PITFALLS ================= -->
  <h2 id="pitfalls"><span class="num">7</span>Common Pitfalls &amp; Debugging</h2>

  <div class="pitfalls">

    <details class="pitfall-item">
      <summary><span class="sym">Occlusion goes subtly wrong with distance after adding the correction</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> You applied the correction to depth as well. Device depth is
          already affine in screen space (Lesson 3.1 &sect;3.4) &mdash; the projection matrix's
          third row did that job in advance. Correcting it a second time produces
          <code>z/w</code> over <code>1/w</code>, which is not depth and is not anything.</p>
        <p><strong>Why it is nasty.</strong> It is self-consistent: every triangle is corrupted the
          same way, so surfaces still occlude each other <em>plausibly</em>. The symptom reads like
          a depth-precision problem and sends you off to change your near plane.</p>
        <p><strong>Fix, and the test.</strong> Interpolate <code>vertex::z</code> directly. Then
          assert the thing that must be true: <strong>the depth buffer's contents must not change
          at all</strong> when perspective correction is toggled. Over a triangle whose colours
          change on 20,590 pixels, the depth buffer must differ on zero.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">The texture is right at the corners and wrong in the middle</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> This is the lesson's own artifact &mdash; affine interpolation.
          But it is worth knowing as a <em>signature</em>, because it identifies the whole family:
          an error that is exactly zero at every vertex and maximal in the interior is a chord drawn
          under a curve.</p>
        <p><strong>Why it survives review.</strong> Every check you would naturally run &mdash; print
          the corner uvs, verify the mesh, look at the wireframe &mdash; passes perfectly. The error
          lives only where nothing is printed.</p>
        <p><strong>Fix.</strong> Interpolate <code>a/w</code> and <code>1/w</code>, then divide. And
          add the diagnostic that would have caught it: sample a point in the <em>middle</em> of a
          triangle and compare against the ray-plane intersection.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">Everything looks perfect on your test scene and breaks in the game</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> Your test scene faces the camera. If a triangle's plane is
          parallel to the screen then <code>w</code> is constant across it, <code>1/w</code> is
          constant, and affine and perspective-correct interpolation agree <em>exactly</em>. Sprites,
          UI quads, billboards and the front faces of axis-aligned boxes are all in this family.</p>
        <p><strong>Why it matters beyond this bug.</strong> The same blind spot hid Lesson 3.1's
          view-space-depth error, for the same reason. When something "works on my test scene", the
          useful question is not "what is different about the game" but <strong>"what family of
          input does my test scene structurally exclude?"</strong></p>
        <p><strong>Fix.</strong> Put a steeply-angled surface in the test scene permanently. Ours is
          the floor, and it is one keypress away.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">A seam of doubled cells where the texture coordinate crosses zero</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> A procedural pattern using <code>static_cast&lt;int&gt;</code>
          instead of <code>std::floor</code>. The cast truncates toward zero, so
          <code>-0.5</code> and <code>+0.5</code> both land in cell <code>0</code> and that cell is
          twice as wide as every other.</p>
        <p><strong>Why you may not hit it.</strong> Only if your uvs go negative &mdash; which they
          do the moment a texture is tiled, a uv is offset, or (as here) a floor is centred on the
          origin.</p>
        <p><strong>Fix.</strong> <code>std::floor</code>. The same trap appears in tile lookups, grid
          hashing and spatial partitioning, and Module 7's broadphase will meet it again.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">Subdividing helps, so you conclude the bug is fixed</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> Affine error falls with the <em>square</em> of the subdivision, so
          a few rounds of tessellation make it invisible on your test camera &mdash; and it comes
          straight back when someone walks closer, or the field of view widens, or a level designer
          builds a long corridor.</p>
        <p><strong>The measurement that settles it.</strong> At 2,048 triangles our floor is still
          wrong on 381 pixels. Convergence is not termination.</p>
        <p><strong>Fix.</strong> Pay the divide. If you genuinely cannot &mdash; and on 1996
          hardware people genuinely could not &mdash; then subdivide <em>adaptively</em>, by the
          range of <code>w</code> a triangle spans rather than uniformly in world space, because the
          near triangles are the only ones that need it.</p>
      </div>
    </details>

  </div>

  <!-- ================= 8. EXERCISES ================= -->
  <h2 id="exercises"><span class="num">8</span>Exercises</h2>

  <div class="exercise">
    <h3><span class="xnum">3.2.1</span>Move the seam<span class="difficulty">warm-up</span></h3>
    <p>
      In affine mode the checker kinks along the diagonal each quad is split on. Change
      <code>build_floor</code> to split its quads the other way &mdash;
      <code>(0,1,2)+(0,2,3)</code> becomes <code>(0,1,3)+(1,2,3)</code> &mdash; and predict what
      happens before you run it.
    </p>
    <details>
      <summary>Solution</summary>
      <p>
        The kink moves to the other diagonal, and the pattern is wrong in a mirrored way. This is
        worth doing because of what it proves: <strong>the artifact depends on a choice that is not
        in the geometry, not in the texture, and not in the camera.</strong> Two renderers that
        triangulate a quad differently produce different images of the same scene. That is a
        decisive argument on its own &mdash; a correct renderer's output cannot depend on how you
        happened to cut a rectangle in half.
      </p>
      <p>
        Check the perspective-correct mode too: it is <em>identical</em> under both splits, to the
        pixel. It must be, because it reconstructs the surface rather than the triangulation.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.2.2</span>Hoist the divide<span class="difficulty">medium</span></h3>
    <p>
      When all three corners share the same <code>inv_w</code> &mdash; every 2-D fill, every
      screen-parallel surface &mdash; the denominator is constant across the triangle and the
      per-pixel divide is pure waste. Detect that case once per triangle and hoist it. Measure the
      difference on the triangle demo (<kbd>Tab</kbd>) and on the floor.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        <code>iw0 == iw1 &amp;&amp; iw1 == iw2</code> is an exact float comparison, and here that is
        correct rather than sloppy: the values are either copied from the same source or they are
        not. Think about what the reciprocal should be in that case &mdash; it is not 1.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        The hoisted reciprocal is <code>1 / iw</code>, not 1 &mdash; the weights sum to one, so the
        denominator is <code>iw</code> itself. You will find the 2-D demos get measurably faster and
        the floor does not change at all, which is the honest result: you have optimised the case
        that was already cheap. The interesting question the exercise leaves you with is whether a
        branch per triangle is worth it for a divide per pixel, and the answer depends entirely on
        how many of your triangles are screen-parallel &mdash; in a UI-heavy application, most of
        them.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.2.3</span>Find the worst pixel<span class="difficulty">medium</span></h3>
    <p>
      Our counter says how <em>many</em> pixels are wrong, which saturates (&sect;3.6). Write a
      diagnostic that reports how <em>wrong</em> the worst one is: for each covered pixel compute
      both the affine and the corrected <code>u</code>, and track the maximum difference. Report it
      in the HUD in checker cells.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        You already have both numbers inside the loop &mdash; the affine value is the numerator
        before the divide-back, scaled differently. Easier: compute both explicitly in a debug
        build. Also ask <em>where</em> the worst pixel is; the answer is not where most people guess.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        You should reproduce the harness's numbers: 4.23 cells at 1&times;1, falling to 0.03 at
        32&times;32. The worst pixel is not at the far end &mdash; out there both schemes agree
        because everything is compressed into nothing. It is in the <em>middle</em> of the biggest
        triangle, where the chord is furthest from the curve, which is exactly what Figure 4 shows
        and exactly where nobody looks.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.2.4</span>Adaptive subdivision<span class="difficulty">hard</span></h3>
    <p>
      Uniform tessellation is a blunt instrument: the near quads span an enormous range of
      <code>w</code> and the far ones span almost none, yet they get the same treatment. Subdivide
      a triangle only while the ratio <code>w_max / w_min</code> across it exceeds some threshold.
      How many triangles does it take to match 16&times;16 uniform, and where do they go?
    </p>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Far fewer, and they all pile up near the camera. This is the algorithm the better
        fifth-generation engines actually shipped, and playing with the threshold gives you a feel
        for why it was a losing battle: the triangle budget needed depends on the camera, so it has
        to be recomputed every frame, and a player who walks up to a wall can bankrupt it. Meanwhile
        one divide per pixel costs the same no matter where anyone stands. Write down the threshold
        that makes the two approaches cost the same on your machine &mdash; that number is the whole
        history of the technique.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.2.5</span>What else is on the curve?<span class="difficulty">open</span></h3>
    <p>
      The derivation never mentioned texture coordinates. Give the floor's vertices per-corner
      <em>colours</em> instead of uvs, switch the shading to <code>vertex_colour</code>, and toggle
      <kbd>I</kbd>. Then reason about what happens in Lesson 3.6 when we interpolate a normal.
    </p>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Colour is wrong in exactly the same way and for exactly the same reason &mdash; but it is
        far harder to <em>see</em>, because a colour gradient has no sharp features for your eye to
        register the distortion against. This is a general and slightly uncomfortable lesson: how
        visible a bug is depends on the frequency content of what it is corrupting, not on how big
        the error is. The checkerboard is a good test pattern precisely because it is nearly all
        edge.
      </p>
      <p>
        Normals are worse, for a reason worth anticipating: an interpolated normal must also be
        <em>renormalised</em> after interpolation, because the average of two unit vectors is not a
        unit vector. So a normal gets both the perspective divide and a normalise &mdash; two
        corrections, in that order, and Lesson 3.6 has to get both right before any lighting means
        anything.
      </p>
    </details>
  </div>

  <!-- ================= 9. RECAP & NEXT ================= -->
  <h2 id="recap"><span class="num">9</span>Recap &amp; Next</h2>

  <div class="recap">
    <h2>What the engine can do now that it could not before</h2>
    <ul>
      <li>
        <strong>Carry any attribute across a triangle correctly</strong>, at any angle, at any depth
        &mdash; texture coordinates today, normals in 3.6, anything a vertex ever holds.
      </li>
      <li>
        <strong>Draw a plane running to the horizon with two triangles.</strong> Not two thousand.
      </li>
      <li>
        <strong>Reproduce the failure on demand</strong>, with a live count of how wrong it is, and
        a subdivision control that shows exactly what the alternative would have cost.
      </li>
      <li>
        <strong>Render state as an object</strong> rather than a growing tail of parameters &mdash;
        the shape Module 4's pipelines have.
      </li>
    </ul>
    <h2>And the ideas worth taking beyond this engine</h2>
    <ul>
      <li>
        <strong>Interpolate the quantity that is affine in the space you are walking.</strong> You
        are walking pixels, so ask what is affine <em>in pixels</em>. Depth already was;
        <code>a/w</code> and <code>1/w</code> are; <code>a</code> is not. This one sentence covers
        Lessons 3.1 and 3.2 together.
      </li>
      <li>
        <strong>An error that is zero at the corners and maximal in the middle is a chord under a
        curve.</strong> Recognise the shape and you know where to look, and why every vertex-level
        check passed.
      </li>
      <li>
        <strong>Ask what your test scene structurally cannot show.</strong> Screen-parallel geometry
        cannot reveal either of Module 3's interpolation bugs. That is not bad luck; it is a
        property of the test set, and it is findable in advance.
      </li>
    </ul>
  </div>

  <p>
    <strong>Next: Lesson 3.3 &mdash; Near-Plane Clipping.</strong> You have already seen its
    motivation: orbit the floor and it disappears. Our projection guard drops any triangle with a
    vertex closer than <code>w = 0.05</code>, whole &mdash; which is survivable when the geometry is
    a small object in the middle of the view and catastrophic when it is the wall you are walking
    into. The fix is not a bigger guard, it is to <em>cut</em> the triangle against the near plane
    and rasterize the part that is in front, which turns one triangle into one or two. That is
    Sutherland&ndash;Hodgman, and it is the last thing standing between our rasterizer and geometry
    it cannot be trusted with.
  </p>

  <!-- ================= 10. FURTHER READING ================= -->
  <h2 id="reading"><span class="num">10</span>Further Reading</h2>

  <div class="reading">
    <ul>
      <li>
        <strong>Kurt Akeley &amp; Tom Jermoluk, &ldquo;High-Performance Polygon Rendering&rdquo;</strong>
        (SIGGRAPH 1988), and <strong>Paul Heckbert &amp; Henry Moreton, &ldquo;Interpolation for
        Polygon Texture Mapping and Shading&rdquo;</strong> (1991). The primary sources for
        rational-linear interpolation. Heckbert&rsquo;s treatment is the one to read if you want the
        projective-geometry framing rather than the substitution we did.
      </li>
      <li>
        <strong>Chris Hecker, &ldquo;Perspective Texture Mapping&rdquo;</strong> (Game Developer
        Magazine, 1995&ndash;96, five parts). Written while this was still an open engineering
        problem, on hardware where a divide per pixel was unaffordable. The best possible
        illustration of &sect;3.6: watch a very good engineer spend five articles on the escape
        routes we can now decline in one line.
      </li>
      <li>
        <strong>Real-Time Rendering, 4th edition, &sect;23.1&ndash;23.2.</strong> How the hardware
        does it, and why the reciprocal is computed once per fragment for all varyings at once.
      </li>
      <li>
        <strong>Scratchapixel, &ldquo;Rasterization: a Practical Implementation&rdquo;.</strong>
        Covers depth and perspective-correct interpolation together, with the same
        first-principles bent as this course.
      </li>
      <li>
        <strong>Fabien Sanglard, <em>Game Engine Black Book: DOOM</em> and <em>Wolfenstein 3D</em>.</strong>
        Both engines dodge this problem entirely by restricting geometry &mdash; axis-aligned walls
        and floors of constant height &mdash; which is the third escape route: <em>make the case
        that breaks impossible to author</em>. Worth knowing as an option; it is why those games
        look the way they do.
      </li>
      <li>
        <strong>The PlayStation&rsquo;s GTE and GPU documentation</strong> (the psx-spx community
        reference). The hardware that made this artifact famous, described in enough detail to see
        that omitting perspective correction was a deliberate, costed decision rather than an
        oversight.
      </li>
    </ul>
  </div>

  <details class="state">
    <summary>STATE</summary>
    <pre>course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: Module 3, Lesson 3.2 complete

conventions:
  right-handed world, +Y up, camera looks down -Z
  column-major matrices, written row-by-row, applied as M * v
  clip/NDC: SDL_GPU — x,y in [-1,1] with +Y UP; z in [0,1], 0 = near
  DEPTH: smaller is nearer; clear to 1.0 (far); compare with &lt;
  INTERPOLATION: attributes are perspective-corrected (a/w and 1/w, then divide);
                 DEPTH IS NOT — device depth is already affine in screen space
  viewport: +Y DOWN; the flip lives in viewport::to_screen; it is a PARAMETER
  winding: counter-clockwise = front face (stated in NDC)
  units: 1 unit = 1 metre; angles in radians internally
  naming: snake_case; types, functions and files alike
  axis colours: x/y/z = red/green/blue

completed:
  MODULE 0 (0.1-0.6) · MODULE 1 (1.1-1.8) · MODULE 2 COMPLETE (2.1-2.12)
  MODULE 3 (in progress)
  - 3.1 The Painter&#39;s Problem and the Z-Buffer
  - 3.2 Perspective-Correct Interpolation

capabilities:
  - window, event loop, input state, fixed timestep with interpolation
  - CPU framebuffer presented through an SDL streaming texture
  - lines (Bresenham), triangles (edge functions), barycentric interpolation
  - vec2/3/4, mat2/3/4 (look_at, perspective), transform, viewport
  - mesh (indexed geometry + uvs: cube, quad, icosahedron, runtime floor)
  - THE COMPLETE GEOMETRY PIPELINE: model -&gt; world -&gt; view -&gt; clip -&gt; NDC -&gt; screen
  - DEPTH BUFFER: per-pixel hidden-surface removal, D32/D24/D16 precision
  - PERSPECTIVE-CORRECT INTERPOLATION of every vertex attribute, with the affine
    failure summonable on [I] and measured live
  - fill_style: render state as an object (interpolation, shading, blend space)

files:
  src/: main.cpp
  src/core/: input.hpp/.cpp, clock.hpp/.cpp, fixed_step.hpp/.cpp
  src/gfx/: colour.hpp/.cpp, depth_buffer.hpp/.cpp, framebuffer.hpp/.cpp,
            raster.hpp/.cpp, viewport.hpp, mesh.hpp
  src/math/: vec2.hpp, vec3.hpp, vec4.hpp, mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp
  src/game/: pong.hpp/.cpp
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01 .. 00-06, 01-01 .. 01-08, 02-01 .. 02-12, 03-01, 03-02

next: 3.3 — Near-Plane Clipping</pre>
  </details>

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="03-01-z-buffer.html">
      <span class="dir">&larr; Previous</span>
      <span class="ttl">3.1 &mdash; The Painter&rsquo;s Problem and the Z-Buffer</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-03-near-plane-clipping.html">
      <span class="dir">Next &rarr;</span>
      <span class="ttl">3.3 &mdash; Near-Plane Clipping</span>
    </a>
  </nav>

  <footer class="foot">
    <p>
      Build a Professional 3D Game Engine &middot;
      <a href="../index.html">Contents</a> &middot;
      <a href="../conventions.html">Conventions</a> &middot;
      <a href="../math-toolbox.html">Math Toolbox</a> &middot;
      <a href="../cpp-style.html">C++ Style</a>
    </p>
    <p>MIT licensed. Copyright &copy; 2026 digster.</p>
  </footer>
</div>

<!-- ==========================================================================
     PAGE SCRIPTS
     ========================================================================== -->
<!-- SHARED-SCRIPT:BEGIN -->
<!-- SHARED-SCRIPT:END -->
</body>
</html>
''')

open('docs/lessons/03-02-perspective-correct.html', 'a').write(out.getvalue())
print("part 6:", len(out.getvalue()), "chars")
