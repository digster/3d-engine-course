# Module 5 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## Refactoring facts, learned the hard way (Lesson 5.1)

### A refactor's claim is falsifiable, so build the falsifier first

The claim is "nothing observable changed". If nothing can prove it wrong, it is not a claim — it
is a hope with a commit message. Before a single file moved, `sandbox --shot PATH` was written:
seven pinned frames to one binary PPM, everything that could vary (time, camera, light, every
mode) a `constexpr` inside the function. 1,209,600 bytes, hash `905BF27E`.

Then the work was done in **two passes, verified separately**:

1. move 2,425 lines and 57 files without changing them → `cmp` → identical
2. redesign `collect_triangles` without moving anything → `cmp` → identical

Relocation breaks on a missing include or a namespace slip; redesign breaks on a mis-ordered
argument. One edit containing both leaves two suspects and no way to separate them.

### A characterization test that does not reach a branch has not pinned it

The first version had six frames, and the log said `straddle = 0` on every one of them — no
triangle crossed the near plane, so Lesson 3.3's clipper, several hundred lines about to be moved,
was **never called**. Frame 6 stands the camera on the floor: 32 triangles in, 8 straddling, 36
out.

The general form: **read your instrument's own counters and ask them what you missed.** The
counter that revealed this (`clip_stats::straddling`) existed for the HUD and was doing a second
job nobody had asked it to do.

### The include path is a better boundary than the style guide

```cmake
target_include_directories(engine PUBLIC include PRIVATE src)
```

`include/` holds exactly one directory, `engine`, so from outside the only spellings that resolve
are `<engine/…>`. `#include "gfx/raster.hpp"` fails with *file not found*. A boundary maintained
by discipline lasts until the first time somebody is in a hurry; this one is maintained by a
compiler, which is never in a hurry. Prefer rules a machine enforces over rules people remember.

`add_library(engine::engine ALIAS engine)` for the same reason: a name with `::` cannot be
mistaken for a file, so a typo fails at *configure* time rather than becoming `-lengine` at link
time.

### A fifteen-parameter function is a design nobody was asked to defend

`collect_triangles` had fifteen parameters at eight call sites. Every one was added by a lesson
that needed it; every addition was locally reasonable; nobody ever read the total. Read by *kind*
rather than in order, seven of the sixteen rows were one thing (policy) and three more were the
camera spelled as separate values a caller had to remember to derive together.

15 → 8, and the cost of the old shape was not ugliness: **a ninth knob meant editing eight calls
forty lines apart, so nobody would ever add one.** A bad signature is a tax on improving the thing
it belongs to.

Four rules that fell out, each with a receipt:

- the **caller's** vocabulary, not the implementation's (`camera_view`, not two loose values)
- state that always travels together travels as one thing — `fill_style` (3.2), `projector` (3.3),
  `render_options` (5.1): three for three
- **every default is the correct answer**; reaching a wrong one costs a line that says its name
- instrumentation is optional and says so in the type — a required `clip_stats&` had produced
  **seven** throwaway `clip_stats ignored;` variables

### A header is a promise about rebuild time

Measured on this tree, best of three, incremental:

| touched | seconds |
|---|---|
| `demos/sandbox/main.cpp` | 0.38 |
| `engine/src/gfx/raster.cpp` (private) | 0.42 |
| `engine/include/engine/gfx/gpu_scene.hpp` (public) | 0.81 |
| `engine/include/engine/gfx/raster.hpp` (public) | 0.97 |

2.3× on ~22k lines. The seconds are trivial and saying otherwise would be dishonest; **the ratio
is what carries** to a codebase fifty times the size, where the same two edits are a coffee break
apart. Related, and checked: 37/37 public headers compile **alone**. A header that only works
because of what came before it in your TU breaks for the next person who reorders two lines.

### The umbrella header is cheaper than folklore says, for an unflattering reason

`<engine/engine.hpp>` versus one header, same program: 262 ms / 82,507 preprocessed lines against
215 ms / 72,942. Only 22% worse — because **SDL already dominates**. Nearly 73,000 lines arrive
before we contribute anything, since `framebuffer.hpp` includes `<SDL3/SDL_stdinc.h>` for one
typedef. If we cared about compile time we would go after that first, not the umbrella.

### Names only collide once they can see each other

`enum class demo` in the sandbox's anonymous namespace **hides** `namespace demo`, so
`demo::build_scene` would not compile — lookup finds the enum and stops. Four modules of private
names had never had to be distinct from anything. The enum was the vaguer name and became
`screen`.

### A measurement taken on content that cannot express the effect is not a measurement

Two of `verify_50`'s probes reported 0 px and both were the probe's fault:

- `correct_normal_matrix = false` on the stock scene — Lesson 4.8's theorem again: a box's
  model-space normals **are** its axes, a diagonal scale sends an axis to a multiple of itself,
  and `normalize()` discards the length. Invisible on crates *by construction*. A squashed
  icosahedron reports 454 px.
- `normals = face` — the stock meshes carry **no vertex normals at all**, so both settings fall
  back to the face normal. `normal_stats::fell_back = 132` said so, which is why that counter
  exists.

### A test that asserts a known defect is not a mistake

`cull_choice` is applied in two places: `collect_triangles` reads exactly one of its four values
(`back_by_forward`, which must happen before the divide) and `draw_triangles` applies the rest via
`fill_style`. `verify_50` pins that — `cull = back, in collect only → 0 px, expected 0`. It stops
one known defect from quietly becoming two, and tells whoever repairs it which line the repair
must change.

### If a refactor does not make the harnesses simpler, it was decoration

`build_verify_49.sh` listed eighteen engine translation units by hand and grew by a line every
time the engine gained a file. It now names one include directory and one archive. All five
earlier harnesses were rebuilt against the new layout and re-run (`ALL PASS`), at a cost of 68
respelled include directives — real work, and worth counting rather than hiding.

## SDL3 main-callback facts, verified at SDL 3.4.12 (Lesson 5.2)

All of the below was read out of `build/_deps/sdl3-src/` rather than remembered. Master prompt
§10 forbids guessing entry-point signatures, and this is the one place where a wrong guess does
not produce a compiler error — it produces a program with no entry point.

| Fact | Value | Source |
|---|---|---|
| Enable the callbacks | `#define SDL_MAIN_USE_CALLBACKS 1` **before** `#include <SDL3/SDL_main.h>` | `SDL_main.h:99` |
| Init | `SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])` | `SDL_main.h:345` |
| Iterate | `SDL_AppResult SDL_AppIterate(void *appstate)` | `SDL_main.h:396` |
| Event | `SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)` | `SDL_main.h:~446` |
| Quit | `void SDL_AppQuit(void *appstate, SDL_AppResult result)` | `SDL_main.h:~485` |
| Results | `SDL_APP_CONTINUE` / `SDL_APP_SUCCESS` / `SDL_APP_FAILURE` | `SDL_init.h:109` |
| Linkage | declared inside `SDL_main.h`'s `extern "C" {` block | so C++ definitions get C linkage automatically |
| Quit always runs | *"called in all cases, even if SDL_AppInit requests termination at startup"* | `SDL_main.h` doc comment |
| SDL inits events itself | `SDL_InitSubSystem(SDL_INIT_EVENTS)` **after** `SDL_AppInit` returns `CONTINUE` | `src/main/SDL_main_callbacks.c` |

### `SDL_main.h` emits a non-inline function definition, and that is the whole rule

Under `SDL_MAIN_USE_CALLBACKS`, `SDL_main.h` ends by including `SDL_main_impl.h`, which emits:

```c
#define SDL_MAIN_CALLBACK_STANDARD 1
int SDL_main(int argc, char **argv)
{
    return SDL_EnterAppMainCallbacks(argc, argv, SDL_AppInit, SDL_AppIterate, SDL_AppEvent, SDL_AppQuit);
}
```

…plus the platform's real `main`/`WinMain`. Neither is `inline`. So:

- **One translation unit per program may include it.** Two is `duplicate symbol _main`. This is
  the One Definition Rule, not an SDL quirk.
- **That file must not define `main()`.** The header ends with `#define main SDL_main`, so a
  `main` written below the include is renamed and collides with SDL's. The header says the app
  *"SHOULD NOT ALSO SUPPLY"* one.
- **The entry point therefore cannot live in a static library.** Not a linker subtlety — it is
  that `libengine.a` defining `main` would impose it on every program that links the engine, with
  no way to opt out, and `demos/sandbox` deliberately wants its own.

Corollary for the umbrella header: `engine/platform/main.hpp` is the one public header **not**
included by `engine.hpp`. An umbrella whose promise is "include everything, it is harmless" must
not be a way to acquire a `main()` by accident.

### The event/iterate ordering guarantee is three lines, and they are readable

The worry when inverting control is that Lesson 1.2's "drain, then `update()`" contract dies,
because events now arrive one at a time from a function SDL calls whenever it likes. It does not,
and the proof is in the tree we already fetch:

```c
/* src/main/SDL_main_callbacks.c */
SDL_AppResult SDL_IterateMainCallbacks(bool pump_events)
{
    if (pump_events) { SDL_PumpEvents(); }
    SDL_DispatchMainCallbackEvents();
    /* …then, only if the result is still CONTINUE, the iterate callback. */
}
```

**When a guarantee moves out of your code, go and read the code that now provides it.** The
contract survived the inversion; its enforcer changed.

Also from the same file: SDL reads its result atom *before* calling iterate, so a callback that
returned `SDL_APP_SUCCESS` is never iterated again. That is worth knowing precisely because it
means a missing guard in your own `iterate` cannot show up in a running program — only in a test
that drives the callbacks directly.

### `SDL_Init(SDL_INIT_VIDEO)` fails where there is no display

Obvious in hindsight, and it invalidated a test we had already written. Lesson 5.1 built
`sandbox --shot` specifically so a refactor could be checked automatically — and the code called
`SDL_Init(SDL_INIT_VIDEO)` first, then never used video. On the machine it was written on that is
free. On a build server it is a hard failure, so the automation-shaped test could not run anywhere
automatic.

The fix is that the surface implies the subsystems rather than the caller requesting them, which
is also why `app_config`'s field is named `extra_subsystems`. The first draft was
`SDL_InitFlags subsystems = SDL_INIT_VIDEO`, which would have forced the class to *subtract* a
flag the caller had explicitly set whenever the surface was headless — **a class quietly
overruling its own configuration is worse than a field with a clumsier name.**

### `event.key.repeat` is information `input::key_pressed()` cannot give you

`SDL_KeyboardEvent` carries `scancode`, `key`, `mod`, `down` and `repeat` (verified at
`SDL_events.h:360`). Holding a key produces a stream of key-down events with `repeat = true`.
`engine::input`'s edge queries collapse that to one edge per physical press, which is usually what
you want — but when you need to distinguish "pressed" from "held down and auto-repeating", the
event hook is the only place the answer exists. That is the clearest single reason `on_event`
earns its place beside `on_fixed_step`.

### Ownership through a `void*`: publish before you can fail

