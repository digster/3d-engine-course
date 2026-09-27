// src/gfx/image.hpp — pixels, decoded from a file somebody else's tool wrote.
//
// Lesson 4.7, and the first third-party code in this project that is not SDL.
//
// WHY WE DO NOT HAND-ROLL THIS, stated once and meant. Everything else in this
// course that could be written has been: the maths, the rasterizer, the OBJ
// parser, the ECS to come. An image decoder is different in KIND, and the
// difference is not difficulty. A baseline PNG decoder is an afternoon. PNG in
// the wild is DEFLATE plus five per-scanline filter modes plus Adam7 interlacing
// plus sixteen-bit channels plus palettes plus tRNS transparency plus gamma and
// colour-profile chunks — and a decoder that handles only the files you happened
// to test is worse than none, because it fails on a user's asset rather than
// yours. There is nothing about the ENGINE in the fifth filter mode.
//
// Compare with `parse_obj` (Lesson 3.5), which we DID write. OBJ is a line-based
// text format whose difficulty is entirely in the thing the course is about —
// the mismatch between how a file describes a vertex and how hardware fetches
// one. That is the test for hand-rolling: is the hard part the SUBJECT?
//
// WHAT WE ASK FOR AND WHAT WE GET. Always four channels, always eight bits,
// whatever the file contained. stb converts, and asking for one fixed shape means
// the rest of the engine never branches on what a file happened to hold. The cost
// is a byte per pixel for images that had no alpha, which is the right trade at
// this size and is worth revisiting only when an atlas is measured in hundreds of
// megabytes.
//
// COLOUR SPACE IS NOT DECIDED HERE. These bytes are whatever the file stored,
// which for a PNG is very nearly always sRGB-encoded. Lesson 3.9 established that
// an albedo must be LINEAR before it multiplies a quantity of light, and Lesson
// 4.7 hands that decode to the hardware by choosing an `_SRGB` texture format —
// the sampler then decodes for free, which is the thing Lesson 3.9's software
// path had to do by hand per texel.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

/// How a decode ended. `ok` is the only success.
///
/// An enum and not an exception, for the reason Lesson 3.5 gave and the engine
/// core still obeys: the caller writes the branch.
enum class image_status
{
    ok,
    cannot_open,    ///< the file is missing or unreadable
    bad_format,     ///< present, but stb could not make sense of it
    too_large       ///< beyond what we are willing to allocate at this point
};

[[nodiscard]] const char* name_of(image_status s);

/// Decoded pixels: `width * height` texels, four bytes each, row-major, top row
/// first.
///
/// **Top row first is the file's convention and the GPU's**, and it is why this
/// struct stores what it stores without flipping anything. Lesson 3.9's uv flip
/// happens at the *geometry* import, not here — the same rule as `load_obj`,
/// which stores what the file said. A loader that quietly reorients its data is a
/// loader whose output cannot be compared with the file it came from.
struct image_data
{
    std::vector<std::uint8_t> pixels;   ///< RGBA, 8 bits each, row-major
    int width = 0;
    int height = 0;

    /// Channels the FILE contained, before stb converted to four. Diagnostic
    /// only — the pixels above are always RGBA — but worth reporting, because
    /// "why is my alpha 255 everywhere" has this as its answer.
    int source_channels = 0;

    [[nodiscard]] std::size_t byte_count() const { return pixels.size(); }
    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0
            && pixels.size() == static_cast<std::size_t>(width) * height * 4u;
    }
};

/// The most texels we will decode in one image, and a deliberate policy rather
/// than a technical limit.
///
/// 8192 x 8192 is 256 MB at four bytes a texel. Beyond that, something has gone
/// wrong with the asset or with the path we were handed, and failing with a name
/// beats allocating a quarter of a gigabyte and finding out later.
inline constexpr std::size_t k_max_image_texels = 8192u * 8192u;

/// Decode `path` into `out`.
///
/// @param path an absolute path, or one relative to the working directory. Use
///             `asset_path()` (Lesson 3.5) to get a file that sits beside the
///             executable, which is where the build puts them.
[[nodiscard]] image_status load_image(const char* path, image_data& out);

} // namespace engine
