#pragma once
#include "local_skater.h"
#include <cstdint>

namespace dingosdk {
bool start_no_bail(std::uintptr_t image_base) noexcept;
bool no_bail_available() noexcept;
// Publish from the validated local client tick, after publish_local_skater: protection
// applies to that skater. Manual protection expires if ticks stop arriving.
bool update_no_bail(bool manual, bool flying, std::uint64_t flight_expires) noexcept;
void clear_no_bail() noexcept;
// S.K.A.T.E.: while `locked`, the local skater cannot get back on the board once it is off
// (its mount request is dropped). Publish from the client tick; it expires if ticks stop.
// Releasing it also leaves the skater's teleport option on the board again, since a turn's
// teleport may have set it off (skater component +0xc0) and the SDK's own teleports keep it.
void update_board_lock(std::uintptr_t client, std::uintptr_t entity, bool locked) noexcept;
// The physics state the local skater's selector last chose, for the trainer (air time, bail
// markers). Publish the skater to watch from the client tick; it expires if ticks stop.
struct PhysicsStateWatch {
    bool valid{};
    std::uint32_t state{};
    std::uint32_t previous{}; // the state before this one
    float previous_seconds{}; // how long that one lasted
    std::uint64_t changes{}, wipeouts{}; // counted since the process started
};
void watch_physics_state(std::uintptr_t client, std::uintptr_t entity) noexcept;
PhysicsStateWatch watched_physics_state() noexcept;
// Stopping flight must not discard the independent manual preference.
void clear_no_bail_flight() noexcept;
// No Bail owns the hook on the native skeleton response, which runs once per physics
// step right after the step's body contacts. It hands each step of the published local
// skater (local_skater.h, verified again) to one observer, after its own filtering, so
// a filtered wipeout never happened. Physics thread; whether or not No Bail protects.
struct LocalSkeletonStep {
    LocalSkater skater;
    bool wipeout{};
};
using LocalSkeletonObserver = void (*)(const LocalSkeletonStep&) noexcept;
// One observer at a time; nullptr removes it.
void set_local_skeleton_observer(LocalSkeletonObserver observer) noexcept;
}