`SDL_AppInit` hands you a `void** appstate` and SDL carries that pointer for the life of the
program. The instinct is to publish it only once construction has succeeded. Do the opposite.

`SDL_AppQuit` runs *in all cases*, including a failed init. If a failure path destroys the
instance and leaves `*appstate` null, then null means both "never built" and "already cleaned up",
and you now have two teardown paths. Publishing first — `release()`, store, *then* do everything
that can fail — gives one owner, one teardown path, and an unowned interval containing no
branches at all.

### 0, 1, 2, 1, 2 — the fixed step is not once per frame, and callbacks make that easier to forget

Five frames from a machine at very close to 60 fps, run through `engine::fixed_step` (h =
0.01666667 s) — measured, not hand-computed:

| frame | dt (s) | steps | alpha |
|---|---|---|---|
| 1 | 0.0161 | **0** | 0.9660 |
| 2 | 0.0172 | 1 | 0.9980 |
| 3 | 0.0170 | **2** | 0.0180 |
| 4 | 0.0165 | 1 | 0.0080 |
| 5 | 0.0333 | 2 | 0.0060 |

Under a hand-written loop the `while` is visible; under `on_fixed_step` it is a function somebody
else calls, and assuming it runs once per frame is much easier. **Edges belong to the frame,
levels belong to the step**: `key_down()` inside a step is safe (input is frame-coherent since
Lesson 1.2), `key_pressed()` is not — on a two-step frame it fires twice from one press.

### Measure with comments stripped

`measure_52.py`'s first run reported that `sandbox` still made four lifecycle SDL calls. One of
them was the sentence *explaining that `SDL_Init(SDL_INIT_VIDEO)` is no longer called there*.
**A measurement that counts its own footnotes is not a measurement.** Strip `//` and `/* */`
before any grep-based count of API usage; string literals are safe as long as the pattern requires
a following `(`.

---

## Course-infrastructure facts (docs/, Lesson 5.2)

### `nofold` was half a feature until something used it

`course.js` has honoured `class="listing nofold"` since the fold shipped, by declining to add an
expand control. `course.css` never did — the `max-height` clamp on `.listing pre` applied
regardless. So a `nofold` listing longer than the 12-line peek was **clamped by CSS and given no
control by JS**: silently unreachable code on a page that looks fine.

Nothing had used the class until Lesson 5.2, and `check-page.js`'s `clippedWithoutToggle` caught it
on the first run. The fix is one rule, `.listing.nofold pre { max-height: none; }`, and the lesson
is general: **a half-implemented opt-out is worse than no opt-out**, because the documentation
promises the behaviour and only one of the two files delivers it.

### Long URLs need `class="reading"` on the Further Reading list

`course.css` has `.reading a { overflow-wrap: anywhere; }` — and *only* there. A Further Reading
`<ul>` written without the class renders link text as one unbreakable token;
`martinfowler.com/bliki/InversionOfControl.html` is 394px wide and pushed a 302px column into
horizontal page scroll at 390px. `pageScrollsX` catches it, but the report is a single boolean, so
finding the culprit needs a walk over every element whose `right` exceeds the viewport **and**
which has no scrollable ancestor — the ancestor test is what filters out the hundreds of tokens
legitimately scrolled off inside a `<pre>`.

### `overPx` in `svgSpill` is not always horizontal

A label sitting below the viewBox floor reports exactly like one running off the right edge. When a
spill makes no sense horizontally, check the figure's height against the y of its last row — in
Lesson 5.2's Figure 6 a caption line added late sat 12 units under a 348-unit viewBox.

---

## SDL3 logging and assertion facts, verified at SDL 3.4.12 (Lesson 5.3)

### `SDL_Log()` is not neutral — it is a specific choice made for you

```c
/* src/SDL_log.c */
void SDL_Log(const char *fmt, ...)
{
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
}
```

So every `SDL_Log` claims to be *the application speaking, at information level*. For a demo that
is exactly right. For an engine it is a lie that cannot be filtered, and it is how this codebase
ended up with 197 calls at one category and one level.

### The default priority table, and why custom categories are free

From `SDL_HINT_LOGGING`'s documentation in `SDL_hints.h`, implemented in `src/SDL_log.c`:

```
app=info, assert=warn, test=verbose, *=error
```

**Every category SDL does not know about defaults to ERROR.** `SDL_LOG_CATEGORY_CUSTOM` is where
SDL stops and applications begin, so any category an app invents lands in the `*` arm. Moving an
engine's messages off `SDL_LOG_CATEGORY_APPLICATION` therefore makes it silent-by-default with no
configuration and no filtering code — which is the single strongest argument for using SDL's
logger instead of wrapping it. A wrapper would have had to reimplement this and would have got a
different answer.

(`DEBUG_INVOCATION=1` in the environment changes the defaults to `assert=warn,test=verbose,*=debug`.
Worth knowing before concluding that your levels are broken.)

### `SDL_SetLogOutputFunction` replaces; chain it if you mean "also"

Call `SDL_GetLogOutputFunction(&prev, &prev_userdata)` *before* installing yours, and call `prev`
from your hook. Otherwise "log to a file" silently costs the user their console, which is not what
anybody means by that phrase. SDL holds a mutex across the callback, so a file sink is thread-safe
for free.

**SDL filters before the hook.** A message below its category's threshold never reaches your
output function at all, so the file and the level are independent controls — and an empty log file
is almost always the level, not the file.

### SDL's assertion level keys off `__OPTIMIZE__`, not `NDEBUG`

```c
#elif defined(_DEBUG) || defined(DEBUG) || \
      (defined(__GNUC__) && !defined(__OPTIMIZE__))
#define SDL_ASSERT_LEVEL 2
#else
#define SDL_ASSERT_LEVEL 1
#endif
```

`-O2` **alone** takes you to level 1 — `SDL_assert` disabled, `SDL_assert_release` live — with no
`-DNDEBUG` anywhere. Measured, per-function code size from the object file:

| | `-O0` | `-O0 -DNDEBUG` | `-O2` | `-O2 -DNDEBUG` |
|---|---|---|---|---|
| empty function | 20 B | 20 B | 4 B | 4 B |
| `ENGINE_LOG_TRACE` | 80 | 28 | 44 | **4** |
| `ENGINE_ASSERT` | 148 | 148 | **4** | **4** |
| `ENGINE_CHECK` | 148 | 148 | 100 | 100 |

Columns 2 and 3 are exact inverses: `-O0 -DNDEBUG` gives live assertions and dead trace logging;
`-O2` gives the opposite. **When you gate your own machinery on a build flag, find out what your
dependencies gate theirs on.**

### `SDL_disabled_assert` wraps the condition in `sizeof`

```c
#define SDL_disabled_assert(condition) \
    do { (void) sizeof ((condition)); } while (SDL_NULL_WHILE_LOOP_CONDITION)
```

Compiled, never evaluated. That is good — the condition cannot rot, and
`assert(fp = fopen(path, "r"))` still compiles and still does not open the file — and it is the
trap that made this lesson's first `ENGINE_VERIFY` wrong:

```cpp
#ifdef NDEBUG
#  define ENGINE_VERIFY(e) ((void)(e))     // fine
#else
#  define ENGINE_VERIFY(e) SDL_assert(e)   // NOT fine at -O2 with no -DNDEBUG
#endif
```

At `-O2` without `NDEBUG` we take the `#else`, SDL is at level 1, and the expression is never
evaluated — the macro whose entire purpose is guaranteed evaluation silently stopped evaluating.
**Measured at 4 bytes: a bare `ret`.** Fix: evaluate into a named `bool` unconditionally and gate
only the check, on `SDL_ASSERT_LEVEL` rather than on `NDEBUG`.

### `SDL_enabled_assert` is a `while` loop, so RETRY genuinely re-tests

```c
while ( !(condition) ) {
    static struct SDL_AssertData sdl_assert_data = { … };
    const SDL_AssertState st = SDL_ReportAssertion(&sdl_assert_data, …);
    if (st == SDL_ASSERTION_RETRY) { continue; }
    else if (st == SDL_ASSERTION_BREAK) { SDL_AssertBreakpoint(); }
    break;
}
```

Fix the state in a debugger, answer RETRY, and the program proceeds as though the bug had not
happened. `SDL_ASSERTION_ALWAYS_IGNORE` latches in that `static`, which is *per expansion site* —
so an assertion fires once and then goes quiet, which is a feature when grinding past a known
glitch and a trap when counting.

### Assertions are testable, and almost nobody tests them

`SDL_SetAssertionHandler` + a handler returning `SDL_ASSERTION_IGNORE`, then walk
`SDL_GetAssertionReport()`'s linked list: condition text, filename, line, `trigger_count`. That is
enough to prove an assertion fires on the input that should fire it, and to prove it does *not*
fire in a build where it was compiled out. Use `IGNORE`, not `ALWAYS_IGNORE` — the latter latches
and your second count comes back short.

### `if constexpr`, not `#if`, for a compile-time log floor

`#define X(...) ((void)0)` in release means the **arguments are never compiled**, so a trace call
naming a since-renamed variable keeps building in release and breaks in debug — discovered by the
person least able to explain it. `if constexpr` with a literal condition discards the statement
(nothing in the object file, and the symbol is not even referenced) while still parsing and
type-checking it. Deletion *plus* type-checking.

### Validate a whole spec before applying any of it

`--log gfx=debug,gpu=nonsens` must be a no-op, not "the first entry took effect". Parsing straight
into `SDL_SetLogPriority` leaves the user with a configuration they did not ask for and cannot
see. Two passes: parse everything into a small stack array, then apply. The array is sized from
the category count, so it needs no allocation.

---

## Course-infrastructure facts (docs/, Lesson 5.3)

### `svgSpill`'s `overPx` is *usually* vertical, in practice

Second lesson running where every `svgSpill` finding turned out to be a note line below the
viewBox floor rather than text off the right edge. The generator computes note positions as
`y0 + i * 14` and the height is a literal — add a line and the last one falls off. Worth checking
the figure's height against its last row *before* hunting for a long string.

### A figure can pass every automated check and still be wrong

`check-page.js` verified geometry, overlap, spill and theming on all six of this lesson's figures,
and two of them had **each other's captions** — the builder's `FIGURES` dict mapped
`FIG_REPORT → l53_fig5.svg` while the generator wrote `l53_fig5.svg = fig_build_matrix`. The page
rendered a table of byte counts under the caption "three subsystems, arrived at independently".

Only a screenshot catches this. The fix that prevents it recurring is to **number the SVG
filenames by page order and keep the generator's map in that order too**, so a filename that
disagrees with its figure number is visible in one file rather than across two.

### Recreate throwaway tooling inside the repo, not `/tmp`

`/tmp` is cleared between sessions, so `check-pages.mjs` and the screenshot driver had to be
rewritten from memory. They now live in `scratch/` (gitignored) where they survive:

```sh
(cd docs && python3 -m http.server 8765 &)
node scratch/check-pages.mjs $(cd docs && ls *.html lessons/*.html)
node scratch/shot-figs.mjs lessons/05-03-logging-and-errors.html scratch/figs53
```

