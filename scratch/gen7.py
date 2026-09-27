# -*- coding: utf-8 -*-
import io, sys
sys.path.insert(0, 'scratch')
from gen_geom import weave
out = io.StringIO(); w = out.write
AMB, TEA, VIO = "#E0A83C", "#3CB8A8", "#A070D8"

CL, PL, OL = weave(150, 152, 58, 15, 26)   # left  panel (painter)
CR, PR, OR = weave(490, 152, 58, 15, 26)   # right panel (z-buffer)

w('''
  <!-- ================= 6. BUILD & RUN ================= -->
  <h2 id="build"><span class="num">6</span>Build &amp; Run</h2>

  <figure class="listing shell">
    <figcaption>
      <span class="path">all platforms</span>
      <span class="lang" data-lang="bash">shell</span>
    </figcaption>
    <pre><code class="lang-bash">cmake -S . -B build
cmake --build build</code></pre>
  </figure>

  <figure class="listing shell">
    <figcaption>
      <span class="path">run</span>
      <span class="lang" data-lang="bash">shell</span>
    </figcaption>
    <pre><code class="lang-bash"># macOS / Linux
./build/engine

# Windows (multi-config generators put binaries in a config subdirectory)
.\\build\\Debug\\engine.exe</code></pre>
  </figure>

  <div class="callout note">
    <span class="label">One new source file</span>
    <p>
      <code>src/gfx/depth_buffer.cpp</code> is added to the <code>add_executable</code> list by hand,
      as every source in this project is. CMake will notice the changed <code>CMakeLists.txt</code> and
      reconfigure on its own; if you are using an IDE that caches aggressively and the new file seems
      not to exist, delete <code>build/</code> and configure again.
    </p>
  </div>

  <h3>What you should see</h3>

  <p>
    The icosahedron is <strong>solid</strong>. Not shaded &mdash; there is no light in the scene until
    Lesson 3.6 &mdash; but faceted, opaque, and correctly occluded from every angle, with the slab and
    plinth cutting properly in front of and behind it as the camera orbits. The faces carry a
    per-triangle brightness so you can tell them apart; that is a debug palette and the lesson says so.
  </p>

  <p>The four keys this lesson adds:</p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">New demo controls</caption>
      <thead><tr><th>Key</th><th>Cycles</th><th>What to look for</th></tr></thead>
      <tbody>
        <tr><td><kbd>F</kbd></td><td>wireframe &rarr; painter&rsquo;s &rarr; z-buffer &rarr; depth view</td>
            <td>The whole lesson, on one key.</td></tr>
        <tr><td><kbd>C</kbd></td><td>solids &rarr; cycle &rarr; intersecting &rarr; near-coplanar</td>
            <td>Each scene breaks one thing. The camera resets to face it.</td></tr>
        <tr><td><kbd>B</kbd></td><td>D32_FLOAT &rarr; D24_UNORM &rarr; D16_UNORM</td>
            <td>Z-fighting, summoned on demand.</td></tr>
        <tr><td><kbd>X</kbd></td><td>selected object</td><td>Unchanged from 2.12; the HUD follows it.</td></tr>
      </tbody>
    </table>
  </div>

  <p>
    Press <kbd>C</kbd> once to load the cycle and then <kbd>F</kbd> to flip between the painter&rsquo;s
    algorithm and the z-buffer. Figure 6 is what the two look like.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 330" role="img" aria-labelledby="fig6-t fig6-d">
      <title id="fig6-t">The cycle scene rendered by the painter&#39;s algorithm and by the z-buffer</title>
      <desc id="fig6-d">Two renderings of the same three woven panels. On the left, the painter&#39;s
        algorithm: two of the three crossings are woven correctly and the third is wrong, with the
        violet panel lying on top of the teal one where the teal one should be in front. On the right,
        the z-buffer: all three crossings are woven correctly.</desc>
''')

# ---- LEFT: painter. Draw order B, A, C (sorted): A over B ok, C over A ok, C over B WRONG.
w('      <text x="150" y="34" class="sm" text-anchor="middle" font-weight="700">painter&rsquo;s algorithm</text>\n')
w('      <text x="150" y="50" class="xs t-bad" text-anchor="middle">144 px wrong &mdash; and no sort can do better</text>\n')
w('      <g>\n')
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (PL[1], TEA))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (PL[0], AMB))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (PL[2], VIO))
w('      </g>\n')
w('      <circle cx="%.1f" cy="%.1f" r="26" fill="none" class="hi" stroke-width="2.5" stroke-dasharray="5 4"/>\n'
  % (sum(p[0] for p in OL[1]) / 4.0, sum(p[1] for p in OL[1]) / 4.0))
w('      <text x="252" y="296" class="xs t-bad" text-anchor="end">this crossing is inside out</text>\n')

