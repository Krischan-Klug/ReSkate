#pragma once

namespace dingosdk::overlay {
// The DEBUG window: opens with the console, next to it (right, or left without room), with the step recording's
// start/stop button and every probe with its switch and live status. Presentation thread, menu frames.
void draw_debug_window();
// The recording card in the top left corner, drawn like Hall of Meat's card: small while idle, full while recording.
// Presentation thread; `debug_hud_pending` keeps the overlay drawing for it while no menu is open.
bool debug_hud_pending();
void draw_debug_hud();
}