Note also that `pageScrollsX` is a bare boolean in the checker's result, so a reporting loop that
only prints non-empty **arrays** shows a failing page with no reason at all. `check-pages.mjs`
special-cases it.

---

## C++ and container facts (Lesson 5.4)

### A member function named `handle()` blocks the type `handle<T>`

`engine::handle<T>` is a good name and it collides with an idiom this codebase uses everywhere —
`gpu_device::handle()`, `gpu_texture::handle()`, `gpu_mesh::handle()` all return the underlying
SDL object. Inside such a class, unqualified `handle<mesh_data> h;` does not compile:

```
error: explicit qualification required to use member 'handle' from dependent base class
```

Reproduced in four lines. Name lookup finds the member first. The fix is to use an alias
(`mesh_handle`) or qualify (`engine::handle<mesh_data>`), which is why every line of engine and
demo code uses the alias. Worth knowing before naming any short, generic type that shares a word
with a common accessor.

### `pop_back()` does not free what a moved-from element owned

Removing from a dense container by "move the last element into the hole, then `pop_back`" leaves
the moved-from `T` sitting in the vector's *capacity*, and a moved-from `std::vector` is only
required to be "valid but unspecified" — keeping its buffer is a perfectly valid unspecified
state. So a pool of `mesh_data` can hold a freed mesh's megabytes indefinitely while every size
and count reads correctly. Assign `T{}` over the vacated slot before popping.

### Guard the self-move in swap-and-patch

`items_[dead] = std::move(items_[last])` when `dead == last` is self-move-assignment, which is not
required to leave the object in any particular state — a `std::vector` member is entitled to end
up empty. Removing the *last* element is the common case, so an unguarded version is wrong in the
case it will hit most often, and intermittently. `if (dead != last)` is correctness, not an
optimisation.

### A dense index doubling as the occupancy flag costs nothing and cannot desync

`slot::dense == 0xFFFFFFFF` means "this slot holds nothing". A separate `bool occupied` would be
a byte per slot, a second thing to update on every insert and remove, and a second thing to forget
to check. One sentinel answers both questions and cannot disagree with itself.

### Bump the generation on *removal*, never on insertion

Bumping on free means a vacated slot immediately carries a value no outstanding handle holds, so
the window between free and re-allocation needs no separate flag. Bumping on allocate leaves that
window open, and closing it needs exactly the extra flag the sentinel above avoided.

### Resolving a handle costs about 0.13 ns more than a dereference

Measured over 4,096 objects × 20,000 reps, alternating the two loops so thermal drift hits both:
`pool.get(h)` 0.98 ns against a raw `const T*` at 0.85 ns, stable across five runs. That is well
under a cycle — the two extra loads are adjacent in one cache line and sit in the shadow of a
dependent load that misses anyway. It is negligible **once per object** and would not be once per
vertex, which is the entire argument for resolving at the top of the object loop rather than at
each use.

### LIFO free lists concentrate generation churn; FIFO spreads it

Reusing the most recently freed slot is the cheapest insert (that slot is still in L1) and it
means a hot allocate/free pair hammers *one* slot's generation — which is the only thing that
makes a 12-bit generation wrap reachable at all. FIFO reuse multiplies time-to-wrap by the number
of slots and costs a `deque` and some cache locality. Know which one you picked and why.

### The "handle from the wrong pool" hazard turned up the same afternoon

Lesson 5.4's Pitfall 5 warns that the type system cannot catch a handle minted by a *different*
pool of the same type. `scratch/verify_50.cpp` hit it immediately: it built a `scene_object` by
hand with `geometry = icosahedron_mesh()`, which is now a type error, and the obvious fix — mint a
handle in the harness and assign it — would have produced a handle the render's own pool could not
resolve. The honest fix is that **a struct that describes an object for somebody else to render
cannot carry a handle at all**; it carries a `mesh` view and the renderer's pool owner stores it.

General rule: a handle is only meaningful alongside the pool that issued it, so any type that
crosses a boundary where the pool changes should carry the *data* or take the pool with it.

### Adding a header can give a previously SDL-free header an SDL dependency

`gfx/mesh.hpp` included no SDL until it included `core/pool.hpp`, which includes `core/log.hpp`
for its one warning, which includes `SDL_log.h`. Harmless here — `SDL3::SDL3` is `PUBLIC` on the
engine target, and all 44 public headers still compile standalone — but worth noticing, because a
container template pulling in a logging dependency is the kind of thing that is free until the day
somebody wants the container somewhere SDL is not.

### Count, do not increment

Lesson 5.3's STATE recorded "43 public headers"; `find engine/include -name '*.hpp' | wc -l` said
42 before 5.4 and 44 after. The 43 was arrived at by adding to a previous number rather than by
counting, and it had been wrong for a lesson. Any number in a document that can be produced by a
one-line command should be produced by that command every time it is written down.

---

## Asset-system facts (Lesson 5.5)

### `SDL_GetPathInfo(path, NULL)` is the documented existence check — and it hands you the size

SDL3's `SDL_GetPathInfo` "returns true on success or false if the file doesn't exist", and fills
an `SDL_PathInfo` with `type`, `size` and three timestamps. So a search path can report which root
answered *and* how many bytes the file is without a second syscall. Test
`info.type == SDL_PATHTYPE_FILE`: a **directory** of the right name is otherwise a hit, and the
caller then fails much later, inside a loader, with a far worse message. Both are SDL 3.2.0.

### A cache's map must nest a `contains()` on the pool

`std::unordered_map<key, handle>` is a **hint**, not the truth. If an asset is freed by handle,
the map entry survives and looks live. Every probe therefore reads

```cpp
if (auto it = by_key_.find(key); it != by_key_.end()) {
    if (pool_.contains(it->second)) { /* hit */ }
    by_key_.erase(it);            // the pool freed it; the map catches up
}
```

This is only possible because a generational handle cannot lie about whether its asset exists. A
pointer-keyed cache has no way to ask, which is precisely the 4.8 mesh cache's problem.

### The default configuration must serialise to nothing

`asset_key(name, settings)` originally always appended `"|flip=1"` or `"|flip=0"`. Generated
content (`insert_mesh("cube", …)`) has no settings and was stored under the bare name, so
`find_mesh("cube")` — which encodes default settings — looked up `"cube|flip=1"` and missed.
**Two key spaces wearing one name**, silent, no crash, no log. Making the *default* encode to
nothing puts generated and loaded content in one key space by construction. Same shape as
reserving generation 0 so an all-bits-zero handle is null for free.

### Collect, then recurse, when a cascade erases from the container it walks

`release_mesh` walks `mesh_derivations_` looking for children and recurses; the recursion erases
from that vector. With **one** dependent per source — which is every mesh the demo has — the
unguarded version is correct. Copy the children into a local first. The worst class of bug:
correct in every test you would think to write.

### Timing stages separately double-counts unless you know what nests inside what

The first version of `measure_55.py` stacked `read` beside `parse` and summed them. `load_obj`
**opens the file itself**, so the separately-timed read is a *subset* of the parse. It also
stacked `validate`, which the asset store does not perform at all. The bar totalled 6.20 ms for
an operation that takes 3.69. Before stacking timings, write down which call contains which.

### On this machine, an OBJ acquire is 99.9% parse

torus.obj, 200 KB, 1,225 vertices: resolve 0.0022 ms, read 0.0148, parse 3.6922, uv flip 0.0034,
store into the pool ~0. "Asset loading is slow because disks are slow" is a sentence from a
different decade. It is also why a *cooked* format is a real optimisation and a faster text format
is not — the win is not reading fewer bytes, it is not parsing them.

And a finding that fell out of separating the stages: the demo's `validate()` pass costs **2.45 ms,
another 66% of the acquire**, and has been quietly doubling load times since Module 3. Cost you
can see is cost you can choose.

### A member function hides a namespace-scope function of the same name

`asset_store::load_image` hides `engine::load_image`, so an unqualified call inside the class
fails with "too few arguments" — a confusing way to be told about name lookup. Qualify it. This is
the second lesson running to hit the shape (5.4: a member `handle()` blocking the type
`handle<T>`); short, generic names collide with accessor idioms.

### Test edits are honest only when they record what changed

Lesson 5.5's `load_model` stopped freeing the previous mesh (a load is not a free), which broke a
`verify_54` check written one lesson earlier asserting the opposite. The harness was right to
fail. The fix is to assert the *new* contract with the reason written next to it — a test edited
to match the code teaches nothing unless the edit says what moved.

---

## Measurement facts (Lesson 5.6)

### Three ways a microbenchmark lies, all three hit in one afternoon

1. **The timer is coarser than the work.** `SDL_GetPerformanceFrequency()` is 24 MHz on this
   machine, so one tick is 41.67 ns and a pass over four objects finishes inside one. Every sample
   read 0 or 1 ticks and the table said `0.0000 ns/item`. *Tell:* a suspiciously round number,
   especially zero. *Fix:* repeat the pass inside the timed region until a sample is ~100 µs.
2. **The compiler deleted the loop.** Repeating a pure function of nothing lets it be computed
   once. The tree arm read 0.166 ns/item — 2.3 cycles for four matrix builds of nine multiplies
   and sixteen stores each. *Tell:* a number that is not physically possible. *Fix:* make each
   pass genuinely different work — a per-pass bias added **per element**, so the running sum's
   rounding depends on it and FP non-associativity forbids hoisting.
3. **The accumulator is the bottleneck.** `hits += 1.0` is a serial chain of ~4-cycle double
   additions against a body of three multiplies. Both arms were latency-bound and the layout
   difference was completely masked. *Tell:* two arms that should differ, agreeing exactly.
   *Fix:* an `int` counter.

### Sanity-check every published number against the hardware

ns → cycles → instructions, and instructions are countable. All three lies above were found by
asking "could the machine physically do that", not by suspecting the code. No tool finds this one.

### An effect that survives the removal of its explanation had a different explanation

SoA beat a flat array by 0.66× on a full-transform loop, identically at 384 bytes and at 9.6 MB.
The cache story fits perfectly and is wrong: rebuild with `-fno-vectorize -fno-slp-vectorize` and
the win is **1.00× at every size**. It was auto-vectorisation — a contiguous `mat3` array
vectorises, a 96-byte stride does not. The *same* header's cull workload (12 of 96 bytes) keeps a
0.36× win with the vectoriser off, and only above the knee: that one really is the cache.

The general move: find the knob that disables your hypothesised mechanism and see whether the
effect goes with it.

### The real rule is not "use SoA"

Split the data a loop does not read away from the data it does, and **the size of the prize is the
fraction you leave behind**. 60 of 96 bytes buys nothing from the memory system; 12 of 96 buys 5×.

### An array of pointers is not a linked list

Both are "pointer chasing" and they differ by 3×. An array's addresses are known in advance, so a
core issues a dozen dependent loads before any returns and the misses **overlap** — 2.38× at
100,000 objects. A linked list stores each address inside the previous node, so the misses are a
serial dependency chain and **add** — 6.47×. That, not the hierarchy and not the OOP, is what
makes a decayed scene tree slow.

