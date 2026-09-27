// engine/src/core/log.cpp — the parser, and the file sink.
//
// Everything here is the small amount of work SDL's logging system leaves to the
// application: naming our categories, turning a human-typed filter into calls to
// SDL_SetLogPriority, and adding a second destination without removing the first.

#include <engine/core/log.hpp>

#include <cstddef>
#include <iterator>   // std::size — over a C array, so no container is involved

namespace engine {
namespace {

struct named_category
{
    const char* name;
    log_category value;
};

// The table is the single source of truth for both directions: `name_of` reads
// it forwards and the parser reads it backwards. Two lists that must agree is
// two lists that eventually will not.
constexpr named_category k_categories[] = {
    {"core",     log_core},
    {"platform", log_platform},
    {"gfx",      log_gfx},
    {"gpu",      log_gpu},
    {"asset",    log_asset},
};

static_assert(std::size(k_categories) == static_cast<std::size_t>(log_category_count)
                                      - static_cast<std::size_t>(log_core),
              "every log_category needs a name — add it to k_categories");

struct named_priority
{
    const char* name;
    SDL_LogPriority value;
};

// `quiet` maps to CRITICAL+1 rather than to a real priority, because SDL has no
// "off" value: silencing a category means setting its threshold above anything
// that can be emitted. SDL_LOG_PRIORITY_COUNT is exactly that number, and using
// the enum's own count rather than a literal 8 means this stays correct if SDL
// ever adds a level.
constexpr named_priority k_priorities[] = {
    {"trace",    SDL_LOG_PRIORITY_TRACE},
    {"verbose",  SDL_LOG_PRIORITY_VERBOSE},
    {"debug",    SDL_LOG_PRIORITY_DEBUG},
    {"info",     SDL_LOG_PRIORITY_INFO},
    {"warn",     SDL_LOG_PRIORITY_WARN},
    {"error",    SDL_LOG_PRIORITY_ERROR},
    {"critical", SDL_LOG_PRIORITY_CRITICAL},
    {"quiet",    SDL_LOG_PRIORITY_COUNT},
};

/// One parsed entry, held until the whole spec has been validated.
struct pending
{
    int category;               ///< or -1 for `*`
    SDL_LogPriority priority;
};

// The file sink's state. A file-scope pointer rather than a parameter because
// SDL's output-function hook takes a single `void* userdata` and we would
// otherwise be allocating something to hold two fields — and this is genuinely
// process-global: there is one log.
SDL_IOStream* g_log_file = nullptr;
SDL_LogOutputFunction g_previous_output = nullptr;
void* g_previous_userdata = nullptr;

/// Write to the file, then hand the message on to whoever had the hook before us.
///
/// The order matters on a crash: if the program dies inside the default handler
/// (it will not, but the shape is the point) the file already has the line. Log
/// files exist for the runs that ended badly.
void SDLCALL tee_to_file(void* userdata, int category, SDL_LogPriority priority,
                         const char* message)
{
    (void)userdata;

    if (g_log_file != nullptr)
    {
        char line[1024];
        const int n = SDL_snprintf(line, sizeof(line), "[%s] %s: %s\n",
                                   name_of(priority),
                                   name_of(static_cast<log_category>(category)),
                                   message);
        if (n > 0)
        {
            const std::size_t len = SDL_min(static_cast<std::size_t>(n), sizeof(line) - 1);
            SDL_WriteIO(g_log_file, line, len);
            // Flushed by writing, not buffered by us: SDL_IOStream on a file is
            // already buffered by the platform, and a log that is lost because
            // the process aborted before a flush is a log that was not worth
            // keeping.
        }
    }

    if (g_previous_output != nullptr)
    {
        g_previous_output(g_previous_userdata, category, priority, message);
    }
}

[[nodiscard]] bool token_equals(const char* begin, const char* end, const char* name)
{
    const std::size_t len = static_cast<std::size_t>(end - begin);
    return SDL_strlen(name) == len && SDL_strncmp(begin, name, len) == 0;
}

/// Trim ASCII spaces and tabs from both ends of [begin, end).
void trim(const char*& begin, const char*& end)
{
    while (begin < end && (*begin == ' ' || *begin == '\t')) { ++begin; }
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) { --end; }
}

} // namespace

const char* name_of(log_category c)
{
    for (const named_category& n : k_categories)
    {
        if (n.value == c) { return n.name; }
    }
    return "?";
}

const char* name_of(SDL_LogPriority p)
{
    for (const named_priority& n : k_priorities)
    {
        if (n.value == p) { return n.name; }
    }
    return "?";
}

SDL_LogPriority compiled_log_floor()
{
    return static_cast<SDL_LogPriority>(ENGINE_LOG_MIN_PRIORITY);
}

