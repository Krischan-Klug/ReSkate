// The simulation rate that keeps the game's steps each real second at a slower speed, and the step
// length the game keeps for a rate.
#include "Engine/Game/Settings/game_speed.h"
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void check(bool condition, const char* what) {
    if (!condition) { std::cerr << "FAILED: " << what << "\n"; ++failures; }
}
} // namespace

int main() {
    using dingosdk::game_speed::simulation_rate;
    using dingosdk::game_speed::step_seconds;
    check(simulation_rate(60, 1.0f) == 60, "full speed keeps the rate");
    check(simulation_rate(60, 0.3f) == 200, "slow motion steps more often: 200 a second of game time is 60 a real one");
    check(simulation_rate(60, 0.5f) == 120, "half speed doubles it");
    check(simulation_rate(60, 2.0f) == 60, "never faster than the game's own");
    check(simulation_rate(60, 0.0f) == 1200 && simulation_rate(60, -1.0f) == 1200, "a stop is the slowest it can follow");
    check(simulation_rate(60, 0.01f) == 1200, "at most the highest rate");
    check(simulation_rate(60, std::numeric_limits<float>::quiet_NaN()) == 60, "a broken speed is full speed");
    check(std::bit_cast<std::uint32_t>(step_seconds(60)) == 0x3c888888, "60 a second: the game's 1/60, from whole nanoseconds");
    check(step_seconds(200) == 0.005f, "200 a second: 5 ms");
    check(step_seconds(0) == 0.0f, "no rate, no step");
    if (failures) return 1;
    std::cout << "Game speed tests passed.\n";
    return 0;
}