### A virtual call costs inlining, not prediction

1.5–1.7× on this workload, and *monomorphic and polymorphic agree within 4% at every size* — so it
is not branch misprediction. It is also flat across N, including four objects inside L1 — so it is
not the vtable load. What is left is that the compiler cannot see through the call.

### Dead-code elimination is not symmetric across arms of different inlinability

The accumulator read 5 of a matrix's 16 entries. `parent_from_local` is inlined into the flat,
pointer, SoA and tree arms — so the compiler could skip computing the other 11 — and it **cannot**
see through the virtual call. The virtual arm built whole matrices while its rivals built a third
of one. "The cost of `virtual`" read **3.4×**; reading all 16 entries it reads **1.7×**.

### You cannot make the allocator fragment, so measure what it did

A benchmark placed spacers between its heap objects so that "pointer chasing" would not quietly
measure a contiguous walk. 24-byte spacers between 96-byte objects went into a different size class
and separated nothing — **497 of 499 consecutive objects exactly `sizeof(T)` apart**. Matching the
sizes moved a headline from 1.09× to 1.98×, which looked like the fix.

It is not a fix. The *same layout code* gives different answers in different processes:

| process | n | consecutive objects `sizeof(T)` apart |
|---|---|---|
| the benchmark, `-O2` | 4 … 100,000 | **0** at every size |
| the benchmark, `-fno-vectorize` | 100,000 | 99,411 of 99,999 |
| a standalone probe | 100,000 | 0 of 99,999 |
| a standalone probe | 500 | 485 of 499 |

macOS's allocator interleaves same-size blocks, or does not, depending on that size class's
magazine state — which depends on everything the process allocated earlier. **Adjacency is a
property of the process, not of the layout**, no separate harness can assert another program's heap
state, and matching the spacer size only changes the odds.

So the resolution is a protocol, not a fix: the benchmark **reports its own allocation layout at
every size**, the driver flags any build whose objects came out contiguous, and the test asserts
only what the code controls (that the spacers were allocated) and prints the rest.
**A number whose provenance you cannot state is a number you should not publish.**

### Alternating two arms introduces cache interference

Alternation is the fix for thermal drift (3.10's pitfall) and it has its own cost: at large N the
arms evict each other, and the *same* flat arm read 0.98 ns/item in one pairing and 1.83 in
another. Quote **ratios within a pairing**, never an absolute from one pairing against an absolute
from another.

### Do not assert a property of the data and call it a property of the design

A check asserted "at least one reordering arm is not bit-identical". It failed: at n = 2,000 all
three reorderings happened to land on the same bits. Whether a reordering differs is a property of
the numbers. Replaced with `(a+b)-a != (a-a)+b` at 1e16, which is true regardless of the data.

## ECS storage and measurement facts (Lesson 5.7)

### A design can be measured before it is built, when the candidates differ in an access pattern

Archetype and sparse set differ in exactly **two** operations — reading K components of every
matching entity, and adding or removing one component from one entity. Everything else about them
(entity creation, single-component get, registry, scheduling) is either identical in shape or
orthogonal. So both were simulated in ~300 lines with **no entity manager, no component registry,
no type erasure, no views and no scheduler**, and timed against each other. That is worth
generalising: when an architecture argument reduces to "which of these two access patterns is
cheaper", you can answer it in an afternoon instead of building both and then defending the sunk
cost.

The corollary is a duty: **say what the simulation gives away.** The probe's archetype has
statically typed columns, so it pays no per-archetype column lookup and no type-erased stride the
compiler cannot see. Every archetype number is therefore an *upper bound* on a real archetype's,
and the lesson has to state that rather than quietly benefit from it.

### The sparse-set redirect is latency, and latency hides behind work

`data[sparse[e]]` is a two-deep dependency chain, which is the shape 5.6 measured at 6.47× for a
linked list. It is not the same cost, because the chains of *different entities* are independent
and the entity ids that start them come off a dense array sixteen to a cache line — 5.6's
memory-level parallelism, so the misses overlap.

With the vectoriser off, so codegen is held still, the redirect on a real body (build a model
matrix per entity) costs:

| entities | 4 | 100 | 1,000 | 10,000 | 100,000 |
|---|---|---|---|---|---|
| pools aligned | 0.99× | 0.99× | 0.98× | 1.02× | 1.02× |
| pools scrambled | 1.00× | 1.00× | 1.01× | 1.21× | **1.39×** |

**1.00× up to a thousand entities on the worst-case world.** Two conditions must hold together
before it costs anything: the working set must exceed cache (so the latency to hide is ~200 cycles
rather than ~4) *and* the pools' dense orders must have diverged (so the prefetcher cannot have
fetched the line already). Either alone is free. On a cheap body with nothing to hide behind, the
same code pays up to **2.79×** — so the amount of arithmetic a system does per entity is a
first-class parameter of any such comparison, and quoting one number for "sparse set overhead"
without stating it is meaningless.

### A cost model in bytes under-predicts when the bytes live in separate allocations

An archetype move copies an entity's components out of one chunk and into another and re-packs the
source. Counting bytes: 92 B travelling + 92 B re-packed ≈ 200 B for a 4-component entity, ≈ 456 B
for a 12-component one, predicting **2.3×**. Measured at 100,000 entities: 13.10 ns → 58.48 ns,
which is **4.5×**.

The missing term is that **a column is a separate allocation**. Eight extra components are eight
more vectors in eight unrelated places, each touched *twice* per move (the entity's row, and the
chunk's last row moved into the hole). 45.4 ns for sixteen additional independent touches is
~2.8 ns each, which is the right order for a partially-overlapped memory access. The model to
carry forward is **independent memory streams touched**, not bytes moved.

Meanwhile the sparse set went 4.25 → 4.23 ns across the same widening, because an insert is three
writes into one pool and has no way to discover what else the entity owns.

### A group is an archetype you can add later — and that asymmetry decides the design

A pool's dense order is nobody else's business: no id outside the pool depends on where an element
sits. So two pools can be **sorted into a common order**, putting the entities that have both
components at the front of both dense arrays in the same sequence. Index *i* of one then means the
same entity as index *i* of the other, the query reads **no sparse entry at all**, and what it
walks is byte-identical to an archetype chunk. Measured at **0.99×–1.01×** of a real archetype, on
the archetype's own best case (a query matching one entity in four, where the ungrouped sparse set
pays 1.46×–1.94×).

EnTT calls this a group; it is a maintenance obligation on top of a storage design, not a
different one. **The migration only runs one way** — a sparse set can be given an archetype's
query later, incrementally, guided by a profile; an archetype cannot be given O(1) structural
change at any price, because moving between archetypes *is* what an archetype is. When two designs
are close on the numbers, prefer the one that can become the other.

### Convenience is admissible as a tiebreaker and inadmissible as evidence

`engine::pool<T>` (5.4) already *is* a sparse set: `slots_` sparse and stable, `items_` dense and
packed, `owners_` the way back, plus generations. Noticing that is worth real credit and it is not
an argument — adopting sparse sets *because* we own one is choosing an architecture by an accident
of what meshes needed two lessons earlier. Measure first; reach for the convenience only after the
numbers have decided. (What `pool<T>` actually lacks is one **shared** id space: each pool mints
its own slot indices, so a handle from one means nothing to another.)

### Measure the argument against your own position too

The standard case against archetypes is fragmentation. Measured — the same entities split across
up to 4,096 chunks — it is **1.12× at worst**, at nineteen entities per chunk. Splitting a
contiguous array into 4,096 contiguous arrays does not stop it being contiguous. The lesson
publishes that even though it argues for sparse sets, and separately marks what it did *not*
measure (query matching over thousands of archetypes, per-archetype column lookup, allocator
pressure) so the reader knows which part of the claim is evidence and which is silence.

### The bug that needs N tries: write the index map after the re-pack

`archetype_churn::add_material` recorded where the entity landed in the destination chunk, then
called `pop_row` on the source. `pop_row` moves the source's last row into the hole and patches
**that** entity's row index — and when the moved entity was itself the last row, the stale index
overwrites the fresh one. Silent, no crash, one entity corrupted.

A single-frame test passes forever. It was caught because `verify_57` §E churns **200 frames** and
then walks the entire row map. **For a 1-in-N bug, the test has to run N times** — and the cheap
way to get that is to repeat the operation rather than to enumerate the cases.

## Course-infrastructure facts (docs/, Lesson 5.7)

### A colour pattern carried by fill alone does not read

`figs_*.py`'s `box()` emits `fill="{colour}" stroke="{colour}"` plus `fill-opacity`, so the
**stroke is always full opacity**. Figure 6 drew 32 small elements at fill-opacity 0.26 and 0.07 to
show "one element in four is wanted" and rendered as 32 identical amber outlines — the whole point
of the row, invisible. When a diagram distinguishes elements by weight, mute the stroke as well:
emit the rect directly with `stroke-opacity` (and a grey stroke for the de-emphasised ones).

### `eval(check-page.js)` returns a Promise

`const r = eval(src); return { pass: r.pass }` yields `undefined` for every field, and the MCP
result serializer **drops undefined keys** — so the output looks like a small, clean result rather
than an error. Either `await eval(src)` or `return eval(src)` directly and let Playwright await it.

### `figure.dia:nth-of-type(N)` counts every `<figure>`, listings included

Code listings are `<figure class="listing">`, so `nth-of-type` numbers them alongside diagrams and
asking for figure 6 hands you figure 4. Use Playwright's `figure.dia >> nth=N`, which indexes the
matched set rather than the sibling type.

### Two adjacent SVG text runs with no space between them — the third occurrence

5.5 fig 6, now 5.7 fig 3: `"AND THE COST IS PER COLUMN."` at x=24 ends at ~x=176 in the 9.5 px
face, and the following run started at x=178. No check looks for it — `svgTextOverlap` needs >2 px
of actual overlap — so only reading the rendered figure finds it. Leave ≥ 12 px between the end of
one run and the start of the next when they share a line.

### An annotation that contradicts its own caption

Figure 3 drew a dashed box around the material pool — the one thing that *is* in the picture —
under a caption whose point was the pools that are *not*. It passed every automated check because
it is geometrically fine. Read each figure against its own caption once, out loud.

## ECS runtime facts (Lesson 5.8)

### The one thing a slot map lacks is a shared id space, and it is structural

`engine::pool<T>` from Lesson 5.4 is a sparse set in every respect — `slots_` sparse and stable,
`items_` dense and mobile, `owners_` the way back, generations, O(1) everything. What it cannot do
is answer "does the thing in mesh slot 7 also have a texture?", and the reason is not that the
answer is slow: **each pool mints its own keys off its own free list, so the question is not well
formed.** An ECS is the arrangement in which it is. Mint the id once and hand it to every pool;
everything else — views, systems, composition — is a consequence of that single inversion. Recorded
because it is easy to look at 5.4's three arrays and conclude the ECS is already written.

### Reuse the bit layout, not the type

