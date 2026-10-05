#include "skater_state_debug.h"
#include "local_skater_state.h"
#include "Engine/Core/Log/logging.h"
#include <Windows.h>
#include <atomic>
#include <format>
#include <string>

namespace dingosdk::skater_state::debug {
namespace {
constexpr std::uint64_t flash_ms = 1500;

struct Field {
    std::string label, value;
    bool heading{};
    bool continuous{};          // a measurement: changes every tick, so it neither flashes nor is logged
    std::uint64_t changed_at{}; // GetTickCount64(); 0 = never changed
};
struct State {
    std::atomic<bool> enabled{};
    SRWLOCK lock = SRWLOCK_INIT;
    std::vector<Field> fields; // empty until the first sample
};
State& state() { static auto* value = new State; return *value; }

std::string yes_no(bool value) { return value ? "yes" : "no"; }
std::string_view mode_name(Mode mode) {
    return mode == Mode::on_board ? "on board" : mode == Mode::on_foot ? "on foot" : "ragdoll";
}

// Always the same fields in the same order, on the board or off it; "-" for what was not readable.
std::vector<Field> fields_of(bool known, const SkaterState& s) {
    const auto value = [known](std::string text) { return known ? std::move(text) : std::string("-"); };
    const auto flag = [&](bool on) { return value(s.offboard_known ? yes_no(on) : "-"); };
    std::vector<Field> fields;
    fields.push_back({"SKATER", {}, true});
    fields.push_back({"Physics state", value(s.physics_state_name.empty() ? std::to_string(s.physics_state)
        : std::format("{} {}", s.physics_state, s.physics_state_name))});
    fields.push_back({"Mode", value(std::string(mode_name(mode(s))))});
    fields.push_back({"Airborne", value(yes_no(airborne(s)))});
    fields.push_back({"OFF THE BOARD", {}, true});
    const auto& f = s.flags;
    fields.push_back({"Substate", value(s.offboard_known ? std::string(substate_name(f.substate)) : "-")});
    fields.push_back({"Height above ground", value(s.offboard_known ? std::format("{:.2f} m", f.height_above_ground) : "-"),
        false, true});
    fields.push_back({"In the air", flag(f.in_the_air)});
    fields.push_back({"Falling", flag(f.falling)});
    fields.push_back({"Landing", flag(f.landing)});
    fields.push_back({"Mounting", flag(f.mounting)});
    fields.push_back({"Off the board", flag(f.off_the_board)});
    fields.push_back({"Foot A planted", flag(f.foot_a_planted)});
    fields.push_back({"Foot B planted", flag(f.foot_b_planted)});
    return fields;
}
}

bool enabled() noexcept { return state().enabled.load(std::memory_order_acquire); }

void set_enabled(bool enabled) noexcept {
    auto& s = state();
    s.enabled.store(enabled, std::memory_order_release);
    AcquireSRWLockExclusive(&s.lock);
    s.fields.clear();
    ReleaseSRWLockExclusive(&s.lock);
}

void on_client_tick() noexcept {
    auto& s = state();
    if (!s.enabled.load(std::memory_order_acquire)) return;
    try {
        LocalSkater skater;
        SkaterState live;
        const bool known = current_local_skater(skater) && read(skater, live);
        auto next = fields_of(known, live);
        const auto now = GetTickCount64();
        std::string changes, height;
        AcquireSRWLockExclusive(&s.lock);
        if (s.fields.size() == next.size()) {
            for (std::size_t i = 0; i < next.size(); ++i) {
                next[i].changed_at = s.fields[i].changed_at;
                if (next[i].continuous) height = next[i].value;
                if (next[i].heading || next[i].continuous || next[i].value == s.fields[i].value) continue;
                next[i].changed_at = now;
                changes += std::format(" | {} {}->{}", next[i].label, s.fields[i].value, next[i].value);
            }
        }
        s.fields = std::move(next);
        ReleaseSRWLockExclusive(&s.lock);
        // Each change is logged with the height above ground at that moment.
        if (!changes.empty())
            logging::log(logging::Level::info, logging::Channel::skater, "Skater state:{} | height {}", changes, height);
    } catch (...) { /* A lost sample is never worth the client tick. */ }
}

std::vector<overlay::DebugField> fields() {
    auto& s = state();
    std::vector<overlay::DebugField> result;
    if (!s.enabled.load(std::memory_order_acquire)) return result;
    const auto now = GetTickCount64();
    AcquireSRWLockShared(&s.lock);
    for (const auto& field : s.fields) {
        const auto age = field.changed_at ? now - field.changed_at : flash_ms;
        result.push_back({field.label, field.value,
            age < flash_ms ? 1.0f - static_cast<float>(age) / static_cast<float>(flash_ms) : 0.0f, field.heading});
    }
    ReleaseSRWLockShared(&s.lock);
    return result;
}
}
