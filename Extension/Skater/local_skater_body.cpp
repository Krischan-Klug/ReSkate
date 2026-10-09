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
// Where the local skater's physics step length is kept now (a float, seconds of game time), 0 while
// there is no local skater.
std::uintptr_t step_length_address() noexcept {
    LocalSkater skater;
    std::uintptr_t input{};
    if (!current_local_skater(skater) || !memory::peek(skater.core + build::core_step_input_offset, input) || !input) return 0;
    return input + build::step_input_length_offset;
}
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

bool set_step_length(float seconds) noexcept {
    const auto address = step_length_address();
    float current{};
    if (!address || !memory::peek(address, current)) return false;
    if (current == seconds) return true;
    __try {
        *reinterpret_cast<volatile float*>(address) = seconds; // read by the physics thread each step
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool read_contacts(const LocalSkater& skater, Contacts& result) noexcept {
    using namespace build;
    result = {};
    std::uintptr_t contacts{};
    Records records{};
    std::array<std::uint8_t, count> touching{};
    if (!contact_struct(skater, contacts) || !memory::peek(contacts + bone_records_offset, records) ||
        !memory::peek(contacts + bone_touching_offset, touching))
        return false;
    for (std::size_t body = 0; body < count; ++body) {
        auto& b = result.bodies[body];
        b.touching = touching[body] != 0;
        const float ordinary = speed(field<float>(records, body, bone_peak_offset));
        const float tracked = speed(field<float>(records, body, bone_tracked_peak_offset));
        b.impact = std::max(ordinary, tracked);
        // The slide of the contact that set the peak: one near a tracked point keeps its own.
        const std::uintptr_t side = tracked > ordinary ? bone_tracked_offset : 0;
        b.slide = vec3(field<std::array<float, 4>>(records, body, bone_slide_offset + side));
        const auto hit = [&](std::uintptr_t offset) { return field<std::uint8_t>(records, body, offset) != 0; };
        b.hit = {hit(bone_hit_board_offset), hit(bone_hit_vehicle_offset), hit(bone_hit_world_offset),
            hit(bone_hit_kind_5_offset), hit(bone_hit_kind_11_offset)};
    }
    return true;
}

}
