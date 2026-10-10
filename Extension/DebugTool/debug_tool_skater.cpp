#include "debug_tool_internal.h"
#include "debug_tool.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/physics_tuning.h"
#include "Engine/Game/Build/20260929/skater_step.h"
#include "Extension/Skater/no_bail.h"
#include <Windows.h>
#include <algorithm>
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
Step pre_tick_original, physics_step_original, advance_state_original;
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
    std::atomic<std::uint64_t> bytes{}, steps{}, total_bytes{};
    std::atomic<int> part{};
    std::string stem;
    bool tuning_written{};
} recording;
// No time limit: the recording runs until switched off; past this size it continues in the next part file.
constexpr std::uint64_t recording_part_bytes = 400ull << 20;
constexpr std::uint32_t state_bytes = 0x1000; // the largest state object fits; past its end is other heap

void switch_state(bool on);
void switch_recording(bool on);
std::string state_status();
std::string motion_status();
std::string recording_status();

std::array<Probe, 3> probe_list{{
    {"skater.state", "skater", "State changes",
     "Each change of the local skater's physics state (ROLL_IN, PHYSICS_AIR, ...) and how long the last one lasted.",
     {}, [] { return hooked.load(); }, &switch_state, &state_status},
    {"skater.motion", "skater", "Motion",
     "Position, velocity and speed from the skater's ctx, ten times per second.",
     {}, [] { return hooked.load(); }, nullptr, &motion_status},
    {"skater.step", "skater", "Step recording",
     "Every physics step: core, ctx, rig, state, controller and outputs before and after, and what the state choice reads, to logs/step-*.rsrec until stopped (F9 or the DEBUG window; parts of 400 MB).",
     {}, [] { return hooked.load(); }, &switch_recording, &recording_status},
}};
Probe& state_probe = probe_list[0];
Probe& motion_probe = probe_list[1];
Probe& step_probe = probe_list[2];

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
// PAD_: the first connected XInput pad as the OS reports it (index, packet number, XINPUT_GAMEPAD: buttons, LT, RT,
// LX, LY, RX, RY), so pad axes and buttons can be matched against the ctx inputs. Loaded at first use.
struct PadSample {
    std::uint32_t index = 0, packet = 0;
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0, right_trigger = 0;
    std::int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
};
bool read_pad(PadSample& sample) {
    struct State { DWORD packet; std::uint16_t buttons; std::uint8_t lt, rt; std::int16_t lx, ly, rx, ry; };
    using Get = DWORD(WINAPI*)(DWORD, State*);
    static const Get get = [] {
        HMODULE module = LoadLibraryW(L"xinput1_4.dll");
        if (!module) module = LoadLibraryW(L"xinput9_1_0.dll");
        return module ? reinterpret_cast<Get>(GetProcAddress(module, "XInputGetState")) : nullptr;
    }();
    if (!get) return false;
    for (DWORD i = 0; i < 4; ++i) {
        State state{};
        if (get(i, &state) != ERROR_SUCCESS) continue;
        sample = {i, state.packet, state.buttons, state.lt, state.rt, state.lx, state.ly, state.rx, state.ry};
        return true;
    }
    return false;
}
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

// Opens the current part (caller holds the lock). Part 1 is step-<time>.rsrec, later parts step-<time>-pN.rsrec;
// every part is a complete recording with its own header and tuning.
bool open_part(Recording& r) {
    const auto part = r.part.load();
    const auto name = part <= 1 ? r.stem + ".rsrec" : std::format("{}-p{}.rsrec", r.stem, part);
    r.path = logging::status().directory / name;
    if (_wfopen_s(&r.file, r.path.c_str(), L"wb") != 0 || !r.file) {
        r.file = nullptr;
        report(step_probe, "cannot write {}", r.path.string());
        return false;
    }
    std::setvbuf(r.file, nullptr, _IOFBF, 8 << 20);
    const std::uint32_t version = 1, reserved = 0;
    const std::uint64_t base = image_base;
    std::fwrite("RSKSTEP1", 1, 8, r.file);
    std::fwrite(&version, 4, 1, r.file);
    std::fwrite(&reserved, 4, 1, r.file);
    std::fwrite(&base, 8, 1, r.file);
    std::fwrite(&r.frequency, 8, 1, r.file);
    r.bytes = 32;
    r.tuning_written = false;
    return true;
}

