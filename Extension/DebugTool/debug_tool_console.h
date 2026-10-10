#pragma once

namespace dingosdk::overlay {
// The DEBUG window: opens with the console, next to it (right, or left without room), with the step recording's
// start/stop button and every probe with its switch and live status. Presentation thread, menu frames.
void draw_debug_window();
// While the step recording runs: an always visible REC bar at the top of the screen; F9 starts and stops the
// recording without the console. Presentation thread, every frame.
void draw_debug_hud();
}