# ---- RIGHT: z-buffer, correct weave (planks then the three "over" patches)
w('      <text x="490" y="34" class="sm" text-anchor="middle" font-weight="700">z-buffer</text>\n')
w('      <text x="490" y="50" class="xs t-ok" text-anchor="middle">correct, from every angle, with no sort at all</text>\n')
w('      <g>\n')
for poly, col in zip(PR, (AMB, TEA, VIO)):
    w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (poly, col))
w('      </g>\n')
w('      <g>\n')
for poly, col in zip(OR, (AMB, TEA, VIO)):
    w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (poly, col))
w('      </g>\n')

w('''    </svg>
    <figcaption><span class="fignum">Figure 6.</span> The same three panels, the same camera, the same
      triangles &mdash; only the visibility strategy differs. On the left the sort has produced
      <em>an</em> order and two of the three crossings happen to come out right; the circled one cannot,
      because satisfying it would break one of the others. On the right nothing was sorted at all. The
      demo prints the disagreement live: <strong>144 pixels</strong> at this camera.</figcaption>
  </figure>

  <div class="callout ok">
    <span class="label">Checkpoint &mdash; five things to verify</span>
    <ol>
      <li>
        <strong>The solid is solid.</strong> On the default scene, in z-buffer mode, you cannot see
        the far side of the icosahedron from any angle. Orbit all the way round and it stays true.
      </li>
      <li>
        <strong>The counter tells the story.</strong> On the solids scene the HUD reads about
        <code>29 px differ</code>; press <kbd>C</kbd> and it jumps to roughly <code>144</code> on the
        cycle and <code>162</code> on the intersecting pair. If your cycle number is near zero, your
        planks are touching rather than crossing.
      </li>
      <li>
        <strong>The order genuinely does not matter.</strong> This is the claim worth testing yourself.
        Reverse the triangle list before drawing &mdash; <code>std::reverse(scene_tris.begin(),
        scene_tris.end())</code> &mdash; and the z-buffered image must be pixel-identical while the
        painter&rsquo;s changes. (The demo does half of this for you already: the comparison pass
        re-draws a list the sort has just reordered.)
      </li>
      <li>
        <strong>Depth is crammed at the far end.</strong> Press <kbd>F</kbd> to the depth view. The HUD
        reports the occupied range as about <code>[0.9503, 0.9620]</code> &mdash; <strong>1.17% of
        [0,1]</strong>. The picture is stretched to that range or you would see nothing but white.
      </li>
      <li>
        <strong>Z-fighting is a format decision.</strong> On the near-coplanar scene, D32_FLOAT and
        D24_UNORM give the near panel all 875 covered pixels; D16_UNORM loses 478 of them.
      </li>
    </ol>
  </div>

  <figure class="listing shell">
    <figcaption>
      <span class="path">expected HUD &mdash; cycle scene, z-buffer, D32_FLOAT</span>
      <span class="lang" data-lang="bash">text</span>
    </figcaption>
    <pre><code>SCENE   rotation_z(t)                t = +0.60
[O] model matrix = T * R * S   (correct)
[F] Z-BUFFER             [C] scene = CYCLE  (A&gt;B&gt;C&gt;A)
[B] depth = D32_FLOAT   6 tris   painter vs z-buffer: 144 px differ</code></pre>
  </figure>

  <!-- ================= 7. PITFALLS ================= -->
  <h2 id="pitfalls"><span class="num">7</span>Common Pitfalls &amp; Debugging</h2>

  <div class="pitfalls">

    <details class="pitfall-item">
      <summary><span class="sym">Nothing draws at all &mdash; a clean background and no geometry</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> The depth buffer was cleared to <code>0</code> instead of
          <code>1</code>. Zero is the <em>near</em> plane, so the buffer is claiming every pixel is
          already covered by something pressed against the lens, and every fragment loses its test.</p>
        <p><strong>Why it wastes an hour.</strong> The symptom looks exactly like a broken transform,
          so people go and check their matrices. The tell is that it is <em>total</em> &mdash; a broken
          matrix usually leaves something on screen, in the wrong place.</p>
        <p><strong>Fix.</strong> Clear to <code>depth_buffer::k_far</code>. Our constructor does it for
          you, which is why <code>depth_.assign(&hellip;, k_far)</code> rather than a default-initialised
          vector: the buffer cannot exist in the broken state.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">The image slowly eats itself &mdash; holes appear as the camera moves</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> The depth buffer is not cleared every frame. Last frame&rsquo;s
          depths survive, so this frame&rsquo;s geometry is tested against surfaces that are no longer
          anywhere.</p>
        <p><strong>Symptom in detail.</strong> The first frame is perfect. Then, wherever something was
          previously close to the camera, new geometry cannot get in &mdash; so you get a growing
          stencil of holes shaped like the history of the scene. Stop moving and it stops getting
          worse, which misleads people into blaming the camera.</p>
        <p><strong>Fix.</strong> <code>depth.clear()</code> immediately next to <code>fb.clear()</code>,
          every frame, unconditionally. On the GPU this is the same idea spelled
          <code>SDL_GPU_LOADOP_CLEAR</code> in the depth target&rsquo;s <code>load_op</code>.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">Occlusion is right on walls and floors, wrong on anything steeply angled</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> You are interpolating view-space <code>z</code> instead of device
          depth. Section 3.4 shows why: view depth is a hyperbola in screen space, and interpolating it
          linearly draws a chord under a curve.</p>
        <p><strong>Why it hides.</strong> A surface parallel to the screen has constant depth, so both
          choices agree exactly. Test scenes are full of axis-aligned boxes and flat floors, which is
          precisely the family of geometry that cannot reveal the bug. It shows up on ramps, terrain
          and anything viewed at a glancing angle &mdash; and the error grows with how much depth the
          triangle spans, which is why it looks intermittent.</p>
        <p><strong>Fix.</strong> Store <code>viewport::to_screen(ndc).z</code>. If you want a
          <em>linear</em> depth for a debug view or a fog term, reconstruct it from the device value
          with <code>w = B / (z_ndc + A)</code> &mdash; do not interpolate it.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">A one-pixel seam of wrong occlusion where two triangles meet</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> The depth was interpolated with the <em>biased</em> edge-function
          values &mdash; the ones carrying the top-left rule&rsquo;s <code>-1</code>. That bias is a
          statement about who owns a boundary pixel, not about where the pixel is; leaving it in
          displaces the whole depth field by <code>1 / edge_length</code> of a pixel and stops the three
          weights summing to one.</p>
        <p><strong>Why it is worse here than for colour.</strong> Lesson 2.4 met this bug on colour,
          where a sub-pixel shift is invisible. On depth, the same shift changes which of two adjacent
          triangles wins along their shared edge, and a visible seam appears in exactly the places
          your eye is drawn to.</p>
        <p><strong>Fix.</strong> Subtract the bias before dividing &mdash; <code>(w0 - s.bias0) *
          inv_area</code> &mdash; and use the same <code>f0, f1, f2</code> for depth as for colour.
          If they are computed twice, that is the bug waiting to happen.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">Two surfaces shimmer and interleave as the camera moves</span></summary>
      <div class="pitfall-body">
        <p><strong>Cause.</strong> Z-fighting: the two surfaces are closer together than one depth code
          at that distance, so they store the same number and the tie-break decides &mdash; differently
          from pixel to pixel as the rounding tips.</p>
        <p><strong>Diagnose it with arithmetic, not by staring.</strong> One code spans
          <code>&Delta;w = &Delta;z &middot; w&sup2; &middot; (1/near &minus; 1/far)</code>. Put in your
          numbers. If the answer is bigger than the gap between your surfaces, you have found it, and
          no amount of nudging geometry will help for long.</p>
        <p><strong>Fix, in order of what usually works.</strong> Push the near plane out &mdash; it is
          free and it is nearly always the real problem. Use a wider format. Move to reversed-Z with a
          float buffer (Exercise 3.1.4). Only then start biasing geometry, which is what shadow mapping
          will force us to do properly in Module 6.</p>
      </div>
    </details>

    <details class="pitfall-item">
      <summary><span class="sym">The disagreement counter never reads zero, even on the scene where sorting is correct</span></summary>
      <div class="pitfall-body">
        <p><strong>This one is ours, and it is not a bug.</strong> On the solids scene the counter sits
          at a couple of dozen pixels rather than zero, in thin one-pixel runs along silhouettes.</p>
        <p><strong>Cause.</strong> We do not cull back faces yet, so a back face and a front face are
          both drawn along every silhouette edge &mdash; and along that shared edge they have
          <em>exactly equal</em> depth. <code>fill_triangle</code> reorients backwards-wound triangles
          so they still fill (Lesson 2.2), which means the shared edge is traversed in the same
          direction by both and the top-left rule lets both claim the boundary pixel. The painter
          gives it to whichever is drawn last (the front face); the z-buffer&rsquo;s strict
          <code>&lt;</code> gives it to whichever was drawn first (the back face). Neither is wrong
          &mdash; both triangles genuinely contain that point.</p>
        <p><strong>Verified.</strong> <code>scratch/verify_31_render.cpp</code> drops screen-space
          back-facing triangles and re-measures: <code>with back faces culled: 6 of 12 tris kept, 0 px
          differ</code>. The residue is entirely silhouette ties and nothing else.</p>
        <p><strong>Fix.</strong> Lesson 3.4. Back-face culling removes the offending triangles before
          they are ever rasterized, and it does so for a performance reason &mdash; the correctness
          improvement here is a side effect. Until then, read the counter as
          &ldquo;a couple of dozen means agreement&rdquo;.</p>
      </div>
    </details>

  </div>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 7 appended:", len(out.getvalue()), "chars")
