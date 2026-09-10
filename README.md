# Build a Professional 3D Game Engine

A complete course that takes you from "I can program a little" to "I can design, build, and ship
a 3D game engine" — by actually building one, in **C++20** on **SDL3**, lesson by lesson.

There is no engine to download here and no framework doing the interesting parts for you. You
write the math library, the rasterizer, the ECS, the renderer, the physics, and the editor. By
the end you have a real engine and a game built on its public API.

**Status:** curriculum and conventions published; lessons in progress — **Modules 0–4 are complete**, Module 5 needs only its checkpoint game (5.12), and Module 6 is under way (66 of 107 lessons). The CPU software rasterizer is finished end to end, the same scene is drawn on a real GPU through SDL_GPU, and the tree is now a static library with a public API, a platform/application layer, its own logging, handle-based resource storage, an asset system that can load, share and free, a reusable A/B timing harness that has decided three architecture questions with numbers instead of folklore, and **a working from-scratch ECS** — a generational entity id honoured by every pool, sparse-set component storage, queries that lead with the smallest pool, a **transform hierarchy** whose resolve cost is flat in depth with a camera that is an ordinary entity, (5.10) an **input layer that knows what the player meant** rather than which key they hit, (5.11) **a debug-draw system and a tooling UI** — a queue of world-space geometry with lifetimes that anything in the engine can fill, and Dear ImGui behind a 182-line facade — and, opening Module 6, (6.1) **a colour pipeline that is correct at both ends**, which found and fixed a four-module-old bug that had every GPU-rendered pixel too dark, (6.2) **a shading equation with units** — a BRDF measured in inverse steradians, a light measured in irradiance, and a hemisphere integrator that turns "is this model physically plausible?" into a number — (6.3) **a story about what a surface is**: three microfacet distributions and a masking term, each held to an identity that has a right-hand side, and (6.4) **a physically-based BRDF, live in both renderers** — Cook–Torrance with its `4(n·l)(n·v)` denominator *derived* rather than quoted, Fresnel built from the physics with `F0` read off an index of refraction, the metallic workflow arriving as a consequence, and energy conservation measured at a worst hemispherical reflectance of 0.9255 where the model it replaced reached 1.4300, (6.5) **a material system** &mdash; one home for what a surface is, referencing its textures by handle rather than by pointer, with the rule that decides membership set by the hardware rather than by taste &mdash; and (6.6) **a glTF 2.0 loader**, text and binary, with node hierarchies, multi-primitive meshes and the full metallic-roughness material model arriving through the asset store with *no conversion at all*, because three of the four convention checks against this engine come out as "do nothing" &mdash; (6.7) **per-pixel normals in both renderers**, from a tangent frame derived out of the uv chart rather than copied, with a texture that finally knows whether it holds colour or data, and (6.8) **shadows in both renderers** &mdash; a z-buffer rendered from the light, an orthographic projection derived from an interval remap, and a bias that is *derived rather than tuned*: shadow acne turns out to be a sampling error whose magnitude follows from half a texel of lateral travel times the surface's slope, predicted at 50% of a lit plane and measured at 50.1%, with the worst real error reaching 92% of the derived bound, and (6.9) **cascaded shadow maps** — the light's box fitted to a slice of the *camera's* frustum rather than to the scene, with the two artefacts that only exist once that box moves: a corner-fitted box changes size by **30.2%** when the camera merely turns (cured by a bounding sphere, measured spread 9.9&times;10⁻⁸, at a cost of 29% of the resolution), and a sliding texel grid makes every edge crawl (cured by snapping, drift 0.499 texels → 7.6&times;10⁻⁶). Its real subject is an **audit**: 6.8 derived its bias from `world_per_texel`, cascades change that quantity by 7.3&times;, and not one line of the derivation had to move — while the *device-space* bias turns out to be constant across cascades because the sphere's radius cancels, with the one exception the mechanism predicts. It ends by measuring a scene where cascades **lose**, and (6.10) **mipmapping, trilinear filtering and anisotropy** — a debt Lesson 3.9 measured and deferred, and that four later lessons repeated, once in a *shipped public header*. The level is `log2` of the footprint, and the footprint is the CPU-specific part: a GPU shades in 2×2 quads so `ddx` is a subtraction, but a scanline rasterizer has no neighbour, so the derivative comes from the triangle by the quotient rule — **exact**, and checked against a central difference of the real interpolation to 5.79×10⁻⁶. It also quantifies the famous linear-light bug: a chain averaged in sRGB bytes delivers **42.2%** of the light it should at level 1 and compounds, which is why the symptom is a surface that dims as it recedes. Start at
[`docs/index.html`](docs/index.html).

---

## The two-stage spine

The course's central pedagogical bet is that you learn the graphics pipeline twice:

1. **Modules 1–3 — a CPU software rasterizer.** You own every pixel. Triangles, the z-buffer,
   perspective-correct interpolation, clipping, and texturing are all code *you* wrote, so the
   pipeline becomes intuitive instead of incantational.
2. **Module 4 onward — SDL_GPU.** SDL3's modern cross-platform GPU API (Vulkan / D3D12 / Metal).
   Every concept maps back to the software counterpart you already built.

The software rasterizer deliberately targets **the same NDC as SDL_GPU** (+Y up, depth 0..1), so
moving to the GPU is an *API change, not a math change*.

---

## Who this is for

- You can program in **some** language. C++ specifics (RAII, ownership, move semantics,
  templates, `const`-correctness, translation units) are taught in place, the first time each
  appears.
- **No assumed background** in graphics, linear algebra, or calculus. All math is built from
  zero, geometrically — intuition first, then derivation, then formula, then code.
- Budget roughly **450–550 hours** across ~107 lessons.

**Exit profile:** implement techniques straight from papers, debug GPU work in RenderDoc, reason
about frame budgets and cache behaviour, design and defend engine architecture, and read real
engine codebases without drowning.

---

## Reading the course

The lessons are **plain HTML files** — no build step, no server, no npm. Open the index and
navigate:

```sh
open docs/index.html          # macOS
xdg-open docs/index.html      # Linux
start docs\index.html         # Windows
```

Styling and page behaviour come from two shared files, `docs/shared/course.css` and
`docs/shared/course.js`, which every page links. They resolve straight off the filesystem, so
this works offline with no server — but it does mean **a lesson file is only readable inside the
`docs/` tree**. Copy one out on its own and it renders unstyled; keep the folder together, or
just clone the repository.

Three living pages sit alongside the lessons and are updated at every module boundary:

| Page | What it is |
|---|---|
| [`docs/index.html`](docs/index.html) | Course home — module map, every lesson, progress |
| [`docs/conventions.html`](docs/conventions.html) | Handedness, matrices, NDC/depth, winding. **Read before Module 2.** |
| [`docs/math-toolbox.html`](docs/math-toolbox.html) | Cumulative math appendix, grows as you go |
| [`docs/cpp-style.html`](docs/cpp-style.html) | The C++ style guide the codebase obeys |

> The "no build step" rule applies to the **tutorial HTML only**. The C++ obviously builds with
> CMake — see below.

---

## Building the code

The engine itself is a normal CMake project. You write the first `CMakeLists.txt` yourself in
**Module 0**, because CMake is taught from zero rather than handed over as a magic file — so if
you are following along from the start, ignore the one in this repository until Lesson 0.4 asks
you to write it.

Once you are past Module 0, the build is the standard incantation everywhere:

```sh
cmake -S . -B build
cmake --build build
```

The code is the state of the engine as of the most recently published lesson (see
[STATE.md](STATE.md)). Since **Lesson 5.1** it is split in two: `engine/` is a static library
whose public headers live under `engine/include/engine/`, and `demos/` holds the programs built
on it. Running the sandbox demo gives you **Module 3's scene, drawn by the GPU** — and, on
<kbd>V</kbd>, the same frame drawn by the software rasterizer *beside it*, from one scene
description:

```sh
./build/demos/sandbox                  # macOS / Linux
.\build\demos\Debug\sandbox.exe        # Windows
```

> **Renamed in Lesson 5.1.** This was `build/engine` through Module 4. That name now belongs to
> the library — to the thing the demo links — which was exactly the confusion the old name was
> papering over. Anything in Modules 0–4 that says `./build/engine` means
> `./build/demos/sandbox`.

**The sandbox has five modes**, and Lesson 4.8 inverted the default that Lesson 4.2 set up:

| command | what runs |
|---|---|
| `sandbox` | Module 3's scene on the GPU (Lesson 4.8) |
| `sandbox --software` | the CPU renderer, its HUD, and four demos on <kbd>Tab</kbd> |
| `sandbox --probe` | the instrument Lessons 4.2–4.7 were built on |
| `sandbox --gpu` | an alias for `--probe`, kept because six lessons tell you to type it |
| `sandbox --shot FILE` | seven pinned frames to a PPM, **no window and no display** — Lesson 5.1's characterization test, made genuinely headless in 5.2 |

There are three more executables, and each is an acceptance test for a boundary rather than a
demo of anything:

