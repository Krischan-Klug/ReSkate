// The skater state model: which mode the skater is in and whether it is in the air.
#include "Engine/Game/Skater/skater_state.h"
#include <iostream>
#include <stdexcept>

using namespace dingosdk::skater_state;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

SkaterState on_board(bool in_the_air) {
    SkaterState state;
    state.on_board_in_the_air = in_the_air;
    state.offboard_known = true;
    return state;
}
SkaterState off_board(bool in_the_air, Substate substate = Substate::ground) {
    SkaterState state;
    state.offboard = true;
    state.offboard_known = true;
    state.flags.in_the_air = in_the_air;
    state.flags.substate = substate;
    return state;
}

void the_board_goes_by_the_physics_state() {
    check(mode(on_board(false)) == Mode::on_board && !airborne(on_board(false)), "rolling on the board");
    check(airborne(on_board(true)), "an ollie is in the air");
    auto stale = on_board(false);
    stale.flags.in_the_air = true; // the offboard state keeps its last values
    stale.flags.substate = Substate::ragdoll;
    check(mode(stale) == Mode::on_board && !airborne(stale), "on the board the offboard flags do not count");
}

void off_the_board_the_offboard_state_decides() {
    check(mode(off_board(false)) == Mode::on_foot && !airborne(off_board(false)), "walking");
    check(airborne(off_board(true, Substate::free_fall)), "a jump on foot is in the air");
    check(mode(off_board(true, Substate::ragdoll)) == Mode::ragdoll && airborne(off_board(true, Substate::ragdoll)),
        "a bail's fall is a ragdoll in the air");
    auto sliding = off_board(false, Substate::ragdoll);
    sliding.flags.falling = true;
    check(!airborne(sliding), "a ragdoll moving down on the ground is not in the air");
}

void an_unreadable_offboard_state_says_nothing() {
    auto unknown = off_board(true, Substate::ragdoll);
    unknown.offboard_known = false;
    check(mode(unknown) == Mode::on_foot && !airborne(unknown), "without the offboard state: on foot, not in the air");
}
}

int main() {
    try {
        the_board_goes_by_the_physics_state();
        off_the_board_the_offboard_state_decides();
        an_unreadable_offboard_state_says_nothing();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skater state tests passed.\n";
    return 0;
}
