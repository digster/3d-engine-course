// src/gfx/gpu_shader.hpp — a compiled shader, and the four numbers SDL wants with it.
//
// Lesson 4.3. Three facts shape this whole file, and all three are things you
// cannot know by reading SDL's function signature:
//
//   1. THE DEVICE DECIDES WHICH FILE TO OPEN. A Metal device cannot load SPIR-V
//      and a Vulkan device cannot load MSL. `SDL_GetGPUShaderFormats` answers
//      what this device accepts — Lesson 4.2 logged it and promised this lesson
//      would use it — and the answer picks the file extension.
//
//   2. THE ENTRY POINT IS NOT ALWAYS "main". Our HLSL says `main`, but `main` is
//      a reserved identifier in MSL, so SPIRV-Cross renames it to `main0` on the
//      way through. Pass "main" with an MSL shader and creation fails with a
//      message about a missing function, which is a good error only if you know
//      why the name changed.
//
//   3. SDL WANTS RESOURCE COUNTS, AND THEY MUST BE RIGHT. Four numbers —
//      samplers, storage textures, storage buffers, uniform buffers — and
//      SDL_gpu.h's FAQ names getting them wrong as the commonest cause of a
//      shader that does not work. We do not count them by hand; shadercross
//      computes them from the compiled code into a JSON file, and we read that.
//
// Nothing here draws. Lesson 4.4 binds these objects into a pipeline; this file
// is about getting them onto the device correctly, which is a separate problem
// and the one that actually goes wrong.

#pragma once

#include "gfx/gpu_device.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// Which stage a shader is. Enumerator order matches `SDL_GPUShaderStage`
/// exactly — `VERTEX`, `FRAGMENT` — the same discipline Lesson 3.9 applied to
/// the sampler enums, and `verify_43` checks it.
enum class shader_stage
{
    vertex,
    fragment
};

/// The four counts `SDL_GPUShaderCreateInfo` asks for, and nothing else.
///
/// This struct is deliberately a subset of that create-info rather than a mirror
/// of it: the other fields (code, size, format, entry point, stage) are things
/// *we* know, and these four are things only the compiled shader knows. Keeping
/// the boundary at "what must be discovered" is what makes reading them from a
/// file the obvious move rather than a workaround.
struct shader_resources
{
    Uint32 samplers = 0;
    Uint32 storage_textures = 0;
    Uint32 storage_buffers = 0;
    Uint32 uniform_buffers = 0;

    [[nodiscard]] Uint32 total() const
    {
        return samplers + storage_textures + storage_buffers + uniform_buffers;
    }
};

/// One vertex input a shader declares, exactly as shadercross reports it.
///
/// Lesson 4.5. Lesson 4.4 ended with the vertex layout declared **twice, in two
/// languages, with nothing checking that the halves agree** — HLSL says
/// `float3 position : TEXCOORD0`, C++ says `FLOAT3 at offset 0` — and left the
/// cross-check as an exercise. This is that exercise, promoted to a feature,
/// because the same JSON file that already saves us from guessing the resource
/// counts has been carrying the answer since Lesson 4.3:
///
///     "inputs": [{ "name": "input.position", "type": "float3", "location": 0 },
///                { "name": "input.colour",   "type": "float4", "location": 1 }]
///
/// `location` is the semantic index — `TEXCOORD0` is location 0 — and it is the
/// only field SDL matches on. The name is for humans and for error messages.
struct shader_input
{
    std::string name;       ///< "input.position" — SPIRV-Cross's spelling, struct prefix and all
    std::string type;       ///< "float3", "float4", "uint" — what the SHADER receives
    Uint32 location = 0;    ///< the TEXCOORD index
};

/// Every vertex input one shader declares, in the order the reflection lists them.
///
/// A `std::vector` rather than the fixed array `pipeline_desc` uses, and the
/// asymmetry is deliberate: the pipeline's arrays are pointed at by a create-info
/// and must never reallocate (Lesson 4.4's dangling-pointer trap), while this one
/// is read once at load time and never handed to SDL at all.
using shader_inputs = std::vector<shader_input>;

/// Which compiled artefact to load on this device, and what its entry point is
/// called once it gets there.
struct shader_target
{
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;

    /// The file extension our build produces for this format — "msl", "spv",
    /// "dxil". Not owned; a string literal.
    const char* extension = "";

    /// **The trap.** "main0" for MSL, "main" for everything else. See the file
    /// header; `verify_43` §D measures what happens when it is wrong.
    const char* entrypoint = "main";

    [[nodiscard]] bool ok() const { return format != SDL_GPU_SHADERFORMAT_INVALID; }
};

