/* ==========================================================================
   Build a Professional 3D Game Engine — shared page script
   ==========================================================================
   Theme toggle, syntax highlighter, and the listing fold, for every page in
   docs/. Linked (not inlined) between each page's SHARED-SCRIPT
   markers, immediately before the two KaTeX CDN tags — which stay inline in the
   pages because they carry SRI hashes and an inline onload handler.

   Loaded as a plain classic script at end of body, so it runs at exactly the
   point the inline copy used to: after the DOM is parsed, before KaTeX's
   deferred render.

   Not a build artifact — see the note in course.css.
   ========================================================================== */

(function () {
  "use strict";

  /* ---- Theme toggle: OS default, with a manual override in localStorage ---- */
  var root = document.documentElement;
  var KEY = "engine-course-theme";
  try {
    var saved = localStorage.getItem(KEY);
    if (saved) { root.setAttribute("data-theme", saved); root.style.colorScheme = saved; }
  } catch (e) { /* private mode — fall back to the OS preference */ }

  var btn = document.getElementById("theme-toggle");
  if (btn) {
    btn.addEventListener("click", function () {
      var cur = root.getAttribute("data-theme");
      if (!cur) {
        cur = window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
      }
      var next = cur === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", next);
      root.style.colorScheme = next;
      try { localStorage.setItem(KEY, next); } catch (e) {}
    });
  }

  /* ---- Syntax highlighting -------------------------------------------------
     A deliberately small tokeniser for C++/HLSL, CMake, and shell. It reads
     textContent and rebuilds escaped HTML, so it can never execute page content
     or mangle a listing — worst case a token is mis-coloured.

     Why not highlight.js from a CDN? Because a lesson must render from a bare
     filesystem with no network, and colour is not worth a dependency that can
     be absent. This is ~60 lines and always there.

     Keyword vs type matters: `kw` is consulted before `ty`, so a word listed in
     both lands in .tok-k. Fundamental types (bool, char, int, …) belong in
     CPP_TYPES only — putting them in CPP_KEYWORDS would silently recolour every
     declaration in the course.
     -------------------------------------------------------------------------- */
  var CPP_KEYWORDS = ("alignas alignof and asm auto break case catch class concept const consteval "
    + "constexpr constinit const_cast continue co_await co_return co_yield decltype default defined "
    + "delete do dynamic_cast else enum explicit export extern false final for friend goto if inline "
    + "mutable namespace new noexcept not nullptr operator or override private protected public "
    + "register reinterpret_cast requires return sizeof static static_assert static_cast struct "
    + "switch template this thread_local throw true try typedef typeid typename union using virtual "
    + "volatile while cbuffer register_space numthreads in out inout").split(" ");

  var CPP_TYPES = ("bool char char8_t char16_t char32_t double float int long short signed unsigned "
    + "void wchar_t size_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t "
    + "Uint8 Uint16 Uint32 Uint64 Sint8 Sint16 Sint32 Sint64 "
    + "float2 float3 float4 float4x4 float3x3 half int2 int3 int4 uint uint2 uint3 uint4 matrix "
    + "Texture2D TextureCube SamplerState StructuredBuffer").split(" ");

  var kw = Object.create(null), ty = Object.create(null);
  CPP_KEYWORDS.forEach(function (w) { kw[w] = 1; });
  CPP_TYPES.forEach(function (w) { ty[w] = 1; });

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }
  function span(cls, s) { return '<span class="tok-' + cls + '">' + esc(s) + "</span>"; }

  // One regex, alternation ordered so longer/greedier forms win.
  var RE = new RegExp([
    "(\\/\\*[\\s\\S]*?\\*\\/|\\/\\/[^\\n]*|#[^\\n]*)",       // 1 comment / preprocessor
    '("(?:\\\\.|[^"\\\\])*"|\'(?:\\\\.|[^\'\\\\])*\')',      // 2 string / char
    "(\\b\\d[\\w.]*\\b)",                                     // 3 number
    "([A-Za-z_]\\w*)(?=\\s*\\()",                             // 4 call site
    "([A-Za-z_]\\w*)",                                        // 5 identifier
    "([{}()\\[\\];,.<>+\\-*/%=!&|^~?:]+)"                     // 6 punctuation
  ].join("|"), "g");

  function highlightCLike(src) {
    var out = "", last = 0, m;
    RE.lastIndex = 0;
    while ((m = RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[1])      out += span(m[1].charAt(0) === "#" ? "p" : "c", m[1]);
      else if (m[2]) out += span("s", m[2]);
      else if (m[3]) out += span("n", m[3]);
      else if (m[4]) out += kw[m[4]] ? span("k", m[4]) : span("f", m[4]);
      else if (m[5]) out += kw[m[5]] ? span("k", m[5])
                          : ty[m[5]] ? span("t", m[5])
                          : esc(m[5]);
      else if (m[6]) out += span("o", m[6]);
      last = RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  // Shell: `#` comments anywhere, plus Windows-batch `::` comments — but only at
  // the start of a line, because `::` is also a namespace separator and a CMake
  // target separator (`SDL3::SDL3`), which do appear mid-line in shell listings.
  var SH_RE = /(^|\n)(\s*)(::[^\n]*)|(#[^\n]*)|("(?:\\.|[^"\\])*"|'[^']*')|(^|\n)(\s*)([\w.\/\\-]+)/g;
  function highlightShell(src) {
    var out = "", last = 0, m;
    SH_RE.lastIndex = 0;
    while ((m = SH_RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[3])      out += esc(m[1] || "") + esc(m[2] || "") + span("c", m[3]);
      else if (m[4]) out += span("c", m[4]);
      else if (m[5]) out += span("s", m[5]);
      else           out += esc(m[6] || "") + esc(m[7] || "") + span("f", m[8]);
      last = SH_RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  // CMake: comments, strings, ${vars}, and command names (word before "(").
  var CMAKE_RE = /(#[^\n]*)|("(?:\\.|[^"\\])*")|(\$\{[^}]*\})|(^|\n)(\s*)([A-Za-z_]\w*)(?=\s*\()/g;
  function highlightCMake(src) {
    var out = "", last = 0, m;
    CMAKE_RE.lastIndex = 0;
    while ((m = CMAKE_RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[1])      out += span("c", m[1]);
      else if (m[2]) out += span("s", m[2]);
      else if (m[3]) out += span("n", m[3]);
      else           out += esc(m[4] || "") + esc(m[5] || "") + span("f", m[6]);
      last = CMAKE_RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  var DISPATCH = {
    "lang-cpp": highlightCLike, "lang-hlsl": highlightCLike, "lang-c": highlightCLike,
    "lang-bash": highlightShell, "lang-sh": highlightShell,
    "lang-cmake": highlightCMake
  };
  document.querySelectorAll(".listing pre code").forEach(function (el) {
    for (var cls in DISPATCH) {
      if (el.classList.contains(cls)) {
        el.innerHTML = DISPATCH[cls](el.textContent);
        return;
      }
    }
  });

  /* ---- Folding long listings -----------------------------------------------
     The course prints whole files, so a lesson can carry thousands of lines of
     code between two paragraphs of prose — Lesson 5.1 has 9,444 across 34
     listings, one of which is 5,727 on its own. Long listings are clamped to a
     peek and expanded on demand.

     The clamp itself is CSS (`max-height` on `.listing pre`) and is already in
     force before this file runs. That is deliberate and load-bearing: this is a
     classic script at end of body, so by the time it executes the browser has
     painted the full-height page, and shrinking the document here would break
     anchor jumps and scroll restoration. Everything below is enhancement on top
     of a page that is already correctly clipped.

     Runs AFTER the highlighter, which rewrites each <code>'s innerHTML.
     -------------------------------------------------------------------------- */
  var LISTINGS_KEY = "engine-course-listings";

  // Two lines of slack over --listing-peek-lines (12). A 13-line listing loses a
  // hairline at most, and a "Show all 13 lines" bar on it would cost more space
  // than it could ever save.
  var FOLD_THRESHOLD = 14;

  var folds = [];

  // Sole mutator of a listing's state, so the two controls can never disagree.
  function setOpen(entry, open, keepInView) {
    entry.fig.classList.toggle("is-collapsed", !open);
    entry.fig.classList.toggle("is-open", open);

    entry.buttons.forEach(function (b) { b.setAttribute("aria-expanded", String(open)); });
    entry.chev.setAttribute("aria-label",
      (open ? "Collapse " : "Expand ") + entry.path);

    // Rebuilt from nodes rather than innerHTML — same rule the highlighter
    // follows, so page content can never be interpreted as markup.
    entry.bar.textContent = "";
    var glyph = document.createElement("span");
    glyph.className = "chev";
    glyph.textContent = open ? "\u25b4" : "\u25be";
    entry.bar.appendChild(glyph);
    entry.bar.appendChild(document.createTextNode(
      open ? "Collapse" : "Show all " + entry.lines + " lines"));
    entry.chev.textContent = open ? "\u25b4" : "\u25be";

    // Collapsing a listing the reader has already scrolled past removes
    // thousands of pixels from ABOVE the viewport, which throws them down the
    // page. Pull the listing back into view instead.
    if (keepInView && !open && entry.fig.getBoundingClientRect().top < 0) {
      entry.fig.scrollIntoView({ block: "nearest" });
    }
  }

  document.querySelectorAll(".listing").forEach(function (fig, i) {
    if (fig.classList.contains("nofold")) { return; }   // author opt-out
    var pre = fig.querySelector("pre");
    var code = pre && pre.querySelector("code");
    if (!pre || !code) { return; }

    // Decide by LINE COUNT, never by measuring. textContent needs no layout, so
    // this is correct before webfonts settle — and correct for a listing inside
    // a closed <details>, where scrollHeight and clientHeight both read 0 and
    // any measurement would call a 400-line file "short".
    var lines = code.textContent.replace(/\n$/, "").split("\n").length;
    if (lines <= FOLD_THRESHOLD) { fig.classList.add("is-short"); return; }

    if (!pre.id) { pre.id = "listing-body-" + i; }
    var pathEl = fig.querySelector(".path");

    var chev = document.createElement("button");
    chev.type = "button";
    chev.className = "listing-fold";
    chev.setAttribute("aria-controls", pre.id);

    var bar = document.createElement("button");
    bar.type = "button";
    bar.className = "listing-toggle";
    bar.setAttribute("aria-controls", pre.id);

    var cap = fig.querySelector("figcaption");
    if (cap) { cap.appendChild(chev); }
    fig.appendChild(bar);
    fig.classList.add("is-collapsible");

    var entry = {
      fig: fig, chev: chev, bar: bar, buttons: [chev, bar],
      lines: lines.toLocaleString(),
      path: pathEl ? pathEl.textContent.trim() : "listing"
    };
    folds.push(entry);

    entry.buttons.forEach(function (b) {
      b.addEventListener("click", function () {
        setOpen(entry, entry.fig.classList.contains("is-collapsed"), true);
      });
    });

    setOpen(entry, false, false);
  });

  /* Page-level escape hatch. Find-in-page can match text inside a clipped
     listing but cannot scroll to it, so a reader searching a whole lesson needs
     one click that opens everything. Injected next to the theme toggle rather
     than authored into markup — the masthead is hand-copied into all 50 pages
     and the template, and injection retrofits every one of them. */
  if (folds.length) {
    var themeBtn = document.getElementById("theme-toggle");
    var allBtn = document.createElement("button");
    allBtn.type = "button";
    allBtn.className = "theme-toggle listings-toggle";
    allBtn.setAttribute("aria-label", "Expand all code listings on this page");

    function setAll(open, persist) {
      folds.forEach(function (e) { setOpen(e, open, false); });
      allBtn.setAttribute("aria-pressed", String(open));
      allBtn.textContent = open ? "Collapse all" : "Expand all";
      if (persist) {
        try {
          if (open) { localStorage.setItem(LISTINGS_KEY, "expanded"); }
          else { localStorage.removeItem(LISTINGS_KEY); }
        } catch (e) { /* private mode — the choice just does not outlive the page */ }
      }
    }

    allBtn.addEventListener("click", function () {
      setAll(allBtn.getAttribute("aria-pressed") !== "true", true);
    });

    if (themeBtn && themeBtn.parentNode) {
      themeBtn.parentNode.insertBefore(allBtn, themeBtn);
    }

    // Honour a persisted preference, and #expand-all for linking someone
    // straight to a fully-open page. Only the masthead button writes the key —
    // per-listing toggles stay ephemeral, or reading one long file would
    // silently reconfigure the whole course.
    var wantOpen = location.hash === "#expand-all";
    if (!wantOpen) {
      try { wantOpen = localStorage.getItem(LISTINGS_KEY) === "expanded"; } catch (e) {}
    }
    setAll(wantOpen, false);
  }

  /* ---- Cross-references: "8.10 §6" becomes a link ---------------------------
     About 220 sentences send the reader to another lesson's section ("8.4
     §3.2's argument"), and until 2026-09-27 every one was plain text. Two
     halves, and both work without touching a page:

     1. Every numbered heading gains an anchor, id="sec-N". Headings number
        themselves in two styles: Modules 3 onward print the number ("6.2 The
        interface"), Module 2 prints only a title and means "the second h3 of
        section 6". The number is the printed one when there is one, and the
        h2's number plus the h3's ordinal otherwise — the rule the references
        were written against. check-curriculum.py checks every reference
        resolves under the same rule.
     2. The text "N.M §X" (optionally "N.M's §X") outside code, links and
        diagrams becomes <a class="xref"> to that lesson's #sec-X.

     The map from lesson id to file is generated from the index by
     `check-curriculum.py --write-lesson-map`, which also fails when the two
     disagree. Without this script the text simply stays text. */
  var LESSONS = {/* LESSON-MAP:BEGIN */
    "0.1": "00-01-what-is-an-engine.html",
    "0.2": "00-02-how-this-course-works.html",
    "0.3": "00-03-toolchain.html",
    "0.4": "00-04-cmake-from-zero.html",
    "0.5": "00-05-first-window.html",
    "0.6": "00-06-headers-and-debugger.html",
    "1.1": "01-01-events-properly.html",
    "1.2": "01-02-input-state-vs-events.html",
    "1.3": "01-03-delta-time.html",
    "1.4": "01-04-fixed-timestep.html",
    "1.5": "01-05-framebuffer.html",
    "1.6": "01-06-colour.html",
    "1.7": "01-07-vectors-2d.html",
    "1.8": "01-08-pong.html",
    "2.1": "02-01-lines.html",
    "2.2": "02-02-triangle-edge-functions.html",
    "2.3": "02-03-barycentric.html",
    "2.4": "02-04-attribute-interpolation.html",
    "2.5": "02-05-matrices.html",
    "2.6": "02-06-mat4.html",
    "2.7": "02-07-homogeneous.html",
    "2.8": "02-08-space-chain.html",
    "2.9": "02-09-view-matrix.html",
    "2.10": "02-10-perspective.html",
    "2.11": "02-11-viewport.html",
    "2.12": "02-12-wireframe-mesh.html",
    "3.1": "03-01-z-buffer.html",
    "3.2": "03-02-perspective-correct.html",
    "3.3": "03-03-near-plane-clipping.html",
    "3.4": "03-04-back-face-culling.html",
    "3.5": "03-05-obj-loader.html",
    "3.6": "03-06-normals-and-lambert.html",
    "3.7": "03-07-specular-blinn-phong.html",
    "3.8": "03-08-shading-models.html",
    "3.9": "03-09-textures.html",
    "3.10": "03-10-profiling-capstone.html",
    "4.1": "04-01-how-gpus-work.html",
    "4.2": "04-02-sdl-gpu-model.html",
    "4.3": "04-03-shader-toolchain.html",
    "4.4": "04-04-first-triangle.html",
    "4.5": "04-05-vertex-buffers.html",
    "4.6": "04-06-uniforms.html",
    "4.7": "04-07-textures-and-depth.html",
    "4.8": "04-08-porting-the-scene.html",
    "4.9": "04-09-renderdoc.html",
    "5.1": "05-01-the-refactor.html",
    "5.2": "05-02-platform-layer.html",
    "5.3": "05-03-logging-and-errors.html",
    "5.4": "05-04-handles.html",
    "5.5": "05-05-asset-system.html",
    "5.6": "05-06-data-oriented-design.html",
    "5.7": "05-07-ecs-storage.html",
    "5.8": "05-08-ecs-runtime.html",
    "5.9": "05-09-transform-hierarchy.html",
    "5.10": "05-10-input-mapping.html",
    "5.11": "05-11-imgui-debug-draw.html",
    "5.12": "05-12-checkpoint-game.html",
    "6.1": "06-01-linear-and-srgb.html",
    "6.2": "06-02-what-a-brdf-is.html",
    "6.3": "06-03-microfacet-theory.html",
    "6.4": "06-04-cook-torrance.html",
    "6.5": "06-05-material-system.html",
    "6.6": "06-06-gltf.html",
    "6.7": "06-07-normal-mapping.html",
    "6.8": "06-08-shadow-mapping.html",
    "6.9": "06-09-cascaded-shadows.html",
    "6.10": "06-10-mipmaps.html",
    "6.11": "06-11-transparency.html",
    "6.12": "06-12-hdr-tonemapping.html",
    "6.13": "06-13-bloom-post-stack.html",
    "6.14": "06-14-antialiasing.html",
    "6.15": "06-15-skybox-ibl.html",
    "6.16": "06-16-frustum-culling.html",
    "6.17": "06-17-frame-graph.html",
    "6.17b": "06-17b-local-lights.html",
    "6.18": "06-18-text-overlay.html",
    "7.1": "07-01-euler-angles.html",
    "7.2": "07-02-axis-angle.html",
    "7.3": "07-03-complex-numbers.html",
    "7.4": "07-04-quaternions.html",
    "7.5": "07-05-slerp.html",
    "7.6": "07-06-skeletal-animation.html",
    "7.7": "07-07-sampling-blending.html",
    "7.8": "07-08-audio.html",
    "8.1": "08-01-integrators.html",
    "8.2": "08-02-forces-and-bodies.html",
    "8.3": "08-03-angular-dynamics.html",
    "8.4": "08-04-collision-primitives.html",
    "8.5": "08-05-gjk.html",
    "8.6": "08-06-epa.html",
    "8.7": "08-07-contact-manifolds.html",
    "8.8": "08-08-broadphase.html",
    "8.9": "08-09-impulse-response.html",
    "8.10": "08-10-sequential-impulses.html",
    "8.11": "08-11-constraints-and-joints.html",
    "8.12": "08-12-ragdolls.html",
    "8.13": "08-13-character-controller.html"
  /* LESSON-MAP:END */};

  function headingNumber(h) {
    var num = h.querySelector(".num");
    var text = (num ? num.textContent : h.textContent).replace(/^\s+/, "");
    var m = /^(\d+[a-z]?(?:\.\d+)*)\.?(?:\s|$)/.exec(text);
    return m ? m[1] : null;
  }

  function anchorSections() {
    var current = null, ordinal = 0;
    var heads = document.querySelectorAll("h2, h3");
    for (var i = 0; i < heads.length; i++) {
      var h = heads[i], n = headingNumber(h);
      if (h.tagName === "H2") { current = n; ordinal = 0; }
      else { ordinal += 1; if (!n && current) { n = current + "." + ordinal; } }
      if (n && !document.getElementById("sec-" + n)) {
        var a = document.createElement("span");
        a.id = "sec-" + n;
        a.className = "sec-anchor";
        h.insertBefore(a, h.firstChild);
      }
    }
  }

  var XREF = /\b(\d\.\d{1,2}b?)((?:’s|'s)?\s*§\s*)(\d+[a-z]?(?:\.\d+)?)/g;
  var XREF_ONE = new RegExp(XREF.source);
  var SKIP = /^(A|PRE|CODE|SCRIPT|STYLE|TEXTAREA|BUTTON|H1|H2|H3|H4|svg|SVG)$/;

  function linkCrossReferences() {
    var prefix = /\/lessons\/[^\/]*$/.test(location.pathname) ? "" : "lessons/";
    var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
      acceptNode: function (node) {
        if (!XREF_ONE.test(node.nodeValue)) { return NodeFilter.FILTER_SKIP; }
        for (var p = node.parentNode; p && p !== document.body; p = p.parentNode) {
          if (SKIP.test(p.nodeName) || p.namespaceURI === "http://www.w3.org/2000/svg") {
            return NodeFilter.FILTER_SKIP;
          }
        }
        return NodeFilter.FILTER_ACCEPT;
      }
    });
    var found = [];
    while (walker.nextNode()) { found.push(walker.currentNode); }
    found.forEach(function (node) {
      var text = node.nodeValue, last = 0, frag = document.createDocumentFragment(), m;
      XREF.lastIndex = 0;
      while ((m = XREF.exec(text)) !== null) {
        var file = LESSONS[m[1]];
        if (!file) { continue; }                       // an unpublished lesson stays text
        frag.appendChild(document.createTextNode(text.slice(last, m.index)));
        var link = document.createElement("a");
        link.className = "xref";
        link.href = prefix + file + "#sec-" + m[3];
        link.textContent = m[0];
        frag.appendChild(link);
        last = m.index + m[0].length;
      }
      if (last === 0) { return; }
      frag.appendChild(document.createTextNode(text.slice(last)));
      node.parentNode.replaceChild(frag, node);
    });
  }

  anchorSections();
  linkCrossReferences();
  // The anchors are created by this script, and KaTeX (async) reflows the page
  // after it runs, so land on a #sec- target explicitly once everything is in.
  window.addEventListener("load", function () {
    if (/^#sec-/.test(location.hash)) {
      var target = document.getElementById(decodeURIComponent(location.hash.slice(1)));
      if (target) { target.scrollIntoView(); }
    }
  });

  /* ---- Figure widths: legible labels at every screen size ------------------
     A diagram is authored in a viewBox a few hundred units wide, with labels
     sized for that width (.xs is 9.5 units). CSS alone scales it to the text
     column, which on a 390 px phone puts every label near 4 px — unreadable —
     and on a desktop shrinks the widest figures to about 7 px.

     So publish each figure's NATIVE width to CSS as --vbw. course.css then
     lets a wide figure break out of the column up to 1:1 on a desktop, and on
     a phone gives it a minimum width (3/4 of native) inside its own sideways
     scroll, the way code listings already scroll rather than wrap. Without
     this script nothing changes: figures fit the column exactly as before. */
  var figures = document.querySelectorAll("figure.dia");
  for (var f = 0; f < figures.length; f++) {
    var svg = figures[f].querySelector("svg");
    var box = svg && svg.viewBox && svg.viewBox.baseVal;
    if (box && box.width > 0) {
      figures[f].style.setProperty("--vbw", box.width + "px");
      figures[f].classList.add("has-vbw");
      addEnlarge(figures[f]);
    }
  }

  /* The "Enlarge" button (shown by course.css on narrow screens only) opens a
     full-screen layer holding a clone of the figure at its native width. The
     clone's ids are stripped — the page must not hold two elements with one
     id — and its `url(#…)` marker references still resolve to the original
     figure's <defs>, which stays in the document underneath. */
  function addEnlarge(fig) {
    var num = fig.querySelector(".fignum");
    var name = num ? num.textContent.replace(/[.\s]+$/, "") : "Figure";
    var btn = document.createElement("button");
    btn.type = "button";
    btn.className = "fig-enlarge";
    btn.textContent = "Enlarge";
    btn.setAttribute("aria-label", "Enlarge " + name);
    btn.addEventListener("click", function () { openZoom(fig, btn, name); });
    // Directly under the drawing, not after a caption that can run to a
    // screen and a half on a phone.
    var caption = fig.querySelector("figcaption");
    fig.insertBefore(btn, caption && caption.parentNode === fig ? caption : null);
  }

  function openZoom(fig, btn, name) {
    var layer = document.createElement("div");
    layer.className = "fig-zoom";
    layer.setAttribute("role", "dialog");
    layer.setAttribute("aria-modal", "true");
    layer.setAttribute("aria-label", name + ", enlarged");

    var copy = fig.cloneNode(true);
    copy.classList.remove("has-vbw", "bleed");
    var stale = copy.querySelectorAll("[id], .fig-enlarge");
    for (var i = 0; i < stale.length; i++) {
      if (stale[i].classList.contains("fig-enlarge")) { stale[i].remove(); }
      else { stale[i].removeAttribute("id"); }
    }
    copy.removeAttribute("id");

    var close = document.createElement("button");
    close.type = "button";
    close.className = "fig-zoom-close";
    close.textContent = "Close";
    layer.appendChild(close);
    layer.appendChild(copy);
    document.body.appendChild(layer);

    // The page beneath must not scroll while the layer is up, or a swipe
    // meant for the figure moves the lesson instead.
    var root = document.documentElement;
    var before = root.style.overflow;
    root.style.overflow = "hidden";

    function shut() {
      layer.remove();
      root.style.overflow = before;
      document.removeEventListener("keydown", onKey);
      btn.focus();
    }
    function onKey(e) { if (e.key === "Escape") { shut(); } }
    close.addEventListener("click", shut);
    document.addEventListener("keydown", onKey);
    close.focus();
  }
})();
