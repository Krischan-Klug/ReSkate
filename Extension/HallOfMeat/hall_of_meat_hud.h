#pragma once
#include <cstdint>

// skate.'s HUD in the bottom left corner (the d-pad menu, and the score HUD there while a line goes
// on), hidden while Hall of Meat's card shows in its place (Engine/Game/Build/20260929/hud_corner.h):
// - the d-pad hides itself while it is not alone in its stack: an empty item of ours lies in the
//   stack beside it (the game's items are never touched);
// - the score HUD shows by its view model alone: a hook on the UI model's write of one value
//   (Engine/Game/Build/20260929/ui_model.h) writes ours over its HudWidgetActive and
//   ExtraInfoStyle and keeps what the game meant, so the game keeps writing them but the widget only
//   ever sees the HUD down, until it is given back.
// The game's models are read and written on the client thread only, under the model lock its own
// writers take too.
namespace dingosdk::hall_of_meat {
// Startup, with the game's image base: without the model write hook the score HUD stays as it is.
void start_hud(std::uintptr_t base) noexcept;
// Client thread, every tick: hides the corner while `hidden`, and gives the game's own back after.
void hide_hud(bool hidden) noexcept;
}
