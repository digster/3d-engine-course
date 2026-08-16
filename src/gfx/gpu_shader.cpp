// src/gfx/gpu_shader.cpp — choosing a format, reading the reflection, creating
// the shader.

#include "gfx/gpu_shader.hpp"

#include <cctype>
#include <utility>

namespace engine {

const char* name_of(SDL_GPUShaderFormat single_format)
{
    switch (single_format)
    {
    case SDL_GPU_SHADERFORMAT_PRIVATE:  return "PRIVATE";
    case SDL_GPU_SHADERFORMAT_SPIRV:    return "SPIRV";
    case SDL_GPU_SHADERFORMAT_DXBC:     return "DXBC";
    case SDL_GPU_SHADERFORMAT_DXIL:     return "DXIL";
    case SDL_GPU_SHADERFORMAT_MSL:      return "MSL";
    case SDL_GPU_SHADERFORMAT_METALLIB: return "METALLIB";
    default:                            return "INVALID";
    }
}

shader_target choose_shader_target(SDL_GPUShaderFormat granted)
{
    shader_target t;

    // Order matters, and it is an order of PREFERENCE among formats our build
    // actually emits — not an order of quality. A device usually accepts more
    // than one; Lesson 4.2 measured this one accepting MSL | METALLIB, and we
    // emit MSL, so MSL is what we ask for.
    //
    // METALLIB is deliberately absent: it is Metal's *precompiled* form, which
    // needs Apple's toolchain at build time and buys startup time we are not
    // short of. Naming it here without producing it would be a lie the loader
    // would then trip over.
    if ((granted & SDL_GPU_SHADERFORMAT_MSL) != 0u)
    {
        t.format = SDL_GPU_SHADERFORMAT_MSL;
        t.extension = "msl";

        // SPIRV-Cross renames the entry point, because `main` is reserved in
        // MSL. This single line is the difference between a shader that loads
        // and an error message about a function that does not exist.
        t.entrypoint = "main0";
    }
    else if ((granted & SDL_GPU_SHADERFORMAT_SPIRV) != 0u)
    {
        t.format = SDL_GPU_SHADERFORMAT_SPIRV;
        t.extension = "spv";
        t.entrypoint = "main";
    }
    else if ((granted & SDL_GPU_SHADERFORMAT_DXIL) != 0u)
    {
        t.format = SDL_GPU_SHADERFORMAT_DXIL;
        t.extension = "dxil";
        t.entrypoint = "main";
    }

    return t;
}

std::string shader_path(const char* relative)
{
    // Same contract as asset_path (gfx/obj.cpp): SDL3 returns a CACHED pointer
    // the caller must not free, and it can be null on platforms that cannot
    // answer, in which case a bare relative path is the honest fallback.
    const char* base = SDL_GetBasePath();

    std::string path = (base != nullptr) ? std::string(base) : std::string();
    path += "shaders/";
    path += relative;
    return path;
}

namespace {

/// Find `"key"` and read the non-negative integer that follows it.
///
/// The whole scanner, and the reason it can be this small is in the header: this
/// input is a build artefact we generated, not user data.
bool read_count(std::string_view json, std::string_view key, Uint32& out)
{
    // Search for the key WITH its quotes. Without them, "samplers" would also
    // match inside a longer key, which is the kind of bug that surfaces the day
    // somebody adds a field.
    std::string quoted;
    quoted.reserve(key.size() + 2);
    quoted += '"';
    quoted += key;
    quoted += '"';

    const std::size_t at = json.find(quoted);
    if (at == std::string_view::npos) { return false; }

    std::size_t i = at + quoted.size();
    while (i < json.size() && (json[i] == ':' || std::isspace(static_cast<unsigned char>(json[i]))))
    {
        ++i;
    }
    if (i >= json.size() || std::isdigit(static_cast<unsigned char>(json[i])) == 0) { return false; }

    Uint32 value = 0;
    while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i])) != 0)
    {
        value = value * 10u + static_cast<Uint32>(json[i] - '0');
        ++i;
    }
    out = value;
    return true;
}

} // namespace

bool parse_shader_reflection(std::string_view json, shader_resources& out)
{
    shader_resources r;

    // All four, or none. A partially-parsed reflection would mean creating a
    // shader with some counts read and some assumed zero, which is precisely the
    // failure this file exists to make impossible.
    const bool ok = read_count(json, "samplers", r.samplers)
                 && read_count(json, "storage_textures", r.storage_textures)
                 && read_count(json, "storage_buffers", r.storage_buffers)
                 && read_count(json, "uniform_buffers", r.uniform_buffers);

    if (ok) { out = r; }
    return ok;
}

gpu_shader::~gpu_shader()
{
    destroy();
}

