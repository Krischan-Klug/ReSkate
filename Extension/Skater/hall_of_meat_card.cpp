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
constexpr const char* fall_icon = "hallofmeat/gap_height";
constexpr const char* speed_icon = "hallofmeat/flaming_wheel";
constexpr const char* logo = "hallofmeat/thrasher";

overlay::GameImage image(const char* key, const ui::Texture& texture, std::uint32_t side) {
    const auto& r = texture.region;
    return {key, std::string(texture.toc), std::string(texture.bundle), std::string(texture.name), side, true,
        frostbite::ImageRegion{r.left, r.top, r.right, r.bottom}};
}
std::string hits(int value) { return grouped(value) + (value == 1 ? " hit" : " hits"); }
}

std::vector<overlay::GameImage> card_images() {
    return {image(duration_icon, ui::stopwatch, icon_side), image(hits_icon, ui::wipeout, icon_side),
        image(broken_icon, ui::wipeout_broken, icon_side), image(road_rash_icon, ui::spread_eagle, icon_side),
        image(airtime_icon, ui::airtime, icon_side), image(fall_icon, ui::gap_height, icon_side),
        image(speed_icon, ui::flaming_wheel, icon_side),
        image(logo, ui::thrasher_wordmark, logo_side)};
}

overlay::ScoreCard score_card(const View& view, const Standing& against) {
    if (view.phase == Phase::riding) return {};
    const auto& t = view.tally;
    overlay::ScoreCard card;
    card.opacity = view.alpha;
    const float road_rash = display_format::feet(t.scraped), fallen = display_format::feet(t.fallen),
        speed = display_format::mph(t.top_speed);
    const auto row = [&](bool shown, const char* key, const char* icon, std::string value, int points) {
        if (shown) card.rows.push_back({key, icon, std::move(value), points});
    };
    row(true, "time", duration_icon, std::format("{:.1f} s", t.seconds), t.time_points);
    row(t.impacts >= hits_shown, "hits", hits_icon, hits(t.impacts), t.hit_points + t.head_bonus + t.vehicle_bonus);
    row(t.broken >= broken_shown, "broken", broken_icon, grouped(t.broken) + " broken", t.broken * points_per_break);
    row(road_rash >= road_rash_shown_feet, "road_rash", road_rash_icon, std::format("{:.1f} ft", road_rash), t.scrape_points);
    row(t.airtime >= airtime_shown_seconds, "airtime", airtime_icon, std::format("{:.1f} s", t.airtime), t.airtime_points);
    row(fallen >= fall_shown_feet, "fall", fall_icon, std::format("{:.1f} ft", fallen), t.fall_points);
    row(speed >= speed_shown_mph, "speed", speed_icon, std::format("{:.1f} MPH", speed), t.speed_points);
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