void close_part(Recording& r) {
    if (!r.file) return;
    std::fclose(r.file);
    r.file = nullptr;
    r.total_bytes += r.bytes.load();
    report(step_probe, "saved part {}, {:.1f} MB: {}", r.part.load(), double(r.bytes.load()) / (1 << 20), r.path.string());
}

void switch_recording(bool on) {
    auto& r = recording;
    std::lock_guard guard(r.lock);
    if (on) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        r.stem = std::format("step-{:04}{:02}{:02}-{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        r.frequency = std::uint64_t(frequency.QuadPart);
        r.part = 1;
        r.steps = 0;
        r.total_bytes = 0;
        if (!open_part(r)) return;
        r.started = now();
        report(step_probe, "recording to {} (until stopped)", r.path.string());
        return;
    }
    if (!r.file) return;
    close_part(r);
    report(step_probe, "recording stopped: {} steps, {} part(s), {:.1f} MB", r.steps.load(), r.part.load(),
        double(r.total_bytes.load()) / (1 << 20));
}
std::string recording_status() {
    if (!recording.frequency) return {};
    return std::format("{:.0f} s, {} steps, {:.0f} MB, part {}", double(now() - recording.started) / double(recording.frequency),
        recording.steps.load(), double(recording.total_bytes.load() + recording.bytes.load()) / (1 << 20), recording.part.load());
}

// A 2D curve flattened for the recording: u32 count, then per inner curve f32 outer key, u32 points, points (0x1c B each).
std::vector<unsigned char> curve2d_bytes(std::uintptr_t asset) {
    std::vector<unsigned char> out;
    std::uint32_t count = 0;
    const auto records = asset ? pointer_at((asset & ~std::uintptr_t(4)) + step::curve2d_records) : 0;
    if (records && memory::peek(records - 4, count)) count &= 0x7fffffff;
    if (count > 64) count = 0;
    put(out, count);
    for (std::uint32_t i = 0; i < count; ++i) {
        float key = 0;
        memory::peek(records + i * 0x10 + 8, key);
        const auto curve = pointer_at(records + i * 0x10) & ~std::uintptr_t(4);
        const auto points = curve ? pointer_at(curve + step::float_curve_points) : 0;
        std::uint32_t n = 0;
        if (points && memory::peek(points - 4, n)) n &= 0x7fffffff;
        if (n > 256) n = 0;
        put(out, key);
        const auto at = out.size();
        put(out, n);
        out.resize(at + 4 + n * 0x1c);
        if (n && !memory::peek_bytes(points, out.data() + at + 4, n * 0x1c)) {
            out.resize(at + 4);
            std::memset(out.data() + at, 0, 4);
        }
    }
    return out;
}

// A segment list flattened for the recording: u32 count, then count segments of segment_bytes each.
std::vector<unsigned char> segments_bytes(std::uintptr_t array) {
    std::vector<unsigned char> out;
    std::uint32_t count = 0;
    if (array && memory::peek(array - 4, count)) count &= 0x7fffffff;
    if (count > step::segments_max) count = 0;
    put(out, count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto segment = pointer_at(array + i * 8) & ~std::uintptr_t(4);
        const auto at = out.size();
        out.resize(at + step::segment_bytes);
        if (!segment || !memory::peek_bytes(segment, out.data() + at, step::segment_bytes))
            std::memset(out.data() + at, 0, step::segment_bytes);
    }
    return out;
}

