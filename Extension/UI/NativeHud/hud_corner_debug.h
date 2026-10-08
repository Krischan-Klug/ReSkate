#pragma once
#include "Extension/Debug/debug_panel.h"

// skate.'s bottom left HUD corner as a debug panel source (Extension/Debug/debug_panel.h): who
// covers what, what the d-pad's stack holds (each item's widget, priority and whether it is
// active, ours marked), and whether the score HUD shows or is held down, live.
namespace dingosdk::hud_corner {
debug_panel::Source debug_source();
}
