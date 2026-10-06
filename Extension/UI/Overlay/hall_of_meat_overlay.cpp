#include "overlay_internal.h"
#include <algorithm>
#include <cmath>

// Hall of Meat (Extension/Skater/hall_of_meat.h): from the local skater's bail until they get
// up, the bones it hurt over the world and the bail's score card. The game side hands over the
// posed skeleton mesh in world space with the live camera, drawn as the overlay's x-ray
// (skeleton_overlay.cpp): only the hurt bodies, in their injury's colour, as in skate. 3. The card
// is the overlay's score card (score_card_overlay.cpp). All of it on the background draw list,
// under ReSkate's own menus and chat, taking no input.

namespace dingosdk::overlay {
namespace {
std::atomic<MeatFrame (*)()> frame_feed{};
std::atomic<bool (*)()> enabled_feed{};
}
void set_hall_of_meat_hooks(HallOfMeatHooks hooks) noexcept {
    frame_feed.store(hooks.frame);
    enabled_feed.store(hooks.enabled);
}
bool hall_of_meat_available() noexcept { return enabled_feed.load() != nullptr; }
bool hall_of_meat_enabled() noexcept {
    const auto enabled = enabled_feed.load();
    return enabled && enabled();
}
} // namespace dingosdk::overlay

namespace dingosdk::overlay::detail {
namespace {
constexpr ImU32 bruised = IM_COL32(255, 196, 36, 255), broken = IM_COL32(232, 32, 32, 255),
    dark_red = IM_COL32(150, 12, 12, 255), hot = IM_COL32(255, 255, 255, 255);

struct MeatState {
    MeatFrame frame;
    ScoreCardMotion motion;
};
MeatState& meat() {
    static MeatState value;
    return value;
}
ImU32 mix(ImU32 from, ImU32 to, float amount) {
    const auto channel = [&](int shift) {
        const float a = static_cast<float>((from >> shift) & 0xff), b = static_cast<float>((to >> shift) & 0xff);
        return static_cast<int>(a + (b - a) * std::clamp(amount, 0.0f, 1.0f));
    };
    return IM_COL32(channel(IM_COL32_R_SHIFT), channel(IM_COL32_G_SHIFT), channel(IM_COL32_B_SHIFT),
        channel(IM_COL32_A_SHIFT));
}
// Bruised bones are yellow, broken ones a slowly pulsing red; a fresh hit flashes white-hot.
ImU32 bone_colour(MeatInjury injury, float flash, double time) {
    const ImU32 colour = injury == MeatInjury::broken
        ? mix(dark_red, broken, 0.5f + 0.5f * static_cast<float>(std::sin(time * 6.0))) : bruised;
    return mix(colour, hot, flash * 0.8f);
}

// The hurt bodies, near solid in their injury's colour; the rest are not drawn.
void draw_meat_skeleton(const MeatSkeleton& value) {
    SkeletonPaints paints{};
    const double time = ImGui::GetTime();
    for (std::size_t body = 0; body < skater_body::count; ++body)
        if (value.injuries[body] != MeatInjury::none)
            paints[body] = SkeletonPaint{bone_colour(value.injuries[body], value.flashes[body], time), 0.6f};
    draw_skeleton(value.frame, paints, value.alpha);
}
}

bool hall_of_meat_pending() {
    auto& m = meat();
    m.frame = {};
    if (const auto feed = frame_feed.load()) {
        try { m.frame = feed(); } catch (...) { m.frame = {}; }
    }
    return !m.frame.skeleton.frame.positions.empty() || m.frame.card.opacity > 0;
}

void draw_hall_of_meat() {
    auto& m = meat();
    draw_meat_skeleton(m.frame.skeleton);
    draw_score_card(m.frame.card, m.motion);
}
} // namespace dingosdk::overlay::detail
