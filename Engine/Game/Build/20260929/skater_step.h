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
} // namespace dingosdk::game::build::v20260929::skater_step
