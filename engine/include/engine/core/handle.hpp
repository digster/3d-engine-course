// engine/include/engine/core/handle.hpp — a reference that can answer "am I still valid?"
//
// Every resource this engine has ever handed around has been a pointer, a
// reference, or a `std::span` (which is a pointer and a length). That has worked
// for exactly one reason, and `demos/hello_cube/main.cpp` says it out loud in a
// comment: "the data must outlive every frame that uses it, which it does, being
// a local of main()". Nothing is ever destroyed, so nothing can ever dangle.
//
// Lesson 5.5's asset system can unload. The moment it can, every one of those
// pointers becomes a question nobody in the program is able to answer:
//
//     IS THIS STILL VALID?
//
// A pointer cannot answer it. That is not an oversight in C++; it is what a
// pointer IS. A pointer to freed memory holds the same bits it always held, and
// those bits will happily name whatever moved in afterwards. There is no test you
// can write, no assertion you can add, no compiler flag that turns a dangling
// pointer into a diagnosable event — because from the pointer's side, nothing
// happened.
//
// Lesson 5.4 replaces the pointer with two integers.
//
//     INDEX       which slot           — "where"
//     GENERATION  which occupant       — "which one"
//
// The index alone is the obvious idea, and on its own it makes things WORSE: it
// survives the storage moving (good) and it silently starts naming a different
// object when the slot is reused (much worse than dangling, because dangling at
// least tends to crash). The generation is what closes it. Freeing a slot bumps
// its generation, so a handle issued before the free carries a generation the
// slot no longer has, and the lookup FAILS — loudly, cheaply, and every time.
//
// What that buys, beyond not crashing:
//
//   - STORAGE BECOMES RELOCATABLE. Nothing outside the pool holds an address, so
//     the pool may grow, move, compact or reorder its items whenever it likes.
//     `pool` in core/pool.hpp does exactly that on every removal.
//   - SERIALIZATION BECOMES TRIVIAL. A handle is a 32-bit integer. Write it,
//     read it, done — no pointer fixups, no relocation table, no "which base
//     address was this saved against". Module 9's scene format needs this.
//   - IT IS CHEAP TO COPY AND SAFE TO STORE. Four bytes, trivially copyable,
//     no lifetime relationship to anything. Which is precisely what a component
//     in Module 5's ECS has to be.
//
// See docs/lessons/05-04-handles.html for the derivation.

#pragma once

#include <cstdint>