bool set_log_levels(const char* spec)
{
    if (spec == nullptr) { return false; }

    // TWO PASSES, AND THE SECOND ONE IS THE POINT. Parsing straight into
    // SDL_SetLogPriority would mean a typo in the last entry leaves the first
    // three applied — you asked for one thing, got another, and nothing said so.
    // Validating first makes a rejected spec a no-op, which is the only
    // behaviour a user can reason about.
    pending parsed[log_category_count - log_core + 1];
    int count = 0;

    const char* cursor = spec;
    while (*cursor != '\0')
    {
        const char* entry_end = SDL_strchr(cursor, ',');
        if (entry_end == nullptr) { entry_end = cursor + SDL_strlen(cursor); }

        const char* begin = cursor;
        const char* end = entry_end;
        trim(begin, end);

        if (begin == end)
        {
            ENGINE_LOG_ERROR(log_core, "--log: empty entry in \"%s\"", spec);
            return false;
        }

        // Split on '=' if there is one; otherwise the whole token is a priority
        // and the category is `*`.
        const char* eq = begin;
        while (eq < end && *eq != '=') { ++eq; }

        const char* cat_begin = begin;
        const char* cat_end = (eq < end) ? eq : begin;
        const char* pri_begin = (eq < end) ? eq + 1 : begin;
        const char* pri_end = end;
        trim(cat_begin, cat_end);
        trim(pri_begin, pri_end);

        SDL_LogPriority priority = SDL_LOG_PRIORITY_INVALID;
        for (const named_priority& n : k_priorities)
        {
            if (token_equals(pri_begin, pri_end, n.name)) { priority = n.value; break; }
        }
        if (priority == SDL_LOG_PRIORITY_INVALID)
        {
            ENGINE_LOG_ERROR(log_core, "--log: \"%.*s\" is not a level"
                             " (trace/debug/info/warn/error/critical/quiet)",
                             static_cast<int>(pri_end - pri_begin), pri_begin);
            return false;
        }

        int category = -1;   // -1 means `*`
        if (cat_begin != cat_end && !token_equals(cat_begin, cat_end, "*"))
        {
            bool found = false;
            for (const named_category& n : k_categories)
            {
                if (token_equals(cat_begin, cat_end, n.name))
                {
                    category = static_cast<int>(n.value);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ENGINE_LOG_ERROR(log_core, "--log: \"%.*s\" is not a category"
                                 " (core/platform/gfx/gpu/asset/*)",
                                 static_cast<int>(cat_end - cat_begin), cat_begin);
                return false;
            }
        }

        if (count >= static_cast<int>(std::size(parsed)))
        {
            ENGINE_LOG_ERROR(log_core, "--log: more entries than there are categories");
            return false;
        }
        parsed[count++] = {category, priority};

        cursor = (*entry_end == ',') ? entry_end + 1 : entry_end;
    }

    // Pass two: nothing below here can fail.
    for (int i = 0; i < count; ++i)
    {
        if (parsed[i].category >= 0)
        {
            SDL_SetLogPriority(parsed[i].category, parsed[i].priority);
        }
        else
        {
            // `*` means OUR categories, not everything. Silencing SDL's own
            // diagnostics is not ours to do on the user's behalf — if they want
            // that, SDL_LOGGING is right there and it is documented.
            for (const named_category& n : k_categories)
            {
                SDL_SetLogPriority(static_cast<int>(n.value), parsed[i].priority);
            }
        }
    }

    return true;
}

bool configure_logging(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--log") != 0) { continue; }

        if (i + 1 >= argc)
        {
            ENGINE_LOG_ERROR(log_core, "--log needs a spec, e.g. --log gfx=debug,*=warn");
            return false;
        }
        return set_log_levels(argv[++i]);
    }
    return true;   // no flag is not a failure
}

bool set_log_file(const char* path)
{
    if (path == nullptr)
    {
        if (g_log_file != nullptr)
        {
            SDL_CloseIO(g_log_file);
            g_log_file = nullptr;
            SDL_SetLogOutputFunction(g_previous_output, g_previous_userdata);
            g_previous_output = nullptr;
            g_previous_userdata = nullptr;
        }
        return true;
    }

    if (g_log_file != nullptr)
    {
        ENGINE_LOG_WARN(log_core, "set_log_file: a log file is already open;"
                        " close it with set_log_file(nullptr) first");
        return false;
    }

    SDL_IOStream* const file = SDL_IOFromFile(path, "w");
    if (file == nullptr)
    {
        ENGINE_LOG_ERROR(log_core, "set_log_file: cannot open %s: %s", path, SDL_GetError());
        return false;
    }

    // Capture whoever currently owns the hook BEFORE installing ours, so the
    // chain is honest even if somebody else got here first.
    SDL_GetLogOutputFunction(&g_previous_output, &g_previous_userdata);
    g_log_file = file;
    SDL_SetLogOutputFunction(tee_to_file, nullptr);
    return true;
}

} // namespace engine
