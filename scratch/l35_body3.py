# Lesson 3.5 body — part 3: implementation, build/run, pitfalls, exercises, recap.
BODY3 = r"""
  <!-- =================================================================
       SECTION 5 — IMPLEMENTATION
       ================================================================= -->
  <h2 id="implementation"><span class="num">4</span>Implementation</h2>

  <p>
    Four files. <code>mesh.hpp</code> grows an owning type and a validator;
    <code>mesh.cpp</code> is new and holds their implementations; <code>obj.hpp</code> and
    <code>obj.cpp</code> are the loader. We build them in the order the problems arrive.
  </p>

  <h3 id="impl-owning">4.1 Geometry that owns itself</h3>

  <p>
    Before a single line of parsing, there is a question we have been able to duck since
    Lesson 2.12 and cannot duck any longer: <strong>who owns the arrays?</strong>
  </p>

  <p>
    <code>engine::mesh</code> is two — now four — <code>std::span</code>s. A span is a pointer
    and a length: it <em>views</em> memory it does not own. That was exactly right while every
    mesh was an <code>inline constexpr</code> array with program lifetime, because such a view
    can never dangle. Lesson 3.2 already felt the strain when the tessellated floor needed
    somewhere to live and got a bag of <code>std::vector</code>s called
    <code>floor_geometry</code>; the lesson admitted at the time that the awkwardness was the
    pressure that would eventually produce a real type. Here it is.
  </p>

  <!--EXC:mesh_data-->

  <div class="callout cpp">
    <span class="label">C++ in place — owner types and view types</span>
    <p>
      C++ draws this distinction everywhere once you start looking for it:
      <code>std::string</code> owns and <code>std::string_view</code> views;
      <code>std::vector</code> owns and <code>std::span</code> views;
      <code>mesh_data</code> owns and <code>mesh</code> views. The pairing is deliberate. An
      owner manages a resource's lifetime; a view is a cheap, copyable handle for code that
      only reads and does not care where the bytes came from — which is why every function in
      this engine that <em>draws</em> geometry takes a <code>mesh</code>, and works equally on
      compiled-in arrays and on a file loaded a moment ago.
    </p>
    <p>
      <strong>The rule that comes with it: a view must not outlive its owner.</strong> Nothing
      in the type system enforces that. Write a function that builds a <code>mesh_data</code>
      locally and returns <code>data.view()</code> and it compiles cleanly and hands you a
      dangling pointer. That is why <code>load_obj</code> fills an out-parameter the caller
      owns rather than returning geometry: the shape of the API removes the mistake.
    </p>
    <p>
      Notice also what <code>mesh_data</code> does <em>not</em> have — no destructor, no copy
      constructor, no assignment operator. It holds four <code>std::vector</code>s, each of
      which manages its own memory correctly, so the compiler-generated versions are all
      right. That is RAII working as intended: own resources through members that already
      know how to clean up, and you write no cleanup code and cannot leak. The moment you
      write one of the five special members by hand, you owe all of them.
    </p>
  </div>

  <h3 id="impl-text">4.2 Reading numbers out of text</h3>

  <p>
    The parser walks the buffer a line at a time and each line token by token. That part is
    unremarkable. The number conversion is not, and it is worth two paragraphs because it is
    where a loader most easily starts lying.
  </p>

  <!--EXC:to_float-->

  <p>
    Three decisions in twelve lines. <strong>The whole token, or nothing:</strong>
    <code>strtof("1.0abc", &amp;stop)</code> returns 1.0 quite happily and points
    <code>stop</code> at the <code>a</code>. Accepting that means a typo in a model file
    silently becomes geometry. We compare <code>stop</code> against the token's end and reject
    anything short.
  </p>

  <p>
    <strong>Finite, or nothing.</strong> A value like <code>1e400</code> overflows to infinity,
    and an infinity in a position propagates into NaNs through the whole pipeline — Lesson 3.3
    had to add three guards against exactly that after <code>std::clamp</code> turned out to
    be unable to remove a NaN. Refusing it at the boundary is cheaper than guarding
    everywhere downstream.
  </p>

  <div class="callout verify">
    <span class="label">Checked, not assumed — why not <code>SDL_strtod</code>?</span>
    <p>
      SDL has its own string-to-double conversion and it would have been the natural choice
      for a program that already links SDL. Reading the header says otherwise —
      <code>SDL3/SDL_stdinc.h</code> documents <code>SDL_strtod</code> as making <em>fewer</em>
      guarantees than the C runtime's: "Only decimal notation is guaranteed to be supported.
      The handling of scientific and hexadecimal notation is unspecified." Exporters emit
      <code>1.0e-5</code> constantly. So the C runtime's <code>std::strtof</code> it is.
    </p>
  </div>

  <div class="callout pitfall">
    <span class="label">The locale trap, and the modern fix</span>
    <p>
      <code>std::strtof</code> reads the decimal separator through the C locale's
      <code>LC_NUMERIC</code>. On a machine configured for a locale that writes
      <code>1,5</code>, a program that has called <code>setlocale(LC_ALL, "")</code> will parse
      <code>"1.5"</code> as <strong>1</strong> and drop the fraction — silently, for every
      number in every asset. This is one of the classic asset-pipeline bugs.
    </p>
    <p>
      We are safe because neither our program nor SDL calls <code>setlocale</code>. The
      principled fix is <code>std::from_chars</code>, which is locale-independent by
      definition and also faster. It is not used here for one reason:
      floating-point <code>from_chars</code> was the last piece of C++17 to reach the standard
      libraries and arrived very late in libc++. Check <code>__cpp_lib_to_chars</code> before
      relying on it — Exercise 3.5.4 does the swap behind a feature test.
    </p>
  </div>

  <h3 id="impl-corner">4.3 One face corner</h3>

  <p>
    A corner token is one of <code>v</code>, <code>v/vt</code>, <code>v//vn</code> or
    <code>v/vt/vn</code>. That empty middle field is the whole reason this is not a
    <code>sscanf</code> one-liner: <code>"1//3"</code> means position 1, no texture
    coordinate, normal 3, and every pattern-based parse that expects a number after the first
    slash gets it wrong.
  </p>

  <!--EXC:parse_corner-->

  <p>
    Splitting on <code>/</code> into up to three possibly-empty fields handles all four forms
    with no special cases at all. Then resolution, which is §3.1 and §3.2 turned into five
    lines:
  </p>

  <!--EXC:resolve_index-->

  <h3 id="impl-unify">4.4 The unified vertex</h3>

  <p>
    Here is §3.3, in code. The key is the resolved triple, with <code>−1</code> standing for an
    absent attribute:
  </p>

  <!--EXC:corner_key-->

  <p>
    <code>= default</code> on the comparison is C++20 doing something genuinely useful: it
    generates a member-wise <code>operator==</code>, and — this is the part that matters — it
    will keep generating the <em>right</em> one if a fourth attribute is ever added. A
    hand-written comparison that forgets the new field compiles perfectly and merges vertices
    that should have been split.
  </p>

  <p>
    The hash has to be hand-written, because the standard library cannot guess which fields of
    a user-defined type matter:
  </p>

  <!--EXC:corner_hash-->

  <p>
    FNV-1a: start from an offset basis, and for each input word XOR it in and multiply by a
    prime. Four lines, no table, and it mixes well enough that the map's buckets stay short.
    The temptation is to write something like <code>p + t * 31 + n * 61</code> and move on;
    the trouble is that a weak hash does not fail visibly, it just turns a hash map into a
    linked list and makes loading quadratic — a bug that only shows up on big models, which is
    to say on the models that matter.
  </p>

  <p>And then the lookup itself, which is the heart of the loader:</p>

  <!--EXC:unify-->

  <p>
    Two things in there are deliberate and easy to get wrong.
  </p>

  <p>
    <strong>Find-then-insert, not <code>try_emplace</code>.</strong> One lookup would be
    faster. But <code>try_emplace</code> takes the value as an argument, so the new index —
    <code>uint16_t(out.vertices.size())</code> — is computed <em>before</em> the ceiling test
    can run, and at exactly 65,536 that expression is a silent 0. The value is never used,
    because we return immediately after. Code whose correctness depends on "we return before
    that wrong number matters" is code that breaks the next time somebody edits it.
  </p>

  <p>
    <strong>A uv and a normal for every vertex, always.</strong> If the corner had no texture
    coordinate we push <code>(0,0)</code> anyway, so <code>vertices</code>, <code>uvs</code>
    and <code>normals</code> stay exactly as long as each other. <code>mesh</code> depends on
    that — <code>uv_at(i)</code> indexes the uv array with the vertex index — and a file that
    mixes face formats (our <code>quirks.obj</code> does) would otherwise produce a ragged
    array and an out-of-bounds read on the first mesh that mixes them. If it turns out the
    file had no <code>vt</code> lines at all, the whole array is discarded at the end, because
    <em>empty</em> means "this mesh has no texture coordinates" and an array full of
    <code>(0,0)</code> means "every pixel samples the same texel", which is a different and
    much more confusing claim.
  </p>

  <h3 id="impl-errors">4.5 Errors without exceptions</h3>

  <p>
    The engine core does not throw (CLAUDE.md §4), and this is the first place in the course
    where that rule has to actually do something, because a file can be missing, truncated, or
    nonsense.
  </p>

  <!--EXC:obj_status-->

  <p>
    An enum and a line number. Not an exception — the reasons are Module 5's to lay out
    properly, but the short version is that an exception crossing a library boundary drags in
    unwind machinery, forbids <code>noexcept</code>, and makes the cost of an error path
    invisible at the call site; an enum makes the caller write the branch. And not a
    general-purpose <code>Result&lt;T, E&gt;</code> either: building one of those here would be
    inventing a language feature to solve a problem we have exactly once. When Module 5 has
    ten loaders wanting the same shape, that is when the shape earns a name.
  </p>

  <p>
    The report that comes back carries counts as well as a status, and the counts are the part
    you reach for when a model looks wrong rather than fails:
  </p>

  <!--EXC:obj_report-->

  <div class="callout note">
    <span class="label">The policy: malformed is fatal, silly is not</span>
    <p>
      A file that says <code>f 1/x/2</code> is <strong>not an OBJ file</strong>, and guessing
      what it meant helps nobody — that stops the load with a line number.
    </p>
    <p>
      A file containing a face whose three corners are the same vertex is a perfectly good
      OBJ file that happens to describe a triangle with no area, usually left behind by a
      merge operation. Real files contain these. That one is <strong>dropped and
      counted</strong>, and the load succeeds.
    </p>
    <p>
      Drawing this line consciously — and reporting both sides of it — is most of what makes
      a loader usable by somebody who is not its author. The alternatives are a loader that
      dies on real-world data, and one that swallows genuine corruption.
    </p>
  </div>

  <p>
    One error deserves singling out, because it is the one that most looks like it will never
    happen until it does:
  </p>

  <!--EXC:ceiling-->

  <p>
    Our index type is <code>std::uint16_t</code>, so a mesh can name at most 65,536 vertices.
    That is not an arbitrary choice we are stuck with — it is one of exactly two widths GPU
    index buffers come in, spelled <code>SDL_GPU_INDEXELEMENTSIZE_16BIT</code> and
    <code>_32BIT</code> in Module 4, and sixteen bits halves the bandwidth of every index
    fetch. Real models exceed it routinely. The loader's job is to <em>notice</em>: wrapping
    silently at 65,536 produces triangles joining unrelated corners, which looks like a bag of
    glass shards and gives no clue why. Our harness pushes 66,600 vertices through and checks
    for a clean refusal at line 88,446.
  </p>

  <h3 id="impl-io">4.6 Files, paths, and the build</h3>

  <p>
    Reading the bytes is four lines, and every one of them is an SDL3 detail worth knowing:
  </p>

  <!--EXC:load_obj-->

  <p>
    <code>SDL_LoadFile</code> reads a whole file into a fresh allocation and — the detail that
    makes it a good fit here — <strong>appends a zero byte that is not counted in
    <code>size</code></strong>. A parser over a <code>std::string_view</code> of that buffer
    can therefore never run off the end even if it peeks one past a token. Freeing it needs
    <code>SDL_free</code>, not <code>delete</code> or <code>free</code>: on Windows a DLL may
    be linked against a different C runtime than the program, and returning memory to the
    wrong allocator corrupts the heap.
  </p>

  <p>
    Then there is the question of <em>where</em> the file is, which is more interesting than it
    sounds:
  </p>

  <!--EXC:asset_path-->

  <p>
    The naive version is <code>"assets/cube.obj"</code> — a path relative to the
    <strong>current working directory</strong>, which is wherever the user's shell happened to
    be. Run the program from the build folder and it works; run it by double-clicking, or from
    a debugger with a different working directory, or after installing it, and it does not.
    <code>SDL_GetBasePath()</code> answers a different and much more useful question: where
    does the executable live? Assets go next to the binary, and the binary looks beside
    itself.
  </p>

  <div class="callout verify">
    <span class="label">Checked against <code>SDL3/SDL_filesystem.h</code></span>
    <p>
      In SDL3, <code>SDL_GetBasePath()</code> returns <code>const char *</code> and the result
      is <strong>cached by SDL — the caller must not free it.</strong> This is a genuine
      change from SDL2, where it returned a <code>char *</code> that you owned and had to
      <code>SDL_free</code>. Code and tutorials written for SDL2 will free it; doing that here
      is a double-free. It may also return <code>NULL</code> on platforms that cannot answer,
      which the code above handles by falling back to a relative path.
    </p>
  </div>

  <p>
    Which leaves the build to actually put them there:
  </p>

  <!--EXC:cmake_assets-->

  <p>
    <code>POST_BUILD</code> runs after every successful link, so editing a model and
    rebuilding is enough and there is no separate step to forget.
    <code>$&lt;TARGET_FILE_DIR:engine&gt;</code> is a <strong>generator expression</strong> — a
    value CMake cannot know at configure time and resolves when it writes the build rules.
    It matters here because multi-config generators (Visual Studio, Xcode) put the binary in
    <code>Debug/</code> or <code>Release/</code>, and a hard-coded path would be right in at
    most one of them.
  </p>

  <div class="callout pitfall">
    <span class="label">Your model files are already gitignored, and you have not noticed</span>
    <p>
      <code>*.obj</code> is MSVC's object-file extension. It is in essentially every C++
      project's <code>.gitignore</code>, including ours since Lesson 0.4 — and it is also
      Wavefront's model extension. <code>git add assets/cube.obj</code> silently does nothing,
      the build works perfectly on your machine, and the repository is broken for everybody
      else. One negation fixes it:
    </p>
    <!--EXC:gitignore-->
    <p>
      Confirm with <code>git check-ignore -v assets/cube.obj</code>, which prints the rule that
      decided — the pattern with the <code>!</code> means the file is <em>not</em> ignored.
    </p>
  </div>

  <h3 id="impl-writer">4.7 A writer, and the round trip</h3>

  <p>
    A loader can only be checked against expectations you typed by hand — which tests your
    typing as much as the loader. A <em>writer</em> makes something much stronger possible:
    generate a mesh, write it out, read it back, and compare. Fifty lines buys a test that
    exercises the entire path.
  </p>

  <p>
    The trick is that <code>save_obj</code> must <strong>compact</strong> — write each distinct
    position, uv and normal once, with faces naming them independently — because that is what
    a real exporter does, and because it makes the round trip exercise the index problem in
    both directions:
  </p>

  <!--EXC:save_precision-->

  <p>
    <code>%.9g</code> is not a guess. Nine significant decimal digits is the number that makes
    a 32-bit float survive a trip through decimal text <em>exactly</em> — it is
    <code>FLT_DECIMAL_DIG</code>. Fewer is prettier and loses bits; more is noise. The
    exactness is what lets the round-trip test assert equality rather than "close enough".
  </p>

  <div class="callout ok">
    <span class="label">What the round trip actually proves</span>
    <p>
      <code>make_torus(48, 24)</code> produces 1,225 vertices with 1,152 distinct positions.
      <code>save_obj</code> compacts that to a file with <strong>1,152</strong> <code>v</code>
      lines, <strong>1,225</strong> <code>vt</code> lines and 2,304 <code>f</code> lines.
      <code>load_obj</code> reads it and splits the seam back apart to <strong>exactly
      1,225</strong> vertices, and every triangle corner — position, uv <em>and</em> normal —
      matches the original bit for bit. Rendered side by side, the two meshes differ by
      <strong>0 pixels</strong>.
    </p>
    <p>
      Note the comparison is of <em>expanded triangle corners</em>, not of the vertex arrays.
      The vertex array is an encoding; the geometry is what the triangles cover. Requiring the
      arrays to match in order would be requiring the loader to reproduce an ordering it has
      no way to know, and would fail for a reason that means nothing.
    </p>
    <p>
      One number in that paragraph deserves a second look: the file has 1,152 positions but
      only <strong>1,150</strong> normals. That is not a bug. On a torus the normal at
      <code>(u, v)</code> is exactly the normal at <code>(u + π, π − v)</code> — both cosines
      flip together — so every normal is shared by two points of the surface in exact
      arithmetic. In <code>float</code> the identity survives to the last bit in two of the
      576 pairs. It is a good reminder that the three streams really are independent, and
      their lengths carry no relationship at all.
    </p>
  </div>

  <h3 id="impl-validate">4.8 The validator</h3>

  <p>
    Lesson 2.12 checked the icosahedron against four properties in prose, by hand, because the
    data was twenty lines long and we had written it. Now the data is four thousand lines long
    and we did not. The checks become a function.
  </p>

  <p>
    Everything topological reduces to one question asked of every edge: <strong>how many times
    is it traversed, and in which directions?</strong>
  </p>

  <!--EXC:edge_use-->

  <p>
    That comment is the whole test. In a closed, consistently wound surface every edge is
    shared by exactly two triangles, and those two walk it in <em>opposite</em> directions —
    because each walks its own boundary counter-clockwise, so the shared edge lies on the left
    of one and the right of the other. Every way of departing from <code>forward == 1 &amp;&amp;
    backward == 1</code> is a different, nameable defect, and the validator reports each as its
    own count rather than collapsing them into "invalid".
  </p>

  <p>
    The edges are counted on the <strong>welded</strong> graph, for the reason §3.8 gives,
    and welding uses the exact bits of the three floats — with one subtlety:
  </p>

  <!--EXC:key_of-->

  <p>
    Negative zero. <code>-0.0f</code> and <code>+0.0f</code> compare equal as floats and have
    different bit patterns, so a bitwise key files them separately and a mesh whose seam
    straddles an axis fails to weld. Adding <code>+0.0f</code> normalises the sign of zero and
    leaves every other value untouched. And <code>std::memcpy</code> rather than a cast,
    because reading a <code>float</code> through an <code>unsigned*</code> is undefined
    behaviour — the strict-aliasing rule. Every compiler turns that memcpy into no
    instructions at all; it is a note to the optimiser about types, not a copy.
  </p>

  <p>
    Finally the signed volume, §3.7 in three lines, with one numerical precaution:
  </p>

  <!--EXC:volume-->

  <p>
    Accumulated in <code>double</code>, because it is a sum of signed terms that very nearly
    cancel — each triangle's cone is large and the enclosed volume is what survives — and that
    is the textbook way to lose every digit you had.
  </p>

  <h3 id="impl-demo">4.9 Wiring it to the demo</h3>

  <p>
    The demo gains a scene, a key, and — most importantly — stops taking a promise where it
    can take a measurement:
  </p>

  <!--EXC:scene_model-->

  <p>
    That <code>closed</code> flag was, until this lesson, a <code>bool</code> typed next to
    each mesh by whoever typed the mesh. Lesson 3.4 said at the time that a real engine keeps
    it on the material, and that the demo's version was a promise. Now it is the validator's
    answer, and it takes <em>both</em> conditions: <code>twisted.obj</code> is topologically
    closed and still unsafe to cull, because culling reads winding and its winding disagrees
    with itself.
  </p>

  <p>
    And the round trip runs live, every frame it is on screen, in the currency this module has
    used for every claim it has made:
  </p>

  <!--EXC:roundtrip-->

  <!-- =================================================================
       SECTION 6 — COMPLETE CODE LISTINGS
       ================================================================= -->
  <h2 id="listings"><span class="num">5</span>Complete Code Listings</h2>

  <p>Every file this lesson touched, in full. Nothing elided.</p>

  <figure class="tbl">
    <div class="tbl-scroll">
      <table class="manifest">
        <thead><tr><th>Path</th><th>Status</th><th>Purpose</th></tr></thead>
        <tbody>
          <tr><td>src/gfx/obj.hpp</td><td><span class="badge new">new</span></td>
              <td>The loader's interface: status, report, parse, load, save, asset paths.</td></tr>
          <tr><td>src/gfx/obj.cpp</td><td><span class="badge new">new</span></td>
              <td>Parsing, index resolution, the de-duplicating map, fan triangulation, the writer.</td></tr>
          <tr><td>src/gfx/mesh.cpp</td><td><span class="badge new">new</span></td>
              <td><code>validate()</code> and <code>make_torus()</code> — the first .cpp mesh.hpp has needed.</td></tr>
          <tr><td>src/gfx/mesh.hpp</td><td><span class="badge mod">modified</span></td>
              <td>Normals on <code>mesh</code>; the owning <code>mesh_data</code>; <code>mesh_report</code>.</td></tr>
          <tr><td>src/main.cpp</td><td><span class="badge mod">modified</span></td>
              <td>The model scene, <code>[L]</code>, the live round-trip comparison, and <code>closed</code> from a measurement.</td></tr>
          <tr><td>CMakeLists.txt</td><td><span class="badge mod">modified</span></td>
              <td>Two new sources, and a POST_BUILD copy of <code>assets/</code> next to the binary.</td></tr>
          <tr><td>.gitignore</td><td><span class="badge mod">modified</span></td>
              <td>Stop <code>*.obj</code> from swallowing every model we ship.</td></tr>
          <tr><td>assets/cube.obj</td><td><span class="badge new">new</span></td>
              <td>Twenty readable lines containing the whole index problem.</td></tr>
          <tr><td>assets/twisted.obj</td><td><span class="badge new">new</span></td>
              <td>The same cube with one face wound backwards. Lesson 3.4's debt, made visible.</td></tr>
          <tr><td>assets/quirks.obj</td><td><span class="badge new">new</span></td>
              <td>Every awkward-but-legal construct at once, small enough to check by hand.</td></tr>
          <tr><td>assets/torus.obj</td><td><span class="badge new">new</span></td>
              <td>2,304 triangles, written by <code>save_obj</code> from <code>make_torus</code>. Not listed — it is 205 KB of numbers.</td></tr>
        </tbody>
      </table>
    </div>
    <figcaption><span class="fignum">Manifest.</span> Files touched in this lesson.</figcaption>
  </figure>

  <!--LISTINGS-->

  <!-- =================================================================
       SECTION 7 — BUILD & RUN
       ================================================================= -->
  <h2 id="build"><span class="num">6</span>Build &amp; Run</h2>

  <figure class="listing shell">
    <figcaption><span class="path">all platforms</span><span class="lang" data-lang="bash">shell</span></figcaption>
    <pre><code class="lang-bash">cmake -S . -B build
cmake --build build</code></pre>
  </figure>

  <p>
    The build now copies <code>assets/</code> next to the executable, so there is nothing extra
    to do — but it is worth looking once, because a missing asset directory is the first thing
    to check if the model scene comes up empty:
  </p>

  <figure class="listing shell">
    <figcaption><span class="path">run</span><span class="lang" data-lang="bash">shell</span></figcaption>
    <pre><code class="lang-bash"># macOS / Linux
ls build/assets            # cube.obj  quirks.obj  torus.obj  twisted.obj
./build/engine

# Windows (multi-config generators put binaries in a config subdirectory)
dir build\Debug\assets
.\build\Debug\engine.exe</code></pre>
  </figure>

  <h3>What you should see</h3>

  <p>
    The program starts in the <code>solids</code> scene as before. Press <kbd>C</kbd> five
    times — or <kbd>L</kbd> once, which jumps straight there — and the scene becomes
    <strong>MODEL</strong>: a torus, turning, with the camera looking down on it at about 26°
    so the hole is visible. On the console, one line per load:
  </p>

  <figure class="listing shell">
    <figcaption><span class="path">console</span><span class="lang" data-lang="bash">shell</span></figcaption>
    <pre><code class="lang-bash">Loaded torus.obj      1225 verts (+73 split),  2304 tris, euler +0, volume +3.1134, closed  [0.59 ms]</code></pre>
  </figure>

  <p>
    Every number there was derived in §3, and the HUD carries the same ones beside the
    picture. <kbd>L</kbd> cycles through the five models; the three worth stopping on are:
  </p>

  <ul>
    <li><strong><code>torus.obj</code></strong> — the payoff. 2,304 triangles from a file, and
      the readout <code>round trip: 0 px differ</code>, which is the loaded mesh being
      compared against <code>make_torus()</code> in memory, every frame.</li>
    <li><strong><code>cube.obj</code></strong> — the HUD reads <code>8+16 -&gt; 24 verts,
      12 tris</code>. Eight positions in the file, sixteen splits, twenty-four vertices:
      §3.3, on screen.</li>
    <li><strong><code>twisted.obj</code></strong> — identical to the cube until you press
      <kbd>U</kbd> to cull back faces, at which point the top of the box disappears and you
      are looking into it. The HUD's winding field turns red and reads
      <code>WINDING!</code> <em>before</em> you press anything, because the validator found it
      at load time.</li>
  </ul>

  <div class="callout ok">
    <span class="label">Checkpoint</span>
    <p>
      In the model scene with <code>torus.obj</code> loaded: a spinning torus with a visible
      hole; the headline <code>[L] torus.obj  1152+73 -&gt; 1225 verts, 2304 tris</code>; the
      right-hand panel reading <code>euler +0  closed  wound ok</code> and
      <code>volume +3.113</code>; and <code>round trip: 0 px differ</code> in green. Press
      <kbd>U</kbd> twice for back-face culling and the picture must not change — the counter
      beside it reads the pixels culling cost you, and on this mesh it is 0.
    </p>
  </div>

  <!-- =================================================================
       SECTION 8 — PITFALLS
       ================================================================= -->
  <h2 id="pitfalls"><span class="num">7</span>Common Pitfalls &amp; Debugging</h2>

  <div class="pitfalls">
    <details class="pitfall-item">
      <summary>The model looks like shattered glass — <span class="sym">right size, no recognisable shape</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>Triangles everywhere, roughly the right extent, no surface. Often with a few
            enormous triangles reaching across the whole model.</dd>
          <dt>Cause</dt>
          <dd>The 1-based index (§3.1). Every triangle is built from corners belonging to its
            neighbours, so the mesh is connected but wrong. The giant triangles are the wrap
            at the end of the array.</dd>
          <dt>Fix</dt>
          <dd>Subtract one when resolving. Confirm it with the smallest possible test rather
            than by eye: parse <code>"v 1 0 0\nv 0 1 0\nv 0 0 1\nf 1 2 3"</code> and assert
            that <code>vertices[0] == (1,0,0)</code> and the triangle is <code>(0,1,2)</code>.
            That is §A of the harness, and it is four lines.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Correct shape, nonsense texture — <span class="sym">the silhouette is perfect</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>The model's outline and shading are right, but any texture is smeared or
            scrambled across it. Flat-shaded models look fine, which is what makes this one
            survive so long.</dd>
          <dt>Cause</dt>
          <dd>Using the position index for all three streams — reading <code>f 3/7/2</code> as
            "vertex 3" and taking <code>uvs[2]</code>. The positions are right, so the shape
            is right; only the attributes are wrong.</dd>
          <dt>Fix</dt>
          <dd>Key the de-duplicating map on the whole triple (§3.3). The tell-tale is in the
            report: if <code>split_vertices</code> is 0 on a faceted model like a cube, the
            triple is not being used — a cube <em>must</em> report +16.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>A watertight model reports boundary edges — <span class="sym">a seam-shaped hole in a mesh with no hole</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>The validator reports a run of boundary edges — often exactly one row or column
            of them — on geometry that renders perfectly and obviously has no hole. χ is wrong
            by a corresponding amount.</dd>
          <dt>Cause</dt>
          <dd>Topology measured on the raw vertex array instead of the welded one (§3.8). The
            uv seam stores its positions twice, so the two sides of the seam are different
            vertices and the edges between them never pair up.</dd>
          <dt>Fix</dt>
          <dd>Weld by position before counting edges. If it still reports a seam <em>after</em>
            welding, the two copies are not bit-identical — check whether the generator
            computed the wrapped angle from <code>u = 1.0</code> instead of from a wrapped
            index, which leaves them 1.7 × 10⁻⁷ apart (the warning in §3.8).</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Holes appear only when culling is enabled — <span class="sym">fine until [U]</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>The model draws correctly with <code>cull_mode::none</code>. Turn on back-face
            culling and whole faces vanish; you can see the inside of the object through the
            gaps.</dd>
          <dt>Cause</dt>
          <dd>The file's winding is not consistent — some faces are listed the other way round.
            Lesson 3.4's culler believes winding, so a backwards face is discarded exactly when
            it should be drawn. Nothing about this is visible without culling, which is why it
            ships.</dd>
          <dt>Fix</dt>
          <dd>Run <code>validate()</code> at load and look at <code>reversed_edges</code>; it is
            4 for <code>twisted.obj</code>. Then either fix the asset, or turn culling off for
            that mesh, or (Exercise 3.5.3) walk the mesh and reorient it. Do not "fix" it by
            disabling culling globally — that hides a data problem behind a render setting.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Works for you, broken for everyone else — <span class="sym">assets missing after a fresh clone</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>The model scene is empty on a colleague's machine and the console says
            <code>cannot open file</code>. Your working copy is fine.</dd>
          <dt>Cause</dt>
          <dd>Either <code>*.obj</code> in <code>.gitignore</code> quietly swallowed the models
            when you added them (§4.6), or the assets were never copied next to the binary and
            you have only ever run from a directory where the relative path happened to work.</dd>
          <dt>Fix</dt>
          <dd><code>git check-ignore -v assets/cube.obj</code> settles the first;
            <code>git status --untracked-files=all assets/</code> shows what is really tracked.
            For the second, <code>SDL_GetBasePath()</code> plus the POST_BUILD copy makes the
            question independent of where the program was launched.</dd>
        </dl>
      </div>
    </details>

    <details class="pitfall-item">
      <summary>Loads on Linux, fails on Windows — <span class="sym">"malformed face" on line 1</span></summary>
      <div class="pitfall-body">
        <dl>
          <dt>Symptom</dt>
          <dd>The same file parses on one machine and is rejected on another, usually at the
            first line with numbers on it.</dd>
          <dt>Cause</dt>
          <dd>CRLF. A file written on Windows ends every line with <code>\r\n</code>, and the
            <code>\r</code> rides along on the last token — so <code>"0.5\r"</code> reaches the
            number parser, fails the whole-token check, and the model is refused. (A parser
            that is <em>not</em> strict about whole tokens has the opposite problem: it accepts
            the value and you never find out.)</dd>
          <dt>Fix</dt>
          <dd>Trim trailing <code>\r</code> — and trailing spaces and tabs while you are there —
            from every line before tokenising. Two lines. <code>assets/quirks.obj</code> is
            deliberately saved with CRLF so this stays tested.</dd>
        </dl>
      </div>
    </details>
  </div>

  <!-- =================================================================
       SECTION 9 — EXERCISES
       ================================================================= -->
  <h2 id="exercises"><span class="num">8</span>Exercises</h2>

  <div class="exercise">
    <h3><span class="xnum">3.5.1</span>Predict the vertex count<span class="difficulty">warm-up</span></h3>
    <p>
      Before running anything: how many vertices will the loader produce for a mesh shaped
      like a cylinder — 32 sides, with flat caps, smooth around the tube — exported with one
      normal per smooth vertex and a uv seam down one side? Count the positions too, and say
      where each split comes from. Then build one (Exercise 3.5.2 gives you the generator, or
      export one from any modelling tool) and check.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Three sources of splitting, and they stack: the uv seam duplicates one column; the
        crease where the tube meets a cap gives those positions two normals; and the cap
        centres are single positions used by many triangles. Count the tube and the caps
        separately.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        With 32 sides: the tube has 32 positions per ring × 2 rings = 64, but the seam makes
        it 33 × 2 = 66 vertices. The rim positions are shared with the caps and carry a
        different normal there, so each of the 64 rim positions appears again for its cap —
        another 64 (the caps typically have their own uvs too, which changes nothing since
        they are already split by normal). Plus 2 cap centres. So roughly 66 + 64 + 2 = 132
        vertices from 66 positions. The exact number depends on how the exporter lays out cap
        uvs, and <em>that</em> is the real lesson: the vertex count is a property of the
        export, not of the shape.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.5.2</span>Another procedural mesh, and its characteristic<span class="difficulty">guided</span></h3>
    <p>
      Add <code>make_cylinder(int sides, float radius, float height)</code> to
      <code>mesh.cpp</code>, capped, wound counter-clockwise seen from outside. Run
      <code>validate()</code> on it and check three things: χ = 2 (a cylinder is a sphere
      topologically — you can round it off without tearing), the signed volume converges to
      <code>π r² h</code>, and there are no boundary edges. Then <em>remove</em> the caps and
      watch all three answers change.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        The uncapped tube is the interesting case: it has two rims, so
        <code>boundary_edges</code> becomes 2 × sides, χ becomes 0, and the signed volume
        becomes meaningless — it is a surface that encloses nothing. Note that χ = 0 here for
        a completely different reason than the torus's χ = 0; the formula 2 − 2g only applies
        to closed surfaces, and an open one needs the boundary count too.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Build the tube exactly as <code>make_torus</code> does — including computing the
        wrapped angle from <code>i % sides</code>, or your seam will not weld — then fan each
        cap from a centre vertex. The winding trap is that the two caps must be wound
        <em>opposite</em> ways in index order, since one faces +y and the other −y; get it
        wrong and <code>reversed_edges</code> lights up along that rim, which is the validator
        doing precisely the job it was written for.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.5.3</span>Repair the winding<span class="difficulty">medium</span></h3>
    <p>
      Write <code>bool reorient(mesh_data&amp; m)</code> that takes a mesh whose winding is
      inconsistent — <code>twisted.obj</code> — and fixes it, so that afterwards
      <code>validate()</code> reports <code>reversed_edges == 0</code> and a positive signed
      volume. Return false if the mesh cannot be consistently oriented at all.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Flood fill over face adjacency. Pick any triangle, declare it correct, and walk to its
        neighbours across shared edges: a neighbour is consistent if it traverses the shared
        edge in the opposite direction, and needs flipping if it does not. Repeat until every
        face has been visited. Afterwards check the total signed volume; if it is negative,
        flip every face — you oriented the whole surface inward.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        You need an edge→faces map, which <code>validate()</code> already builds — factoring
        that out is half the exercise and a good instinct. The "cannot be oriented" case is
        real: a Möbius strip is <em>non-orientable</em>, and the flood fill returns to its
        starting face demanding the opposite of what it decided. Detecting that (a face
        reached twice with conflicting requirements) is what distinguishes a correct
        implementation from one that merely works on your test file. Note that this only fixes
        <em>consistency</em>; if the whole mesh came in inside-out, the volume sign is what
        tells you, which is exactly why §3.7's test is worth having.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.5.4</span>Locale-proof and faster<span class="difficulty">medium</span></h3>
    <p>
      Replace <code>to_float</code>'s use of <code>std::strtof</code> with
      <code>std::from_chars</code>, behind a <code>#if __cpp_lib_to_chars &gt;= 201611L</code>
      feature test that falls back to the current implementation. Then measure: how much
      faster does <code>assets/torus.obj</code> load? Prove the locale claim while you are
      there — call <code>std::setlocale(LC_NUMERIC, "de_DE.UTF-8")</code> in a test and watch
      the <code>strtof</code> path lose every fraction while the <code>from_chars</code> path
      does not.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        <code>from_chars</code> takes a begin and end pointer and returns a struct with
        <code>ptr</code> and <code>ec</code>; the "whole token or nothing" check becomes
        <code>result.ptr == token.data() + token.size()</code>, which is nicer than the
        <code>strtof</code> version because no copy into a buffer is needed at all. Guard the
        locale test — the locale may not be installed on the test machine, in which case
        <code>setlocale</code> returns null and the test should skip rather than fail.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Expect a solid speed-up on the parse — <code>from_chars</code> avoids both the copy
        and the locale lookup — but measure the whole load, not just the conversion, because
        the hash map is a serious fraction of the time. This is a good, small instance of
        Module 3's rule: the honest number is the one for the operation the user waits on.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.5.5</span>Triangulate honestly<span class="difficulty">open</span></h3>
    <p>
      Replace the fan with <strong>ear clipping</strong>, so concave faces triangulate
      correctly (§3.5, Figure 4). Then build a test that a fan fails and ear clipping passes:
      an L-shaped hexagonal face, whose area you can compute by hand, where you assert that
      the sum of the triangle areas equals the polygon's area. The fan will overshoot.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Work in 2D. Find the face's plane from its Newell normal (which is robust for
        near-degenerate polygons in a way that a single cross product is not), drop the
        largest component of that normal, and use the other two axes as your 2D coordinates.
        Then repeatedly find an "ear": a corner whose triangle is convex <em>and</em> contains
        no other corner of the polygon. Snip it, and repeat until three corners remain.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        The naive implementation is <em>O(n²)</em> and that is completely fine for face sizes
        that occur in the wild — the largest n-gon you will meet is a cylinder cap with a few
        dozen corners. The subtle parts are self-intersecting polygons (no valid
        triangulation exists; detect and fall back to the fan with a warning rather than
        looping forever) and holes (OBJ cannot express them, glTF does not have them either,
        so genuinely out of scope). Weigh the honest tradeoff before you ship it: eighty lines
        and a failure mode against an artifact you may never see, because virtually every
        exporter emits convex faces. The right answer for an engine may well be "fan, and
        validate", which is what we do — but you should have made that choice deliberately
        rather than by not knowing the alternative.
      </p>
    </details>
  </div>

  <!-- =================================================================
       SECTION 10 — RECAP & NEXT
       ================================================================= -->
  <h2 id="recap"><span class="num">9</span>Recap &amp; Next</h2>

  <div class="recap">
    <h2>What the engine can do now that it could not before</h2>
    <ul>
      <li><strong>It reads geometry it did not write.</strong> A hand-rolled OBJ parser with
        1-based and negative index resolution, all four corner formats, n-gon triangulation,
        and a documented policy on what is fatal and what is merely counted.</li>
      <li><strong>It reconciles two incompatible ideas of "vertex".</strong> The
        de-duplicating map over <code>(position, uv, normal)</code> — the thing that turns 8
        positions into 24 vertices, and the first time this course has reshaped data to suit
        the hardware rather than the other way round. That is what an asset pipeline
        <em>is</em>.</li>
      <li><strong>It owns its geometry.</strong> <code>mesh_data</code> holds the arrays and
        <code>mesh</code> views them, with the lifetime rule stated and the API shaped so the
        dangling case is hard to write.</li>
      <li><strong>It measures instead of assuming.</strong> <code>validate()</code> reports
        welded vertices, splits, edges, χ, boundary and non-manifold edges, winding conflicts
        and signed volume — so "is this safe to back-face cull" is now a question with an
        answer. Lesson 3.4's promise is a measurement.</li>
      <li><strong>It writes, too</strong>, which is what makes the loader testable: generate,
        save, load, compare — 0 pixels different, corner for corner, bit for bit.</li>
    </ul>
    <p>
      <strong>Next:</strong> <a href="03-06-normals-and-lambert.html">3.6 — Normals and
      Lambert's Cosine Law</a>. We have been shading with a debug palette since Lesson 3.1 —
      five brightness steps indexed by triangle number, which owes nothing to any light and
      does not change when the object turns. Today's loader read a <code>vn</code> for every
      vertex and put them in an array that nothing reads. Next lesson those normals meet a
      light direction, the palette goes, and surfaces start responding to where they face —
      which is also where the fact that a non-uniform scale does <em>not</em> transform a
      normal the way it transforms a point finally has to be dealt with.
    </p>
  </div>

  <!-- =================================================================
       SECTION 11 — FURTHER READING
       ================================================================= -->
  <h2 id="reading"><span class="num">10</span>Further Reading</h2>

  <ul class="reading">
    <li><span class="src">Format spec</span> <em>Object Files (.obj)</em>, the original
      Wavefront Advanced Visualizer appendix — still the authoritative description, and short.
      Read the <code>f</code> statement section and the note on negative indices; skip the
      free-form geometry, which nobody ships.</li>
    <li><span class="src">Real-Time Rendering</span> Chapter 16, <em>Polygonal Techniques</em>
      — triangulation, welding tolerances, consistency and repair, and mesh validation, all in
      one place and all with the production caveats this lesson has had to compress.</li>
    <li><span class="src">SDL3 wiki</span> <a
      href="https://wiki.libsdl.org/SDL3/SDL_LoadFile">SDL_LoadFile</a> and <a
      href="https://wiki.libsdl.org/SDL3/SDL_GetBasePath">SDL_GetBasePath</a> — note the SDL2
      → SDL3 change in ownership of the returned path.</li>
    <li><span class="src">Topology</span> David Eppstein's <em>Nineteen Proofs of Euler's
      Formula</em> — for the reader who wants to know <em>why</em> V − E + F is invariant
      rather than merely that it is. The proof by "spanning tree plus dual spanning tree" is
      the one that makes the genus term obvious.</li>
    <li><span class="src">Numerics</span> <em>What Every Computer Scientist Should Know About
      Floating-Point Arithmetic</em>, Goldberg — the background for §3.8's ulp and for why
      <code>%.9g</code> is the right precision to write a float with.</li>
    <li><span class="src">Ahead</span> glTF 2.0's specification, for later comparison. Module 6
      loads it, and the interesting exercise is to notice how it solves each problem this
      lesson solved by hand: it stores already-unified vertices in binary buffers, so there is
      no index problem to solve at load time — the exporter paid it.</li>
  </ul>
"""
