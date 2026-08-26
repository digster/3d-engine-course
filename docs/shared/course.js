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
})();