/// Pick a shader format this device accepts, preferring the ones our build
/// actually emits.
///
/// @param granted the mask from `SDL_GetGPUShaderFormats` — what the DEVICE
///                accepts, which is not the mask passed to device creation.
[[nodiscard]] shader_target choose_shader_target(SDL_GPUShaderFormat granted);

/// Where compiled shaders live: beside the executable, exactly like assets.
///
/// A near-twin of `asset_path` in `gfx/obj.cpp`, and the duplication is left
/// visible on purpose. Two functions that both prepend `SDL_GetBasePath()` are
/// one function too many, and Module 5's asset system is where that gets its
/// answer — inventing a general resource-path abstraction now, for two callers,
/// would be building a system to avoid three lines.
[[nodiscard]] std::string shader_path(const char* relative);

/// Read the four counts out of shadercross's JSON reflection output.
///
/// **This is not a JSON parser and does not pretend to be one.** It scans for
/// four known keys and reads the integer after each. That is acceptable here for
/// a reason worth being explicit about: this file is a *build artefact we
/// produced ourselves*, three seconds ago, from a compiler we invoked. It is not
/// a trust boundary. `parse_obj` in Lesson 3.5 *is* one — it reads files a user
/// supplies — and that is why it validates everything and this does not.
///
/// @return false if any key is missing, which means the file is not what we think
///         it is, and guessing would be worse than failing.
[[nodiscard]] bool parse_shader_reflection(std::string_view json, shader_resources& out);

/// Read the `inputs` array out of the same JSON.
///
/// Separate from `parse_shader_reflection` because the two have different
/// contracts. The four counts are **mandatory** — a shader created without them
/// is a shader that reads garbage — so a missing key there is a hard failure. The
/// input list is *diagnostic*: a shader with no inputs at all is perfectly legal
/// (Lesson 4.4's fragment stage is one), so an absent or empty array is an answer,
/// not an error.
///
/// @return false only if the array is present and malformed.
[[nodiscard]] bool parse_shader_inputs(std::string_view json, shader_inputs& out);

/// Owns one `SDL_GPUShader`.
///
/// Move-only, like every device resource in this engine. A shader is *immutable
/// once created* and is consumed by pipeline creation; Lesson 4.4 will show that
/// the pipeline holds its own reference, so a shader may be released as soon as
/// every pipeline using it exists.
class gpu_shader
{
public:
    gpu_shader() = default;
    ~gpu_shader();

    gpu_shader(const gpu_shader&) = delete;
    gpu_shader& operator=(const gpu_shader&) = delete;

    gpu_shader(gpu_shader&& other) noexcept;
    gpu_shader& operator=(gpu_shader&& other) noexcept;

    /// Load `shaders/<name>.<ext>` and `shaders/<name>.json`, and create the
    /// shader object.
    ///
    /// @param name  "triangle.vert" — the stem our build emits, without extension.
    /// @return      false on any failure, with the reason logged. Both files must
    ///              exist: a shader whose reflection is missing would have to be
    ///              created with guessed counts, and a guessed count is the bug
    ///              this class exists to prevent.
    [[nodiscard]] bool load(const gpu_device& dev, const char* name, shader_stage stage);

    void destroy();

    [[nodiscard]] bool valid() const { return shader_ != nullptr; }
    [[nodiscard]] SDL_GPUShader* handle() const { return shader_; }
    [[nodiscard]] const shader_resources& resources() const { return resources_; }

    /// The vertex inputs this shader declares — empty for a fragment stage that
    /// takes none, and for any stage whose reflection did not list them.
    ///
    /// Lesson 4.5 feeds this straight to `pipeline_desc::check_layout`, which is
    /// the only place in the engine where the two halves of the vertex
    /// declaration are ever compared.
    [[nodiscard]] const shader_inputs& inputs() const { return inputs_; }
    [[nodiscard]] const shader_target& target() const { return target_; }

    /// Bytes of compiled code loaded — the interesting half of "what did that
    /// HLSL become", and it differs by an order of magnitude between formats.
    [[nodiscard]] std::size_t code_bytes() const { return code_bytes_; }

    [[nodiscard]] const std::string& name() const { return name_; }

private:
    SDL_GPUDevice* device_ = nullptr;   ///< NOT owned; gpu_device owns it
    SDL_GPUShader* shader_ = nullptr;   ///< owning by contract
    shader_resources resources_{};
    shader_inputs inputs_;
    shader_target target_{};
    std::size_t code_bytes_ = 0;
    std::string name_;
};

/// A human-readable name for a shader format, for logs.
[[nodiscard]] const char* name_of(SDL_GPUShaderFormat single_format);

} // namespace engine
