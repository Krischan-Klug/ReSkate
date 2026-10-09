#include "hall_of_meat.h"
#include "Extension/Console/commands.h"

// The `hallofmeat` console command; the menu's switch (Custom Stuff > Player) sends it too.
namespace dingosdk::console {
void register_hall_of_meat_commands(Commands &registry) {
    auto meat = variable("hallofmeat", "Hall of Meat: the bones the skater hurt when they bail, bruised ones yellow and broken ones red, and the bail's Meat score",
        Group::movement, argument("on|off", Type::boolean));
    meat.execution = Execution::local;
    meat.inspect = [](const Model &) { return boolean_state(true, hall_of_meat::enabled()); };
    meat.run = [](const Model &, const Values &args, const Output &out) {
        const bool on = std::get<bool>(args[0]);
        hall_of_meat::set_enabled(on);
        out(on ? "Hall of Meat on." : "Hall of Meat off.");
    };
    registry.add(std::move(meat));
}
} // namespace dingosdk::console