`entity` uses `core/handle.hpp`'s constants (20 index / 12 generation, generation 0 reserved, bump
on removal) so the bit budget has exactly one home — and is still a **different type**, because
`handle<mesh_data>` names an item *in* one container while an entity names a row *across* every
pool there is. Aliasing them compiles, and then `pool<T>::get(handle<T>)` and a component lookup
are the same spelling for opposite operations. The phantom parameter exists to stop ids mixing;
spending it to make them mix is an odd use of it.

### Store the whole id in the dense array, or the third test cannot be written

`contains()` is three tests: index in range, sparse entry not the "absent" sentinel, and
`dense_[at] == e`. Only the third rejects a **stale or recycled** id, and it is writable only
because `dense_` holds the full 32-bit entity word rather than a bare index. Storing an index saves
nothing and reinstates Lesson 5.4's *aliasing* failure — a probable crash converted into a
guaranteed silent wrong answer.

### The sparse patch is one line and its absence has no local symptom

```cpp
if (at != last)
{
    dense_[at] = dense_[last];
    data_[at]  = std::move(data_[last]);
    sparse_[dense_[at].index()] = at;   // <- this one
}
```

The entity swapped into the hole did not ask to move, and its sparse entry still names its old
position. Leave the patch out and the pool is not obviously broken but **silently** broken: one
entity reads another's data, an arbitrary number of frames later, in a different system. The
invariant that catches the whole class in one line is `sparse_[dense_[i].index()] == i` for every
`i`, asserted after a few hundred frames of churn — not after one.

### The last-element case is saved by the order, not by the branch

When `at == last` the entity that "moves" *is* the entity being erased. If the swap ran anyway it
would copy the element onto itself and patch its sparse entry to the position it already occupies —
immediately before the final `sparse_[e] = k_none` overwrites it. Harmless either way. **A test
that only ever erases from the middle passes forever**, so the last-element erase needs its own
check.

### Type erasure without RTTI: the cast is justified by construction

`component_id_of<T>()` is a monotonic counter behind a function-local static; the id indexes
`vector<unique_ptr<pool_base>>`, and `static_cast<pool<T>*>` recovers the type. That is safe
because **the id is what created the pool** — the object at that slot was constructed as a
`pool<T>` and nothing else can ever be stored there. This is the difference between a cast that is
safe and a cast that is merely usually right, and it is why `dynamic_cast` is not merely
unavailable but unnecessary.

Two consequences worth writing down. The ids are **global to the program**, not per registry, so a
registry using only the tenth type ever registered allocates eleven slots, ten of them null —
about eighty wasted bytes, in exchange for an array index instead of a hash. And the magic-static
initialisation is thread-safe while the **counter increment is not**: touch every component type
once before any job system starts.

### A type-erased base is fine as long as every virtual on it is cold

Lesson 5.6 measured virtual dispatch on an iteration at 1.5–1.7× *at every world size*, four
objects included, so "no virtual on the ECS hot path" is a constraint rather than a preference.
`pool_base` keeps it by having only cold virtuals: `erase` (once per pool per entity destruction),
`clear` (teardown), `size`, and `entities()` (once per **view**, never per entity). The hot path
holds concrete `pool<T>*`; `pool<T>` is `final` so even the cold calls devirtualise. **Design the
base around what may be cold, rather than adding virtuals and hoping.**

### One type-independent accessor removes all the metaprogramming

`entities()` returns `std::span<const entity>` for *every* `T`. That single signature is what turns
"lead with the smallest pool" into five lines:

```cpp
const std::span<const entity> spans[] = {pools->entities()...};
for (std::size_t i = 1; i < sizeof...(Ts); ++i)
    if (spans[i].size() < spans[lead_].size()) { lead_ = i; }
```

A pack of differently-typed pointers becomes an ordinary array, and the choice is a `for` loop.
**When a variadic problem looks like it needs dispatch, look for the projection that erases the
type first** — the type-dependent part (fetching components) can stay in the pack expansion where
it belongs.

### The lead pool pays twice

Fewer candidates is the advertised win — over pools of 60 / 30 / 12 the walk considers twelve
rather than sixty, six rejections instead of fifty-four, for the same six answers. The unadvertised
one: for the lead pool the dense position **is the loop counter**, so its own component needs no
sparse read at all. Only the other *K*−1 pools pay the redirect. `I == lead_` compares a
compile-time constant against a value fixed for the whole loop, so the branch predicts perfectly
after the first iteration.

### A rule nothing can observe quietly stops being true

Every correctness test of a view passes just as well if the view walks the *biggest* pool — the
answer is identical, only slower. So `view` exposes `lead()` and `size_hint()` purely so the
harness can assert that a 60/30/12 view leads with the 12. **When a decision is about behaviour
rather than output, give it an observable and test the observable.**

### Do not cache the span you are about to iterate

The view re-reads `lead_pool_->entities()` at the top of every walk instead of caching it at
construction. A cached span dangles the moment anything inserts into the lead pool between building
the view and walking it — a bug that reproduces only when a vector happens to reallocate, which is
the worst possible schedule for finding it. One virtual call per walk (not per entity) buys the
whole class away.

### Two containers may share a name if the namespace explains the relationship

`engine::pool<T>` and `engine::ecs::pool<T>` are the same three arrays doing opposite jobs: one
mints its own keys, the other is keyed by an id it did not mint. Renaming the second to
`component_storage` would have hidden a relationship the course spends two lessons drawing out. The
nesting keeps them apart, both header comments state the difference, and the one arrangement that
breaks — `using namespace engine;` plus `using namespace engine::ecs;` — produces a compiler
ambiguity, which is the good kind of breakage.

### Two independent defences mean a single targeted test proves nothing

A recycled entity does not inherit its predecessor's components for **two** reasons:
`registry::destroy` erases the rows, *and* the generation moved so `dense_[at] == e` fails. Either
alone would prevent the symptom, so a test aimed at the symptom passes with one of them broken.
That is why the harness runs 200 frames of churn against an independent `std::map` shadow model
that shares no code with the thing under test, and then walks every invariant.

### The free list's real payoff is the size of every sparse array

Because a freed slot is reused before a new one is minted, the id space's high-water mark is **peak
simultaneously live entities**, not entities ever created — measured at **220 slots after creating
518 entities** over 200 frames of churn. Every component pool's sparse array is sized by that same
number, which is the entire reason paged sparse arrays can wait. The failure mode is the one 5.4
already named: high-water marks do not come back down, so **things that exist in millions and die
in seconds should not be entities.**

---

## Course-infrastructure facts (docs/, Lesson 5.8)

### Figure numbers follow page order — for the third time

`figs_58.py` authored the type-erasure figure before the view figure, so `l58_fig5.svg` was the
erasure one while §3.4 (the view) came first on the page. The reader saw "Figure 6" above
"Figure 5". Caught by asking the DOM for `.fignum` and `<title>` together rather than by reading
the source. **Number figures by where they land, name the file to match, and put the section in a
comment** — this is the same trap as 5.3 and 5.6.

### `browser_navigate` to a URL you just rebuilt can serve the old page

A figure fix looked like it had not applied: the check reported `unique_ptr<pool_base>` overflowing
its box when the SVG on disk had already been split into two lines. The page was cached. `grep` the
built file first, then force `location.reload(true)` (and `fetch(..., {cache:'no-store'})` for the
checker itself) before believing a post-rebuild verification result.

### A label can overflow its own box without spilling the viewBox

`check-page.js` catches labels outside the SVG, labels on top of each other, and labels on strokes
— none of which sees a 21-character monospace run in a 108-unit box. `unique_ptr<pool_base>` at
9.5px is about 120px wide; it rendered across the box edges in every one of five columns. A
throwaway check (find the `<rect>` whose bounds contain the text's vertical centre, compare
horizontal extents) found all five at once and is worth keeping in the authoring pass.

### A diagram that carries a derived value must be re-derived, not pattern-matched

Figure 3's third panel showed the free list as `[1]` after `create()` had popped slot 1 — because
the code derived the display from "is this panel highlighted?" rather than from what the free list
actually holds. The panels were right about generations and wrong about the list, in a figure whose
whole subject is reuse. **Spell out per-panel state explicitly instead of deriving it from a
rendering flag.**

### Count what you claim, including in the figure's own text

The lesson said "five component types" in six places — the deck, the milestone, a figure caption,
the figure's `<title>`, the figure's on-canvas heading, and the index card — while the demo has six
(`spin` was omitted). Only building the demo and reading its own printed totals caught it. Two
sub-lessons: the SVG's accessible `<title>`/`<desc>` are prose too and drift with the rest, and a
program that prints its own arithmetic (`121 entities, 468 components, 5 pools, 97 drawn`) turns a
claim into something checkable — 4 + 384 + 32 + 48 = 468 can be verified by hand.

### `pool_count()` is 5 while the demo has 6 component types, and that is the feature

A component type nobody has attached has **no pool**, because every read path goes through
`storage_if<T>()` rather than `storage<T>()`. Nothing has a `lifetime` until `[Space]` spawns a
spark, so the at-rest count is five. Worth stating in the lesson rather than quietly reconciling:
if `has<T>()` created a pool as a side effect, asking a question would be a mutation and a `const`
registry would be impossible.

### A retired build script keeps stamping what the rule retired

`build_57.py` still had the `<details class="state">` block in its `TAIL` and still read
`l57_state.txt`, months after the STATE-block consolidation stripped that markup from all 53 pages.
Re-running it to repoint one navigation link would have re-added a 331 KB block. Fixed by editing
the builder *and* proving it: rebuilding 5.7 to a temp file and `diff`-ing it against the shipped
page, which also caught a one-line whitespace drift left by the original strip. **When a builder is
the source of a page, "the page is correct" is not the same claim as "the builder is correct".**

---

## Transform hierarchy facts (Lesson 5.9)

### Name the function for the day it will be needed, and that day costs nothing

`math/transform.hpp` has called its composition function `parent_from_local` since Lesson 2.8,
with a comment explaining that in Module 2 an object's parent *is* the world and that Module 5
would widen the meaning. Adding a hierarchy seven lessons later changed **not one character** of
that file, or of `transform`. The general form: when you know a concept will generalise, name it
for the general case and document why it currently looks like the special one. The alternative —
`world_from_local` — would have been a lie that had to be gone back and corrected in every
caller.

### The maths of a scene graph is one line; the ordering constraint is the whole problem

`world(child) = world(parent) × parent_from_local(child)` reads the parent's *world* matrix, so
the parent must be finished first. That is a constraint on the **graph**, and a component pool's
dense order is insertion order — the two are in direct tension, and reconciling them is what the
lesson costs. Anyone who says a hierarchy is "just a matrix multiply" has not said where the
multiply happens in the loop.

### Depth is a topological sort, and that is the cheapest one available

A parent's depth is always exactly one less than its child's, so **sorting by depth puts every
parent before every child** — and a counting sort by a small integer key is O(*n*). Two things
follow, and the second is the one worth having: the resolve loop needs no visited set and no "has
my parent been done yet" test, because the order guarantees what those would check; and **within
a level nothing depends on anything else in it**, so a level is a `parallel_for`. That parallelism
was not designed. It arrived with the choice of order.

