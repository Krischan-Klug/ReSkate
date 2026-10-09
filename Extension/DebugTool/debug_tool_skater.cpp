#include "debug_tool_internal.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/physics_tuning.h"
#include "Engine/Game/Build/20260929/skater_step.h"
#include "Extension/Skater/no_bail.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <vector>

// The skater's probes, all fed by one physics step of the local skater (re/controller.md "Ein Schritt"):
// before the state tick (core_state_pre_physics_tick) and after the step (core_physics_step).
namespace dingosdk::debug_tool {
namespace {
namespace step = addr::skater_step;
namespace tuning = addr::physics_tuning;

using Step = void(__fastcall*)(std::uintptr_t core);
std::uintptr_t image_base;
Step pre_tick_original, physics_step_original;
std::atomic<bool> hooked;

std::uintptr_t pointer_at(std::uintptr_t address) {
    std::uintptr_t value{};
    return memory::peek(address, value) ? value : 0;
}
float float_at(std::uintptr_t address) {
    float value{};
    return memory::peek(address, value) ? value : 0.0f;
}
struct Vec { float x, y, z, w; };
Vec vec_at(std::uintptr_t address) {
    Vec value{};
    (void)memory::peek(address, value);
    return value;
}

// The local skater's core as No Bail names it, looked up again a few times a second.
std::atomic<std::uintptr_t> local_core_cached;
std::atomic<ULONGLONG> local_core_at;
std::uintptr_t local_core() {
    const auto tick = GetTickCount64();
    if (tick - local_core_at.load(std::memory_order_relaxed) >= 250) {
        NoBailSkater skater;
        local_core_cached.store(no_bail_skater(skater) ? skater.core : 0, std::memory_order_relaxed);
        local_core_at.store(tick, std::memory_order_relaxed);
    }
    return local_core_cached.load(std::memory_order_relaxed);
}

std::uintptr_t call_slot(std::uintptr_t object, std::uintptr_t slot) {
    using Get = std::uintptr_t(__fastcall*)(std::uintptr_t);
    const auto vtable = pointer_at(object);
    const auto function = vtable ? pointer_at(vtable + slot) : 0;
    if (!function) return 0;
    __try {
        return reinterpret_cast<Get>(function)(object);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
int state_id(std::uintptr_t state) { return state ? static_cast<int>(call_slot(state, step::state_id_slot)) : -1; }
std::string state_name(std::uintptr_t state) {
    char name[64]{};
    const auto text = state ? call_slot(state, step::state_name_slot) : 0;
    if (!text || memory::peek_cstring(text, name, sizeof name) < 0) return "?";
    std::string_view view(name);
    if (view.starts_with("PHYSICS_STATE_")) view.remove_prefix(14);
    return std::string(view);
}

// skater.state: every change of the physics state, with how long the previous one lasted (simulated time).
struct StateWatch {
    int id = -2;
    std::string name;
    double seconds{};
    std::atomic<std::uint64_t> changes{};
} state_watch;

// skater.motion: position, velocity and speed from the ctx, ten times per simulated second.
struct MotionWatch {
    double since{};
    std::atomic<float> speed{};
} motion_watch;

// skater.step: the step recorder (format: re/scripts/step_recording.py).
struct Recording {
    std::mutex lock; // the two ends of a step may run on different threads
    FILE* file{};
    std::filesystem::path path;
    std::uint64_t started{}, frequency{};
    std::atomic<std::uint64_t> bytes{}, steps{};
    bool tuning_written{};
} recording;
constexpr double recording_seconds = 60;
constexpr std::uint64_t recording_bytes = 400ull << 20;
constexpr std::uint32_t state_bytes = 0x1000; // the largest state object fits; past its end is other heap

void switch_state(bool on);
void switch_recording(bool on);
std::string state_status();
std::string motion_status();
std::string recording_status();

std::array<Probe, 3> probes{{
    {"skater.state", "skater", "State changes",
     "Each change of the local skater's physics state (ROLL_IN, PHYSICS_AIR, ...) and how long the last one lasted.",
     {}, [] { return hooked.load(); }, &switch_state, &state_status},
    {"skater.motion", "skater", "Motion",
     "Position, velocity and speed from the skater's ctx, ten times per second.",
     {}, [] { return hooked.load(); }, nullptr, &motion_status},
    {"skater.step", "skater", "Step recording",
     "Every physics step: core, ctx, rig, state, controller and outputs before and after, to logs/step-*.rsrec (60 s max).",
     {}, [] { return hooked.load(); }, &switch_recording, &recording_status},
}};
Probe& state_probe = probes[0];
Probe& motion_probe = probes[1];
Probe& step_probe = probes[2];

void switch_state(bool on) {
    if (on) state_watch.id = -2;
}
std::string state_status() {
    return state_watch.changes.load() ? std::format("{} changes", state_watch.changes.load()) : "waiting for a change";
}
std::string motion_status() { return std::format("{:.1f} m/s", motion_watch.speed.load()); }

std::uint32_t tag(const char (&name)[5]) {
    return std::uint32_t(name[0]) | std::uint32_t(name[1]) << 8 | std::uint32_t(name[2]) << 16 | std::uint32_t(name[3]) << 24;
}
std::uint64_t now() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return std::uint64_t(value.QuadPart);
}
struct Section {
    std::uint32_t tag;
    std::uintptr_t address;
    std::uint32_t size;
};
template<class T> void put(std::vector<unsigned char>& out, const T& value) {
    const auto at = out.size();
    out.resize(at + sizeof value);
    std::memcpy(out.data() + at, &value, sizeof value);
}
// Record: 'RECD', kind (1 before the state tick, 2 after the step, 3 tuning), QPC time, core, state id, section
// count; per section its tag, size (0 = unreadable) and bytes.
void write_record(std::uint32_t kind, std::uintptr_t core, int state, const Section* sections, std::size_t count) {
    thread_local std::vector<unsigned char> out;
    out.clear();
    put(out, tag("RECD"));
    put(out, kind);
    put(out, now());
    put(out, std::uint64_t(core));
    put(out, std::int32_t(state));
    put(out, std::uint32_t(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto at = out.size();
        put(out, sections[i].tag);
        put(out, sections[i].size);
        out.resize(at + 8 + sections[i].size);
        if (!memory::peek_bytes(sections[i].address, out.data() + at + 8, sections[i].size)) {
            out.resize(at + 8);
            std::memset(out.data() + at + 4, 0, 4);
        }
    }
    std::lock_guard guard(recording.lock);
    if (!recording.file) return;
    std::fwrite(out.data(), 1, out.size(), recording.file);
    recording.bytes += out.size();
    if (kind == 2) ++recording.steps;
}

void switch_recording(bool on) {
    auto& r = recording;
    if (on) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char name[64];
        std::snprintf(name, sizeof name, "step-%04u%02u%02u-%02u%02u%02u.rsrec", t.wYear, t.wMonth, t.wDay,
            t.wHour, t.wMinute, t.wSecond);
        std::lock_guard guard(r.lock);
        r.path = logging::status().directory / name;
        if (_wfopen_s(&r.file, r.path.c_str(), L"wb") != 0 || !r.file) {
            r.file = nullptr;
            report(step_probe, "cannot write {}", r.path.string());
            return;
        }
        std::setvbuf(r.file, nullptr, _IOFBF, 8 << 20);
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        r.frequency = std::uint64_t(frequency.QuadPart);
        const std::uint32_t version = 1, reserved = 0;
        const std::uint64_t base = image_base;
        std::fwrite("RSKSTEP1", 1, 8, r.file);
        std::fwrite(&version, 4, 1, r.file);
        std::fwrite(&reserved, 4, 1, r.file);
        std::fwrite(&base, 8, 1, r.file);
        std::fwrite(&r.frequency, 8, 1, r.file);
        r.bytes = 32;
        r.steps = 0;
        r.tuning_written = false;
        r.started = now();
        report(step_probe, "recording to {}", r.path.string());
        return;
    }
    std::lock_guard guard(r.lock);
    if (!r.file) return;
    std::fclose(r.file);
    r.file = nullptr;
    report(step_probe, "saved {} steps, {:.1f} MB: {}", r.steps.load(), double(r.bytes.load()) / (1 << 20), r.path.string());
}
std::string recording_status() {
    if (!recording.frequency) return {};
    return std::format("{:.0f} s, {} steps, {:.0f} MB", double(now() - recording.started) / double(recording.frequency),
        recording.steps.load(), double(recording.bytes.load()) / (1 << 20));
}

void record_step(std::uint32_t kind, std::uintptr_t core, std::uintptr_t state, int id, std::uintptr_t ctx) {
    auto& r = recording;
    if (r.frequency && (double(now() - r.started) / double(r.frequency) > recording_seconds || r.bytes > recording_bytes)) {
        step_probe.enabled.store(false);
        switch_recording(false);
        return;
    }
    bool tuning_due;
    {
        std::lock_guard guard(r.lock);
        tuning_due = !r.tuning_written;
        r.tuning_written = true;
    }
    if (tuning_due) {
        const Section once[]{
            {tag("TUNA"), pointer_at(image_base + tuning::asset_global), tuning::asset_size},
            {tag("TUNB"), pointer_at(core + tuning::core_tuning_block), 0xc98},
        };
        write_record(3, core, -1, once, std::size(once));
    }
    if (kind == 1) {
        const Section before[]{{tag("CTX_"), ctx, step::ctx_size}, {tag("STAT"), state, state_bytes}};
        write_record(kind, core, id, before, std::size(before));
        return;
    }
    const Section after[]{
        {tag("CORE"), core, step::core_size},
        {tag("CTX_"), ctx, step::ctx_size},
        {tag("RIG_"), pointer_at(core + step::core_rig_offset), step::rig_size},
        {tag("STAT"), state, state_bytes},
        {tag("SURF"), pointer_at(core + step::core_surface_offset), step::surface_size},
        {tag("TRAJ"), pointer_at(core + step::core_trajectory_offset), step::trajectory_size},
        {tag("CTRL"), pointer_at(core + step::core_controller_offset), step::controller_size},
    };
    write_record(kind, core, id, after, std::size(after));
}

void watch_state(std::uintptr_t state, int id, float dt) {
    auto& w = state_watch;
    w.seconds += dt;
    if (id == w.id) return;
    auto name = state_name(state);
    if (w.id == -2) report(state_probe, "{} ({})", name, id);
    else report(state_probe, "{} ({}) after {:.2f} s in {}", name, id, w.seconds, w.name);
    if (w.id != -2) ++w.changes;
    w.id = id;
    w.name = std::move(name);
    w.seconds = 0;
}

void watch_motion(std::uintptr_t ctx, std::uintptr_t state, float dt) {
    auto& w = motion_watch;
    w.since += dt;
    if (w.since < 0.1) return;
    w.since = 0;
    const auto p = vec_at(ctx + step::ctx_position_offset);
    const auto v = vec_at(ctx + step::ctx_velocity_offset);
    const float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    w.speed.store(speed);
    report(motion_probe, "pos {:.2f} {:.2f} {:.2f}  vel {:.2f} {:.2f} {:.2f}  {:.2f} m/s  {}", p.x, p.y, p.z, v.x, v.y, v.z,
        speed, state_name(state));
}

void observe(std::uint32_t kind, std::uintptr_t core) {
    const bool states = state_probe.enabled.load(std::memory_order_relaxed);
    const bool motion = motion_probe.enabled.load(std::memory_order_relaxed);
    const bool steps = step_probe.enabled.load(std::memory_order_relaxed);
    if (!(states || motion || steps) || core != local_core()) return;
    try {
        const auto state = pointer_at(core + step::core_state_offset);
        const auto ctx = pointer_at(core + step::core_ctx_offset);
        const int id = state_id(state);
        if (kind == 1) {
            const float dt = ctx ? float_at(ctx + step::ctx_step_seconds_offset) : 0.0f;
            if (states) watch_state(state, id, dt);
            if (motion && ctx) watch_motion(ctx, state, dt);
        }
        if (steps) record_step(kind, core, state, id, ctx);
    } catch (...) {}
}

void __fastcall pre_tick_hook(std::uintptr_t core) {
    observe(1, core);
    pre_tick_original(core);
}
void __fastcall physics_step_hook(std::uintptr_t core) {
    physics_step_original(core);
    observe(2, core);
}

bool fingerprint(std::uintptr_t address, const game::build::Fingerprint& expected) {
    std::array<unsigned char, 32> actual{};
    return memory::read_bytes(address, actual.data(), actual.size()) && actual == expected.bytes;
}
}

std::span<Probe> skater_probes() { return probes; }

bool start_skater_probes(std::uintptr_t base) noexcept {
    if (hooked.load()) return true;
    image_base = base;
    const auto pre = base + step::pre_tick_contract.rva;
    const auto after = base + step::physics_step_contract.rva;
    if (!fingerprint(pre, step::pre_tick_contract) || !fingerprint(after, step::physics_step_contract)) {
        logging::write(logging::Level::warning, logging::Channel::diagnostics,
            "Debug tool: the skater step's code differs from this build's; skater probes off.");
        return false;
    }
    void* pre_original{};
    void* after_original{};
    if (hook_prepare(reinterpret_cast<void*>(pre), reinterpret_cast<void*>(&pre_tick_hook), &pre_original) != HookOk)
        return false;
    if (hook_prepare(reinterpret_cast<void*>(after), reinterpret_cast<void*>(&physics_step_hook), &after_original) != HookOk) {
        hook_remove(reinterpret_cast<void*>(pre));
        return false;
    }
    pre_tick_original = reinterpret_cast<Step>(pre_original);
    physics_step_original = reinterpret_cast<Step>(after_original);
    if (hook_enable(reinterpret_cast<void*>(pre)) != HookOk) return false;
    if (hook_enable(reinterpret_cast<void*>(after)) != HookOk) {
        hook_disable(reinterpret_cast<void*>(pre));
        return false;
    }
    hooked.store(true);
    return true;
}
}
