#include "skater_state_debug.h"
#include "local_skater_state.h"
#include "Engine/Game/UI/display_format.h"
#include <format>
#include <numbers>
#include <string>

namespace dingosdk::skater_state {
namespace {
using debug_panel::Field;
using debug_panel::yes_no;

std::string speed_text(const game::Vec3& velocity) {
    const float speed = game::length(velocity);
    return std::format("{:.1f} m/s ({:.1f} MPH)", speed, display_format::mph(speed));
}
std::string_view mode_name(Mode mode) {
    return mode == Mode::on_board ? "on board" : mode == Mode::on_foot ? "on foot" : "ragdoll";
}

std::vector<Field> sample() {
    LocalSkater skater;
    SkaterState s;
    const bool known = current_local_skater(skater) && read(skater, s);
    const auto value = [known](std::string text) { return known ? std::move(text) : std::string("-"); };
    const auto flag = [&](bool on) { return value(s.offboard_known ? yes_no(on) : "-"); };
    std::vector<Field> fields;
    fields.push_back({"SKATER", {}, true});
    fields.push_back({"Physics state", value(s.physics_state_name.empty() ? std::to_string(s.physics_state)
        : std::format("{} {}", s.physics_state, s.physics_state_name))});
    fields.push_back({"Mode", value(std::string(mode_name(mode(s))))});
    fields.push_back({"Airborne", value(yes_no(airborne(s)))});
    fields.push_back({"MOTION", {}, true});
    const auto motion = [&](const game::Vec3& velocity) { return value(s.motion_known ? speed_text(velocity) : "-"); };
    fields.push_back({"Speed", motion(velocity(s)), false, true});
    fields.push_back({"Board", motion(s.board_velocity), false, true});
    fields.push_back({"Body (pelvis)", motion(s.body_velocity), false, true});
    fields.push_back({"Vertical", value(s.motion_known ? std::format("{:+.1f} m/s", velocity(s)[1]) : "-"), false, true});
    const float spin = game::length(s.body_spin) * 180.0f / std::numbers::pi_v<float>;
    fields.push_back({"Spin (pelvis)", value(s.motion_known ? std::format("{:.0f} deg/s ({:.1f} turns/s)", spin, spin / 360.0f) : "-"),
        false, true});
    fields.push_back({"OFF THE BOARD", {}, true});
    const auto& f = s.flags;
    fields.push_back({"Substate", value(s.offboard_known ? std::string(substate_name(f.substate)) : "-")});
    fields.push_back({"Height above ground", value(s.offboard_known ? std::format("{:.2f} m", f.height_above_ground) : "-"),
        false, true});
    fields.push_back({"In the air", flag(f.in_the_air)});
    fields.push_back({"Falling", flag(f.falling)});
    fields.push_back({"Landing", flag(f.landing)});
    fields.push_back({"Mounting", flag(f.mounting)});
    fields.push_back({"Off the board", flag(f.off_the_board)});
    fields.push_back({"Foot A planted", flag(f.foot_a_planted)});
    fields.push_back({"Foot B planted", flag(f.foot_b_planted)});
    return fields;
}
}

debug_panel::Source debug_source() { return {"skaterstate", "Skater state", &sample}; }
}
