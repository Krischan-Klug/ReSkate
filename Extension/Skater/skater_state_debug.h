#pragma once
#include "Extension/UI/Overlay/overlay.h"
#include <vector>

// The SKATER menu's skater state debug panel: the local skater's live state
// (local_skater_state.h), sampled every client tick, the same fields on the board and off
// it. Each change is also logged. Off by default and not saved; a new value is one more
// field in fields_of().
namespace dingosdk::skater_state::debug {
bool enabled() noexcept;
void set_enabled(bool enabled) noexcept;
// Client thread, after the local skater was published.
void on_client_tick() noexcept;
// Render thread: the fields to show, empty while off.
std::vector<overlay::DebugField> fields();
}
