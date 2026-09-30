// engine/src/gfx/gpu_compute.cpp — loading a compute pipeline.
//
// Lesson 6.18b. The shape of `gpu_shader::load`, and deliberately so: choose the
// format this device accepts, read the code, read the reflection, refuse to
// guess, create, name. What differs is only what the reflection says and which
// SDL function receives it.

#include <engine/gfx/gpu_compute.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/gpu_debug.hpp>   // scoped_properties: named at CREATION (4.9)

#include <string_view>
#include <utility>

namespace engine {

gpu_compute_pipeline::~gpu_compute_pipeline()
{
    destroy();
}

gpu_compute_pipeline::gpu_compute_pipeline(gpu_compute_pipeline&& other) noexcept
    : device_(other.device_), pipeline_(other.pipeline_), resources_(other.resources_),
      code_bytes_(other.code_bytes_), name_(std::move(other.name_))
{
    other.device_ = nullptr;
    other.pipeline_ = nullptr;
    other.resources_ = {};
    other.code_bytes_ = 0;
}

gpu_compute_pipeline& gpu_compute_pipeline::operator=(gpu_compute_pipeline&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        pipeline_ = other.pipeline_;
        resources_ = other.resources_;
        code_bytes_ = other.code_bytes_;
        name_ = std::move(other.name_);
        other.device_ = nullptr;
        other.pipeline_ = nullptr;
        other.resources_ = {};
        other.code_bytes_ = 0;
    }
    return *this;
}

bool gpu_compute_pipeline::load(const gpu_device& dev, const char* name)
{
    destroy();

    if (!dev.valid() || name == nullptr) { return false; }

    device_ = dev.handle();
    name_ = name;

    // ---- 1. Which file does this device want? --------------------------------
    //
    // The same choice a graphics shader makes, and the same trap with it: under
    // MSL the entry point is `main0`, because SPIRV-Cross renames a function
    // called `main` (Lesson 4.3). A kernel is no exception.
    const shader_target target = choose_shader_target(dev.report().granted);
    if (!target.ok())
    {
        ENGINE_LOG_ERROR(log_gpu, "compute '%s': this device accepts no format we emit", name);
        destroy();
        return false;
    }

    // ---- 2. The compiled code ------------------------------------------------
    const std::string code_file = shader_path((name_ + "." + target.extension).c_str());
    std::size_t code_size = 0;
    void* code = SDL_LoadFile(code_file.c_str(), &code_size);
    if (code == nullptr)
    {
        ENGINE_LOG_ERROR(log_gpu, "compute '%s': cannot read %s (%s)", name, code_file.c_str(),
                         SDL_GetError());
        destroy();
        return false;
    }

    // ---- 3. The reflection -----------------------------------------------------
    const std::string json_file = shader_path((name_ + ".json").c_str());
    std::size_t json_size = 0;
    void* json = SDL_LoadFile(json_file.c_str(), &json_size);
    if (json == nullptr)
    {
        ENGINE_LOG_ERROR(log_gpu, "compute '%s': cannot read %s (%s)", name, json_file.c_str(),
                         SDL_GetError());
        SDL_free(code);
        destroy();
        return false;
    }

    const bool parsed = parse_compute_reflection(
        std::string_view(static_cast<const char*>(json), json_size), resources_);
    SDL_free(json);

    if (!parsed)
    {
        // The usual cause is a GRAPHICS shader's JSON: four counts, no thread
        // group. The message names the file so the mistake names itself.
        ENGINE_LOG_ERROR(log_gpu, "compute '%s': %s is not a compute reflection — refusing "
                         "to guess six counts and a group size", name, json_file.c_str());
        SDL_free(code);
        destroy();
        return false;
    }

    // ---- 4. Create it --------------------------------------------------------
    SDL_GPUComputePipelineCreateInfo info{};
    info.code_size = code_size;
    info.code = static_cast<const Uint8*>(code);
    info.entrypoint = target.entrypoint;
    info.format = target.format;
    info.num_samplers = resources_.samplers;
    info.num_readonly_storage_textures = resources_.readonly_storage_textures;
    info.num_readonly_storage_buffers = resources_.readonly_storage_buffers;
    info.num_readwrite_storage_textures = resources_.readwrite_storage_textures;
    info.num_readwrite_storage_buffers = resources_.readwrite_storage_buffers;
    info.num_uniform_buffers = resources_.uniform_buffers;

    // "This should match the value in the shader" — SDL_gpu.h. It is read FROM
    // the shader, so it cannot fail to.
    info.threadcount_x = resources_.threads_x;
    info.threadcount_y = resources_.threads_y;
    info.threadcount_z = resources_.threads_z;

    scoped_properties props;
    props.set_name(SDL_PROP_GPU_COMPUTEPIPELINE_CREATE_NAME_STRING, name_.c_str());
    info.props = props.id();

    pipeline_ = SDL_CreateGPUComputePipeline(device_, &info);

    // SDL copies the code during creation, exactly as it does for a shader.
    SDL_free(code);

    if (pipeline_ == nullptr)
    {
        ENGINE_LOG_ERROR(log_gpu, "compute '%s': SDL_CreateGPUComputePipeline failed: %s", name,
                         SDL_GetError());
        destroy();
        return false;
    }

    code_bytes_ = code_size;
    ENGINE_LOG_INFO(log_gpu, "compute '%s': %u x %u x %u threads a group; %u RO + %u RW "
                    "buffers, %u uniform block(s); %zu B of %s",
                    name, resources_.threads_x, resources_.threads_y, resources_.threads_z,
                    resources_.readonly_storage_buffers, resources_.readwrite_storage_buffers,
                    resources_.uniform_buffers, code_size, name_of(target.format));
    return true;
}

void gpu_compute_pipeline::destroy()
{
    if (device_ != nullptr && pipeline_ != nullptr)
    {
        SDL_ReleaseGPUComputePipeline(device_, pipeline_);
    }
    device_ = nullptr;
    pipeline_ = nullptr;
    resources_ = {};
    code_bytes_ = 0;
    name_.clear();
}

} // namespace engine
