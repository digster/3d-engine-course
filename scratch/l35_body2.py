# Lesson 3.5 body — part 2: the theory.
BODY2 = r"""
  <!-- =================================================================
       SECTION 4 — THE THEORY
       ================================================================= -->
  <h2 id="theory"><span class="num">3</span>The Theory</h2>

  <h3 id="theory-onebased">3.1 Indices start at one</h3>

  <p>
    OBJ numbers its elements from 1. The first <code>v</code> line in a file is position 1,
    the second is position 2, and so on, with each of the three streams counting separately.
    C++ arrays start at 0. The conversion is a subtraction, and it is the single most common
    bug in every OBJ loader ever written — including, historically, several shipped ones.
  </p>

  <div class="eq">
    <span class="katex-src">\[ \text{slot} = \text{index} - 1 \qquad (\text{index} \ge 1) \]</span>
    <span class="eq-plain">slot = index - 1, for index &gt;= 1</span>
  </div>

  <p>
    In prose: <em>subtract one to turn a file index into an array subscript.</em> The reason
    this bug is so persistent is that it does not crash and it does not look random. It
    produces geometry — wrong geometry, but geometry — because index <code>k</code> instead of
    <code>k−1</code> is still a perfectly good corner, just the next one along.
  </p>

  <div class="worked">
    <span class="label">Worked example — <code>assets/cube.obj</code>, first face</span>
    <p>The file's first face statement is:</p>
    <p><code>f 1/1/1 4/4/1 3/3/1 2/2/1</code></p>
    <p>Taking only the position indices — 1, 4, 3, 2 — and subtracting one gives slots 0, 3,
      2, 1. Reading those from the file's <code>v</code> list:</p>
    <ul>
      <li>slot 0 → <code>(-0.5, -0.5, -0.5)</code></li>
      <li>slot 3 → <code>(-0.5, +0.5, -0.5)</code></li>
      <li>slot 2 → <code>(+0.5, +0.5, -0.5)</code></li>
      <li>slot 1 → <code>(+0.5, -0.5, -0.5)</code></li>
    </ul>
    <p>
      All four have <code>z = −0.5</code>: they are the four corners of the cube's back face,
      exactly as they should be. Check the winding with Lesson 3.4's tool — take the first
      three, compute <code>cross(b − a, c − a)</code>:
    </p>
    <ul>
      <li><code>b − a = (0, 1, 0)</code>, <code>c − a = (1, 1, 0)</code></li>
      <li><code>cross = (1·0 − 0·1, 0·1 − 0·0, 0·1 − 1·1) = (0, 0, −1)</code></li>
    </ul>
    <p>
      The face normal points along <strong>−z</strong>, away from the cube's centre — the face
      is wound counter-clockwise seen from outside, which is this course's convention. And the
      file's own <code>vn 1</code> is <code>(0, 0, −1)</code>: the data agrees with itself.
    </p>
    <p>
      Now make the mistake. Read <code>1, 4, 3, 2</code> as slots directly and you get
      <code>(0.5,−0.5,−0.5)</code>, <code>(−0.5,−0.5,0.5)</code>, <code>(−0.5,0.5,−0.5)</code>
      and <code>(0.5,0.5,−0.5)</code> — four corners that do not lie in a plane, from three
      different faces of the cube. The model still draws. It draws as shrapnel.
    </p>
  </div>

  <h3 id="theory-negative">3.2 …and may count backwards</h3>

  <p>
    OBJ also allows <strong>negative</strong> indices, and they mean something genuinely
    different: <code>−1</code> is the most recently defined element of that stream,
    <code>−2</code> the one before it, and so on. Not "the last element in the file" —
    <em>the last one seen so far</em>, at the point the face statement appears.
  </p>

  <div class="eq">
    <span class="katex-src">\[ \text{slot} = n + \text{index} \qquad (\text{index} \le -1) \]</span>
    <span class="eq-plain">slot = n + index, for index &lt;= -1, where n is how many elements have been read so far</span>
  </div>

  <p>
    In prose: <em>a negative index counts back from the current end, so with <code>n</code>
    elements read, <code>−1</code> is slot <code>n − 1</code>.</em> Add the negative number and
    the arithmetic works out; there is no extra <code>−1</code>, which is a pleasant asymmetry
    worth noticing rather than fighting.
  </p>

  <p>
    Why would a format do this? Because it makes files <strong>concatenable</strong>. A tool
    emitting one object at a time does not have to know how many vertices came before it: if
    every face refers to its own vertices relatively, you can paste two files together and
    both halves still work. Exporters that write per-object chunks really do use it, so a
    loader that ignores it will one day meet a file it cannot read.
  </p>

  <div class="worked">
    <span class="label">Worked example — the same three numbers, twice, meaning different things</span>
    <p>Consider this file, which our test harness parses in §A:</p>
    <figure class="listing">
      <figcaption><span class="path">two chunks, both using −3 −2 −1</span><span class="lang" data-lang="cpp">OBJ</span></figcaption>
      <pre><code class="lang-cpp">v 0 0 0
v 1 0 0
v 0 1 0
f -3 -2 -1
v 5 0 0
v 6 0 0
v 5 1 0
f -3 -2 -1</code></pre>
    </figure>
    <p>
      At the first face, <code>n = 3</code>. So <code>−3 → 0</code>, <code>−2 → 1</code>,
      <code>−1 → 2</code>: the triangle is slots 0, 1, 2.
    </p>
    <p>
      At the second face, <code>n = 6</code>. Now <code>−3 → 3</code>, <code>−2 → 4</code>,
      <code>−1 → 5</code>: a completely different triangle, from the same three numbers.
      Verified — the harness checks that the second <code>−3</code> resolves to the fourth
      position, <code>(5, 0, 0)</code>.
    </p>
    <p>
      This is why resolution must happen <strong>as the file is read</strong>, against the
      counts so far, and cannot be deferred to a tidy second pass over collected face data.
    </p>
  </div>

  <div class="callout pitfall">
    <span class="label">Index zero is not a thing</span>
    <p>
      Because the numbering starts at 1 and negatives count backwards, <strong>0 is not a
      legal OBJ index</strong>. That is convenient: it gives us a free sentinel. In the code
      below, a <code>corner_ref</code> holds <code>0</code> to mean "this corner did not
      specify a texture coordinate", and there is no valid element for the sentinel to
      collide with.
    </p>
  </div>

  <!-- ---- WIDGET: face-statement resolver ---- -->
  <div class="widget">
    <p style="margin-top:0"><strong>Try it.</strong> Type face statements below — one per
      line — and watch them resolve against this miniature file. Every rule from §3.1 to §3.5
      is live here: 1-based numbering, negative indices, the four corner formats, fan
      triangulation, and the de-duplicating map that decides how many vertices come out. The
      map persists across lines, which is the point: <strong>reuse only ever happens between
      two different faces</strong>. Compare the <em>shared</em> and <em>split</em> examples —
      same positions, one changed normal, two more vertices.</p>

    <div style="display:flex;flex-wrap:wrap;gap:1.2rem">
      <div style="flex:0 0 auto">
        <div class="widget-readout" style="display:block;white-space:pre;line-height:1.5">v  1  (0,0,0)
v  2  (1,0,0)
v  3  (1,1,0)
v  4  (0,1,0)
vt 1  (0,0)
vt 2  (1,0)
vt 3  (1,1)
vn 1  (0,0,1)
vn 2  (0,1,0)</div>
      </div>
      <div style="flex:1 1 280px;min-width:280px">
        <label for="w35-in" style="display:block;font-family:var(--font-ui);font-size:0.8rem;margin-bottom:0.3rem">face statements — one per line</label>
        <textarea id="w35-in" rows="3" spellcheck="false"
               style="width:100%;font-family:var(--font-mono);font-size:0.85rem;padding:0.4rem;box-sizing:border-box;resize:vertical">f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/1/1</textarea>
        <div class="widget-controls" style="margin-top:0.5rem">
          <button type="button" class="w35-eg" data-v="f 1/1/1 2/2/1 3/3/1&#10;f 1/1/1 3/3/1 4/1/1">shared</button>
          <button type="button" class="w35-eg" data-v="f 1/1/1 2/2/1 3/3/1&#10;f 1/1/2 3/3/2 4/1/2">split</button>
          <button type="button" class="w35-eg" data-v="f 1/1/1 2/2/1 3/3/1 4/1/1">a quad</button>
          <button type="button" class="w35-eg" data-v="f -4 -3 -2">negative</button>
          <button type="button" class="w35-eg" data-v="f 1//2 2//2 3//2">v//vn</button>
          <button type="button" class="w35-eg" data-v="f 0 1 2">index 0</button>
          <button type="button" class="w35-eg" data-v="f 1 9 2">out of range</button>
        </div>
      </div>
    </div>

    <div id="w35-out" style="margin-top:0.9rem;font-family:var(--font-mono);font-size:0.8rem;white-space:pre-wrap"></div>
  </div>

  <script>
  (function () {
    var POS = [[0,0,0],[1,0,0],[1,1,0],[0,1,0]];
    var UV  = [[0,0],[1,0],[1,1]];
    var NRM = [[0,0,1],[0,1,0]];

    function resolve(raw, n) {
      if (raw > 0) { return raw <= n ? raw - 1 : -2; }   // -2 = out of range
      if (raw < 0) { var i = n + raw; return i >= 0 ? i : -2; }
      return -2;                                         // 0 is not legal
    }

    function run() {
      var out = document.getElementById('w35-out');
      var srcLines = document.getElementById('w35-in').value.split('\n');

      // The map and the vertex array persist ACROSS faces — which is the whole
      // point: reuse only ever happens between two different faces.
      var map = Object.create(null);
      var verts = [];
      var report = [];
      var totalCorners = 0, reused = 0, triangles = 0, dropped = 0;
      var failed = null;

      for (var L = 0; L < srcLines.length && !failed; ++L) {
        var toks = srcLines[L].split(/\s+/).filter(function (t) { return t.length > 0; });
        if (toks.length && (toks[0] === 'f' || toks[0] === 'F')) { toks.shift(); }
        if (toks.length === 0) { continue; }

        var corners = [];
        for (var i = 0; i < toks.length; ++i) {
          var parts = toks[i].split('/');
          if (parts.length > 3 || parts[0] === '') {
            failed = 'line ' + (L + 1) + ': corner "' + toks[i] + '" is not a legal v / v/vt / v//vn / v/vt/vn'; break;
          }
          var rp = parseInt(parts[0], 10);
          var rt = (parts.length > 1 && parts[1] !== '') ? parseInt(parts[1], 10) : 0;
          var rn = (parts.length > 2 && parts[2] !== '') ? parseInt(parts[2], 10) : 0;
          if (isNaN(rp) || (parts.length > 1 && parts[1] !== '' && isNaN(rt))
                        || (parts.length > 2 && parts[2] !== '' && isNaN(rn))) {
            failed = 'line ' + (L + 1) + ': corner "' + toks[i] + '" contains something that is not a number'; break;
          }
          var p = resolve(rp, POS.length);
          var t = rt === 0 ? -1 : resolve(rt, UV.length);
          var nn = rn === 0 ? -1 : resolve(rn, NRM.length);
          if (p === -2) { failed = 'line ' + (L + 1) + ': position index ' + rp + ' names nothing (there are ' + POS.length + ')'; break; }
          if (t === -2) { failed = 'line ' + (L + 1) + ': uv index ' + rt + ' names nothing (there are ' + UV.length + ')'; break; }
          if (nn === -2) { failed = 'line ' + (L + 1) + ': normal index ' + rn + ' names nothing (there are ' + NRM.length + ')'; break; }

          var key = p + ',' + t + ',' + nn;
          var idx, note;
          if (key in map) { idx = map[key]; note = 'reused'; ++reused; }
          else { idx = verts.length; map[key] = idx; verts.push(key); note = 'NEW'; }
          corners.push(idx);
          ++totalCorners;
          report.push('  ' + pad(toks[i], 9) + ' -> (p ' + p + ', t ' + fmt(t) + ', n ' + fmt(nn) + ')'
                      + '  -> vertex ' + idx + '  ' + note);
        }
        if (failed) { break; }

        if (corners.length < 3) {
          failed = 'line ' + (L + 1) + ': a face needs at least three corners; this has ' + corners.length;
          break;
        }
        var tris = [];
        for (var k = 2; k < corners.length; ++k) {
          var a = corners[0], b = corners[k - 1], c = corners[k];
          var bad = (a === b || b === c || a === c);
          tris.push('(' + a + ',' + b + ',' + c + ')' + (bad ? ' DROPPED' : ''));
          if (bad) { ++dropped; } else { ++triangles; }
        }
        report.push('  fan -> ' + tris.join('  '));
        report.push('');
      }

      if (failed) {
        out.textContent = 'REJECTED: ' + failed
          + '\n\nThe loader stops here and reports the line number. A file that is\nmalformed is not a file to guess about.';
        return;
      }
      out.textContent = report.join('\n')
        + totalCorners + ' face corners  ->  ' + verts.length + ' vertices'
        + '   (' + reused + ' reused, ' + (verts.length - new Set(verts.map(function (k) { return k.split(',')[0]; })).size) + ' splits)'
        + '\n' + triangles + ' triangle' + (triangles === 1 ? '' : 's')
        + (dropped ? '   (' + dropped + ' degenerate, dropped)' : '');
    }
    function pad(s, n) { while (s.length < n) { s += ' '; } return s; }
    function fmt(v) { return v < 0 ? '-' : String(v); }

    document.getElementById('w35-in').addEventListener('input', run);
    var btns = document.querySelectorAll('.w35-eg');
    for (var i = 0; i < btns.length; ++i) {
      btns[i].addEventListener('click', function () {
        document.getElementById('w35-in').value = this.getAttribute('data-v');
        run();
      });
    }
    run();
  })();
  </script>

  <h3 id="theory-index">3.3 The index problem, derived</h3>

  <p>
    Now the central result. We have a file that stores three independent streams and faces
    whose corners index them separately; we need one array of vertices and one array of
    indices. What exactly is a vertex?
  </p>

  <p>
    Work backwards from the constraint. Downstream — in our rasterizer today, in a GPU vertex
    buffer in Module 4 — one index selects one vertex, and that vertex carries every attribute
    at once. So two face corners may share a vertex <strong>if and only if</strong> everything
    the vertex carries is the same for both. If the two corners want different texture
    coordinates, or different normals, they cannot be the same slot, no matter that their
    position is identical.
  </p>

  <p>That gives the rule directly:</p>

  <div class="eq">
    <span class="katex-src">\[ \text{vertex} \;\equiv\; (\,i_v,\; i_{vt},\; i_{vn}\,) \]</span>
    <span class="eq-plain">a vertex is the TRIPLE (position index, uv index, normal index)</span>
  </div>

  <p>
    In prose: <em>a vertex is identified by the whole triple of attribute indices, not by its
    position index alone.</em> Two corners are the same vertex when all three components
    match; otherwise they are different vertices, even when they occupy the same point in
    space. The number of vertices in the output is therefore <strong>the number of distinct
    triples appearing across all the file's faces</strong> — a quantity that cannot be known
    without reading every face.
  </p>

  <div class="worked">
    <span class="label">Worked example — counting <code>assets/cube.obj</code> by hand</span>
    <p>
      The file has 8 positions, 4 texture coordinates, 6 normals, and 6 quad faces. Six faces
      of four corners each is <strong>24 corner tokens</strong>. Are any two of them the same
      triple?
    </p>
    <p>Take position 1, the corner at <code>(−0.5, −0.5, −0.5)</code>. Search the file for it:</p>
    <ul>
      <li><code>f <strong>1/1/1</strong> 4/4/1 3/3/1 2/2/1</code> — the −z face</li>
      <li><code>f <strong>1/1/3</strong> 5/2/3 8/3/3 4/4/3</code> — the −x face</li>
      <li><code>f <strong>1/1/6</strong> 2/2/6 6/3/6 5/4/6</code> — the −y face</li>
    </ul>
    <p>
      Three appearances, three triples: <code>(1,1,1)</code>, <code>(1,1,3)</code>,
      <code>(1,1,6)</code>. Same position, <em>same texture coordinate</em>, three different
      normals — so three different vertices. Exactly Figure 2's three corners of paper.
    </p>
    <p>
      Every position of a cube belongs to three faces, and no two faces of a cube share a
      normal, so the pattern repeats: <strong>8 × 3 = 24 distinct triples</strong>, and no
      token is ever a repeat of another. The loader reports precisely this —
      <code>vertices = 24</code>, <code>split_vertices = +16</code>,
      <code>reused_corners = 0</code>.
    </p>
    <p>
      Then triangulation happens <em>after</em>: each quad becomes two triangles, so 12
      triangles with 36 triangle corners — drawn from those same 24 vertices. The 12 extra
      uses are the shared diagonals, and they cost no storage at all, which is exactly what
      the index array is for.
    </p>
  </div>

  <p>
    Figure 3 shows the transformation as a memory layout, which is the way to hold it in your
    head once the counting makes sense.
  </p>

  <!-- ---- FIGURE 3 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 380" role="img" aria-labelledby="fig3-t fig3-d">
      <title id="fig3-t">From three index streams to one: the de-duplicating map</title>
      <desc id="fig3-d">Top: a row of face corner tokens such as 1/1/1, 4/4/1, 1/1/3. Middle:
        a hash map box keyed by the triple, mapping to a vertex number. Bottom: the resulting
        parallel arrays of positions, uvs and normals, all the same length, plus an index
        array. Arrows show the first appearance of a triple creating a new vertex and a repeat
        appearance reusing one.</desc>

      <defs>
        <marker id="f3-ah" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink-soft)"/></marker>
        <marker id="f3-hi" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/></marker>
      </defs>

      <!-- corner tokens -->
      <text x="20" y="26" class="sm" font-weight="700">face corners, in the order the file lists them</text>
      <g class="ink" fill="none" stroke-width="1.2">
        <rect x="20" y="36" width="76" height="28" rx="4"/>
        <rect x="102" y="36" width="76" height="28" rx="4"/>
        <rect x="184" y="36" width="76" height="28" rx="4"/>
        <rect x="266" y="36" width="76" height="28" rx="4"/>
        <rect x="348" y="36" width="76" height="28" rx="4"/>
      </g>
      <text x="58" y="55" class="mono sm" text-anchor="middle">1/1/1</text>
      <text x="140" y="55" class="mono sm" text-anchor="middle">4/4/1</text>
      <text x="222" y="55" class="mono sm" text-anchor="middle">3/3/1</text>
      <text x="304" y="55" class="mono sm" text-anchor="middle">2/2/1</text>
      <text x="386" y="55" class="mono sm" text-anchor="middle">1/1/3</text>
      <text x="440" y="55" class="sm muted">…</text>

      <!-- the map -->
      <rect x="150" y="110" width="400" height="104" rx="8" class="fill-soft hi" stroke-width="1.6"/>
      <text x="350" y="132" class="sm" text-anchor="middle" font-weight="700">unordered_map&lt;(p, t, n), uint16&gt;</text>
      <text x="170" y="156" class="mono sm">(0, 0, 0)  -&gt;  0</text>
      <text x="170" y="176" class="mono sm">(3, 3, 0)  -&gt;  1</text>
      <text x="170" y="196" class="mono sm">(2, 2, 0)  -&gt;  2</text>
      <text x="390" y="156" class="mono sm">(1, 1, 0)  -&gt;  3</text>
      <text x="390" y="176" class="mono sm">(0, 0, 2)  -&gt;  4</text>
      <text x="390" y="196" class="xs muted">…zero-based, after resolution</text>

      <g class="ink-soft" stroke-width="1.4" fill="none" marker-end="url(#f3-ah)">
        <path d="M58,68 C58,90 200,88 240,106"/>
        <path d="M386,68 C386,88 420,92 430,106"/>
      </g>
      <text x="574" y="150" class="xs muted">first time a triple</text>
      <text x="574" y="164" class="xs muted">is seen: NEW vertex,</text>
      <text x="574" y="178" class="xs muted">numbered in order</text>

      <!-- output arrays -->
      <text x="20" y="252" class="sm" font-weight="700">the mesh we build — three parallel arrays, one index array</text>

      <g class="ink" fill="none" stroke-width="1.2">
        <rect x="20" y="262" width="44" height="24" rx="3"/><rect x="64" y="262" width="44" height="24" rx="3"/>
        <rect x="108" y="262" width="44" height="24" rx="3"/><rect x="152" y="262" width="44" height="24" rx="3"/>
        <rect x="196" y="262" width="44" height="24" rx="3"/>
        <rect x="20" y="290" width="44" height="24" rx="3"/><rect x="64" y="290" width="44" height="24" rx="3"/>
        <rect x="108" y="290" width="44" height="24" rx="3"/><rect x="152" y="290" width="44" height="24" rx="3"/>
        <rect x="196" y="290" width="44" height="24" rx="3"/>
        <rect x="20" y="318" width="44" height="24" rx="3"/><rect x="64" y="318" width="44" height="24" rx="3"/>
        <rect x="108" y="318" width="44" height="24" rx="3"/><rect x="152" y="318" width="44" height="24" rx="3"/>
        <rect x="196" y="318" width="44" height="24" rx="3"/>
      </g>
      <text x="248" y="279" class="sm muted">vertices[]  — positions</text>
      <text x="248" y="307" class="sm muted">uvs[]       — same length</text>
      <text x="248" y="335" class="sm muted">normals[]   — same length</text>
      <text x="42" y="279" class="mono xs" text-anchor="middle">0</text>
      <text x="86" y="279" class="mono xs" text-anchor="middle">1</text>
      <text x="130" y="279" class="mono xs" text-anchor="middle">2</text>
      <text x="174" y="279" class="mono xs" text-anchor="middle">3</text>
      <text x="218" y="279" class="mono xs" text-anchor="middle">4</text>

      <rect x="470" y="262" width="210" height="52" rx="6" class="fill-soft ink-soft" stroke-width="1.2"/>
      <text x="575" y="282" class="mono sm" text-anchor="middle">indices: 0,1,2  0,2,3  …</text>
      <text x="575" y="302" class="xs muted" text-anchor="middle">the fan, in vertex numbers</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 3.</span>
      The translation. Each face corner is looked up by its whole triple; a first appearance
      creates a vertex and a repeat reuses one. The output is three <em>index-parallel</em>
      arrays — <code>vertices[k]</code>, <code>uvs[k]</code> and <code>normals[k]</code>
      belong together — plus an index array naming them in triples.
    </figcaption>
  </figure>

  <h3 id="theory-map">3.4 The de-duplicating map</h3>

  <p>
    "Distinct triples, numbered in order of first appearance" is a specification, and it has
    exactly one natural implementation: a hash map from the triple to the index we assigned
    it. For each corner: look it up; if present, reuse; if absent, append a new vertex and
    record the new index.
  </p>

  <p>
    Two properties of that scheme are worth pausing on, because both are load-bearing.
  </p>

  <p>
    <strong>Order of first appearance is not arbitrary — it is what makes the result
    reproducible.</strong> Any numbering of the distinct triples would produce correct
    geometry, so it is tempting to think the choice does not matter. It matters for testing:
    load the same file twice and you must get the same arrays, bit for bit, or you cannot
    compare a load against a reference. First-appearance order is deterministic, does not
    depend on the hash function, and does not depend on the map's iteration order — which for
    <code>std::unordered_map</code> is unspecified and genuinely varies between standard
    library implementations.
  </p>

  <p>
    <strong>The cost is one hash lookup per face corner.</strong> A mesh with
    <em>C</em> corners does <em>C</em> lookups and at most <em>C</em> insertions, so loading
    is linear in the size of the file with a constant that is entirely down to the hash.
    That constant is worth caring about: our torus does 6,912 lookups, and a bad hash turns
    the map into a linked list.
  </p>

  <div class="callout note">
    <span class="label">Why hash the triple at all, rather than sort?</span>
    <p>
      You could collect every corner, sort by the triple, and assign indices to runs of
      equals: <em>O(C log C)</em> instead of <em>O(C)</em>, no hash function to design, and
      the sort is cache-friendly in a way hashing is not. That is a genuinely reasonable
      alternative, and it is how some production importers do it — but it needs a second pass
      to restore the original corner order, and it changes the numbering unless you sort
      stably by first appearance. We take the map because it does the job in one pass over the
      file, which is also the only pass we get for negative-index resolution (§3.2).
    </p>
  </div>

  <h3 id="theory-fan">3.5 Triangulating a face, and what a fan assumes</h3>

  <p>
    OBJ faces may have any number of corners ≥ 3. Quads are overwhelmingly the common case —
    every subdivision-surface modelling workflow produces them — and n-gons appear whenever
    somebody caps a cylinder. Our rasterizer draws triangles and only triangles, so faces have
    to be cut up.
  </p>

  <p>
    The cheapest cut is a <strong>fan</strong>: pick corner 0 and join it to every consecutive
    pair. For a polygon with <em>n</em> corners:
  </p>

  <div class="eq">
    <span class="katex-src">\[ (0,\,k-1,\,k) \quad\text{for } k = 2 \ldots n-1 \qquad\Rightarrow\qquad n-2 \text{ triangles} \]</span>
    <span class="eq-plain">triangles (0, k-1, k) for k = 2 to n-1, giving n-2 triangles</span>
  </div>

  <p>
    In prose: <em>anchor at the first corner and sweep round, emitting one triangle per step;
    an n-corner polygon becomes n−2 triangles.</em> A quad gives 2, a pentagon 3. You have
    seen this exact loop before — Lesson 3.3's near-plane clipper fans its output polygon the
    same way, for the same reason.
  </p>

  <p>
    And it makes the same assumption, which this time deserves to be said out loud rather than
    inherited. It is usually quoted as "a fan needs a convex polygon", and that is true without
    being the tightest statement. What a fan actually needs is this:
  </p>

  <div class="callout note">
    <span class="label">The precise condition</span>
    <p>
      A fan from corner 0 is correct <strong>if and only if corner 0 can see the whole
      polygon</strong> — every interior point joined to corner 0 by a segment that stays inside.
      A shape with that property is called <em>star-shaped</em> about that point.
    </p>
    <p>
      Convexity is the <em>sufficient</em> condition everyone remembers, and it is sufficient
      precisely because in a convex polygon <strong>every</strong> corner sees everything, so any
      anchor works. A concave polygon may still fan perfectly well — from the right corner. Which
      also means the failure depends on where the exporter happened to start listing the face,
      and that is exactly the sort of dependency that makes a bug appear in one file and not the
      next one that looks just like it.
    </p>
  </div>

  <p>Figure 4 shows both halves of that.</p>

  <!-- ---- FIGURE 4 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 270" role="img" aria-labelledby="fig4-t fig4-d">
      <title id="fig4-t">Fan triangulation: exact from any corner of a convex polygon, wrong from the wrong corner of an L</title>
      <desc id="fig4-d">Left: a convex pentagon with the fan drawn from corner zero, all three
        triangles lying inside the shape. Right: an L-shaped hexagon whose corners are numbered
        starting from the top right, with the fan drawn from that corner. Two of its four
        triangles are shaded and visibly stick out past the L's inner edge into empty space,
        because corner zero cannot see the L's lower leg.</desc>

      <!-- LEFT: convex -->
      <g transform="translate(40,20)">
        <text x="120" y="10" class="sm" text-anchor="middle" font-weight="700">convex — every corner sees everything</text>
        <polygon points="40,60 130,40 200,90 170,180 60,170" class="fill-soft ink" stroke-width="1.8"/>
        <g class="ink-soft" stroke-width="1.2" fill="none">
          <line x1="40" y1="60" x2="200" y2="90"/>
          <line x1="40" y1="60" x2="170" y2="180"/>
        </g>
        <circle cx="40" cy="60" r="4.5" fill="var(--dia-hi)"/>
        <text x="18" y="52" class="mono xs t-hi">0</text>
        <text x="132" y="32" class="mono xs muted">1</text>
        <text x="208" y="92" class="mono xs muted">2</text>
        <text x="176" y="196" class="mono xs muted">3</text>
        <text x="46" y="188" class="mono xs muted">4</text>
        <text x="120" y="228" class="xs muted" text-anchor="middle">3 triangles, all inside — any anchor would do</text>
      </g>

      <line x1="352" y1="30" x2="352" y2="240" class="ink-soft" stroke-width="1" stroke-dasharray="4 4"/>

      <!-- RIGHT: an L, anchored at the corner that cannot see its lower leg -->
      <g transform="translate(390,20)">
        <text x="115" y="10" class="sm" text-anchor="middle" font-weight="700">concave — this anchor cannot</text>

        <!-- the two escaping fan triangles, drawn UNDER the outline so the parts
             that stick out past the L's edge are the visible ones -->
        <polygon points="190,40 100,100 100,180" fill="var(--warn-bg)" stroke="var(--warn-bd)" stroke-width="1.2"/>
        <polygon points="190,40 100,180 40,180" fill="var(--warn-bg)" stroke="var(--warn-bd)" stroke-width="1.2"/>

        <polygon points="190,40 190,100 100,100 100,180 40,180 40,40" class="ink" fill="none" stroke-width="2.2"/>

        <g class="ink-soft" stroke-width="1.2" fill="none">
          <line x1="190" y1="40" x2="100" y2="100"/>
          <line x1="190" y1="40" x2="100" y2="180"/>
          <line x1="190" y1="40" x2="40" y2="180"/>
        </g>
        <circle cx="190" cy="40" r="4.5" fill="var(--dia-hi)"/>
        <text x="196" y="34" class="mono xs t-hi">0</text>
        <text x="196" y="110" class="mono xs muted">1</text>
        <text x="90" y="88" class="mono xs muted" text-anchor="end">2</text>
        <text x="92" y="198" class="mono xs muted" text-anchor="end">3</text>
        <text x="34" y="198" class="mono xs muted" text-anchor="end">4</text>
        <text x="34" y="34" class="mono xs muted" text-anchor="end">5</text>
        <text x="115" y="228" class="xs muted" text-anchor="middle">(0,2,3) and (0,3,4) both escape</text>
      </g>
    </svg>
    <figcaption>
      <span class="fignum">Figure 4.</span>
      A fan anchored at corner 0. On the convex pentagon every triangle lies inside the
      boundary, and would from any other corner too. On the L, corner 0 is in the top-right arm
      and <em>cannot see</em> the lower leg — so the triangles <code>(0,2,3)</code> and
      <code>(0,3,4)</code> reach across the notch and cover area the polygon does not (shaded,
      sticking out past the L's inner edge). Anchor the very same L at corner 5 instead and
      the fan is exact. Quads from a modelling tool are reliably convex; hand-authored n-gons
      are not.
    </figcaption>
  </figure>

  <p>
    We take the fan anyway, and say so. The robust answer is <strong>ear clipping</strong>,
    which repeatedly finds a corner whose triangle is inside the polygon and contains no other
    corner, snips it off, and repeats. It needs the polygon's plane (to work in 2D), a
    point-in-triangle test, and a convexity test per corner — perhaps eighty lines. It belongs
    with Module 5's asset pipeline, alongside tolerance-based welding and tangent generation,
    because those all want the same infrastructure. Exercise 3.5.5 is the version for the
    student who wants it now.
  </p>

  <h3 id="theory-euler">3.6 Euler's formula is a statement about spheres</h3>

  <p>
    Lesson 2.12 checked the icosahedron with <code>V − E + F = 2</code> and called it Euler's
    formula. That was true and incomplete, and the incompleteness is about to matter, because
    the model we are going to load is a torus.
  </p>

  <p>
    The full statement is about the <strong>Euler characteristic</strong> χ of a surface, which
    is a topological invariant — a number that does not change when you bend, stretch, or
    subdivide, only when you change the surface's <em>kind</em>:
  </p>

  <div class="eq">
    <span class="katex-src">\[ \chi \;=\; V - E + F \;=\; 2 - 2g \]</span>
    <span class="eq-plain">chi = V - E + F = 2 - 2g, where g is the number of holes (the genus)</span>
  </div>

  <p>
    In prose: <em>vertices minus edges plus faces equals two, less two for every hole through
    the surface.</em> A sphere, a cube and an icosahedron all have no holes, so χ = 2. A torus
    has one hole, so χ = 0. A two-holed pretzel gives −2.
  </p>

  <p>
    The invariance under subdivision is the part worth checking by hand, because it is the
    reason χ is a fact about the shape and not about the mesh.
  </p>

  <div class="worked">
    <span class="label">Worked example — a cube, before and after triangulation</span>
    <p>
      <strong>As six square faces:</strong> V = 8, E = 12, F = 6.
      χ = 8 − 12 + 6 = <strong>2</strong>.
    </p>
    <p>
      <strong>Triangulated:</strong> each square gains a diagonal, so F doubles to 12 and E
      gains one per face, 12 + 6 = 18. V is unchanged at 8.
      χ = 8 − 18 + 12 = <strong>2</strong>.
    </p>
    <p>
      Cutting a face in half added one face and one edge; those cancel in the alternating sum.
      That is exactly why χ survives triangulation, and why our validator can compute it on
      the triangle mesh and still be talking about the cube. Measured on the loaded
      <code>cube.obj</code>: V = 8 welded, E = 18, F = 12, χ = 2.
    </p>
    <p>
      <strong>And the torus:</strong> our 48 × 24 mesh welds to V = 1,152 positions with
      F = 2,304 triangles. Every edge is shared by exactly two triangles, so
      E = 3F⁄2 = 3,456. Then χ = 1,152 − 3,456 + 2,304 = <strong>0</strong> — measured, and
      exactly the genus-1 prediction.
    </p>
  </div>

  <div class="callout pitfall">
    <span class="label">Do not assert χ = 2 in a loader</span>
    <p>
      It is a very natural thing to write, and it rejects every torus, every teacup with a
      handle, every chain link and every pair of glasses. χ is a <em>diagnostic</em>, not a
      validity condition: report it, and let the caller decide what shape it was expecting.
      The conditions that really are errors for a renderer are different ones — boundary
      edges, non-manifold edges, inconsistent winding — and §3.7 and the validator below
      handle those separately.
    </p>
  </div>

  <h3 id="theory-volume">3.7 Signed volume, and what "wound outward" means</h3>

  <p>
    Lesson 3.4 built back-face culling on a promise: our triangles are wound counter-clockwise
    <em>seen from outside</em>. Lesson 2.12 checked that promise on the icosahedron by taking
    each face's normal and dotting it against the vector from the centroid — positive means
    outward. That test is fine for the icosahedron and wrong in general, because it assumes
    the shape is <em>star-shaped</em> about its centroid: that a ray from the centre hits the
    surface exactly once. A torus fails that immediately — its centroid is in the hole, and a
    ray through the tube hits the surface twice.
  </p>

  <p>
    There is a test with no such assumption, and it is one of the prettiest results in
    computational geometry. Take any triangle of the mesh and form a tetrahedron with the
    <strong>origin</strong>. Its signed volume is one sixth of a determinant:
  </p>

  <div class="eq">
    <span class="katex-src">\[ V_{\text{tet}} \;=\; \tfrac{1}{6}\,\det[\mathbf{a}, \mathbf{b}, \mathbf{c}] \;=\; \tfrac{1}{6}\,\mathbf{a} \cdot (\mathbf{b} \times \mathbf{c}) \]</span>
    <span class="eq-plain">V_tet = det[a, b, c] / 6 = dot(a, cross(b, c)) / 6</span>
  </div>

  <p>
    In prose: <em>the signed volume of the tetrahedron from the origin to a triangle is a
    sixth of the scalar triple product of its three corners.</em> That is the same triple
    product Lesson 3.4 met from the other side — there it decided facing, here it measures
    volume, and it being the same quantity is not a coincidence.
  </p>

  <p>
    Now sum it over every triangle of a closed surface. Figure 5 shows why the sum collapses
    to the enclosed volume.
  </p>

  <!-- ---- FIGURE 5 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 340" role="img" aria-labelledby="fig5-t fig5-d">
      <title id="fig5-t">Summing signed tetrahedra gives the enclosed volume</title>
      <desc id="fig5-d">A four-sided solid shown in cross-section, with the origin marked to
        its left, outside it. A large green cone is drawn from the origin to the solid's far
        face, reaching past the solid; a smaller amber cone is drawn from the origin to the
        near face, and it lies entirely inside the green one. The near face turns toward the
        origin so its contribution is negative and the far face's is positive; subtracting the
        amber region from the green one leaves exactly the solid's cross-section. Labels mark
        the amber region as cancelling and the remaining region as the solid.</desc>

      <!-- The FAR cone: origin to the far face. Positive, and it reaches past the solid. -->
      <polygon points="110,160 520,62 520,297" fill="var(--ok-bg)" stroke="var(--ok-bd)"
               stroke-width="1.3"/>
      <!-- The NEAR cone: origin to the near face. Negative, and — because both faces
           subtend the SAME angle from the origin — it sits exactly inside the far one. -->
      <polygon points="110,160 320,110 320,230" fill="var(--warn-bg)" stroke="var(--warn-bd)"
               stroke-width="1.3"/>

      <!-- the solid itself, over both -->
      <polygon points="320,110 520,62 520,297 320,230" class="ink" fill="none" stroke-width="2.2"/>

      <!-- origin -->
      <circle cx="110" cy="160" r="5" fill="var(--dia-hi)"/>
      <text x="96" y="152" class="sm t-hi" font-weight="700" text-anchor="end">O</text>

      <text x="212" y="166" class="xs" text-anchor="middle" font-weight="700">cancels</text>
      <text x="420" y="188" class="sm muted" text-anchor="middle">the solid</text>

      <!-- legend, in its own clear band below everything -->
      <rect x="40" y="292" width="14" height="14" fill="var(--ok-bg)" stroke="var(--ok-bd)" stroke-width="1.2"/>
      <text x="62" y="303" class="xs">far face → positive</text>
      <rect x="210" y="292" width="14" height="14" fill="var(--warn-bg)" stroke="var(--warn-bd)" stroke-width="1.2"/>
      <text x="232" y="303" class="xs">near face → negative</text>

      <text x="350" y="330" class="sm" text-anchor="middle" font-weight="700">green minus amber is exactly the solid — the outside cancels, the interior survives</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 5.</span>
      Why the signed sum works, in cross-section. Each triangle contributes a signed cone from
      the origin. Space outside the solid is swept an even number of times with opposite
      signs and cancels exactly; space inside is swept once. The origin's position is
      irrelevant — which is what makes this test assumption-free, unlike the centroid test.
    </figcaption>
  </figure>

  <p>
    The sign is the payoff. If every face is wound counter-clockwise <em>seen from outside</em>,
    the sum comes out <strong>positive</strong> and equals the enclosed volume. Reverse the
    winding of the whole mesh and every determinant flips, so the sum is exactly the negative
    of the volume. Reverse only <em>some</em> faces and you get a number in between, which is
    a genuinely useful signal.
  </p>

  <div class="worked">
    <span class="label">Worked example — the unit cube, and one flipped face</span>
    <p>
      Our cube is centred on the origin with sides of length 1, so the origin is inside it and
      each face's tetrahedron is a pyramid on a 1 × 1 base with apex at the centre. Its height
      is ½ — the distance from the centre to a face — so its volume is
      <code>base × height / 3 = 1 × 0.5 / 3 = 1/6</code>. Six faces:
      <code>6 × 1/6 = 1</code>. And the mesh reports <code>signed_volume = +1.000000</code>,
      to the last digit printed.
    </p>
    <p>
      Now <code>assets/twisted.obj</code>: the same cube with its <code>+y</code> face listed
      in reverse. That face's contribution flips from <code>+1/6</code> to <code>−1/6</code>, a
      change of <code>−2/6 = −1/3</code>, predicting <code>1 − 1/3 = 0.6667</code>.
      Measured: <code>+0.666667</code>. The validator also reports
      <code>reversed_edges = 4</code> — the flipped quad's four outer edges now disagree with
      their neighbours, while the diagonal <em>between</em> its own two triangles still
      agrees, because both of them flipped together.
    </p>
    <p>
      And the torus, where there is a closed form to check against:
      <code>V = 2π²Rr²</code>. With <code>R = 1</code> and <code>r = 0.4</code> that is
      <code>2 × 9.8696 × 0.16 = 3.158274</code>. Our 48 × 24 mesh measures
      <strong>3.113410</strong> — 1.42% low, because a polyhedron inscribed in a curved solid
      always undershoots. Doubling the resolution quarters the error
      (12.27% → 3.18% → 0.80% → 0.09%), which is the second-order convergence you should
      expect from approximating a smooth surface with flat pieces.
    </p>
  </div>

  <h3 id="theory-weld">3.8 Welding, and the ulp that hides a seam</h3>

  <p>
    One last piece of theory, and it is the one that would have quietly broken everything
    above.
  </p>

  <p>
    §3.3 established that a position may legitimately be stored several times. That means the
    <em>vertex array</em> is not the surface's set of points — several array entries can be
    the same point. Topology is a property of the surface, so every question in §3.6 and §3.7
    has to be asked of the <strong>welded</strong> mesh: merge entries with identical
    positions first, then count edges.
  </p>

  <p>
    Ask them of the raw arrays instead and a perfectly watertight model reports a hole. Figure
    6 is our torus, unrolled, showing exactly where the duplicates come from and why they must
    weld.
  </p>

  <!-- ---- FIGURE 6 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 300" role="img" aria-labelledby="fig6-t fig6-d">
      <title id="fig6-t">The torus's uv seam: the same points, stored twice</title>
      <desc id="fig6-d">A rectangle representing the torus unrolled into u and v parameter
        space, with a grid. The left and right edges are marked as the same circle of points
        on the solid, but carrying u equals zero and u equals one respectively. A note says
        the two columns hold identical positions and different texture coordinates, so they
        must be stored separately and welded before topology is measured.</desc>

      <!-- unrolled grid -->
      <g class="grid" stroke-width="1">
        <path d="M120,60 H520 M120,100 H520 M120,140 H520 M120,180 H520 M120,220 H520"/>
        <path d="M160,60 V220 M200,60 V220 M240,60 V220 M280,60 V220 M320,60 V220 M360,60 V220 M400,60 V220 M440,60 V220 M480,60 V220"/>
      </g>
      <rect x="120" y="60" width="400" height="160" class="ink" fill="none" stroke-width="1.8"/>

      <!-- the two seam columns -->
      <line x1="120" y1="60" x2="120" y2="220" class="hi" stroke-width="4"/>
      <line x1="520" y1="60" x2="520" y2="220" class="hi" stroke-width="4"/>

      <text x="320" y="46" class="sm" text-anchor="middle" font-weight="700">the torus, unrolled into (u, v)</text>

      <text x="120" y="240" class="mono sm t-hi" text-anchor="middle">u = 0</text>
      <text x="520" y="240" class="mono sm t-hi" text-anchor="middle">u = 1</text>
      <text x="320" y="240" class="xs muted" text-anchor="middle">u increases →</text>

      <text x="96" y="150" class="sm muted" text-anchor="end">v</text>

      <text x="320" y="268" class="sm" text-anchor="middle" font-weight="700">the two highlighted columns are the SAME 24 points of the solid</text>
      <text x="320" y="286" class="sm" text-anchor="middle">— identical positions, different u. Stored twice; welded before counting.</text>

      <!-- little torus glyph -->
      <g transform="translate(600,120)">
        <ellipse cx="0" cy="0" rx="52" ry="26" class="ink" fill="none" stroke-width="1.8"/>
        <ellipse cx="0" cy="0" rx="20" ry="9" class="ink-soft" fill="none" stroke-width="1.4"/>
        <!-- The seam is a circle AROUND THE TUBE — drawn at the ring's 9 o'clock,
             where it spans exactly from the outer silhouette to the inner one. -->
        <ellipse cx="-36" cy="0" rx="16" ry="9" class="hi" fill="none" stroke-width="3"/>
        <text x="0" y="48" class="xs muted" text-anchor="middle">one circle round the tube</text>
      </g>
    </svg>
    <figcaption>
      <span class="fignum">Figure 6.</span>
      The torus's seam. Going once around, <code>u</code> must run 0 → 1 and then be 0 again,
      and one vertex cannot hold both values — so the first column of vertices is stored a
      second time at the end. Their positions are identical; only <code>u</code> differs.
      Measured: 1,225 stored vertices, 1,152 distinct positions, <strong>73 splits</strong> —
      which is <code>nu + nv + 1 = 48 + 24 + 1</code>, one extra column, one extra row, and
      the corner they share.
    </figcaption>
  </figure>

  <p>
    Welding by exact equality is the right rule <em>here</em>, and it is worth being precise
    about why, because it is not obviously enough. The two copies of a seam vertex did not
    arrive independently — they were produced by the same computation from the same inputs.
    So they are bit-identical, and exact matching finds them. A tolerance would be needed for
    positions that came from different computations (two objects modelled separately and
    joined by eye), which is a different problem with a threshold nobody can choose correctly
    for all models, and it belongs to Module 5.
  </p>

  <p>
    But "produced by the same computation" is a condition you have to <em>arrange</em>, and it
    is astonishingly easy to lose.
  </p>

  <div class="callout warn">
    <span class="label">The bug this lesson nearly shipped</span>
    <p>
      The obvious way to generate the seam column is to compute its angle from its texture
      coordinate: the last column has <code>u = 1</code>, so its angle is
      <code>1.0f × 2π</code>. The first column has <code>u = 0</code>, so its angle is
      <code>0</code>. Mathematically the same place. In <code>float</code>:
    </p>
    <ul>
      <li><code>sin(0.0f)</code> = <strong>0</strong></li>
      <li><code>sin(1.0f × 6.28318530718f)</code> = <strong>1.74845553e−07</strong></li>
    </ul>
    <p>
      A float cannot hold 2π exactly, so the two columns land 1.7 × 10⁻⁷ apart. Nothing
      renders differently — you would never see it. But no welder recognises them, so the
      seam becomes a <strong>boundary</strong>: a watertight torus reports 48 boundary edges,
      χ comes out wrong, and back-face culling is reported unsafe on a mesh that is perfectly
      closed.
    </p>
    <p>
      The fix is one line: compute the angle from the <em>wrapped index</em>,
      <code>i % nu</code>, so the last column literally reuses the first column's angle. Then
      the two positions are the same number, not merely the same point. This is measured in
      <code>verify_35.cpp</code> §E, and it is the kind of bug that costs a day if you meet it
      for the first time in a model somebody sent you.
    </p>
  </div>
"""