// The torque queue flattened for the recording: u32 count, then the 0x40-byte nodes in queue order.
std::vector<unsigned char> torque_queue_bytes(std::uintptr_t bodies) {
    std::vector<unsigned char> out;
    put(out, std::uint32_t(0));
    if (!bodies) return out;
    const auto sentinel = bodies + step::body_torque_queue;
    std::uint32_t count = 0;
    for (auto node = pointer_at(sentinel); node && node != sentinel && count < step::torque_nodes_max; node = pointer_at(node)) {
        const auto at = out.size();
        out.resize(at + step::torque_node_bytes);
        if (!memory::peek_bytes(node, out.data() + at, step::torque_node_bytes)) {
            out.resize(at);
            break;
        }
        ++count;
    }
    std::memcpy(out.data(), &count, 4);
    return out;
}

void record_step(std::uint32_t kind, std::uintptr_t core, std::uintptr_t state, int id, std::uintptr_t ctx) {
    auto& r = recording;
    if (kind == 1 && r.bytes > recording_part_bytes) {
        // Continue in the next part at a tick boundary, so a tick's records stay in one file.
        std::lock_guard guard(r.lock);
        if (r.file) {
            close_part(r);
            ++r.part;
            if (!open_part(r)) {
                step_probe.enabled.store(false);
                return;
            }
        }
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
            {tag("MATH"), image_base + step::math_constants, step::math_constants_bytes},
        };
        const auto block = pointer_at(core + tuning::core_tuning_block);
        const auto jump = curve2d_bytes(block ? pointer_at(block + step::block_jump_curve2d) : 0);
        const auto asset = pointer_at(image_base + tuning::asset_global);
        const auto slide = curve2d_bytes(asset ? pointer_at(asset + step::asset_slide_curve2d) : 0);
        const Section curves[]{
            {tag("C2DJ"), std::uintptr_t(jump.data()), std::uint32_t(jump.size())},
            {tag("C2DS"), std::uintptr_t(slide.data()), std::uint32_t(slide.size())},
        };
        write_record(3, core, -1, once, std::size(once));
        write_record(3, core, -1, curves, std::size(curves));
    }
    if (kind == 1 || kind == 6) {
        // Around the state tick (1 before, 6 after): what the tick reads and writes, to test a state on its own.
        // POSE is the state's pose helper, PREC its response records (what the tick publishes), BODY the body list
        // the tick writes velocities to.
        const auto pose = state ? pointer_at(state + step::state_pose_offset) : 0;
        const auto provider = pose ? pointer_at(pose + step::pose_provider_offset) : 0;
        const auto bodies = provider ? pointer_at(provider + step::provider_body_list_offset) : 0;
        std::uintptr_t records = 0;
        std::uint32_t record_bytes = 0;
        if (pose) {
            records = pointer_at(pose + step::pose_records_offset);
            const auto end = pointer_at(pose + step::pose_records_offset + 8);
            if (records && end > records)
                record_bytes = std::uint32_t(std::min<std::uintptr_t>(end - records,
                    std::uintptr_t(step::pose_record_size) * step::pose_records_max));
        }
        const auto past = segments_bytes(ctx ? pointer_at(ctx + step::ctx_segments_past) : 0);
        const auto future = segments_bytes(ctx ? pointer_at(ctx + step::ctx_segments_future) : 0);
        const auto torques = torque_queue_bytes(bodies);
        PadSample pad;
        const bool has_pad = kind == 1 && read_pad(pad);
        alignas(16) float center_of_mass[4]{};
        if (pose) {
            const auto vtable = pointer_at(pose);
            const auto get = vtable ? pointer_at(vtable + step::pose_get_center_of_mass_slot) : 0;
            if (get)
                reinterpret_cast<float* (*)(std::uintptr_t, float*)>(get)(pose, center_of_mass);
        }
        const Section tick[]{
            {tag("CTX_"), ctx, step::ctx_size},
            {tag("STAT"), state, state_bytes},
            {tag("RIG_"), pointer_at(core + step::core_rig_offset), step::rig_size},
            {tag("CTRL"), pointer_at(core + step::core_controller_offset), step::controller_size},
            {tag("SURF"), pointer_at(core + step::core_surface_offset), step::surface_size},
            {tag("TRAJ"), pointer_at(core + step::core_trajectory_offset), step::trajectory_size},
            {tag("POSE"), pose, pose ? step::pose_bytes : 0},
            {tag("PREC"), records, record_bytes},
            {tag("BODY"), bodies, bodies ? step::body_list_bytes : 0},
            {tag("I440"), ctx ? pointer_at(ctx + step::ctx_defaults_offset) : 0, step::bound_instance_bytes},
            {tag("I448"), ctx ? pointer_at(ctx + step::ctx_runtime_tuning_offset) : 0, step::bound_instance_bytes},
            {tag("I468"), ctx ? pointer_at(ctx + step::ctx_instance_1468_offset) : 0, step::bound_instance_bytes},
            {tag("I498"), ctx ? pointer_at(ctx + step::ctx_instance_1498_offset) : 0, step::bound_instance_bytes},
            {tag("I478"), ctx ? pointer_at(ctx + step::ctx_instance_1478_offset) : 0, step::bound_instance_bytes},
            {tag("PUMP"), id == 100 && state ? pointer_at(state + step::ground_pumping_offset) : 0, step::pumping_bytes},
            {tag("SEGP"), std::uintptr_t(past.data()), std::uint32_t(past.size())},
            {tag("SEGF"), std::uintptr_t(future.data()), std::uint32_t(future.size())},
            {tag("TORQ"), std::uintptr_t(torques.data()), std::uint32_t(torques.size())},
            {tag("WRC_"), provider ? provider + step::provider_wallride_cache : 0, provider ? step::wallride_cache_bytes : 0},
            {tag("COM_"), pose ? std::uintptr_t(center_of_mass) : 0, pose ? std::uint32_t(sizeof center_of_mass) : 0},
            {tag("PAD_"), has_pad ? std::uintptr_t(&pad) : 0, has_pad ? std::uint32_t(sizeof pad) : 0},
            // The prediction is large; only before the tick and only in the take-off and air states.
            {tag("TRJP"), kind == 1 && (id == 103 || id == 200 || id == 201) ? pointer_at(core + step::core_prediction_offset) : 0,
                step::prediction_size},
        };
        write_record(kind, core, id, tick, std::size(tick));
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

// skater.step, around the state choice (kind 4 before, kind 5 after): everything the chooser reads
// (re/controller/selection/state-selection.md), so the C# rebuild can make the same choice from the same inputs.
void record_choice(std::uint32_t kind, std::uintptr_t core) {
    const auto state = pointer_at(core + step::core_state_offset);
    const auto chooser = pointer_at(core + step::core_chooser_offset);
    if (kind == 5) {
        const Section after[]{{tag("CHSR"), chooser, step::chooser_bytes}};
        write_record(kind, core, state_id(state), after, std::size(after));
        return;
    }
    const auto ctx = pointer_at(core + step::core_ctx_offset);
    const auto bundle = chooser ? pointer_at(chooser) : 0;
    const auto chooser_10 = chooser ? pointer_at(chooser + 0x10) : 0;
    const Section before[]{
        {tag("CTX_"), ctx, step::ctx_size},
        {tag("RIG_"), pointer_at(core + step::core_rig_offset), step::rig_size},
        {tag("CHSR"), chooser, step::chooser_bytes},
        {tag("SBUN"), bundle, step::bundle_bytes},
        {tag("SB10"), bundle ? pointer_at(bundle + 0x10) : 0, step::bundle_10_bytes},
        {tag("SBA_"), bundle ? pointer_at(bundle + 0x20) : 0, step::bundle_state_bytes},
        {tag("SB30"), bundle ? pointer_at(bundle + 0x30) : 0, step::bundle_30_bytes},
        {tag("SB38"), bundle ? pointer_at(bundle + 0x38) : 0, step::bundle_38_bytes},
        {tag("CH18"), chooser_10 ? pointer_at(chooser_10 + 0x18) : 0, step::chooser_owner_bytes},
        {tag("INSG"), ctx ? pointer_at(ctx + step::ctx_instance_g_offset) : 0, step::instance_g_bytes},
        {tag("INSS"), ctx ? pointer_at(ctx + step::ctx_instance_s_offset) : 0, step::instance_s_bytes},
        {tag("INSK"), ctx ? pointer_at(ctx + step::ctx_instance_k_offset) : 0, step::instance_k_bytes},
    };
    write_record(kind, core, state_id(state), before, std::size(before));
}

void __fastcall pre_tick_hook(std::uintptr_t core) {
    observe(1, core);
    pre_tick_original(core);
    if (step_probe.enabled.load(std::memory_order_relaxed) && core == local_core()) try {
        const auto state = pointer_at(core + step::core_state_offset);
        record_step(6, core, state, state_id(state), pointer_at(core + step::core_ctx_offset));
    } catch (...) {}
}
void __fastcall physics_step_hook(std::uintptr_t core) {
    physics_step_original(core);
    observe(2, core);
}
void __fastcall advance_state_hook(std::uintptr_t core) {
    const bool recording_choice = step_probe.enabled.load(std::memory_order_relaxed) && core == local_core();
    if (recording_choice) try { record_choice(4, core); } catch (...) {}
    advance_state_original(core);
    if (recording_choice) try { record_choice(5, core); } catch (...) {}
}

bool fingerprint(std::uintptr_t address, const game::build::Fingerprint& expected) {
    std::array<unsigned char, 32> actual{};
    return memory::read_bytes(address, actual.data(), actual.size()) && actual == expected.bytes;
}
}