### Recursion's locality advantage loses to level order's write pattern

A depth-first walk has the best temporal locality available — the parent's matrix was written one
call ago and is certainly in L1 — and it still loses, because its *writes* jump. Measured at
100,000 entities with only the shape varying: recursion runs 3.34 → 13.55 ns/entity from depth 1
to 32, level order 3.39 → 4.94. **Recursion is depth-dependent; level order is very nearly not**,
and it saturates around depth 8. The lesson generalises: when two orders trade read locality
against write locality, measure, because the intuition points the wrong way as often as not.

### A control row that costs nothing is worth more than an extra data point

The depth-1 row — no hierarchy at all, both arms doing identical work, ratio 1.01× — is what says
the harness measures the tree rather than itself. Had it come out at 0.6× or 1.4×, nothing else in
the table would have meant anything. **Every ratio table wants a row where the ratio must be 1.**

### Split the operation that runs on change from the one that runs every frame

`rebuild()` (produce the level order) costs about **one resolve**, measured at 0.93–1.39. A game
re-parents rarely and moves things constantly, so rebuilding only when the shape changes is worth
more than any choice of visit order. This is the same shape as a dirty flag, applied at the level
where the ratio of "changes" to "frames" is genuinely enormous rather than merely favourable.

### The amplification is what makes dirty flags stop paying

Moving 10% of a depth-8 tree dirties **36%** of it, because every descendant of a moved entity has
to move too; 25% moved reaches two thirds of the world. So the crossover sits at about a quarter
moved, and past it the "optimisation" is *slower* — 1.29× when everything moves, which is exactly
what an animated scene does. **Quote the reach, not the fraction you marked.** A partial-update
scheme's cost is set by its closure, not by its input.

### Publish what the container costs on top of the algorithm

The probe measured a visit order over three plain arrays; the shipped resolver walks the same
order through `registry` and `pool<T>` and costs **1.22–1.40×** more — three sparse lookups per
entity instead of three array reads. Quoting the probe's number for shipped code would be quoting
a measurement of something else. Measure both, publish both, and the gap tells you whether a
caching exercise is worth attempting.

### A hang is a different class of failure from a wrong picture

A parent cycle does not produce a bad frame; it produces no frame. So it gets two independent
defences, and the second exists because the first is not enough: `set_parent()` refuses to create
one (walking the *whole* chain — a one-link check misses a grandchild), and `rebuild()` survives
one written directly into the component, because `parent` is public data and a scene file can
carry anything. Break it, count it, log it, and keep resolving everything else — **one bad link
must not cost the frame.**

### Choose an orphan policy, state it, and give the caller the other one

A child whose parent died becomes a root and is *counted*. Destroying the subtree is a policy a
game may want and a transform system must not impose — silently deleting entities during a resolve
pass is a surprise nobody asked for. So the engine ships `destroy_subtree()` as an explicit,
named alternative. The general rule: when two behaviours are both defensible, the *less
destructive* one is the silent default and the other one gets a function whose name says it.

### Scale composes down the chain, and it is the one that surprises people

A child under a parent of scale 0.16 is 0.16 of its authored size; under two such parents, 0.0256.
The demo's moons are deliberately built this way, with the arithmetic in a comment, because the
first time a "1-metre" prop turns out to be 20 cm everybody assumes a bug. It is not a bug in the
composition — it is what "relative to my parent" means, applied to size.

### `transform` can carry any affine matrix exactly, and that is a bridge rather than a design

`parent_from_local` builds `affine(columns scaled by scale, position)` and `transform::rotation`
is a general `mat3` with no orthonormality requirement, so
`{position = translation_of(m), rotation = linear_of(m), scale = 1}` reproduces **any** affine
matrix bit for bit. That let the demo feed composed world matrices into a renderer that wants a
`transform`, with no engine change and therefore no risk to the golden. It works because a field
named `rotation` is typed as something more general than a rotation — which is exactly why it is
a temporary bridge and is labelled as one.

---

## Measurement and tooling facts (Lesson 5.9)

### A `double` accumulator over `float` data makes order-dependence disappear

Lesson 5.6 established that two arms visiting the same elements in different orders sum them
differently, so `bench_ab::agree` should be false. In 5.9 every arm reported `agree`, exactly, and
the reason is the widening: each addend is a `float` (24-bit mantissa) going into a `double` (53),
and a running sum of 10⁵ values of magnitude ~1 never exceeds 2¹⁷ — so every partial sum needs at
most 17 + 24 = **41 significant bits and nothing is ever rounded**. An exact sum is
order-independent. The knob that proves it: change the accumulator to `float` and the agreement
vanishes (49928.1484 forwards, 49928.2734 backwards). **When a rule you trust does not fire, find
out why before assuming the test is broken.**

### `DIFFER` can be the correct outcome, and the harness has to say which kind it is

The dirty arm legitimately disagrees with the full pass's checksum, because it touched fewer rows
and therefore summed fewer of them. Its claim is not "same checksum" but "same *matrices*", and
that is checked element-by-element in the verification harness with no clock running. **Put the
exactness claim where exactness is affordable**, and make the benchmark say which of its rows are
expected to differ.

### Swap-and-pop makes some scrambles impossible, and a test can pass because its situation never arose

Producing a pool whose dense order puts children before parents took three attempts. Destroying
leaves scrambled nothing — swap-and-pop fills a hole with the **last** element, and in a tree built
level by level the last elements are leaves, so a leaf replaced a leaf. Destroying fillers created
at the *end* failed identically. What works is destroying something **early** while deep entities
sit at the end. Twice in a row the test passed for a reason that had nothing to do with the code
under test, which is the failure mode a test of an *ordering* property is most prone to. The
assertion is now on a count greater than zero, not on the answer being right.

### A rule nothing observes is a rule that quietly stops being true — again

Every correctness test of the resolver passes just as well if it walks in the wrong order and
happens to get lucky, because the *answer* is the same whatever order you compute it in. So
`hierarchy` exposes `level(i)` and `depth_of()` purely so the harness can assert that every
entity's parent appeared in a strictly earlier level. Same lesson as 5.8's `view::lead()`, arriving
one lesson later on a different property.

### A builder is not frozen just because its page is shipped

Re-running `build_58.py` to repoint one navigation link spliced Lesson **5.9's** demo into Lesson
5.8's listings, because listings are read from the live repository. The page then carried code
referencing `engine::ecs::hierarchy`, which does not exist at 5.8's point in the course. This is
the second time an old builder has misbehaved when re-run (5.7's was still stamping a retired
STATE block).

**The fix is a pinned snapshot**: any file a later lesson modifies is frozen into
`scratch/lNN_<name>.cpp` and the builder reads that instead. And the snapshot is *verified* rather
than trusted — `git show <commit>:<path> | diff - <snapshot>` proves it byte-identical to what the
lesson shipped. The reconstruction attempted before the commit existed was one comment off, which
is exactly the error rate you should expect from reconstructing 600 lines from memory.

### A label can overflow its own box, and none of the geometry checks can see it

