#include "hall_of_meat_card.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/ui_textures.h"
#include "Engine/Game/UI/display_format.h"
#include <format>

namespace dingosdk::hall_of_meat {
namespace {
namespace ui = addr::ui_textures;
using display_format::grouped;
constexpr std::uint32_t icon_side = 64, logo_side = 512;
// The images' keys.
constexpr const char* duration_icon = "hallofmeat/stopwatch";
constexpr const char* hits_icon = "hallofmeat/wipeout";
constexpr const char* broken_icon = "hallofmeat/wipeout_broken";
constexpr const char* road_rash_icon = "hallofmeat/spread_eagle";
constexpr const char* airtime_icon = "hallofmeat/airtime";
constexpr const char* speed_icon = "hallofmeat/flaming_wheel";
constexpr const char* logo = "hallofmeat/thrasher";

overlay::GameImage image(const char* key, const ui::Texture& texture, std::uint32_t side) {
    const auto& r = texture.region;
    return {key, std::string(texture.toc), std::string(texture.bundle), std::string(texture.name), side, true,
        {r.left, r.top, r.right, r.bottom}};
}
std::string hits(int value) { return grouped(value) + (value == 1 ? " hit" : " hits"); }
}

std::vector<overlay::GameImage> card_images() {
    return {image(duration_icon, ui::stopwatch, icon_side), image(hits_icon, ui::wipeout, icon_side),
        image(broken_icon, ui::wipeout_broken, icon_side), image(road_rash_icon, ui::spread_eagle, icon_side),
        image(airtime_icon, ui::airtime, icon_side), image(speed_icon, ui::flaming_wheel, icon_side),
        image(logo, ui::thrasher_wordmark, logo_side)};
}

overlay::ScoreCard score_card(const View& view, const Standing& against) {
    if (view.phase == Phase::riding) return {};
    const auto& t = view.tally;
    overlay::ScoreCard card;
    card.opacity = view.alpha;
    card.rows = {
        {duration_icon, std::format("{:.1f} s", static_cast<double>(view.bail_ms) / 1000.0), std::nullopt},
        {hits_icon, hits(t.impacts), t.hit_points + t.head_bonus + t.vehicle_bonus},
        {broken_icon, grouped(t.broken) + " broken", t.broken * points_per_break},
        {road_rash_icon, std::format("{:.1f} ft", display_format::feet(t.scraped)), t.scrape_points},
        {airtime_icon, std::format("{:.1f} s", t.airtime), std::nullopt},
        {speed_icon, std::format("{:.1f} MPH", display_format::mph(t.hardest)), std::nullopt},
    };
    card.logo = logo;
    card.title = "Hall of Meat";
    card.total = t.score;
    if (against.new_best) {
        card.badge = "NEW BEST";
        card.highlight = true;
    } else if (against.best > 0) {
        card.badge = "BEST " + grouped(against.best);
    }
    return card;
}
}
