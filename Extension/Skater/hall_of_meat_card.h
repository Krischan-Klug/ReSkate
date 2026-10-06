#pragma once
#include "hall_of_meat_model.h"
#include "Extension/UI/Overlay/overlay.h"
#include <vector>

// Hall of Meat's score card (overlay.h ScoreCard), as in skate. 3: a row per stat with skate.'s
// own icons, then the Thrasher logo, "Hall of Meat" and the Meat. No game access.
namespace dingosdk::hall_of_meat {
// The game images the card shows (Engine/Game/Build/20260929/ui_textures.h), to add at startup.
std::vector<overlay::GameImage> card_images();
// The card for what the overlay draws now, against the map's best before the bail. No card while
// riding.
overlay::ScoreCard score_card(const View& view, const Standing& against);
}
