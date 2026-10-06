#pragma once
#include "debug_panel_model.h"
#include "Extension/UI/Overlay/overlay.h"
#include <string>
#include <string_view>
#include <vector>

// The debug panel: one source's live values in the bottom right corner, to watch while
// playing and to work out what the game's values mean. Any module adds itself as a source
// at startup; SETTINGS > INTERFACE or `debugpanel <source>` picks the one to show. Every
// change of a shown value is also logged. Hidden by default and not saved.
namespace dingosdk::debug_panel {
struct Source {
    std::string id;    // the console's name for it: lower case, one word
    std::string title; // the menu's, the panel's and the log's
    // Client thread, every tick while it shows: the same fields in the same order each time,
    // "-" for a value that cannot be read.
    std::vector<Field> (*sample)() = nullptr;
};
// Startup: one more source to choose from; a second one with the same id is ignored.
void add(Source source);
std::vector<overlay::DebugSource> sources();
// Shows the source with this id, or hides the panel for an empty one. False when there is
// no such source.
bool select(std::string_view id);
std::string selected(); // the shown source's id, empty while hidden
// Client thread, every tick: samples the shown source and logs what changed.
void on_client_tick() noexcept;
// Render thread: what to draw, no fields while hidden.
overlay::DebugPanel panel();
}
