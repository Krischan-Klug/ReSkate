#pragma once
#include "Extension/Debug/debug_panel.h"

// The skater state as a debug panel source (Extension/Debug/debug_panel.h): the local
// skater's live state (local_skater_state.h), the same fields on the board and off it. A new
// value is one more field in this source's sample.
namespace dingosdk::skater_state {
debug_panel::Source debug_source();
}
