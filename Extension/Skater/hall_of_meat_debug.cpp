#include "hall_of_meat_debug.h"
#include "hall_of_meat.h"
#include "local_skater.h"
#include "local_skater_state.h"
#include "Extension/Settings/named_settings.h"
#include <format>
#include <string>

namespace dingosdk::hall_of_meat {
namespace {
using debug_panel::Field;

std::string_view phase_name(Phase phase) {
    switch (phase) {
    case Phase::bailing: return "bailing";
    case Phase::down: return "down";
    case Phase::getting_up: return "getting up";
    case Phase::riding: break;
    }
    return "riding";
}
// The local skater's body: on the board, on foot or a ragdoll; "-" while there is none to read.
std::string body() {
    LocalSkater skater;
    skater_state::SkaterState state;
    if (!current_local_skater(skater) || !skater_state::read(skater, state) || !skater_state::mode_known(state)) return "-";
    switch (skater_state::mode(state)) {
    case skater_state::Mode::on_board: return "on board";
    case skater_state::Mode::on_foot: return "on foot";
    case skater_state::Mode::ragdoll: break;
    }
    return "ragdoll";
}
// A native setting as the engine holds it now, "-" when it cannot be read.
std::string setting(std::string_view name) { return read_named_setting(name).value_or("-"); }
// What made a hit count more: "head", "vehicle", both or nothing.
std::string bonuses(const Impact& impact) {
    const bool on_head = head(impact.bone);
    if (on_head && impact.vehicle) return " (head, vehicle)";
    return on_head ? " (head)" : impact.vehicle ? " (vehicle)" : "";
}

std::vector<Field> sample() {
    const auto r = report();
    const auto& t = r.tally;
    std::vector<Field> fields;
    fields.push_back({"BAIL", {}, true});
    fields.push_back({"Phase", std::string(phase_name(r.phase))});
    fields.push_back({"Body", body()});
    // The slow motion after a break, and the native settings it holds (Extension/Settings/game_speed.h).
    fields.push_back({"GAME SPEED", {}, true});
    fields.push_back({"Wanted", std::format("{:.2f}", r.game_speed), false, true});
    fields.push_back({"Held", debug_panel::yes_no(slowing())});
    fields.push_back({"TimeScale", setting("SimulationTime.TimeScale")});
    fields.push_back({"ForceSimRate", setting("SimulationTime.ForceSimRate")});
    fields.push_back({"MaxSimFps", setting("SimulationTime.MaxSimFps")});
    fields.push_back({"MEAT", {}, true});
    fields.push_back({"Meat", std::to_string(t.score)});
    fields.push_back({"Hits", std::format("{} ({})", t.hit_points, t.impacts)});
    fields.push_back({"Head", std::format("+{}", t.head_bonus)});
    fields.push_back({"Vehicle", std::format("+{}", t.vehicle_bonus)});
    fields.push_back({"Road rash", std::format("+{} ({:.1f} m)", t.scrape_points, t.scraped), false, true});
    fields.push_back({"Broken", std::format("{} (+{})", t.broken, t.broken * points_per_break)});
    fields.push_back({"Time", std::format("+{} ({:.1f} s)", t.time_points, t.seconds), false, true});
    fields.push_back({"Airtime", std::format("+{} ({:.1f} s)", t.airtime_points, t.airtime), false, true});
    fields.push_back({"Fall", std::format("+{} ({:.1f} m)", t.fall_points, t.fallen), false, true});
    fields.push_back({"Top speed", std::format("+{} ({:.1f} m/s)", t.speed_points, t.top_speed), false, true});
    fields.push_back({"Rotations", std::format("+{} ({:.1f})", t.rotation_points, t.rotations), false, true});
    fields.push_back({"LAST HIT", {}, true});
    fields.push_back({"Bone", r.hit ? std::format("{} {:.1f} m/s", skater_body::names[r.last.bone], r.last.speed) : "-"});
    fields.push_back({"Points", r.hit ? std::format("+{}{}", impact_points(r.last), bonuses(r.last)) : "-"});
    return fields;
}
}

debug_panel::Source debug_source() { return {"hallofmeat", "Hall of Meat", &sample}; }
}