```sh
./build/demos/hello_cube                  # a lit cube, spinning
./build/demos/hello_cube --shot cube.ppm  # one frame, no window, no display

./build/demos/pong                        # Lesson 1.8's game, at last a program

./build/demos/gltf_view                   # shapes.glb, orbiting
./build/demos/gltf_view --model cube.gltf # the textured cube
./build/demos/gltf_view --pose 1.7 --shot out.ppm

# Lesson 6.7 — the comparison that is the whole lesson
./build/demos/gltf_view --model torus.obj --bumps 0   # flat
./build/demos/gltf_view --model torus.obj             # normal-mapped

# Lesson 6.8 — shadows, and the artefact FIRST
./build/demos/gltf_view --model torus.obj --bias none --pcf 0          # shadow acne
./build/demos/gltf_view --model torus.obj --bias slope-scaled --pcf 0  # the derived fix
./build/demos/gltf_view --model torus.obj --pcf 2                      # a soft edge
./build/demos/gltf_view --model torus.obj --shadow 128                 # what resolution buys
```

`hello_cube` is the standing acceptance test for the public API: every symbol in it comes from a
header under `<engine/…>`, so **if a picture cannot be made from outside the library, the library
does not have an API.**

`gltf_view` is **Lesson 6.6's** acceptance test for the *asset system*, and it is `hello_cube`'s
successor in one specific sense: that program proved a picture could be made from outside the
library, and this one proves a file can be *loaded* from outside it — one call to
`asset_store::load_model` and a loop turning handles into `scene_object`s, with no path assembly,
no parser, no cache and no lifetime rule anywhere in it.

Start with `--model cube.gltf`, because one image asserts three things: the uv grid's arrow points
**up** (so `v` was not flipped — a checkerboard could not have told you), there is an image at all
(so the `.gltf`'s `"uri"` resolved against the store's own search path and survived the RGBA→ARGB
channel shuffle), and the highlight has the file's shape (`roughnessFactor: 0.4`, untouched).
Nothing in the demo types a colour.

Then `shapes.glb`, where the metals are nearly black except where they clip to white — which is
not a bug and not the importer. A conductor has no diffuse lobe, so a metal with nothing to reflect
but one directional light has exactly two states, and the bright one is **64× displayable white**
at roughness 0.25. That number is the quantitative case for image-based lighting (6.12) and a
tonemapper (6.10); `--pose` exists so you can sweep past the highlight and watch.

`pong` is **Lesson 5.2's** acceptance test for the *application* layer. Until that lesson it was a
branch of `sandbox`'s five-way <kbd>Tab</kbd> switch, for one reason: a demo needs a loop to exist
in, and there was exactly one loop in the repository. It is now 87 lines of code with no `main`,
no `SDL_Init`, no window handling and no loop — five overrides on `engine::app`, which is built on
SDL3's main callbacks.

> **Two arrangements, both supported.** `engine::platform` owns SDL's lifecycle and leaves the
> loop to you (`sandbox` uses it, deliberately). `engine::app` owns the loop and calls you
> (`hello_cube` and `pong`). `app` is implemented **on** `platform`, never beside it — a harness
> renders six frames down each path and compares the framebuffers byte for byte. Across the three
> demos, **48 SDL lifecycle calls became 3**.

### The engine is quiet; your program is not

Since **Lesson 5.3** the engine logs on its own categories (`core`, `platform`, `gfx`, `gpu`,
`asset`) at six levels, and SDL's own default table (`app=info, …, *=error`) means all five are
silent unless something actually failed. A demo's `SDL_Log` is `APPLICATION` at `info`, so it
still prints. `--log` works on **every** program built on the engine, because `platform::start`
reads it before doing anything else:

```sh
./build/demos/pong                     # one line: the demo's own
./build/demos/pong --log platform=info # …plus the engine's start-up facts
./build/demos/sandbox --log gpu=debug  # everything the GPU layer says, nothing else
./build/demos/sandbox --log '*=quiet'  # silence the engine; the demo still talks
```

A bad spec is rejected with a message and **nothing is applied** — the parser validates the whole
string before touching a single level.

### Nothing holds a borrowed pointer to a resource

Since **Lesson 5.4** geometry is owned by an `engine::mesh_pool` and referred to by an
`engine::mesh_handle` — one 32-bit word, 20 bits of index and 12 of generation. Freeing a slot
bumps its generation, so a reference to something that has been unloaded *fails a lookup*
instead of reading whatever moved into the memory:

```cpp
engine::mesh_pool meshes;
const engine::mesh_handle h = meshes.insert(engine::to_mesh_data(engine::cube_mesh()));

if (const engine::mesh_data* m = meshes.get(h)) { draw(m->view()); }   // resolves
meshes.remove(h);
assert(meshes.get(h) == nullptr);                                     // and now it does not
```

The storage underneath is dense and packed, so the pool relocates freely — inserting may
reallocate everything and removing moves a *surviving* item into the hole — and handles survive
all of it. The rule for users is one line: **resolve late, use immediately, never store**; the
`T*` that `get()` returns is valid until the next `insert` or `remove` and no longer.

### Assets are named, loaded once, and explicitly unloaded

Since **Lesson 5.5** an `engine::asset_store` owns everything loaded, generated or derived, and
finds it by name through an ordered `engine::search_path`:

```cpp
engine::asset_store assets;                       // roots: <exe>/assets by default
assets.paths().prepend_root("mods/dragon_pack");  // …and this one wins

const engine::mesh_load a = assets.load_mesh("torus.obj");   // reads the file
const engine::mesh_load b = assets.load_mesh("torus.obj");   // b.cached == true
assert(a.handle == b.handle);                                // …and the same handle

assets.unload_mesh(a.handle);                     // and now neither resolves
```

**A name is not a path.** `"torus.obj"` is stable and is what a scene file records; the absolute
path is where that name resolved today, under these roots. Order is the feature — put a
directory in front and everything in it shadows the shipped asset of the same name, which is
how mods, localisation packs, live editing and test fixtures all work.

**There is no reference counting**, deliberately: a count would cost a handle every property that
made it worth having. Unloading is explicit, and it is safe to get wrong because staleness is
detectable — forgetting leaks (find it with `live_count()`), unloading early gives a null handle
and a bump in `collect_stats::unresolved`. The one exception is **derived assets** —
`with_normals` output, a mipmap chain, a GPU upload — which are owned by their source and
released with it, transitively.

Press <kbd>Bksp</kbd> in `sandbox --software` to free the current model out from under the scene
that is still drawing it. Exactly one object disappears, and the HUD says so.

### An entity is an id; components are filed under it

Since **Lesson 5.8** the engine has an ECS, and it is a sparse set — chosen in Lesson 5.7 by
measurement rather than taste. An `engine::ecs::entity` is one 32-bit word with the same 20/12
split as a resource handle, minted by **one** allocator and honoured by **every** component pool:

```cpp
struct velocity { engine::vec3 linear; };          // a component is a plain struct

engine::ecs::registry world;

const engine::ecs::entity e = world.create();
world.add<engine::transform>(e, {.position = {0.0f, 1.0f, 0.0f}});
world.add<velocity>(e, {{0.0f, 0.0f, -3.0f}});

// a system is a free function running one query; h is the fixed step
world.view<engine::transform, velocity>().each([h](engine::transform& t, const velocity& v) {
    t.position = t.position + v.linear * h;
});

world.destroy(e);                                  // …and every pool forgets it
```

**That one id opening several pools is the whole point**, and it is the one thing
`engine::pool<T>` could not offer: each of *its* pools mints its own keys, so a mesh handle
7 and a texture handle 7 have nothing to do with each other.

**A component is a plain struct** — no base class, no macro, no registration. `engine::transform`
was written in Lesson 2.8 by someone who had never heard of an ECS, and it is used as a component
unmodified. **A system is a free function**, because Lesson 5.6 measured virtual dispatch on an
iteration at 1.5–1.7× *at every world size*, four objects included.

**A view leads with the smallest pool.** One comparison in its constructor: over pools of 60, 30
and 12 it walks twelve candidates rather than sixty, for the same answers. The one rule it
imposes is the familiar one — *do not add or erase a component the view names while walking it*,
exactly as you would not erase from a `std::vector` inside a range-`for`. Collect, then act.

Run `ecs_swarm` to see what this buys at a scale where the performance argument does not apply:
154 entities, ten component types, and twenty-four of them invisible **because they have no
geometry component**, not because anything hid them. Press <kbd>M</kbd> to strip the material
from a third of the swarm and watch the HUD's lead pool change as the material pool becomes the
smallest one.

### A child's placement is relative to its parent, and the order is the hard part

Since **Lesson 5.9** an entity may carry a `parent`, and `engine::ecs::hierarchy` composes every
`world_transform` from it:

```cpp
engine::ecs::registry world;
engine::ecs::hierarchy tree;

const auto sun    = world.create();
const auto planet = world.create();
const auto moon   = world.create();
engine::ecs::add_hierarchy_components(world, sun,    {.position = {0, 0, 0}});
engine::ecs::add_hierarchy_components(world, planet, {.position = {10, 0, 0}});
engine::ecs::add_hierarchy_components(world, moon,   {.position = {0, 2, 0}});

engine::ecs::set_parent(world, planet, sun);     // refuses to create a cycle
engine::ecs::set_parent(world, moon,   planet);

tree.mark_topology_changed();
tree.rebuild_and_resolve(world);                 // the moon is now at (10, 2, 0)
```

