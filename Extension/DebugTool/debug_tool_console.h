#pragma once
struct ImFont;

namespace dingosdk::overlay {
// The console's DEBUG tab: every probe with its switch, what it reports, and its live status.
// Presentation thread, inside the console window; `k` is the console's scale.
void draw_debug_tool(float k, float height, ImFont* bold, ImFont* body);
}