std::span<Probe> skater_probes() { return probe_list; }

RecordingInfo recording_info() noexcept {
    RecordingInfo info;
    info.active = step_probe.enabled.load() && recording.file != nullptr;
    if (!info.active || !recording.frequency) return info;
    info.seconds = double(now() - recording.started) / double(recording.frequency);
    info.megabytes = double(recording.total_bytes.load() + recording.bytes.load()) / (1 << 20);
    info.steps = recording.steps.load();
    info.part = recording.part.load();
    return info;
}

bool recording_available() noexcept { return hooked.load(); }

void toggle_recording() noexcept { (void)set_enabled(recording_probe, !step_probe.enabled.load()); }

bool start_skater_probes(std::uintptr_t base) noexcept {
    if (hooked.load()) return true;
    image_base = base;
    struct Hook { const game::build::Fingerprint* contract; void* detour; Step* original; };
    const Hook hooks[]{
        {&step::pre_tick_contract, reinterpret_cast<void*>(&pre_tick_hook), &pre_tick_original},
        {&step::physics_step_contract, reinterpret_cast<void*>(&physics_step_hook), &physics_step_original},
        {&step::advance_state_contract, reinterpret_cast<void*>(&advance_state_hook), &advance_state_original},
    };
    for (const auto& hook : hooks)
        if (!fingerprint(base + hook.contract->rva, *hook.contract)) {
            logging::write(logging::Level::warning, logging::Channel::diagnostics,
                "Debug tool: the skater step's code differs from this build's; skater probes off.");
            return false;
        }
    std::size_t prepared = 0;
    for (const auto& hook : hooks) {
        void* original{};
        if (hook_prepare(reinterpret_cast<void*>(base + hook.contract->rva), hook.detour, &original) != HookOk) break;
        *hook.original = reinterpret_cast<Step>(original);
        ++prepared;
    }
    if (prepared != std::size(hooks)) {
        for (std::size_t i = 0; i < prepared; ++i) hook_remove(reinterpret_cast<void*>(base + hooks[i].contract->rva));
        return false;
    }
    std::size_t enabled = 0;
    for (const auto& hook : hooks) {
        if (hook_enable(reinterpret_cast<void*>(base + hook.contract->rva)) != HookOk) break;
        ++enabled;
    }
    if (enabled != std::size(hooks)) {
        // Keep the trampolines: a detour already entered on another thread may still forward through them.
        for (std::size_t i = 0; i < enabled; ++i) hook_disable(reinterpret_cast<void*>(base + hooks[i].contract->rva));
        return false;
    }
    hooked.store(true);
    return true;
}
}