`check-page.js` catches labels outside the viewBox, labels overlapping each other, and labels
sitting on strokes. A 34-character run in a 132-unit box is inside the viewBox, on top of nothing,
and overlapping no other label — it just renders straight through both edges. The throwaway check
(find the `<rect>` whose vertical span contains the text's centre, compare horizontal extents)
found it, plus a full-width prose line running through a dashed box beneath it. Worth keeping in
the authoring pass permanently.

### SVG `<text>` collapses leading whitespace, so indented code in a diagram is not indented

Three lines of pseudo-code written with leading spaces rendered flush left, destroying the nesting
that was the reason to show a loop at all. Indent with explicit `x` offsets. (`xml:space="preserve"`
also works and brings its own surprises with trailing whitespace.)

### Put a chart's annotation where the data is not

The "break even" label on the dirty-flag chart was right-anchored at the plot's right edge —
directly on top of the 1.29× bar it was there to explain. The right-hand bars are the tall ones in
any chart whose series rises. Anchor annotations on the side the data has left empty.

---

## Input mapping facts (Lesson 5.10)

### Four reasons to add a layer, none of them performance

A hard-coded scancode cannot be rebound, is about one device, cannot be recorded (a replay wants
the *intention*, because the binding may have changed since), and **says the wrong thing** —
`key_pressed(SDL_SCANCODE_SPACE)` inside a jump handler is a sentence about hardware in a file
about jumping. Recorded because the fourth is the one that gets dismissed and is the one that
compounds: code that fails to describe itself gets harder to change every year.

### A design lesson should say it is a design lesson

Modules 5.6, 5.7 and 5.9 each turned on a benchmark because each was choosing between designs
that differed in *cost*. This one was not, and manufacturing a benchmark anyway would have been
worse than useless: it would attach a number to a decision the number did not make, and the next
reader would believe the number was the reason. **Say "this measures nothing, and here is why"
rather than measuring something irrelevant to look rigorous.**

### One signed float per binding covers buttons and axes

`jump` bound to Space with +1; `steer` bound to A with −1 and D with +1; `look_x` bound to a mouse
delta with 0.5. Same rule, three jobs. The payoff is the case nobody thinks about: **holding both
halves of an axis gives exactly zero, and no rule anywhere says so.** A design with separate
button and axis kinds would have needed to define, document and test that case.

The corollary is that the value must *not* be clamped. Two keys on one button action sum to 2,
which `held()` does not care about; a mouse flick should be big. Clamping would have to know which
kind of action it was looking at, and not knowing that is the whole point.

### An edge is a change in the action, not in a signal

Bind one action to a key *and* a mouse button. Press the key (one rising edge, correct). Press the
mouse while the key is held — a binding-derived edge fires **again**, and the player jumps twice.
Release the key while the mouse is down — a binding-derived edge fires a *release* while the
action is plainly still active.

Derive from the summed level and all four frames are right. **The bug is invisible with one
binding per action**, which is what a first implementation has and what a first test writes; it
ships as "sometimes it jumps twice" the week a player binds a controller alongside their keyboard.
The test that catches it is four lines of setup and nothing else in the harness would have.

### A fixed-timestep engine needs two kinds of edge

`on_fixed_step` runs zero or more times per frame, so a frame-scoped edge read there fails in
**both** directions: a two-step frame acts on it twice, and a zero-step frame loses it entirely
because the edge came and went while nothing was listening. Neither is fixable by the caller
without cross-frame state, so the map keeps it — every rising edge queues a press,
`consume_pressed()` pops one.

This is Lesson 1.4's trap *closed* rather than documented, and it is worth noticing the
difference. Documenting a hazard moves the cost to every future caller; closing it costs one
`uint8_t` per action.

### A bounded queue is a decision, not an overflow bug

Four deep, and presses past that are dropped. An unbounded queue turns a hitch into a burst of
jumps arriving after the player has stopped asking, which feels worse than losing them. Four
survives a stutter and is short enough not to feel like a recording; a fighting game with a
deliberate input buffer raises it and calls the number a design parameter, **which is exactly
what it is.** The engine's job is to make the number visible and the behaviour predictable, not
to guess the genre.

### "Once per frame" is not the whole requirement

`on_frame` runs exactly once per frame and is still the wrong place to update an input map,
because it runs *after* the simulation steps — so every step reads last frame's actions. The
requirement is **once per frame, after input is published, and before anything reads it**. That
gap had no hook, so 5.10 added `on_input()`.

The symptom of getting it wrong is worth knowing because it gets misattributed: everything works,
nothing is broken, and the game feels 16 ms mushy. People blame the display, the driver or the
mouse.

### A hook that serves one caller is a smell — check before adding it

`on_input` is defensible because the once-per-frame, pre-simulation moment is also where you read
a network snapshot, sample a debug scrubber, or latch a replay's recorded intentions. It was
missing before this lesson happened to need it. If the only answer to "what else is this for" had
been "nothing", the right move would have been to let the caller update the map at the top of its
own first fixed step and document the ordering.

### A concept when the test cannot otherwise exist

`input::update()` samples SDL's live keyboard state, so a harness that wants a key held would have
to persuade SDL that a key is held — which SDL offers no supported way to do. Templating
`action_map::update` on an `input_snapshot` concept (six accessors) makes a hand-written struct a
complete input device, and the shipping logic runs unchanged.

A concept beats an abstract base here on three counts, and all three are worth separating:
**it changes nothing in `input.hpp`** (not one character of Lesson 1.2's header moved);
**it costs no virtual call** on a loop that runs per binding per frame; and **it is open** —
`engine::input` satisfies it without knowing it exists, where a base class is implemented only by
types whose author agreed to. Put a `static_assert` beside the concept so a rename in the source
type fails at the requirement rather than deep inside a template.

### Do not ship device code you cannot run

The lesson's second motivation is "a gamepad has no scancodes", so the obvious move is to add
gamepad support — and it was declined, deliberately and out loud. It could not be exercised on
this machine, and **untested device code in an engine is a liability that looks like support**: it
compiles, it appears in the API, and the first person to plug in a controller discovers it was
never run.

What was shipped instead is the property the claim actually rests on — one action, several
bindings, more than one device — demonstrated with a keyboard and a mouse, plus the five steps and
an explicit ⚠ VERIFY on the SDL3 signatures. **A named gap beats an unverified feature.**

### The hardest part of a new device is disconnection, and it does not belong in the mapper

An action bound to a controller that has been unplugged must go inactive, and the release edge
must fire. The right place for that is the *snapshot*: a disconnected pad answers false, the sum
drops to zero, the level goes false, and the release edge falls out of the existing machinery with
no new code. **If you find yourself special-casing the mapper, the abstraction boundary is in the
wrong place.**

### A lookup that declares turns a typo into a silent no-op

`find()` returns an invalid id and does not create. If it declared on miss, a misspelled action
name in a config file would produce a brand-new action with no bindings and no code reading it —
a perfectly functioning thing that does nothing, with no error anywhere. Same shape as Lesson
5.5's "no fallback asset": substituting something plausible removes the program's ability to
notice.

### `reset()` has to forget the mouse origin too

Clearing levels, edges and the queue on focus loss is obvious. Forgetting the *previous cursor
position* is not, and without it the first frame back delivers the entire distance the cursor
travelled while the window was in the background — as a single camera-flinging delta. The same
applies at start-up, which is why the first frame ever reports a delta of zero.

### A HUD that reads the binding table cannot go stale

The demo prints `spawn on [Space]/[LMB]` by asking the map, not from a string literal. Press the
rebind key and it says `[Enter]`, because the binding really changed. Small, and it is the first
dividend the layer pays: **a program that can describe its own controls**, which a switch
statement could never do.

### Count the thing before writing the number down

The lesson said "Lesson 5.2's application layer offers six hooks". It offered seven. Caught by
grepping `virtual ` in `app.hpp` while updating ARCHITECTURE.md, not by re-reading the prose —
a number in a sentence is a claim, and claims about the codebase are checkable in one command.

---

## Debug drawing and tooling UI facts (Lesson 5.11)

### Two of six parameters were the whole bug

`line3(fb, a, b, colour, pr)` needs a `framebuffer` and a `projector`, so **only code holding
renderer state can call it** — and the code with something worth drawing (a collision system, an
ECS pass, a loader) has neither, and should not. Hand a framebuffer down so one of them can draw a
line and every caller of every caller needs one too. Three apparent problems, one root: it works on
one surface only, and a line lives exactly one frame, both fall out of the same two parameters.

**Removing an argument is a design change. It made nothing faster and it moved the ceiling.**

### The include list is the interface, and it is transitive

The queue could have gone into `debug_draw.hpp` and everything would compile. It would also mean
that a physics translation unit including it to draw one box compiles the framebuffer, the depth
buffer, the projector, the viewport and — via `mesh.hpp` — the whole handle system. Forever, in
every TU.

That constraint is why `wire_mesh()` takes `span<const vec3>` and `span<const uint16_t>` rather
than the `mesh` that owns them. **Take the data, not the type**, when the type's header costs more
than the data does.

### A single-frame marker for a single-frame event is invisible

16.7 ms at 60 Hz, against roughly 250 ms for a person to register that something appeared. So a
debug drawer without lifetimes **can show you state and never events** — and the events (a
contact, a re-parent, a spawn, a raycast hit) are usually what you are hunting. This is the
argument for lifetimes and it is the one that gets dismissed as a nicety.

### The expiry rule: test before you subtract

Measured in `float` at 60 Hz on a 0.5-second line, three rules that all look right:

| rule | advances | 0-second line at `dt == 0` |
|---|---|---|
| `if (r <= 0) drop; r -= dt;` — shipped | 31 | dropped |
| `r -= dt; if (r <= 0) drop;` | 30 | dropped |
| `r -= dt; if (r < 0) drop;` | 30 | **kept for ever** |

The third is the one you write the moment you want a line to survive the frame its clock runs out
on — a one-character change. And `dt == 0` is a paused clock, a single-frame `--shot`, a
breakpoint: **the moments you are looking hardest.** Testing first does not get the answer right,
it *removes the dependency* on `dt` and on the comparison operator, which is the more durable fix.

### Queue → flush → advance, and `advance()` outside every branch

Ageing before the flush deletes every default-lifetime line before it is drawn. The symptom is a
debug system that draws **nothing**, which reads as "my queueing code never ran" and sends you to
instrument the wrong file. Diagnose it in one step: queue something with `seconds = 5`; if that
appears and the default ones do not, it is the order.

Ageing *inside* the "we have a camera" branch is the same bug with a slower fuse — the queue grows
without limit on exactly the frames that draw nothing.

### A bound with no counter is a bug that looks like a rendering artifact

The 4,097th line simply is not there, and a missing line looks exactly like a thing that does not
exist. `queued`, `drawn` and `dropped` only mean anything as a set. Every primitive funnels
through one private `push()` so the bound has one implementation, which is also why a 12-edge box
with two free slots is 2 kept and 10 dropped rather than something bespoke.

### Report what you actually know: `line3` now returns whether it survived

The HUD claimed "drawn below queued means something clipped", and `draw_debug_lines` was counting
what it *submitted*. Giving `line3` a `bool` return made the claim true. It changes no pixel —
both new `return false` paths were already `return` and already drew nothing — and it is
deliberately not `[[nodiscard]]`, because six lessons of call sites correctly ignore it.

**A number in a HUD is a checkable claim. One the code cannot back is worse than no number.**

### Never withhold an event from a level tracker

The tempting fix for two consumers of one keyboard is to route: give the event to the UI, and if
it took it, stop. `engine::input` tracks **levels**, and a level is only ever corrected by the
event that contradicts it — so a key-*up* routed away leaves that key held down for ever, and no
later event fixes it, because the key is not going to be released twice.

Both consumers see every event. Arbitrate one layer later, on the levels.

### …and keep updating the map while the UI has focus

Skipping `action_map::update()` while a text field is focused freezes every level: hold a movement
key, click into the box, and the camera flies away. Updating through a mask reports the masked
keys as *up*, which fires the **release edge** — which is not damage limitation, it is the
behaviour you want. Giving focus to a text field genuinely should let go of the movement keys.

### You cannot mask a delta by masking one of its endpoints

A key is a level, so reporting it as up is a complete lie and it works. The cursor is not:
`action_map` *derives* a delta by differencing two frames. Cursor at 100, UI takes the mouse for
three frames while it travels to 160, then lets go as it moves to 170:

| approach | delta on the release frame |
|---|---|
| report 0 while blocked | **+170** — the whole screen |
| freeze the last position | **+70** — the whole excursion. *This is the version that ships.* |
| virtual cursor | **+10** — one frame of real movement |

The middle one is the trap, because it behaves perfectly until somebody drags a slider, and then
letting go whips the camera round by the distance they dragged.

The fix is a change of frame of reference, not a mask: report `real − offset`, and grow `offset`
by exactly this frame's real movement on every blocked frame. Reported position stops dead while
blocked, and the offset stops *growing* the instant the block lifts, so both boundaries are free.
The cursor stays permanently 60 px behind and is permanently right about how far it moved — which
is the only question anybody asked it.

The wheel needs none of this, and noticing why tells you when the machinery is needed: `input`
publishes the wheel as a per-frame **delta** already, so zeroing it is exact. Only a delta the
*consumer* derives needs the virtual-cursor treatment.

### A concept written for testability turned out to be an architecture

Lesson 5.10 made `action_map::update` a template over an `input_snapshot` concept so a test could
hand it a six-function fake. One lesson later the mask is the concept's second implementor, and
**`action_map` did not change by one character**. Against `const input&` the only options were an
`if` inside the map (the input mapper learning what a UI is) or a copy of `input` with fields
cleared (a second source of truth about the keyboard).

### `NewFrame` computes the capture flags, so it goes first

ImGui's `WantCaptureKeyboard` / `WantCaptureMouse` are computed *inside* `NewFrame`, from the
events processed since the last one. Put `NewFrame` next to the panels in `on_overlay` — where it
looks like it belongs — and every read during the frame answers about the **previous** frame. The
symptom is exactly one frame of leakage: the first keystroke after clicking into a text field also
reaches the game, every time, and it is invisible unless you know to look.

The three backend calls also have a fixed order — renderer, then platform, then core — because the
core derives from what the two backends just filled in.

### Concept versus vocabulary decides whether a dependency can be wrapped

`stb_image` is `PRIVATE` and hidden behind `engine::image` because it wraps a **concept**: decode
these bytes into pixels, one function and one type. Dear ImGui is `PUBLIC` because its value **is
its vocabulary** — four hundred widget calls — and a wrapper around that is a re-spelling with no
content that has to be re-spelt for every widget, forever, by somebody who did not write the
widget.

So the containment is a *rule about which code may speak it* (tooling only) rather than a link
flag, and exactly one engine translation unit includes `<imgui.h>`.

### Some libraries ship sources and no build, and that is a feature

ImGui has no `CMakeLists.txt`. `FetchContent_MakeAvailable` notices and only *populates*; the
target is yours to declare. Being forced to name the seven files you compile is a better position
than inheriting somebody's build options — and it is where you decide, consciously, to keep
`imgui_demo.cpp` (11,299 lines) because `ShowDemoWindow()` is the fastest widget reference there
is and its source is the documentation.

### A static library only contributes what somebody references

`verify_45` … `verify_510` still link without `libimgui.a`, even though the engine archive now
contains `debug_ui.o`. Only `verify_511` needs it, because only `verify_511` touches
`engine::debug_ui`. Worth knowing before you go adding libraries to every link line "just in
case".

### The headless path must be silent, safe, and *logged at info*

`debug_ui::start()` returns false with no window — which is `--shot`, and a build server, and both
are legitimate. Logging that at error level would put a red line in every headless run in the
repository and train the reader to ignore red lines. Same argument `engine_set_warnings` makes
about compiler warnings, one layer up.

Every subsequent call being a no-op — *including* `wants_keyboard()`, which answers false — is
what lets one program be written once and run correctly on all three surfaces.

---

## Course-infrastructure facts (docs/, Lesson 5.11)

### "Pin the demo" was the wrong rule; pin every file the page lists

`build_510.py` carried a warning to pin `demos/ecs_swarm/main.cpp` before 5.11 touched it. Done,
verified, and **still not enough**: re-running the builder produced a 173-line diff, because
`actions.hpp` is also one of 5.10's listings and 5.11 added `masked_input` to it. It read as
"finished" precisely because it was *5.10's own new header*.

The rule is: **pin every file the page lists that a later lesson touches** — and the cheap way to
find out which is to re-run the builder and `git diff` the page *before* shipping anything else.
Third occurrence of an old builder misbehaving (5.7's stamped a retired STATE block, 5.8's spliced
a newer demo, 5.10's needed two pins).

### `check-page.js` must be run at a stated viewport

Run in the Browser pane at its natural (narrow) size, the spill check reported **every label in
every figure** as spilling and `pageScrollsX` as true. Nothing was wrong: the SVGs are scaled and
the comparison degenerates. At 1280×900 and again at 390×844 the same page returns `pass: true`
with zero findings.

A checker that reports 170 failures for a page with none is worse than no checker, because the
next real finding is in that list somewhere. **Set the viewport explicitly before believing the
output.**

### Box-sampling destroys a line drawing; take the peak instead

`figs_45.box_sample` averages each cell, which is right for a shaded surface and catastrophic for
lines: a one-pixel debug line inside a 3×3 block contributes one ninth of its brightness, so 152
crisp radial lines averaged into a uniform haze and the figure showed scattered dots. Taking each
block's **brightest** pixel keeps thin bright features at full strength.

### …and the quantiser's background test is per-channel, not luminance

`figs_45.quantise` calls a cell background only when `r < 14 && g < 14 && b < 14`. This demo's
background is `(12, 14, 20)` — blue is 20 — so every empty cell snapped to the nearest tint at its
dimmest step and the whole panel came out solid slate with the lines invisible inside it. The fix
is to floor dim cells to true black in the sampler, so a cell says "nothing here" in the
vocabulary the quantiser already speaks.

Both bugs were found by *looking at the rendered figure*. No geometry check can see either.

### A label can overflow its own box, still

Third occurrence. Figure 3's one-frame bar is 92 units wide and carried "queued, drawn, gone" —
about 99 units of text, spilling out both ends. Inside the viewBox, on top of nothing, overlapping
nothing, so every automated check passes. Budget roughly **5.2 units per character** for `xs`
text and check in-bar labels against the bar.

### Calibrate the text-width estimate against pages that shipped

A pre-flight width check flagged fourteen labels using a guessed 5.3 units/character. Measuring
the *shipped* 5.10 figures — which were browser-verified — showed they pack 134 characters into
696 units, i.e. 5.19. Half the "failures" were the estimate, not the figures. **Calibrate against
known-good output before acting on a heuristic.**

### The reliable way to look at a figure is one page per figure

Scrolling a 400 KB lesson page and screenshotting lands somewhere unpredictable — `offsetTop` is
relative to the offset parent, and `course.css` sets `scroll-behavior: smooth`. Generating a
throwaway HTML page per SVG that links `course.css`, then `navigate` + `screenshot`, is
deterministic and shows the figure at its real size in both themes. Delete them afterwards.

---

## A document nothing compiles is a document nothing can keep correct

`engine/engine.hpp` calls itself "the whole public API, in one include" and "the fastest way to see
whether something is public". At Lesson 5.11 it listed **40 of the 55 headers it could have** —
missing the entire ECS, the asset store, handles, pools, the logger, the assertions and the action
map, which is very nearly everything Module 5 built.

The mechanism is worth more than the bug. `grep -rl "engine/engine.hpp"` over `demos/` and
`engine/` returns the file *itself* and nothing else. It has never had a consumer, so it has never
been compiled, so no mistake in it could ever produce a symptom. Its history is three commits: 5.1
created it, 5.2 remembered `platform/`, 5.11 remembered `debug_lines` — and seven lessons in
between shipped fourteen public headers and touched it zero times.

That is a rule kept by memory, inside the repository whose Lesson 5.1 insisted the boundary be
"enforced by the include path, not the style guide". The same reasoning was available three lines
below and nobody applied it.

Two things follow. **Check anything a compiler does not** — the fix that lasts is the configure-time
lint, not the fifteen added lines. And **anchor the match**: the first draft grepped the whole file
for each header's name and passed on headers that were only *mentioned in a comment*, including the
one documented exception, which the file names in prose precisely to say it is absent. A check that
reads its own excuse as compliance is worse than no check.

## An incomplete dependency and a cheap one look identical on a stopwatch

Before completing that umbrella, the obvious prediction: it is the expensive way to include the
engine, an explicit list is the cheap way, and completing it widens the gap. Measured, one
translation unit, best of five:

```
nothing at all                        0.01 s
umbrella as shipped (40 headers)      0.28 s   <- cheaper than explicit
the game's own 23 explicit includes   0.37 s
umbrella completed (55 headers)       0.39 s
```

The broken umbrella was cheap **because** it was broken: the fourteen headers it omitted are the
templated ones — `registry.hpp`, `view.hpp`, `pool.hpp`, `asset_store.hpp`, `actions.hpp` — which is
where this engine's compile time actually lives. Completing it costs +39% against the broken version
and +5.4% against including exactly what you use.

So the single-translation-unit number is *not* the reason to avoid an umbrella. The reason is the
incremental rebuild Lesson 5.1 measured — editing one public header recompiles everything — and that
is a property of the dependency graph which no single-file benchmark can see. Put both numbers in
the header: a reader who finds only the first concludes umbrellas are fine, and a reader who finds
only the second concludes they are a disaster.

## An assertion is developer-facing control flow, and `--shot` has no developer

A camera parented to a rolling, non-uniformly scaled rover produces a placement that is not rigid.
`ecs::view_from_camera` asserts exactly that, correctly, on the first frame. The symptom was not a
diagnostic and not a crash: the headless `--shot` run **hung for ever with no output at all**.

`SDL_assert` expands to a `while` loop so that RETRY genuinely re-tests the condition — which Lesson
5.3 made a point of, and which is right at a desk with a dialog in front of you. On a build server
with no display to draw a dialog on and no terminal to prompt at, the default answer keeps arriving
and the condition cannot change.

Diagnosing it took one command — `sample <pid>` on macOS, `gdb -p` elsewhere — and the assertion's
own function was the top frame of all 1,538 samples. Reach for the sampler before the debugger when
a headless run stops producing output; a hang has a stack, and it is the same one every time.

## "Scale" means two things in one header, and it will bite three times

`cube_mesh()` and `quad_mesh()` span ±0.5, so a `transform`'s `scale` is the box's **full size**.
`icosahedron_mesh()`'s vertices sit at distance 1.0, so its `scale` is a **radius**. At `scale = 1`
the ball is twice the diameter of the cube. Both are documented; together they are a trap.

And the trap is not the asymmetry — it is that a **half-extent** is what the rest of the program has
in its hand. A collision test wants one. `debug_lines::box` takes one. So the value you reach for is
wrong by a factor of two at exactly the moment you reach for it. In one file it bit three times:

- the floor came out a **quarter** of its intended area, with the pillars floating beside it in the
  void — which reads as a camera bug for a good ten minutes;
- boxes were half-buried, because the centre height must be `half.y` while the scale is twice that;
- the debug OBBs were **exactly 2× too big**, because `box(world_from_local, half_extent)` takes the
  extent in the *matrix's own space* and the matrix already carries the scale.

The third survived longest because it looked plausible: a box slightly too big around a collision
proxy reads as a deliberate margin. **A plausible margin that is exactly a factor of two is never a
margin.** The answer is one helper — `box_at(centre, half, rotation)` — whose entire content is the
doubling, so the convention has to be understood once rather than at every call site.

## The inverse of a product reverses, and the wrong order is not visibly wrong

To make a camera boom cancel its parent's roll and scale:

```
L = (H · Rz(β) · S)⁻¹ · H = S⁻¹ · Rz(−β)
```

The unscale comes **first**. The English description of the same intent — "undo the roll, then undo
the scale" — produces `Rz(−β) · S⁻¹`, and a rotation and a non-uniform scale do not commute.

What makes this expensive is how *close* the wrong answer is. On the rover's actual numbers the two
products agree on the *x* axis to four decimal places and differ by 36% on *y*. A result that is
right in one component and wrong in another does not look like an algebra error; it looks like
something needs tuning. Derive the expression, then let a test assert the property the derivation
was for — here `is_rigid`, with a *third* chain (no boom at all) as the control, since "the swapped
one is not rigid" is otherwise equally consistent with the boom doing nothing whatsoever.

## Writing a lesson out of order needs two trees, and the delta is the finding

Lesson 5.12 closes Module 5 and was authored after 6.18, so "every listing compiles at its point in
the course" and the state of the repository were in direct conflict. The resolution: write, compile
and run the lesson in a `git archive` checkout of the era commit, and port forward with a script
that *records* the delta.

The delta is the most interesting number the lesson produced: **26 real source lines out of 1,485**,
all of them in two named places. Everything structural — the ECS, the hierarchy, the camera, the
action map, the asset store, the debug lines, the debug UI, the application layer, the whole
software-renderer call — ported unchanged. And the two builds agree *exactly* on gameplay (`2/12
orbs, 25 objects, 380 triangles, 889 debug lines`) while **91.9% of pixels differ**, max channel
delta 128. The structural surface survived eleven lessons; the semantics did not. Module 6 changed
what "lit" means, not what "draw this" means.

Two traps in the porting script itself, both about measuring:

- It first counted changed lines by comparing the two files **position by position**, so a single
  inserted line scored every line after it as changed — 1,107 of 1,196 for a file whose real delta
  is nineteen. Align with `difflib` before you count.
- Folding two fields into a brace **re-indents** the comment block between them, and counting that
  as change credits the API drift with twenty lines of your own tidying. Report the
  whitespace-insensitive diff as the headline and the raw one beside it.
