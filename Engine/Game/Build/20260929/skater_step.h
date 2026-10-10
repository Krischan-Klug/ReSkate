#pragma once
#include "Engine/Game/Build/fingerprint.h"
#include <cstdint>

namespace dingosdk::game::build::v20260929::skater_step {
// Supported SHA-256 fbce74d5e28ef525dbba2cb4adbebc13405bdbd88f31bc940bca45e4ae88b8f9.
// One physics step of a skater's core (re/controller.md "Ein Schritt"), static in Ghidra 2026-10-09.
// Before the state tick: void core_state_pre_physics_tick(SkaterCore*). The step's inputs are in the
// ctx by now (core_fill_step_state ran) and the state is chosen; it calls the state's tick (vtable +0x10).
inline constexpr Fingerprint pre_tick_contract{0x47da250, {
    0x40,0x57,0x48,0x83,0xec,0x30,0x48,0x8b,0xf9,0x48,0x8b,0x89,0xb0,0x03,0x00,0x00,
    0x48,0x8b,0x01,0xff,0x50,0x10,0x48,0x83,0xbf,0x30,0x04,0x00,0x00,0x00,0x0f,0x84}};
// After the world's physics: void core_physics_step(SkaterCore*): body contacts, the solver slot
// (core vtable +0x58), then the skeleton response (no_bail.h).
inline constexpr Fingerprint physics_step_contract{0x47da350, {
    0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xe8,0x2e,0xc5,
    0xfe,0xff,0x48,0x83,0xbb,0x30,0x04,0x00,0x00,0x00,0x74,0x41,0x48,0x8b,0x03,0x48}};
// The state choice: void core_advance_physics_state(SkaterCore*), after the ctx is filled and before
// the state tick; it asks the chooser (core+0x440, no_bail.h hooks the chooser itself) and switches.
inline constexpr Fingerprint advance_state_contract{0x47da1b0, {
    0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48,
    0x8b,0xd9,0x48,0x8b,0x89,0xb0,0x03,0x00,0x00,0x48,0x8b,0x01,0xff,0x50,0x40,0x48}};
// What the choice reads (re/controller/state-selection.md): the chooser's counters, its source bundle B
// (*chooser) with B+0x10, B+0x20 (A), B+0x30, B+0x38, the object at *(*(chooser+0x10)+0x18), and three
// bound tuning instances in the ctx: G +0x1480, S +0x1490, K +0x14a0. Sizes cover the fields it reads.
inline constexpr std::uintptr_t core_chooser_offset = 0x440;
inline constexpr std::uint32_t chooser_bytes = 0x60, bundle_bytes = 0x60, bundle_state_bytes = 0x200,
    bundle_10_bytes = 0x200, bundle_30_bytes = 0x1000, bundle_38_bytes = 0x100, chooser_owner_bytes = 0x2700; // natural air reads +0xa5c and +0x26bc
inline constexpr std::uintptr_t ctx_instance_g_offset = 0x1480, ctx_instance_s_offset = 0x1490,
    ctx_instance_k_offset = 0x14a0;
inline constexpr std::uint32_t instance_g_bytes = 0x50, instance_s_bytes = 0x110, instance_k_bytes = 0x28;
// SkaterCore (0x4170 bytes, allocated by the core base constructor) and what it points at, with their
// allocation sizes (re/scripts/skater_structs.py).
inline constexpr std::uint32_t core_size = 0x4170;
inline constexpr std::uintptr_t core_state_offset = 0x3b0; // the current state object
// State vtable: +0x40 int id() (e.g. 200 air, 603 liptrick), +0x48 const char* name() ("PHYSICS_STATE_<NAME>").
inline constexpr std::uintptr_t state_id_slot = 0x40, state_name_slot = 0x48;
inline constexpr std::uintptr_t core_ctx_offset = 0x3c0;
inline constexpr std::uint32_t ctx_size = 0x19e0;
// SkaterCtx, the step's input: its length (always 1/60 s, measured), and the skater's position (m, y up) and
// velocity (vec4; the backward difference of the position), measured with the debug tool on 2026-10-09.
inline constexpr std::uintptr_t ctx_step_seconds_offset = 0x17ec;
inline constexpr std::uintptr_t ctx_position_offset = 0x4e0, ctx_velocity_offset = 0x4f0;
inline constexpr std::uintptr_t core_controller_offset = 0x3d8; // direction +0x700, target +0x730
inline constexpr std::uint32_t controller_size = 0xf20;
inline constexpr std::uintptr_t core_surface_offset = 0x3f0;
inline constexpr std::uint32_t surface_size = 0x1080;
inline constexpr std::uintptr_t core_trajectory_offset = 0x3f8;
inline constexpr std::uint32_t trajectory_size = 0x2a0;
inline constexpr std::uintptr_t core_rig_offset = 0x438;
inline constexpr std::uint32_t rig_size = 0x5ac0;
// The state's pose helper (state+0x10): truck twist +0x140, ctx +0x178, response records [+0x190, +0x198) of 0x30 B
// (skater_pose_update_truck_twist, skater_pose_sum_record_vectors). Its size is not known; 0x400 covers the fields read.
inline constexpr std::uintptr_t state_pose_offset = 0x10, pose_records_offset = 0x190;
inline constexpr std::uint32_t pose_bytes = 0x400, pose_record_size = 0x30, pose_records_max = 64;
// Body list the states write (pose+0x18 -> provider, provider+0x20 -> list): 26 slots of 0x130 B, frame +0x20,
// dirty flags +0x60, linear velocity +0x70, angular +0x90 (skater_pose_write_primary_body_velocity).
inline constexpr std::uintptr_t pose_provider_offset = 0x18, provider_body_list_offset = 0x20;
inline constexpr std::uint32_t body_list_bytes = 26 * 0x130;
// The board proxy's torque queue (body list +0xa78: circular list, sentinel next/prev +0xa78/+0xa80, count +0xa88) that
// skater_body_append_transformed_angular_response and skater_body_append_projected_world_torque fill: 0x40-byte nodes.
inline constexpr std::uintptr_t body_torque_queue = 0xa78;
inline constexpr std::uint32_t torque_node_bytes = 0x40, torque_nodes_max = 64;
// Bound instances the ground responses read (re/controller/states/physics_ground.md): defaults ctx+0x1440 (+0x1e4..+0x1f4),
// runtime tuning ctx+0x1448 (+0x14/+0x1c/+0x20), 412f194a ctx+0x1468, 47bd7711 ctx+0x1498. Sizes not known; 0x400 each.
inline constexpr std::uintptr_t ctx_defaults_offset = 0x1440, ctx_runtime_tuning_offset = 0x1448,
    ctx_instance_1468_offset = 0x1468, ctx_instance_1498_offset = 0x1498;
inline constexpr std::uint32_t bound_instance_bytes = 0x400;
// PHYSICS_GROUND's pumping helper (state+0x58): +0x40 pumping scalar, +0x48 front-foot absorption input.
inline constexpr std::uintptr_t ground_pumping_offset = 0x58;
// Vector-math constants the controller reads (sin polynomial, 2pi, 1/2pi, pi at +0x759fd50..+0x75a0300); the exe fills
// them at start (.udata), so the recording takes them from memory once.
inline constexpr std::uintptr_t math_constants = 0x759fd50;
inline constexpr std::uint32_t math_constants_bytes = 0x5b0;
// 2D curves (curve_evaluate_float_surface): asset+0x20 records of 0x10 B (tagged FloatCurve* +0, outer key +8), count
// at records-4; a FloatCurve keeps its points at +0x18, count at points-4. Jump: tuning block +0x7e0; slide: the
// asset's PhysicsSlide block (+0x2ad8) +0x320.
inline constexpr std::uintptr_t block_jump_curve2d = 0x7e0, asset_slide_curve2d = 0x2ad8 + 0x320;
inline constexpr std::uintptr_t curve2d_records = 0x20, float_curve_points = 0x18;
// Ground segment lists (anti-pumping, re/controller/anti-pumping.md): ctx+0x11f0 past, +0x11f8 future, arrays of tagged
// pointers with the count at array-4; a segment keeps normal +0x20, direction +0x30, start +0x40, end +0x50, angle +0x64.
inline constexpr std::uintptr_t ctx_segments_past = 0x11f0, ctx_segments_future = 0x11f8;
inline constexpr std::uint32_t segment_bytes = 0x70, segments_max = 128;
// The core's trajectory prediction (core+0x3e8, 0xbba0 B): KNOWN_AIR and the take-off read its samples and query.
inline constexpr std::uintptr_t core_prediction_offset = 0x3e8;
inline constexpr std::uint32_t prediction_size = 0xbba0;
inline constexpr std::uint32_t pumping_bytes = 0x200;
} // namespace dingosdk::game::build::v20260929::skater_step
