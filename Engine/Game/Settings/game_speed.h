#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// How the game runs at a speed below its own, as plain numbers. skate. steps its simulation at a
// fixed rate (SimulationTime.ForceSimRate, 60 a second) on a clock its time scale slows
// (SimulationTime.TimeScale): at 0.3 alone only 18 steps come each real second, and the camera,
// moving every frame, follows a skater that jumps 18 times a second. Stepping the simulation at the
// rate over the speed keeps 60 steps each real second, each the shorter. Measured in play on
// 2026-10-08: at 0.3 with a rate of 200 the slow motion runs smooth.
namespace dingosdk::game_speed {
inline constexpr float slowest = 0.05f;                // what the steps a second can follow
inline constexpr std::uint32_t highest_rate = 1200;    // steps a second of game time, at most

// The simulation rate (steps a second of game time) that keeps `base_rate` steps each real second at `speed`.
inline std::uint32_t simulation_rate(std::uint32_t base_rate, float speed) noexcept {
    const float clamped = std::isfinite(speed) ? std::clamp(speed, slowest, 1.0f) : 1.0f;
    const float rate = std::round(static_cast<float>(base_rate) / clamped);
    return static_cast<std::uint32_t>(std::clamp(rate, 1.0f, static_cast<float>(highest_rate)));
}

// The length of one simulation step at `rate`, seconds, exactly as the game keeps it: whole
// nanoseconds (1e9 / rate), then seconds (Ghidra sim_time_clock_apply_rate; measured 2026-10-09:
// 60 gives 0x3c888888, 200 gives 0.005). 0 for no rate.
inline float step_seconds(std::uint32_t rate) noexcept {
    return rate ? static_cast<float>(static_cast<double>(1000000000u / rate) * 1e-9) : 0.0f;
}
}
