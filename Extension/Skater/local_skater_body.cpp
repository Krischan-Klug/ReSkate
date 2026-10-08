#include "local_skater_body.h"
#include "no_bail.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/skater_body.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace dingosdk::skater_body {
namespace {
namespace build = addr::skater_body;
static_assert(build::body_bone_count == count);

struct State {
    std::atomic<bool> ready{};
    std::array<std::atomic<StepObserver>, max_step_observers> observers{};
};
State& state() { static auto* value = new State; return *value; }

// A hook must not leave the game a different last error.
struct LastError {
    DWORD value = GetLastError();
    ~LastError() { SetLastError(value); }
};

// The contact struct the rig's contact processing fills each physics step.
bool contact_struct(const LocalSkater& skater, std::uintptr_t& contacts) noexcept {
    std::uintptr_t holder{};
    return available() && memory::peek(skater.rig + build::contact_holder_offset, holder) &&
        memory::peek(holder + build::contact_struct_offset, contacts);
}
using Records = std::array<unsigned char, build::body_bone_count * build::bone_record_size>;
template <class T> T field(const Records& records, std::size_t body, std::uintptr_t offset) noexcept {
    T value{};
    std::memcpy(&value, records.data() + body * build::bone_record_size + offset, sizeof(T));
    return value;
}
Vec3 vec3(const std::array<float, 4>& value) noexcept {
    for (std::size_t i = 0; i < 3; ++i)
        if (!std::isfinite(value[i])) return {};
    return {value[0], value[1], value[2]};
}
float speed(float value) noexcept { return std::isfinite(value) && value > 0 && value < 1000 ? value : 0; }
}

bool start(std::uintptr_t base) noexcept {
    auto& s = state();
    if (s.ready.load(std::memory_order_acquire)) return true;
    for (const auto& contract : build::contracts) {
        std::array<unsigned char, 32> actual{};
        if (!memory::peek(base + contract.rva, actual) || actual != contract.bytes) {
            logging::log(logging::Level::warning, logging::Channel::skater,
                "Skater body is unavailable: the native contract at 0x{:x} did not match.", contract.rva);
            return false;
        }
    }
    if (!no_bail_available()) {
        logging::write(logging::Level::warning, logging::Channel::skater,
            "Skater body is unavailable: its physics steps come through No Bail's hooks, which did not start.");
        return false;
    }
    s.ready.store(true, std::memory_order_release);
    return true;
}

bool available() noexcept { return state().ready.load(std::memory_order_acquire); }

bool add_step_observer(StepObserver observer) noexcept {
    auto& s = state();
    if (!observer) return false;
    for (auto& slot : s.observers)
        if (slot.load(std::memory_order_acquire) == observer) return false;
    for (auto& slot : s.observers) {
        StepObserver empty{};
        if (slot.compare_exchange_strong(empty, observer, std::memory_order_acq_rel)) return true;
    }
    return false;
}

void on_physics_step(std::uintptr_t rig, float seconds, bool wipeout) noexcept {
    auto& s = state();
    if (!s.ready.load(std::memory_order_acquire) ||
        std::none_of(s.observers.begin(), s.observers.end(), [](const auto& slot) { return slot.load(std::memory_order_acquire); }))
        return;
    LastError error;
    Step step{{}, seconds, wipeout};
    if (!local_skater_owns(rig, &LocalSkater::rig, &step.skater)) return;
    for (const auto& slot : s.observers)
        if (const auto observer = slot.load(std::memory_order_acquire)) observer(step);
}

std::uintptr_t step_length_address() noexcept {
    LocalSkater skater;
    std::uintptr_t input{};
    if (!current_local_skater(skater) || !memory::peek(skater.core + build::core_step_input_offset, input) || !input) return 0;
    return input + build::step_input_length_offset;
}

bool read_contacts(const LocalSkater& skater, Contacts& result) noexcept {
    using namespace build;
    result = {};
    std::uintptr_t contacts{}, bodies{};
    Records records{};
    std::array<float, count> since{};
    std::array<std::uint8_t, count> touching{}, sensitive{};
    std::array<std::uint8_t, any_contact_offset - other_contact_offset + 1> flags{};
    std::array<std::array<float, 4>, count> velocities{};
    if (!contact_struct(skater, contacts) || !memory::peek(contacts + bone_records_offset, records) ||
        !memory::peek(contacts + bone_since_contact_offset, since) || !memory::peek(contacts + bone_touching_offset, touching) ||
        !memory::peek(contacts + sensitive_bones_offset, sensitive) || !memory::peek(contacts + other_contact_offset, flags) ||
        !memory::peek(contacts + body_state_offset, bodies) || !memory::peek(bodies + bone_velocities_offset, velocities))
        return false;
    const auto flag = [&](std::uintptr_t offset) { return flags[offset - other_contact_offset] != 0; };
    result.any = flag(any_contact_offset);
    result.sensitive = flag(sensitive_contact_offset);
    result.other = flag(other_contact_offset);
    result.feet_on_board = flag(feet_on_board_offset);
    for (std::size_t body = 0; body < count; ++body) {
        auto& b = result.bodies[body];
        b.touching = touching[body] != 0;
        b.since_contact = std::isfinite(since[body]) ? std::max(0.0f, -since[body]) : 0;
        b.sensitive = sensitive[body] != 0;
        b.velocity = vec3(velocities[body]);
        const float ordinary = speed(field<float>(records, body, bone_peak_offset));
        const float tracked = speed(field<float>(records, body, bone_tracked_peak_offset));
        b.tracked_point = tracked > ordinary;
        b.impact = std::max(ordinary, tracked);
        const std::uintptr_t side = b.tracked_point ? bone_tracked_offset : 0;
        b.normal = vec3(field<std::array<float, 4>>(records, body, bone_normal_offset + side));
        b.slide = vec3(field<std::array<float, 4>>(records, body, bone_slide_offset + side));
        b.point = vec3(field<std::array<float, 4>>(records, body, bone_point_offset));
        const auto hit = [&](std::uintptr_t offset) { return field<std::uint8_t>(records, body, offset) != 0; };
        b.hit = {hit(bone_hit_board_offset), hit(bone_hit_vehicle_offset), hit(bone_hit_world_offset),
            hit(bone_hit_kind_5_offset), hit(bone_hit_kind_11_offset)};
    }
    return true;
}

}