**Nobody computed `(10, 2, 0)`.** It is not stored anywhere and no system produced it — it is
what two matrix multiplications happen to say. Move the sun and every world position under it
changes, with no code that says so.

**The maths was free; the order was the lesson.** A parent must be resolved before its children,
and a component pool hands entities back in *insertion* order — churn a 48-entity tree the way a
running game churns it and twelve of them sit ahead of their own parent. So `rebuild()` buckets
entities by **depth**, which is a topological sort (a parent's depth is always one less than its
child's), and `resolve()` walks the buckets. That was chosen by measurement over depth-first
recursion, which ran 3.34 → 13.55 ns/entity from depth 1 to 32 while level order ran 3.39 → 4.94.
It also means **a level is independent work**, which is Module 9's unit of parallelism arriving
for free.

`rebuild()` costs about one resolve, so it runs only when the *shape* changes; `resolve()` runs
every frame. An orphan becomes a root and is counted; a cycle is broken, counted and logged,
because a hang is not a recoverable failure. `destroy_subtree()` is there for callers who want
the other behaviour.

### The camera is an entity

```cpp
const auto cam = world.create();
engine::ecs::add_hierarchy_components(world, cam,
                                      engine::ecs::look_along(eye, target, {0, 1, 0}));
world.add<engine::ecs::camera>(cam, {});
engine::ecs::set_active_camera(world, cam);          // moves a tag; no pointer anywhere

engine::ecs::set_parent(world, cam, car);            // …and now it rides in the car
```

Three things fall out that nobody had to design: a camera can be **parented**, “which camera is
active” is a **component rather than a pointer** (so destroying it dangles nothing), and a camera
is **findable by a query**. Its view matrix is `rigid_inverse` of its resolved placement, which is
bit-for-bit Lesson 2.9's `look_at` — the same maths, finally spelled out in two steps because the
camera now *has* a placement to invert.

Run `ecs_swarm` and press <kbd>F</kbd>: the sun drifts and the entire system — ninety-six
planets, thirty-two moons, twenty-four invisible waypoints — follows it. Press <kbd>C</kbd> and
the camera parks on a planet and rides.

### Nothing names a key

Since **Lesson 5.10** gameplay code asks about *actions*, not scancodes:

```cpp
engine::action_map actions;

const engine::action_id jump  = actions.declare("jump");
const engine::action_id steer = actions.declare("steer");

actions.bind_key(jump, SDL_SCANCODE_SPACE);
actions.bind_mouse_button(jump, SDL_BUTTON_LEFT);   // same action, second device
actions.bind_key(steer, SDL_SCANCODE_A, -1.0f);     // …and an axis is two bindings
actions.bind_key(steer, SDL_SCANCODE_D, +1.0f);     //    with opposite scales

void on_input() override            // once per frame, before the simulation
{
    actions.update(in());
    if (actions.pressed(jump)) { /* a frame-scoped edge, safe here */ }
}

void on_fixed_step(float h) override            // zero or more times per frame
{
    if (actions.consume_pressed(jump)) { /* …and a queued one, safe HERE */ }
    turn(actions.value(steer) * h);             // a level; A and D cancel to 0
}
```

**One mechanism covers buttons and axes:** every binding contributes a signed float and an
action's value is the sum. Hold both halves of `steer` and you get exactly zero — a design with
separate button and axis kinds would have needed a documented rule for that case, and this one
does not, because −1 + 1 = 0.

### The engine can draw what it is thinking

**Lesson 5.11** split debug drawing into a queue anybody can fill and a backend that draws it:

```cpp
engine::debug_lines debug;                        // a value, not a global

// Queued from a system that has no framebuffer, no projector and no camera —
// which is the entire point, and was impossible before the split.
void hierarchy_debug_system(engine::ecs::registry& world, engine::debug_lines& out)
{
    world.view<engine::ecs::world_transform, engine::ecs::parent>().each(
        [&](const engine::ecs::world_transform& w, const engine::ecs::parent& p) {
            if (const auto* pw = world.get<engine::ecs::world_transform>(p.value))
            {
                out.line(engine::translation_of(w.matrix),
                         engine::translation_of(pw->matrix), 0xFF78BEFFu);
            }
        });
}

// …and once per frame, in the one place that knows where the camera is:
engine::draw_debug_lines(fb(), view, projector, debug);   // 2. FLUSH
debug.advance(time().dt());                               // 3. AGE — last, always
```

A queued line carries a lifetime, and that is not a convenience. **A collision normal exists for
one simulation step — 16.7 ms at 60 Hz — against the ~250 ms a person needs to notice anything**,
so a per-frame drawer can show you *state* and never *events*, and it is usually events you are
hunting.

Alongside it, Dear ImGui — taken through the **public** boundary where `stb_image` was hidden
entirely, because that one wraps a *concept* and this one is a *vocabulary*. The engine owns only
the lifecycle, in 182 lines, and refuses to start when there is no window, so a headless
`--shot` run behaves exactly as it did before the UI existed.

### Colour is a pipeline, with exactly two conversions

**Lesson 6.1** is the gamma lesson, and it opens Module 6 because every lighting result in that
module is wrong until the colour space is settled. Two integers carry the subject: **code 128
emits 0.2159 of white's light, not half, and half the light is code 188.**

The rule is not "convert carefully" — it is **convert twice, at the edges**, with light in
between, so that ordinary arithmetic is valid in the middle by construction rather than by anyone
remembering. Applying that rule as an *audit* — fifteen conversion sites, each given a job — found
a hole:

```
  swapchain format: B8G8R8A8_UNORM            # before: values here MEAN sRGB codes
  swapchain format: B8G8R8A8_UNORM_SRGB       # after
  composition     : SDR_LINEAR  (the hardware encodes sRGB)
```

SDL claims windows with a swapchain whose header says *"pixel values are in sRGB encoding"*, and
the fragment shader had been returning linear **light** into it since Module 4. Measured on real
downloaded pixels, linear 0.5 was stored as **128 where 188 was meant**. The error is a *ratio* —
**13× too dark in shadow, 1.1× near white** — which is exactly why four modules of looking at the
picture never found it, and why a pixel-by-pixel comparison in Lesson 4.8 sat on top of it and
reported 87% agreement: the harness had built its own `_SRGB` target and tested a configuration
the shipped program did not use.

### A BRDF is a ratio, and the π was in the wrong place

**Lesson 6.2** gives the shading equation units, and the exercise finds something. Before it,
`shade()` multiplied an albedo by a light and called the product "the light leaving the surface" —
a sentence with no unit anywhere in it, which makes "is this model energy-conserving?" a question
the code cannot even be asked.

With units, the equation separates into three claims that can each be checked alone:

```
L_o = ( f_diffuse + f_specular ) · E_perp · cos θ  +  albedo · L_ambient
      \_______ the surface ______/   \__ the light __/
```

A **BRDF** is radiance out over irradiance in, so its unit is **inverse steradians** — which is
why one may exceed 1 without inventing energy. A surface reflecting *all* the arriving light into
a 20° cone has a BRDF of **2.7210 sr⁻¹** against Lambert's 0.3183; a mirror's is unbounded. The
quantity actually capped at 1 is the *integral*.

And the π in `albedo/π` is **derived, not quoted**: every patch of the hemisphere projects onto
the plane below as its own size times cos θ, those shadows tile the unit disc exactly once, so the
cosine-weighted hemisphere measures π. A constant BRDF *k* therefore returns *k·π* of what
arrives; demanding that equal the albedo forces *k* = albedo/π and nothing else.

Which raises the question the lesson is really about — **if the π belongs in the BRDF, where has
it been?** Four lines of algebra answer it:

```
albedo · (key · intensity · n·l)  ==  (albedo/π) · E_perp · n·l
                    ⟹  intensity = E_perp / π
```

The engine's light scalar was never a free parameter. It was an irradiance with the Lambert
BRDF's own constant divided out of it, unlabelled, **since Lesson 3.6**. Moving it into the open
changes **154,240 of 342,225** floating-point results and **not one 8-bit code** — and the
reference render is byte-identical for the thirteenth lesson, which here is the *measurement*
rather than a survival: had the π been anywhere else, the picture could not have come back.

Two pieces of unexpected news. The ambient term, called "a fudge" since 3.6, turns out to be
**exactly right** for a uniform environment, because its own π cancels against the hemisphere it
is integrated over. And the highlight, run through the new hemisphere integrator, fails energy
conservation — but not in the expected way. The 1/π tames the raw lobe; what survives is that
diffuse and specular are **added with no coupling**, so a white surface with a white highlight
reflects **1.1386** of what hits it. That number is the door into Lesson 6.4's Fresnel term.

### A surface is a landscape of tiny mirrors, and that claim can be tested

**Lesson 6.3** throws out `shininess`. Not because it looks bad — because it is a number that
makes the highlight the right size and measures nothing: not comparable between models (3.7 fitted
Blinn against Phong at 4.38×), not measurable on any real material, not transferable between
renderers.

In its place, one sentence taken literally: **a surface is a landscape of microscopic *perfect*
mirrors.** Shading then collapses into a counting problem, because a facet reflects **l** to
**v** only if its own normal is **h** — so the highlight's shape is a *histogram of surface
slopes*, and roughness is that histogram's width. A profilometer can measure that.

And because a histogram is a **distribution**, it has to satisfy something:

```
∫ D(h) · cos θₕ · dω  =  1
```

which is the first equation in this shading arc with a right-hand side you can hold a model to.
Read backwards it says the microfacets' *projected areas* add up to the flat area they stand on,
so the cosine in it is not a convention.

That test then does the work. It shows **Blinn-Phong was a microfacet distribution all along**,
missing only its constant — and that this engine ships `1/π` where `(s+2)/2π` is required, wrong
by exactly **17× at the default shininess**, silently absorbed by every specular colour an artist
ever tuned. It prices GGX against Beckmann at **456× the tail** 45° off the peak. It explains
6.2's unexplained 5.36× view-angle swing as shadowing and masking — a *consequence* of the
landscape picture rather than a correction bolted on. And it measures the model's own remaining
error: single scattering returns **0.3069** at full roughness, **losing 69%** to the second
bounce it forgets, which is why rough metal renders dark in every engine that has not bought it
back.

Then the test finds a bug that has nothing to do with microfacets. The textbook GGX denominator,
`cos²θₕ(α²−1) + 1`, is a catastrophic cancellation in `float` and **loses 1.1% of the model's
energy at mirror roughness**. One line of algebra removes it — `(1−c)(1+c) + α²c²`, identical
arithmetic, and Sterbenz's lemma makes `1.0f - c` exact for `c ≥ 0.5`. A 450× improvement,
diagnosed in two runs by a rule worth keeping: **quadrature error shrinks when the grid refines;
arithmetic error does not.**

**Lesson 6.4** assembles, and it opens on 6.2's **1.1386** for the third lesson running — because
6.3 normalised the distribution and the number did not move, deliberately. **The fault is the plus
sign.** Diffuse and specular were *added*, so the same photon left the surface twice: once as
having bounced off it, once as having gone into it. Fresnel is the coupling, and the whole lesson
is one sentence — *F is what bounces off, so 1 − F is what goes in* — with the window at night as
its proof.

Then the line every renderer quotes and almost none derives. Put spherical coordinates on the
*fixed* direction and the `4(n·l)(n·v)` denominator falls out in four lines: a facet tilted by θ
turns the reflected ray by **2θ**, so `dω_out/dω_h = 2 sin2θ / sinθ = 4 cos θ = 4(v·h)`. Two
factors of two, one of them handing over the cosine as change. Measured against finite differences
on the sphere at **1.42 × 10⁻³** worst. And the reason the finished formula looks unmotivated
becomes visible: the `(v·h)` that would explain the 4 **cancelled**, against the facets' projected
area, because **h** bisects.

Fresnel gets the same treatment — the shape from physics first (every interface reflects
*everything* at grazing, which is why a window becomes a mirror when you look along it), then
`F0 = ((1−n)/(1+n))²`, so the 0.04 every renderer hard-codes turns out to be **window glass**.
Run backwards it becomes an **audit**, and the engine fails it: the demo's authored
`specular::colour` of 0.85 implies an index of refraction of **24.6**, where diamond is 2.42. It
was never a material — it was 6.3's 17× error wearing a parameter's clothes. Schlick is then
*measured* rather than praised: worst absolute error 0.0357, but worst **relative** error
**23.2%, at 55°** — in the middle of the range where surfaces are actually seen. It ships anyway,
on honest grounds: 23% of 0.04 is 0.019 of a reflectance, which is invisible.

The **metallic workflow arrives as a consequence** rather than a checkbox — a conductor absorbs
whatever crosses its interface, so it has no diffuse lobe and its colour has nowhere to live but
F0 — and the demo's teal slab, faked since 3.7 by hand-typing a matching highlight colour beside
its tint, becomes `metallic = 1` and can no longer disagree with itself.

Then the finding. The coupling **nearly every engine ships**, `1 − F(v·h)` including the glTF
reference BRDF, is exact at normal incidence and still reaches **1.3395** at grazing: it accounts
for the light that got *in* and says nothing about the light that fails to get *out*. The obvious
repair fixes the energy and **fails reciprocity** — 0.2623 one way against 0.2932 the other — and
a BRDF that is not symmetric in **l** and **v** is not a BRDF. That is what selects the
two-crossing form the engine ships, worst **0.9255**, never above 1.

And the reference render **breaks**, on purpose, after fourteen lessons: `905BF27E` → `E917C06C`,
2,400 pixels moved, **all darker** — which is the check that matters, since an energy-conserving
model replacing one that emitted light cannot brighten anything. But only three of seven frames
moved at all, and that was the *instrument's* fault: `shading::textured` is unlit, so the torus
chosen in 3.8 *because* it shows highlights was drawn unlit in the reference shot. So an eighth
frame is added — because **the moment to extend a characterization test is the moment it breaks
for another reason.** It is free at a re-baseline and costs a re-baseline at every other lesson.

**Lesson 6.5** starts from a number rather than a bug. 6.4 changed *one* surface parameter and had
to edit **forty-four call sites** across the engine, four demos and six harnesses — and that is not
a complaint about 6.4, it is what a missing type measures.

Three structs had independently grown the same hole, and two of them said so in their own comments.
The third is the one that settles it: **`ecs_swarm` is a demo**, restricted since Lesson 5.1 to the
engine's public headers, and in 5.7 it declared `struct material` itself because the engine offered
none. **When a consumer that cannot reach inside your library has to invent one of your types, the
type is missing** — not "would be nice"; missing.

The interesting part is not the struct. It is the question that decides what goes in it, and the
hardware settles it rather than taste: **can this be a number in a buffer?** If yes it is per-draw
data, pushed as a uniform, and two objects differing only in it draw back to back. If no it is
*pipeline state*, baked into a pipeline object, and two objects differing in it need two pipelines
and a sort between them — which is exactly what Lesson 4.8 already paid for. So `cull_mode` stays
out, and `verify_65` asserts the rule the bluntest way the language allows: `is_trivially_copyable`.

The texture reference becomes a **handle**, and the comment defending the old pointer is worth
reading — *"`textures` lives for the whole program, so there is nothing here that can dangle"* was
true, and was an argument about that demo rather than about the type. Sixty-four insertions later
the pool has moved from `0x928c03500` to `0x928c16000` and the handle does not care, because it
names a slot rather than an address; a *stale* one resolves to "no image" instead of freed memory,
which is the case a pointer cannot express at all. `bind_albedo` is the named step between them:
**a handle is how you store a reference, a pointer is how you use one, and the conversion happens
once per draw** — because per pixel is precisely where a bounds check is unaffordable.

Sharing is measured, not asserted: ninety-six ring drones over six tints is **3,456 bytes against
600**. And the better argument is not the bytes — with copies, "make the drones rougher" is a loop
over the registry; with handles it is one write. Note the rule that falls out, because the folk
version gets it backwards: `scene_object` still holds its material *by value*, because it has one.
**The rule is about sharing, not about size.**

Then two comments that had been repeating for three modules turn out to be wrong. `closed` was
never material state — a material is explicitly *not* pipeline state, and `closed` is not cull mode
anyway but a fact about the *mesh*, which `validate()` already counts from its own edges. And the
compile-time argument for splitting out `cull.hpp` **did not survive being measured**: 5.1's quoted
0.97 s came back as 0.258 s, against `material.hpp`'s own 0.261 s, so the marginal saving is zero —
and 5.1's note had said, in capitals, that the ratio travels and the seconds do not. The split ships
on the physical-design rule alone. **The only false part of the case was the part added to
strengthen it.**

Everything here is a refactor, so there is exactly one claim: the reference render is
**byte-identical** at `E917C06C`, verified after each of the three stages separately — *move
without changing, then change without moving*.

**Lesson 6.6** does the opposite of Lesson 3.5 and takes a library, and the whole interest is
*why the two answers differ*. The usual test — "is it hard?" — is useless here, because the
rasterizer was hard and we wrote it. The test this course has actually been using is **whether the
hard part is the subject**: for OBJ it was unifying `v/vt/vn` triples, which *is* the index
problem; for glTF it is that an accessor may be any of five component types, normalized or not,
tight or interleaved at a stride, dense or sparse — roughly **forty legal encodings of the same
eight positions**, every one of which some exporter emits, and not one of them about graphics.

But the deeper difference is structural. **OBJ is a bag of triangles; glTF is a scene.** A mesh is
a *list* of primitives and each one has its own material, so a car body and its windscreen must be
two draws — flatten them and you get geometry that is correct and unpaintable.

Then the part that makes this lesson worth its position in the module: **the import is a copy.**
`metallicFactor` and `roughnessFactor` go straight into Lesson 6.4's `microsurface` with *no
conversion*, because glTF's alpha remap is roughness² and so is ours, and its dielectric IOR is
1.5 and so is ours. Gold's `baseColorFactor` of (1.00, 0.71, 0.29) comes out of `f0_of` verbatim
while `diffuse_albedo_of` returns exactly black — the metallic workflow 6.4 *derived*, meeting the
format that assumes it.

Same story on conventions: handedness, winding and texture origin **all agree**, so `gltf.cpp`
contains **zero conversion code** — no basis change, no index reversal, no `1 − v`. That is Module
2 having argued the choice rather than picking one; FBX is Z-up and Unity and Unreal are
left-handed, and any of those costs a *mirroring* basis change, which reverses winding, which then
needs a second correction. The trap runs the other way round: `flip_uv_v` defaults to `true` and is
correct for every mesh this engine has ever loaded and **wrong for every one it loads next**.

The lesson also settles a debt. Lesson 6.4 shipped a ⚠ VERIFY claiming the glTF reference BRDF
uses `1 − F(v·h)`; compared term by term against Khronos Appendix B, **five of six terms are
identical**, and the metallic blend is not even a difference — the spec lerps two whole BRDFs and
this engine lerps the F0, and those commute *exactly* because Schlick is affine in F0 (measured at
1.19 × 10⁻⁷ over 4,851 points). Only the diffuse coupling differs, one Fresnel crossing against
two, and the consequence is **1.036× at normal incidence and 5.62× at 88°** — which is precisely
why it hid for two lessons. The specification then answers its own question: BRDF implementations
**may** vary, and a physically accurate one **must** be energy conserving, which ours is at 0.9255
and the spec's own sample form is not at 1.3395.

On the architecture side, textures and materials become *assets*. A texture is **derived** from its
image, so unloading the image takes it — and the cascade has to erase the *name entry* too, or
`find_texture` keeps handing out a recycled slot, which does not crash and therefore takes an
arbitrary number of frames to notice. `to_texture` turns out to be a bridge that **should have
existed since Lesson 5.3 and did not**: the engine could decode a PNG and could sample a texture,
and nothing joined them, because every CPU texture was generated and every loaded image went
straight to the GPU. Two complete halves with no middle, and no test could see the gap because no
code path crossed it.

The one conformance gap is *reported rather than hidden*: glTF multiplies a base colour factor by
its texture and this engine's image replaces the tint, so a coloured factor **plus** a texture is
counted and logged at the only point that knows both halves. 58 checks, and the reference render
**byte-identical** for the fifteenth lesson — a whole second asset format, and not one pixel moved.

**Lesson 6.7** starts from a limit rather than a bug: every surface this engine has drawn is exactly
as flat as its triangles. Shading has consulted the geometry's normal since 3.6, so a surface can
only *look* bumpy if it *is* bumpy — and giving one torus millimetre-scale detail means
millimetre-scale triangles, tens of millions of them for one prop, every one smaller than a pixel.
So do not add the geometry. **Lie about the normal.**

But a stored direction is meaningless without a frame, and finding that frame is the lesson. It
arrives as *two equations rather than a formula to copy*: a triangle's two edges describe one walk
across the surface in **metres**, the same two edges' uv deltas describe that identical walk in
**texture units**, so *T* and *B* are simply whatever vectors make the two descriptions agree. Two
unknowns; invert a 2×2. The determinant turns out to be twice the signed area the triangle occupies
in the chart — the same signed-area quantity Lesson 2.4 derived barycentric coordinates from — which
makes **zero a real case rather than a degeneracy**: an untextured face, a collapsed unwrap, and the
face contributes nothing instead of an infinity.

Then the question Lesson 6.6 had to defer, settled by one byte. **128 means 0.502 as data and 0.216
as a colour**, and read the wrong way "no tilt" becomes a tilt of **38.8°** — in one direction, on
every surface. It does not look like a bug; the usual diagnosis is "this map was authored too
strong", and the usual fix makes the picture less wrong without making it right. The colour space
goes on the *texture* because that is where the hardware puts it — and the finding is that **the GPU
has had an `srgb` flag since Lesson 4.7 and the software renderer never had the concept at all**,
which is the same shape of gap 6.6 found between `load_image` and `sample`.

Two more decisions with the same character. A tangent is a **`vec4`**, because every symmetric model
has a mirrored uv chart — an artist unwraps one arm and reflects it — so on that half the frame is
left-handed; drop the sign and one side of the model is lit as the *mirror image* of the other. And
a tangent is carried by the **model matrix, not the inverse transpose**, which is 3.6's distinction
arriving on its other side: a normal is defined by being *perpendicular to* the surface, and a
tangent lies *in* it, so it is a difference of positions and transforms the way positions do. Use the
wrong one and the frame is skewed 36.9° — but both answers stay in the plane, so it reads as an asset
authored at the wrong angle, and under a *uniform* scale the two agree to 8.4 × 10⁻⁸.

Three of the hardest bugs available here share that shape: **they produce a picture that is
plausible.** Which is why each is a type or an assertion rather than a comment — and why the round
trip is the test that catches all of them at once. A flat map must return the geometric normal, and
a wrong colour space, decode range, orthonormality, handedness, multiply order or missing
Gram-Schmidt each break it. It asserted zero, measured **5.55 × 10⁻³**, and the renderer was right:
**0.5 is not an 8-bit code**, so 128 decodes to 1/255 and *every flat normal map in existence* tilts
its surface by 0.318°. Predicting that floor from the encoding is a strictly stronger test than the
one that failed.

Ported to both renderers in one commit. `gpu_vertex_pnu` went 32 bytes to 48 — caught by the
`static_assert` Lesson 4.5 wrote — and the new attribute collided with 4.6's instancing at vertex
location 3, caught by 4.5's `check_layout` before anything rendered. The fix is the better design
rather than the smaller one: **a vertex layout is per-pipeline state**, so a shader that reads no
tangent does not declare one. 33 checks; the reference render **byte-identical** for the sixteenth
lesson.

**Lesson 6.8** starts from a sentence that has been true since Lesson 3.6 and never said out loud:
every light in this engine is *unoccluded*. `lambert(n, l)` asks whether a surface **faces** the
light — a fact about one surface's orientation — where the question we want is whether it can
**see** the light, which is a fact about that surface and every other object in the scene. That is
why every object in every picture so far floats.

The fix is one sentence, and the engine already owns both halves: **render the scene from the light
and keep only the depth.** "The nearest surface along every ray from a viewpoint" is not a new
algorithm, it is a z-buffer — Lesson 3.1's — so on the CPU the entire depth pass is
`collect_triangles` followed by `draw_triangles` with a different camera, and not one line of the
rasterizer changes. *A shadow map is not a new renderer; it is the renderer you already have, aimed
somewhere else.*

What is genuinely hard is the **comparison**, and it gets the whole lesson. A directional light has
no position, so there is no eye to hang a frustum on: what replaces the pyramid is a *box*, and
**orthographic projection** is derived here from an interval remap rather than quoted. Its bottom
row is `(0, 0, 0, 1)`, so **w comes out exactly 1** — and that one fact is spent three times: depth
becomes affine so precision is uniform (perspective's near-to-far ratio is measured at over 100×),
the near plane may be *negative* so the light's eye can sit inside the scene it lights, and a
fragment's light-space position can be recovered from Lesson 3.7's interpolated world position for
**zero new varyings**.

Then the artefact, shown before the fix: **shadow acne**, 36,786 pixels of moiré on a 480×270 frame,
with not one line of code misbehaving. It is not a mystery. The map stores *one* depth per texel and
the fragment is somewhere else inside it, so **half of every texel's footprint is downhill of its
own sample** — predicted at 50%, measured at **50.1%**. The magnitude follows from the same picture:
lateral travel × tan θ ÷ depth range. The worst measured error reaches **92% of that bound**, which
is what makes it a derivation rather than a safe over-estimate.

From there each cure is a consequence. A constant bias must **peter-pan**, exactly: it unshadows
everything whose caster is within `bias × depth_range` world units, which bites hardest where a
caster *touches* its receiver — the one cue shadows were added for. **Normal-offset bias needs the
geometric normal**, and this is the first line in the engine that must tell it from Lesson 6.7's
shading normal: acne is a disagreement about where the *triangles* are, and a normal map does not
move a triangle. And PCF arrives with two numbers that settle its order — occluders at 0.3 and 0.9
average to a depth of 0.6, which reports a receiver at 0.5 *fully lit*, where comparing first gives
0 and 1 and a mean of 0.5. That is the entire reason `SamplerComparisonState` exists.

One finding came out of rendering rather than reasoning: **a 3×3 kernel reaches 2.12 texel-diagonals,
not 0.71**, so a correct bias stops being correct the moment PCF is switched on — and the acne
returns looking like a filtering bug. The GPU port is a second render pass with
`num_color_targets = 0`, a depth texture that can be *sampled*, and the third identity-element
fallback in the scene renderer: **1.0 is the identity for a depth comparison**. 53 checks; the
reference render **byte-identical** for the seventeenth lesson.

**An edge is a change in the *action*, not in a signal.** Bind `jump` to both a key and a mouse
button, press one while the other is held, and there is still exactly one press edge. Derive
edges from bindings instead and the player jumps twice — a bug that works perfectly with one
binding and appears the week somebody adds a controller.

**And a fixed-timestep engine needs two kinds of edge.** `on_fixed_step` runs zero or more times
per frame, so a frame-scoped edge read there fires twice on a two-step frame *and* is lost
entirely on a zero-step one. `consume_pressed()` takes one queued press and fires exactly once
however the steps fall.

Run `ecs_swarm` and press <kbd>K</kbd>: spawning is rebound from <kbd>Space</kbd> to
<kbd>Enter</kbd> while the program runs, and the HUD says so — because it reads the binding table
rather than a hard-coded string.

The software path is **not** deprecated. It is the *reference*: every measured claim in Modules 2
and 3 was made against it, and a port whose reference has been deleted is a port nobody can check.

Everything below describes `sandbox --software`, where the HUD lives. Arrow keys orbit the camera,
<kbd>-</kbd>/<kbd>=</kbd> dolly. The keys worth pressing first:

- <kbd>F</kbd> cycles **wireframe → painter's algorithm → z-buffer → depth view**. The HUD counts,
  live, how many pixels the two hidden-surface strategies disagree about.
- <kbd>C</kbd> cycles six scenes, three of which exist to break sorting: a cyclic overlap where
  A is in front of B in front of C in front of A, an intersecting pair where the right answer
  changes halfway across one triangle, and two panels a millimetre apart.
- <kbd>B</kbd> switches the depth format between `D32_FLOAT`, `D24_UNORM` and `D16_UNORM` — the
  last of which makes z-fighting appear on demand.

Why sorting cannot be repaired, why the buffer stores *device* depth rather than view-space `z`,
and how to predict z-fighting in metres before you see it, is
[Lesson 3.1](docs/lessons/03-01-z-buffer.html).

Press <kbd>C</kbd> once more for the **checkered floor**. <kbd>I</kbd> turns perspective-correct
interpolation off and the floor buckles into the warping every PlayStation game had; <kbd>T</kbd>
subdivides the floor and shows you what the era's alternative actually cost. The derivation — five
lines, and it covers every attribute a vertex will ever carry — is
[Lesson 3.2](docs/lessons/03-02-perspective-correct.html).

Back on the solids scene, press <kbd>U</kbd> to cycle back-face culling. Half the triangles stop
being drawn and the picture does not change — except that the painter-versus-z-buffer counter drops
from 29 pixels to **0**, settling a debt [Lesson 3.1](docs/lessons/03-01-z-buffer.html) measured and
could not explain. Keep pressing and the last setting is the classic bug: culling with
`dot(normal, camera_forward)`, which misjudges one triangle in six at this field of view. Why that
is a different question from the right one, and why the answer is a sign the rasterizer was already
computing, is [Lesson 3.4](docs/lessons/03-04-back-face-culling.html).

Press <kbd>L</kbd> and the scene fills with something nobody typed: a **2,304-triangle torus read
off a disk**, non-convex, occluding itself, with the HUD reading `1152+73 -> 1225 verts`. Those
three numbers are the lesson — a file gives every face corner three *independent* indices and a
vertex buffer has one, so 1,152 positions become 1,225 vertices, and a cube's 8 become 24. Keep
pressing: `cube.obj` shows that split at a size you can count by hand, and `twisted.obj` is the
same cube with one face listed backwards — press <kbd>U</kbd> there and 2,459 pixels of it
disappear, because back-face culling believes winding. The HUD says `WINDING!` in red before you
press anything, because the mesh was *validated* at load: Euler characteristic (0 for the torus,
not 2 — a torus is not a sphere), boundary edges, and signed volume by the divergence theorem. And
the reading `round trip: 0 px differ` is the loaded torus being compared, every frame, against the
same mesh generated in memory and written out by our own OBJ writer.
[Lesson 3.5](docs/lessons/03-05-obj-loader.html).

Everything above is **lit**. Press <kbd>G</kbd> to cycle the shading: the debug palette that
coloured this course for five lessons (five brightnesses indexed by *triangle number* — watch it
fail to change as the object spins), then Lambert with one normal per face, then one per vertex,
at which point 2,304 flat triangles become a smooth torus. Hold <kbd>A</kbd>/<kbd>D</kbd> to swing
the light and the terminator sweeps across the surface; orbit the camera instead and **nothing
changes at all**, because Lambert is view-independent.

Then press <kbd>J</kbd>. That transforms normals with the model matrix instead of its inverse
transpose — the mistake nearly everyone makes — and the HUD reports what it costs. The
uniformly-scaled icosahedron is pixel-identical either way; the squashed slab and plinth are lit
as though they were never squashed, with normals tilted by up to 68°. Why a normal is not a
direction you can transform like any other, derived from the one property that defines it, is
[Lesson 3.6](docs/lessons/03-06-normals-and-lambert.html).

Now press <kbd>H</kbd> and orbit again. A highlight slides across the surface — the first thing in
this course that changes when you move your head, which is what makes a surface read as smooth and
hard rather than as plaster. The icosahedron's highlight is **white** on an amber body and the
slab's is **teal** on a teal body: one is a plastic and one is a metal, and no geometry or texture
is doing that work. <kbd>H</kbd> also cycles to Phong's original model, and the HUD counts how
many pixels the two disagree about *at matched exponents* — because Blinn's must be about 4× Phong's
for the same tightness, a fact the lesson derives rather than quotes. The interesting part is
where they differ: Phong's highlight switches off entirely when the light and your eye are on the
same side of a surface's normal, which on a floor means the sun is behind you. Measured on a
plane at 35° sun elevation: Phong lights **0 of 30,806** pixels and Blinn lights all of them.

Then watch the `peak` number while something spins. On the 1,225-vertex torus it sits at 255; on
the icosahedron it lurches, and on `cube.obj` the highlight is absent altogether in **157 of 180**
frames — because per-vertex shading samples a very sharp function at a few points and draws
straight lines between them. That is not a bug in the equation, it is a bug in where the equation
is evaluated, and it is left standing on purpose: it is Lesson 3.8's entire argument.
[Lesson 3.7](docs/lessons/03-07-specular-blinn-phong.html).

So press <kbd>G</kbd> once more. It now cycles four evaluation points — the debug palette, flat,
Gouraud, and **per-pixel** — while <kbd>Q</kbd> independently chooses whether the normal comes
from the face or the vertex. Two keys, because those are two questions, and separating them is
what Lesson 3.8 is about: where a normal comes from is a property of the *mesh*, and where the
shading equation is evaluated is a property of the *pipeline*. The HUD prints how many pixels the
current cell differs from per-pixel by, and with face normals selected and the highlight off it
reads **0** — all three evaluation points give the identical picture, exactly, until you press
<kbd>H</kbd> and the viewer starts to matter.

Per-pixel is the first change in eight lessons to touch the rasterizer's inner loop: `vertex`
grows two *varyings*, the clipper learns to carry them, and `fill_triangle` interpolates a normal
and a position and calls the shading equation itself. What it costs is on the HUD in microseconds
— and the answer contradicts the folklore. At 320×180 the torus covers 2.8 pixels per triangle,
which means more vertices than covered pixels, and per-pixel shading is measured at **0.91×** the
cost of Gouraud. It crosses over around three pixels per triangle and settles at 2.15× by 4K.
[Lesson 3.8](docs/lessons/03-08-shading-models.html).

Press <kbd>C</kbd> for the **FLOOR** and then <kbd>M</kbd>. The checkerboard that has run to the
horizon since Lesson 3.2 was always a *formula* — `checker_at(u, v)` — and <kbd>M</kbd> swaps it
for an actual image, one texel at a time. The picture barely moves, which is the point: the
default image was chosen to match the rule, so everything that *does* change from here is the
sampler's doing. <kbd>S</kbd> switches bilinear for nearest, <kbd>R</kbd> cycles repeat / mirrored
/ clamp (watch clamp turn the tiling into four stretched streaks — the edge texels smeared
outward forever, working exactly as designed), and <kbd>1</kbd> removes the half-texel offset.

That last one is the lesson. A texel is a **sample, not a square**, so its value lives at
`(i + 0.5)/N` — and forgetting the half shifts every bilinear sample by exactly half a texel,
which the HUD counts. Then press <kbd>S</kbd> to nearest and the count drops to **0**, and the HUD
says so in parentheses: nearest-neighbour sampling *cannot see this error*. Swept over 160,801
sample positions, removing the offset changes 0 of them under nearest and 160,632 under bilinear.
That is why the bug ships, and why the filter gets blamed for it.

Then press <kbd>G</kbd> to per-pixel and swing the light with <kbd>A</kbd>/<kbd>D</kbd>. **The
floor is lit** — for the first time, because a procedural rule computes a finished colour and has
no *albedo* for a light to multiply, while a texture does. Texture × light is not a convention
somebody chose: an albedo is a reflectance, a reflectance multiplies a quantity of light, and both
have to be linear, which is why the sampler decodes sRGB *before* it filters. Get that order wrong
and a black-and-white blend delivers 42.8% of the light it should.
[Lesson 3.9](docs/lessons/03-09-textures.html).

Now press <kbd>3</kbd>, and the picture acquires a **frame budget**: six coloured segments in one
stacked bar, the whole frame divided into the phases that made it, with the part nobody is
measuring drawn on the end so it cannot be overlooked. Read the engine time against the wall time
beside it — with vsync on, the wall clock says 16.7 ms no matter what the renderer does, which is
why an fps counter cannot tell you that you have made anything faster.

One segment is essentially the whole bar. It is the fill, at **96.1%** of the frame, and the
numbers underneath are the ones worth arguing with. The 2-triangle floor costs *eight times* what
the 2,304-triangle torus does, because rasterization is paid per **pixel**: 45.01 against 64.14
nanoseconds per covered pixel, while the per-triangle figures differ by a factor of 9,300. Inside
the fragment loop, the perspective divide that Lesson 3.2 warned would cost you is **free**, the
depth test costs a quarter of a nanosecond, and the largest single item is `std::pow` in the sRGB
conversion — more than coverage, interpolation, the divide and the depth test *combined*.

So press <kbd>4</kbd>. That swaps the exact encode for a four-term approximation in nested square
roots, fitted for this course rather than copied from anywhere: the fill drops by **1.30×** and
the frame with it, and the picture does not change — 0.56% of pixels move by one code out of 255,
which the HUD counts because one code is invisible and a counter is the only evidence anything
happened. [Lesson 3.10](docs/lessons/03-10-profiling-capstone.html), which is also where
Module 3's capstone lives: everything above, in one picture, with every microsecond accounted for.

Finally, press <kbd>5</kbd>. The picture does not change — and that is the result. The rasterizer
is now walking triangles in **2×2 quads** the way every GPU does, shading the lanes each triangle
*misses* and throwing the answers away, and the budget panel counts them: on this scene it shades
166,360 lanes to cover 145,355, for **87.4% efficiency**. Press <kbd>5</kbd> again and the wasted
lanes are painted magenta instead of discarded, so you can see the fringe around every triangle.
Then press <kbd>T</kbd> to subdivide, and watch the fringe stop being a fringe.

Those lanes are not waste to be engineered away. `ddx` is lane 1 minus lane 0, so a screen-space
derivative — and therefore every automatic mipmap selection ever made — is a subtraction between
neighbours, and a lane cannot subtract against a neighbour that never ran. Efficiency falls from
96.3% on a 64-pixel triangle to **25.0%** on a one-pixel one, which is what "small triangles are
expensive" has always actually meant. Why a GPU is *slower per lane* than your CPU and wins
anyway, why a branch costs 1.93× when it is incoherent and nothing when it is not, and why render
state lives in an immutable pipeline object — with the usual explanation for that measured and
found **false** — is [Lesson 4.1](docs/lessons/04-01-how-gpus-work.html).

Now run it a second way: `./engine --probe`. Same window, same 320×180 framebuffer, same software
rasterizer — but nothing on screen came through `SDL_Renderer` any more. The picture is uploaded
to a **GPU texture** through a transfer buffer and **blitted** onto the swapchain, with no shader
anywhere in the program, and the bottom strip is a live graph of where the frame's time went:
green for the rasterizer, cyan for recording commands, blue for waiting on the display. Almost the
whole column is blue, which is the first thing worth knowing about GPU programming.

Press <kbd>4</kbd>. A full GPU sync — a fence wait — now happens every single frame, the thing
every guide tells you never to do. An orange band appears and **the columns do not get taller**:
the fence costs 0.761 ms, the acquire falls by 0.730, and the frame is unchanged at 16.667. There
was 16 ms of waiting already there for a sync to hide in. Make the GPU the limit instead and the
same sync costs **1.60×**, because a sync costs you the overlap you had, not the duration of the
wait. That, plus why reading a download before the fence is wrong **64 times out of 64**, why an
`SDL_FColor` of 0.5 lands as byte 128 or 188 depending only on the target's format, and a
benchmark that reported **763 GB/s on a 273 GB/s bus** (kept in the lesson, with both reasons it
was wrong), is [Lesson 4.2](docs/lessons/04-02-sdl-gpu-model.html).

Then the part that turns SDL_GPU from a copier into a renderer: a program to run on the hardware.
`./engine --probe` now shows **four small green squares**, one per shader, and each one means a
lot — an HLSL file compiled to SPIR-V, translated to the binary format *this* device says it
accepts, loaded with an entry point that Metal renamed behind your back, and given four resource
counts read out of the compiler's own reflection rather than typed. That is Lesson 4.3, which
also finds the three ways a correct-looking shader fails silently, and one number that cannot be
true: `SDL_CreateGPUShader` takes **8 microseconds**, so whatever it is doing, it is not
compiling anything. [Lesson 4.3](docs/lessons/04-03-shader-toolchain.html) writes down where the
compile must actually be happening, and Lesson 4.4 goes to check.

It goes to check, and the answer is **about 2.4 ms per new state permutation at pipeline creation
against 0.031 for the shader** — the compile was there all along. How much you pay depends on the
driver's cache, which lives on disk and outlives your process; two separate wrong conclusions were
drawn from that call in drafts of the lesson, and both are kept in it. By then there is a
triangle on screen with red, green and blue corners, drawn by the GPU, sharing a window with the
software rasterizer's picture — and that is what Module 2 was for. Render the *same* triangle
both ways and compare: **zero disagreements anywhere in the interior**, twenty thousand pixels of
exact agreement, and 102 boundary pixels where the two use different tie-breaking rules. You did
not write a toy rasterizer. You wrote a rasterizer.
[Lesson 4.4](docs/lessons/04-04-first-triangle.html) also shows why a first triangle is usually
invisible: reversing two vertices does not flip it, it deletes it — 20,808 pixels to zero — and
there is a two-minute test that tells you so.

Press <kbd>6</kbd> and seven lit, spinning tori appear, from **one vertex buffer, one index
buffer and one draw call**. Getting real geometry onto a device raises a question three hand-typed
vertices never could: a vertex layout is three numbers and one formula — `base + i×pitch +
offset` — evaluated by hardware that has no way to know whether the numbers are right. Hand
pipeline creation six broken layouts and **it refuses exactly one**. So
[Lesson 4.5](docs/lessons/04-05-vertex-buffers.html) builds the missing check out of the
reflection JSON the build has been emitting since 4.3, and is honest that it catches three of the
five. Then it breaks the layout on purpose and photographs the wreckage: <kbd>8</kbd> puts the
pitch four bytes wrong and the torus shatters *progressively*, because the error is *i*×4 — which
is also why vertex 0 is always fine and the bug survives a three-vertex test. A wrong *offset*,
by contrast, collapses the mesh onto a unit sphere; the symptom names the mistake. Along the way
a vertex element format turns out to answer **two** questions with different answers, which makes
Lesson 4.4's vertex **43% smaller with the shader untouched**; index buffers turn out to be
**4.17× smaller** on this mesh and to convert a fixed 6,912 vertex-shader invocations into a
range; and instancing turns out to cost **one enum value** — 196 bytes a frame against 53,024
that never move again.

Then press an arrow key and the camera moves. Lesson 4.5's shader had its camera welded in as
seven `static const` floats, because there was no way to send it anything;
[Lesson 4.6](docs/lessons/04-06-uniforms.html) fixes that and finds a mechanism unlike any other
in the API — **SDL_GPU has no uniform buffer object**, no `UNIFORM` usage bit, nothing to create
or bind or release. You push bytes onto the command buffer and every draw after them reads them.
What then matters is whether the bytes land where the shader looks, and there the lesson finds
two things worth the trip: the packing rule in force is **HLSL's, not the std140 SDL's own header
cites**, so a C++ struct written without thought delivers a trailing `float3` as **(242, 243, 0)**
where (241, 242, 243) was written — shifted one float, silently, with every earlier field intact.
And a claim this course has carried since Lesson 2.6 without ever checking it finally gets
checked: push a matrix, read all sixteen elements back one pixel at a time, and there is **no
transpose anywhere** — even though the compiled SPIR-V says `RowMajor`, which turns out to be the
toolchain describing your data in its own vocabulary rather than a contradiction. Orbit the new
camera until two tori overlap and you will also see exactly why Lesson 4.7 is about the depth
test.

[Lesson 4.7](docs/lessons/04-07-textures-and-depth.html) fixes that, and it is mostly a
*collection*: the depth test is three pipeline fields plus an attachment, and the sampler is two
`static_cast`s — because Lessons 3.1 and 3.9 were written knowing this day was coming, and both
said so in comments you can still read. So the lesson spends its length on what is *not* a port.
Depth precision falls off as the **square** of distance, because the perspective divide puts
1/*d* into the stored value — **70% of our depth range is spent in the first metre**, and a
16-bit buffer cannot separate two walls **41 cm apart at ninety metres**. That is z-fighting with
a number attached at last. The formula is derived and then measured, agreeing to a few per cent
across two orders of magnitude, after two measurement bugs the lesson keeps rather than hides
(one of them aliasing, in the probe itself). Reversed-Z turns out to be worth **180× on a float
format and exactly nothing on a UNORM one** — which is the control that makes the number
believable. Along the way it brings in the first third-party code in the project, with the test
for when to hand-roll and when not to: *is the hard part the subject?*

Which leaves one thing missing, and [Lesson 4.8](docs/lessons/04-08-porting-the-scene.html) is it:
the **scene**. Every piece existed — geometry, camera, texture, depth — but Module 3's floor and
models were still running on the CPU beside the GPU path. Moving them is bookkeeping. The
interesting part is the audit, because two modules of convention work were done on the stated
promise that this would be an API change and not a maths change, and *a promise nobody checks is
a wish*. Of fifteen pipeline stages, **two are literally the same C++ function called from both
sides**, eight became fixed-function hardware, one was translated line for line — and **two could
not cross at all**, because a cull mode and a material are pipeline state. That is Lesson 3.8's
predicted bill arriving, and it is what turns one loop into N draw calls.

Two findings the port forced out, neither of which was in the plan. A vertex shader sees **one
vertex**, so it cannot compute a face normal the way the software rasterizer's per-triangle loop
did — the fallback moves into the *importer*, and Lesson 3.8's flat/smooth key stops being a
keypress and becomes a property of the vertex buffer (a cube's 8 positions become **36**). And the
naive normal matrix turns out to be invisible on boxes **by construction**, not merely usually:
zero pixels change on two non-uniformly scaled boxes, because a box's normals are its own axes and
a diagonal scale only changes their length. Squash the icosahedron instead and **2,160 pixels**
change by up to 143 codes. A test scene made of crates would have reported a clean pass while the
renderer was broken.

Then press <kbd>V</kbd> and compare the two renderers per pixel: **87% byte-identical**, the
shading equation itself agreeing to **one float ULP** across 12,288 comparisons, and the
disagreements each with a cause — sub-pixel coverage on silhouettes, texture undersampling under
minification, and a one-code floor in the *darks*, where our `powf` and the hardware's sRGB write
are two approximations of the same curve. The flat untextured floor reports 100% of its 39,202
pixels differing, all by one code in one channel, which is the lesson's other moral: **never
report a percentage without a magnitude**.

Which leaves one thing, and it is the one nobody warns you about:
[Lesson 4.9](docs/lessons/04-09-renderdoc.html) is what to do when the screen is black and every
call returned success. **A breakpoint cannot catch a GPU bug** — recording and executing are
separated by a submit, and the bug lives on the far side of it. So the lesson covers the class of
tool that can see it, and the engine work that makes a frame worth capturing: every resource named
*at creation* (SDL's own docs steer you away from the setter you find first — and a comment in this
codebase spent four lessons confidently explaining an API asymmetry that turned out not to exist),
debug groups as an immovable C++ scope (Metal scopes a group pushed inside a pass *to* that pass),
and a frame log the engine prints itself. `engine --trace` dumps one frame headlessly — 33 events,
3 draws, 560 uniform bytes — and cross-checks itself against the renderer's own counters.

The awkward fact is stated in the first paragraph rather than buried: **RenderDoc does not support
Metal**, so on a Mac the tool is Xcode's Metal Debugger. Every concept transfers; the screenshots
do not.

Two of the lesson's findings came out of getting it wrong first. The measurement of what
instrumentation costs took three attempts, and the first reported a **negative cost for adding
work** — which is not a puzzle, it is the tell that you have measured noise rather than an effect.
Measure the floor, then scale the workload until the effect clears it: 183.6 ns and 182.0 ns from
two independent estimates, and *the agreement between them* is the evidence. And a debt Lesson 4.5
booked is declared **unpayable** on this hardware rather than fudged — modelling it instead turned
up something better than the original number would have been, which is that vertex reuse is a
property of the index *order*, and shuffling triangles costs 2.83× with the geometry untouched.

Then hold <kbd>=</kbd> on that floor and walk *into* it. <kbd>K</kbd> cycles what happens to a
triangle with a corner behind your eye: **clip** it (correct), **drop** it (the ground vanishes —
31,747 pixels of 57,600), or divide anyway with **no guard** at all (the floor folds inside out and
188 pixels survive). Why the perspective divide has a precondition rather than a guard, why the
near plane is `z_clip ≥ 0` and not `w ≥ 0`, and Sutherland–Hodgman implemented for polygons *and*
line segments, is [Lesson 3.3](docs/lessons/03-03-near-plane-clipping.html).

<kbd>Tab</kbd> cycles the earlier demos, which are kept rather than deleted: the basis-transform
visualiser (2.5), the triangle rasterizer's seven views (2.2–2.4), the line-algorithm fan (2.1),
and the **Module 1 checkpoint: a complete, playable game of Pong**. Left paddle
<kbd>W</kbd>/<kbd>S</kbd>, right paddle <kbd>↑</kbd>/<kbd>↓</kbd> (<kbd>C</kbd> swaps the AI for a
second player). Press <kbd>K</kbd> there to switch to a naive collision test and watch the ball
tunnel straight through a paddle — that failure, and why a 60 Hz machine can never show it to you,
is [Lesson 1.8](docs/lessons/01-08-pong.html).

### Prerequisites

| Platform | Needs |
|---|---|
| **Windows** | Visual Studio 2022 (MSVC v143) with the C++ workload, CMake ≥ 3.24 |
| **Linux** | GCC ≥ 12 or Clang ≥ 15, CMake ≥ 3.24, plus SDL3's build deps (X11/Wayland dev packages) |
| **macOS** | Xcode Command Line Tools (Clang ≥ 15), CMake ≥ 3.24 |

**SDL3 is fetched automatically** by CMake via `FetchContent` at a pinned tag — you do not
install it yourself, and a fresh clone builds with no extra setup. The first configure takes a
few minutes while SDL3 compiles; after that it is cached.

Shaders are authored in **HLSL** and cross-compiled to SPIR-V / DXIL / MSL with
**SDL_shadercross** (introduced in Module 4).

---

## What gets built

By the final module the engine has: a documented public C++ API; an SDL_GPU forward **PBR**
renderer with shadow-mapped lights, HDR, tonemapping and a post-processing stack; skybox and
image-based lighting; an asset pipeline (images, OBJ, glTF); a handle-based resource system; a
from-scratch **ECS** with transform hierarchy; skeletal animation; rigid-body collision and
response; 3D audio; input mapping; an **ImGui editor** with hierarchy, inspector and gizmos;
profiling hooks; serialization and a scene format; hot reload; a job system; and a **capstone
game built solely against the public API**.

Hand-rolled on purpose: the math library (no GLM), rasterizer, OBJ parser, ECS, renderer, asset
system, allocators, and collision/rigid-body basics.

Third-party, each with an explicit "why we don't hand-roll this" justification: `stb_image`,
`stb_truetype`, Dear ImGui (tooling only — never gameplay UI), `cgltf`, SDL_shadercross.

---

## Repository layout

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full tree and the reasoning behind it. The short
version, as of **Lesson 5.11**:

```
engine/include/engine/   the public API — 56 headers, and the only path a demo can name
engine/include/engine/asset/      search_path, asset_store — names, roots, lifetimes
engine/include/engine/core/       clock, input, fixed_step, profile, log, assert,
                                  handle, pool, bench, actions + masked_input (5.10-5.11)
engine/include/engine/ecs/        entity, pool, registry, view — the world (5.8)
                                  hierarchy, camera — structure and viewpoint (5.9)
engine/include/engine/gfx/        debug_lines — the debug QUEUE, which includes
                                  nothing that can draw; debug_draw draws it (5.11)
engine/include/engine/platform/   how a program starts: platform.hpp, app.hpp, main.hpp
engine/include/engine/ui/         debug_ui — the Dear ImGui lifecycle. TOOLING ONLY (5.11)
engine/src/              private implementation; stb_image and <imgui.h> stop here
demos/common/            content shared by demos and verification harnesses
demos/sandbox/           Lessons 2.1–4.9, on [Tab] and four flags; keeps its own main()
demos/hello_cube/        the public-API acceptance test
demos/ecs_swarm/         Lessons 5.8-5.11: 154 entities, three levels, no scancodes,
                         152 debug lines and two ImGui panels
demos/pong/              Lesson 1.8's game, on engine::app — 87 lines, no main()
tools/                   the editor and asset cooker (Module 9)
```

`engine/include/engine/ecs/pool.hpp` declares `engine::ecs::pool<T>`, which is **not**
`engine::pool<T>` from `core/pool.hpp`. Same three arrays, opposite jobs: the core one mints its
own keys, the ECS one is keyed by an id it did not mint. The namespace keeps them apart, and the
header comments say so at both ends.

`engine/include/engine/platform/main.hpp` is the one public header **not** reachable through the
`engine.hpp` umbrella, and the omission is deliberate: including it defines a program's entry
point, so it belongs in exactly one `.cpp` per program. An umbrella whose promise is "include
everything, it is harmless" must not be a way to acquire a `main()` by accident.

**The boundary is law, and it is enforced by the include path rather than by discipline.**
`target_include_directories(engine PUBLIC include PRIVATE src)` means a demo writing
`#include "gfx/raster.hpp"` — the spelling every file in this repository used through Module 4 —
does not compile. Modules 0–4 built a single, library-shaped executable; the shape was already
right, which is why 57 files moved without one of them changing.

---

## Project documents

| File | Purpose |
|---|---|
| [CLAUDE.md](CLAUDE.md) | The master prompt — the binding specification for this course |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Repository layout, engine architecture, and the *why* |
| [LEARNINGS.md](LEARNINGS.md) | Verified SDL3/SDL_GPU facts and hard-won gotchas |
| [PROMPT.md](PROMPT.md) | Prompt log |
| `memory/` | Dated session summaries |

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 digster.
