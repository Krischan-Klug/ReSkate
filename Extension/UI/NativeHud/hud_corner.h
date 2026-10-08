#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// skate.'s HUD in the bottom left corner (the d-pad menu, and the score HUD there while a line goes
// on), covered for any feature that asks, each part the way it shows
// (Engine/Game/Build/20260929/hud_corner.h):
// - the d-pad hides itself while it is not alone in its stack: an empty item of ours lies in the
//   stack beside it (the game's items are never touched);
// - the score HUD shows by its view model alone: its HudWidgetActive and ExtraInfoStyle are taken
//   over through the native UI's model takeover (Extension/UI/NativeUi/model_takeover.h), so the
//   game keeps writing them but the widget only ever sees the HUD down, until it is given back.
// Once no feature covers the corner, the game's own shows again. The game's models are read and
// written on the client thread only.
namespace dingosdk::hud_corner {
// What a feature covers.
enum class Cover : std::uint8_t {
    none,
    dpad, // the d-pad: the score HUD still shows as the game has it
    all,  // the d-pad and the score HUD
};
std::string_view name(Cover cover) noexcept; // "none", "dpad", "all"

// Startup, with the game's image base: without the model takeover the score HUD is not covered.
void start(std::uintptr_t base) noexcept;
// Any thread. `owner` covers this from now on (none: no longer anything). The most any feature covers is covered.
void set_cover(std::string_view owner, Cover cover);
struct Claim {
    std::string owner;
    Cover cover{};
};
std::vector<Claim> claims(); // any thread: every feature that covers something

// What the corner shows now. Client thread.
struct Item {
    std::string widget; // its content's blueprint, "" for ours
    std::int32_t priority{};
    bool active{};
    bool ours{};
};
struct State {
    bool stack_found{}; // the stack the d-pad is in
    std::int32_t target_index{};
    bool stack_active{};
    std::vector<Item> items;
    bool score_found{};   // ScoringHUDViewModel
    bool score_shown{};   // its HudWidgetActive
    int extra_info_style{};
    bool score_held{};    // taken over: the score HUD held down
};
State state() noexcept;
// Client thread, every tick: covers and uncovers the corner as the features ask.
void on_client_tick() noexcept;
}
