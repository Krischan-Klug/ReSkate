// The skater state model: which mode the skater is in and whether it is in the air.
#include "Engine/Game/Skater/skater_state.h"
#include <iostream>
#include <stdexcept>

using namespace dingosdk;
using namespace dingosdk::skater_state;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

SkaterState on_board(bool in_the_air) {
    SkaterState state;
    state.on_board_in_the_air = in_the_air;
    state.offboard_known = true;
    return state;
}
SkaterState off_board(bool in_the_air, bool ragdoll = false) {
    SkaterState state;
    state.offboard = true;
    state.offboard_known = true;
    state.flags.in_the_air = in_the_air;
    state.flags.ragdoll = ragdoll;
    return state;
}

void the_board_goes_by_the_physics_state() {
    check(mode(on_board(false)) == Mode::on_board && !airborne(on_board(false)), "rolling on the board");
    check(airborne(on_board(true)), "an ollie is in the air");
    auto stale = on_board(false);
    stale.flags.in_the_air = true; // the offboard state keeps its last values
    stale.flags.ragdoll = true;
    check(mode(stale) == Mode::on_board && !airborne(stale), "on the board the offboard flags do not count");
}

void off_the_board_the_offboard_state_decides() {
    check(mode(off_board(false)) == Mode::on_foot && !airborne(off_board(false)), "walking");
    check(airborne(off_board(true)), "a jump on foot is in the air");
    check(mode(off_board(true, true)) == Mode::ragdoll && airborne(off_board(true, true)), "a bail's fall is a ragdoll in the air");
    check(!airborne(off_board(false, true)), "a ragdoll on the ground is not in the air");
}

void the_velocity_follows_the_mode() {
    auto riding = on_board(false);
    riding.board_velocity = {3, 0, 4};
    riding.body_velocity = {1, 0, 0};
    check(game::length(velocity(riding)) == 5.0f, "on the board: the board's");
    auto bailing = off_board(true, true);
    bailing.board_velocity = {9, 0, 0};
    bailing.body_velocity = {0, -6, 8};
    check(game::length(velocity(bailing)) == 10.0f && velocity(bailing)[1] == -6.0f, "off it: the body's");
}

void an_unreadable_offboard_state_says_nothing() {
    auto unknown = off_board(true, true);
    unknown.offboard_known = false;
    check(mode(unknown) == Mode::on_foot && !airborne(unknown), "without the offboard state: on foot, not in the air");
    check(!mode_known(unknown), "and the mode is not known");
    check(mode_known(on_board(false)) && mode_known(off_board(false)), "on the board, and off it with the offboard state, it is");
}
}

int main() {
    try {
        the_board_goes_by_the_physics_state();
        off_the_board_the_offboard_state_decides();
        an_unreadable_offboard_state_says_nothing();
        the_velocity_follows_the_mode();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skater state tests passed.\n";
    return 0;
}