namespace engine {

// ---- The bit budget --------------------------------------------------------
//
// This is a real design decision with a real trade, and it deserves numbers
// rather than a shrug. We are splitting one 32-bit word:
//
//     31                    12 11                   0
//     +------------------------+---------------------+
//     |    generation (12)     |     index (20)      |
//     +------------------------+---------------------+
//
//   - 20 index bits  -> 1,048,576 slots. A million meshes, or a million
//     entities, is far past the point where something else in the engine breaks
//     first.
//   - 12 generation bits -> values 0..4,095, of which 0 is reserved (below), so
//     4,095 usable generations per slot. A slot's generation therefore REPEATS
//     after 4,095 frees of that same slot, and a handle held across all 4,095
//     would start resolving to somebody else's object again.
//
// Is 4,095 enough? Do the arithmetic instead of asserting. The dangerous case is
// one slot being recycled hard while an old handle to it survives:
//
//     recycled once a frame at 60 Hz  ->  4,095 / 60      =  68 seconds
//     recycled 100x a second          ->  4,095 / 100     =  41 seconds
//     recycled once a second          ->  4,095 / 1       =  68 minutes
//
// So for ASSETS — meshes, textures, sounds, which are loaded at level boundaries
// and measured in hundreds — 12 bits is enormous. For ENTITIES in a bullet-hell
// spawning thousands per second it is worth thinking about, and two mitigations
// exist: a FIFO free list (core/pool.hpp §free_slots_) spreads reuse across every
// slot instead of hammering the most recent one, and a 64-bit handle split 32/32
// makes wrap-around unreachable in any real program.
//
// We take the 32-bit split, and `pool` COUNTS ITS WRAPS so the assumption is
// monitored rather than believed. The alternative doubles the size of every
// handle in every component of every entity, which in Module 5's ECS is a real
// number of cache lines, and buys safety against a case we can measure and see
// coming. This is the same shape of argument as the 16-bit index buffers in
// gfx/mesh.hpp: pick the small one, write down the ceiling, and check it.
//
// (For reference, EnTT — the most widely used C++ ECS — makes exactly this split
// for its default `entt::entity`: 20 bits of index, 12 bits of version, in 32.)

inline constexpr std::uint32_t k_handle_index_bits = 20;
inline constexpr std::uint32_t k_handle_generation_bits = 12;

static_assert(k_handle_index_bits + k_handle_generation_bits == 32,
              "a handle is exactly one 32-bit word; the two fields must fill it");

/// Mask covering the index field: 0x000FFFFF.
inline constexpr std::uint32_t k_handle_index_mask = (1u << k_handle_index_bits) - 1u;

/// Largest index a handle can name — so 1,048,576 slots, counting zero.
inline constexpr std::uint32_t k_handle_max_index = k_handle_index_mask;

/// Largest generation a handle can carry: 4,095.
inline constexpr std::uint32_t k_handle_max_generation = (1u << k_handle_generation_bits) - 1u;

/// The first generation a live slot ever has.
///
/// **Generation 0 is reserved**, and reserving it is what makes the null handle
/// free. A value-initialised `handle<T>` has all 32 bits zero, which decodes as
/// index 0, generation 0 — and since no live slot ever carries generation 0, that
/// value can never accidentally name a real object. So `handle<T> h{};` is null,
/// `std::vector<handle<T>> v(100)` is a hundred nulls, and a `memset` to zero
/// leaves a struct full of nulls. Every one of those is the answer you wanted,
/// and none of them cost an instruction.
inline constexpr std::uint32_t k_handle_first_generation = 1;

// ---- The handle ------------------------------------------------------------

/// A typed reference to something living in a `pool<T>`: index + generation.
///
/// **`T` is a PHANTOM parameter.** No `T` is stored here — `sizeof(handle<T>)` is
/// 4 for every `T`, and `T` may even be an incomplete type, so a header can
/// declare `handle<material>` without ever seeing what a material is. What the
/// parameter buys is that `handle<mesh_data>` and `handle<texture>` are DIFFERENT
/// TYPES, so passing one where the other is wanted is a compile error rather than
/// a lookup that succeeds and returns a triangle mesh dressed as an image.
///
/// That is the same argument `engine::engine` made in Lesson 5.1 — a distinction
/// the compiler can see costs nothing at runtime and catches a whole category of
/// mistake before it can run. Bare `std::uint32_t` handles look identical in a
/// debugger and are interchangeable in a function call; these are not.
///
/// Trivially copyable, four bytes, no lifetime of its own: copy it, store it in a
/// component, write it to a file, compare two of them for equality. The one thing
/// it cannot do is dereference itself, and that is deliberate. Resolution needs
/// the pool, so every use is forced to go somewhere that can say "no".
template <typename T>
struct handle
{
    /// Generation in the high 12 bits, index in the low 20. Public because a
    /// handle is a value, not an object with an invariant to defend, and because
    /// serialization (Module 9) wants exactly this word.
    std::uint32_t bits = 0;

    /// Which slot this handle names. Meaningless when `!valid()`.
    [[nodiscard]] constexpr std::uint32_t index() const { return bits & k_handle_index_mask; }

    /// Which occupant of that slot. Zero means "no occupant, ever" — see
    /// `k_handle_first_generation`.
    [[nodiscard]] constexpr std::uint32_t generation() const { return bits >> k_handle_index_bits; }

    /// Is this handle anything at all?
    ///
    /// **This is not the same question as "does this still resolve".** A handle
    /// can be perfectly well-formed and still be stale, and only the pool it came
    /// from knows which. `valid()` rejects the null handle; `pool::contains()`
    /// rejects the stale one. Confusing the two is the single most common way to
    /// misuse this type, so the names are deliberately different.
    [[nodiscard]] constexpr bool valid() const { return generation() != 0; }

    /// `if (h)` — the same question as `valid()`, spelled the way C++ expects.
    /// `explicit` so a handle cannot silently decay to an int in arithmetic.
    [[nodiscard]] explicit constexpr operator bool() const { return valid(); }

    /// Two handles are equal when they name the same occupant of the same slot.
    /// Defaulted, so it is a single 32-bit compare — which is the whole reason
    /// Lesson 5.4 can delete a five-field cache key.
    [[nodiscard]] friend constexpr bool operator==(handle, handle) = default;
};

/// Assemble a handle from its two fields. Normally only a `pool` calls this;
/// tests call it to fabricate stale handles on purpose.
///
/// Out-of-range inputs are masked rather than rejected, because this is the
/// bit-twiddling primitive and the range check belongs to whoever is allocating
/// slots. `pool::insert()` refuses to create the millionth-and-first slot; this
/// function just packs.
template <typename T>
[[nodiscard]] constexpr handle<T> make_handle(std::uint32_t index, std::uint32_t generation)
{
    return handle<T>{((generation & k_handle_max_generation) << k_handle_index_bits)
                     | (index & k_handle_index_mask)};
}

}   // namespace engine