gpu_shader::gpu_shader(gpu_shader&& other) noexcept
    : device_(other.device_), shader_(other.shader_), resources_(other.resources_),
      target_(other.target_), code_bytes_(other.code_bytes_), name_(std::move(other.name_))
{
    other.device_ = nullptr;
    other.shader_ = nullptr;
    other.resources_ = {};
    other.target_ = {};
    other.code_bytes_ = 0;
}

gpu_shader& gpu_shader::operator=(gpu_shader&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        shader_ = other.shader_;
        resources_ = other.resources_;
        target_ = other.target_;
        code_bytes_ = other.code_bytes_;
        name_ = std::move(other.name_);
        other.device_ = nullptr;
        other.shader_ = nullptr;
        other.resources_ = {};
        other.target_ = {};
        other.code_bytes_ = 0;
    }
    return *this;
}

bool gpu_shader::load(const gpu_device& dev, const char* name, shader_stage stage)
{
    destroy();

    if (!dev.valid() || name == nullptr) { return false; }

    device_ = dev.handle();
    name_ = name;

    // ---- 1. Which file does this device want? ------------------------------
    target_ = choose_shader_target(dev.report().granted);
    if (!target_.ok())
    {
        SDL_Log("shader '%s': this device accepts no format we emit", name);
        destroy();
        return false;
    }

    // ---- 2. The compiled code ----------------------------------------------
    const std::string code_file = shader_path((name_ + "." + target_.extension).c_str());

    std::size_t code_size = 0;
    void* code = SDL_LoadFile(code_file.c_str(), &code_size);
    if (code == nullptr)
    {
        SDL_Log("shader '%s': cannot read %s (%s)", name, code_file.c_str(), SDL_GetError());
        SDL_Log("  Did the build compile shaders? cmake/Shaders.cmake prints what it found.");
        destroy();
        return false;
    }

    // ---- 3. The reflection --------------------------------------------------
    const std::string json_file = shader_path((name_ + ".json").c_str());

    std::size_t json_size = 0;
    void* json = SDL_LoadFile(json_file.c_str(), &json_size);
    if (json == nullptr)
    {
        SDL_Log("shader '%s': cannot read %s (%s)", name, json_file.c_str(), SDL_GetError());
        SDL_free(code);
        destroy();
        return false;
    }

    const bool parsed = parse_shader_reflection(
        std::string_view(static_cast<const char*>(json), json_size), resources_);
    SDL_free(json);

    if (!parsed)
    {
        // Refusing here rather than defaulting to zero is the whole argument. A
        // shader created with counts that are too low binds nothing at the slots
        // it reads, and reads garbage — silently, at full frame rate.
        SDL_Log("shader '%s': %s is not the reflection we expect — refusing to guess counts",
                name, json_file.c_str());
        SDL_free(code);
        destroy();
        return false;
    }

    // ---- 4. Create it -------------------------------------------------------
    SDL_GPUShaderCreateInfo info{};
    info.code_size = code_size;
    info.code = static_cast<const Uint8*>(code);
    info.entrypoint = target_.entrypoint;
    info.format = target_.format;
    info.stage = (stage == shader_stage::vertex) ? SDL_GPU_SHADERSTAGE_VERTEX
                                                 : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = resources_.samplers;
    info.num_storage_textures = resources_.storage_textures;
    info.num_storage_buffers = resources_.storage_buffers;
    info.num_uniform_buffers = resources_.uniform_buffers;

    // Name it for the debugger, and note the asymmetry: buffers and textures have
    // SDL_SetGPUBufferName / SDL_SetGPUTextureName, and a SHADER DOES NOT. It is
    // named through a creation property instead, because a shader is immutable
    // the moment it exists — there is no later at which to set anything on it.
    // (Checked in SDL_gpu.h at release-3.4.12 rather than assumed from the
    // pattern; assuming the setter existed cost one compile error while this
    // lesson was written.)
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props != 0)
    {
        SDL_SetStringProperty(props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, name_.c_str());
        info.props = props;
    }

    shader_ = SDL_CreateGPUShader(device_, &info);

    if (props != 0) { SDL_DestroyProperties(props); }

    // SDL copies the code during creation, so the buffer is ours to free the
    // instant the call returns, success or not. Holding it would be a leak of
    // exactly the size of every shader in the program.
    SDL_free(code);

    if (shader_ == nullptr)
    {
        SDL_Log("shader '%s': SDL_CreateGPUShader failed: %s", name, SDL_GetError());
        destroy();
        return false;
    }

    code_bytes_ = code_size;
    return true;
}

void gpu_shader::destroy()
{
    if (device_ != nullptr && shader_ != nullptr)
    {
        SDL_ReleaseGPUShader(device_, shader_);
    }
    device_ = nullptr;
    shader_ = nullptr;
    resources_ = {};
    target_ = {};
    code_bytes_ = 0;
    name_.clear();
}

} // namespace engine
