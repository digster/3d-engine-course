// engine/src/core/actions.cpp — the action map's implementation.
//
// Lesson 5.10. `update()` is NOT here — it is a template in the header, because
// it is parameterised on the SHAPE of its input source rather than on `input`
// itself, which is what makes the mapping logic testable without persuading SDL
// that a key is held. See the `input_snapshot` concept.
//
// What is left is declaration, lookup and binding: the operations that happen
// tens of times at start-up rather than thousands of times a second, and that
// therefore belong in a translation unit rather than in every caller.

#include <engine/core/actions.hpp>

#include <engine/core/assert.hpp>
#include <engine/core/log.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

// ---- Declaring -------------------------------------------------------------

action_id action_map::declare(std::string_view name)
{
    if (name.empty())
    {
        ENGINE_LOG_ERROR(log_core, "action_map: an action must have a name");
        return {};
    }

    if (const action_id existing = find(name); existing.valid()) { return existing; }

    if (names_.size() >= action_id::k_invalid)
    {
        ENGINE_LOG_ERROR(log_core, "action_map: out of action ids (%u max)",
                         action_id::k_invalid);
        return {};
    }

    names_.emplace_back(name);
    grow_to_fit();
    return action_id{static_cast<std::uint16_t>(names_.size() - 1u)};
}

action_id action_map::find(std::string_view name) const
{
    // A linear scan, and it is the right structure. Actions are declared once at
    // start-up and number in the dozens; the hot path is `value(id)`, which is an
    // array index. A hash map here would optimise the operation that happens
    // hundreds of times over the lifetime of the program and leave the one that
    // happens thousands of times a second exactly where it was.
    for (std::size_t i = 0; i < names_.size(); ++i)
    {
        if (names_[i] == name) { return action_id{static_cast<std::uint16_t>(i)}; }
    }
    return {};
}

std::string_view action_map::name_of(action_id id) const
{
    return in_range(id) ? std::string_view{names_[id.index]} : std::string_view{};
}

void action_map::grow_to_fit()
{
    values_.resize(names_.size(), 0.0f);
    held_.resize(names_.size(), 0u);
    held_prev_.resize(names_.size(), 0u);
    queued_.resize(names_.size(), 0u);
}

// ---- Binding ---------------------------------------------------------------

bool action_map::bind_key(action_id id, SDL_Scancode key, float scale)
{
    if (!in_range(id)) { return false; }
    if (key < 0 || static_cast<int>(key) >= SDL_SCANCODE_COUNT)
    {
        ENGINE_LOG_ERROR(log_core, "action_map: scancode %d is out of range for action '%s'",
                         static_cast<int>(key), names_[id.index].c_str());
        return false;
    }
    bindings_.push_back(binding{id, input_source::key, static_cast<std::int32_t>(key), scale});
    return true;
}

bool action_map::bind_mouse_button(action_id id, int button, float scale)
{
    if (!in_range(id)) { return false; }
    if (button < 1 || button > 5)
    {
        ENGINE_LOG_ERROR(log_core, "action_map: mouse button %d is out of range (1..5)", button);
        return false;
    }
    bindings_.push_back(binding{id, input_source::mouse_button, button, scale});
    return true;
}

bool action_map::bind_mouse_axis(action_id id, mouse_axis axis, float scale)
{
    if (!in_range(id)) { return false; }
    bindings_.push_back(
        binding{id, input_source::mouse_axis, static_cast<std::int32_t>(axis), scale});
    return true;
}

std::size_t action_map::clear_bindings(action_id id)
{
    if (!in_range(id)) { return 0; }
    const std::size_t before = bindings_.size();
    bindings_.erase(std::remove_if(bindings_.begin(), bindings_.end(),
                                   [id](const binding& b) { return b.action == id; }),
                    bindings_.end());
    return before - bindings_.size();
}

std::size_t action_map::binding_count(action_id id) const
{
    std::size_t n = 0;
    for (const binding& b : bindings_)
    {
        if (b.action == id) { ++n; }
    }
    return n;
}

// ---- The frame -------------------------------------------------------------

void action_map::reset()
{
    std::fill(values_.begin(), values_.end(), 0.0f);
    std::fill(held_.begin(), held_.end(), 0u);
    std::fill(held_prev_.begin(), held_prev_.end(), 0u);
    std::fill(queued_.begin(), queued_.end(), 0u);
    have_mouse_ = false;
}

// ---- Asking ----------------------------------------------------------------

float action_map::value(action_id id) const
{
    return in_range(id) ? values_[id.index] : 0.0f;
}

bool action_map::held(action_id id) const
{
    return in_range(id) && held_[id.index] != 0u;
}

bool action_map::pressed(action_id id) const
{
    return in_range(id) && held_[id.index] != 0u && held_prev_[id.index] == 0u;
}

bool action_map::released(action_id id) const
{
    return in_range(id) && held_[id.index] == 0u && held_prev_[id.index] != 0u;
}

bool action_map::consume_pressed(action_id id)
{
    if (!in_range(id) || queued_[id.index] == 0u) { return false; }
    --queued_[id.index];
    return true;
}

std::uint8_t action_map::pending_presses(action_id id) const
{
    return in_range(id) ? queued_[id.index] : std::uint8_t{0};
}

void action_map::clear_pending()
{
    std::fill(queued_.begin(), queued_.end(), 0u);
}

}   // namespace engine
