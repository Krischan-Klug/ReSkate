#pragma once
#include "local_skater.h"
#include "Engine/Game/Skater/skater_state.h"

// The local skater's live state (Engine/Game/Skater/skater_state.h), read from the game
// (Engine/Game/Build/20260929/skater_state.h) for any feature that needs it: on the board or
// on foot, in the air, in a ragdoll. Read-only, no hooks: call it from any thread with a skater
// local_skater.h resolved.
namespace dingosdk::skater_state {
// False when the physics state cannot be read; the offboard state may still be unknown.
bool read(const LocalSkater& skater, SkaterState& state) noexcept;
}
