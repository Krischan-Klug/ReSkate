#pragma once
#include "Engine/Game/Skater/skater_body.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Hall of Meat, the model: what a bail did to the skater, and what it scores. Fed one
// physics step at a time with what each body touched (Engine/Game/Skater/skater_body.h);
// read by the overlay and the debug panel. A bail counts from its wipeout, with the hits
// just before it, but shows only once it has hurt a bone: a fall that leaves the skater
// unbruised shows nothing. No game access, no locking: the caller owns both.
namespace dingosdk::hall_of_meat {
using skater_body::Bone;

// One physics step of the local skater.
struct Step {
    bool wipeout{};             // the step asks for a wipeout
    bool airborne{};            // the skater is in the air, on the board or off it (skater_state.h)
    skater_body::Contacts body; // what each body touched in the step
};

enum class Injury : std::uint8_t { none, hit, broken };

// Impact speeds along the contact normal, from the bails logged on 2026-10-04: a fall on
// flat ground hits the bones it lands on at 3 to 6 m/s and grazes the rest at 2 to 3; a
// hard landing hits the legs at 8 to 10, a drop from height everything at 13 to 35.
inline constexpr float hit_speed = 4.0f;
inline constexpr float broken_speed = 8.0f;
// A body still tumbling keeps hitting things at least this hard (the feet aside), slides
// along, or is in the air: a bail thrown off a ledge by its first impact (2026-10-06) flew for
// seconds before the next one, and must not end up there.
inline constexpr float tumbling_speed = 1.0f;
inline constexpr std::uint64_t settle_ms = 1500;    // no tumbling for this long ends a bail
inline constexpr std::uint64_t longest_bail_ms = 12000;
inline constexpr std::uint64_t linger_ms = 6000;    // the skeleton and the bail's card stay this long after
inline constexpr std::uint64_t fade_ms = 800;       // the last part of it fades out
inline constexpr std::uint64_t flash_ms = 350;      // a fresh hit flashes this long
// A bone's contact lasts several physics steps: hits of one bone closer together than
// this are one impact, as hard as its hardest step.
inline constexpr std::uint64_t impact_gap_ms = 200;
inline constexpr std::size_t max_impacts = 64;
// Airtime adds up the time between physics steps in the air: the flight a bail starts from
// and every flight of the bail after it. A longer gap between steps (a pause) adds this much
// at most.
inline constexpr std::uint64_t longest_step_ms = 100;
// The game reports the wipeout of an impact a few physics steps after it (0 to 2 in the logs
// of 2026-10-05; a drop from height on 2026-10-06 hit the right thigh at 35 m/s a step before
// its wipeout): a bail takes the flight and the hits of this long before its wipeout.
inline constexpr std::uint64_t wipeout_after_impact_ms = 250;

// The Meat a bail scores: every hit by how hard it was, and each bone broken a bonus once.
// A hit's points grow faster than its speed but slower than its energy: a 5 m/s hit scores
// 100, a 30 m/s slam about 1,500 (15 bruises' worth, not the 36 its energy would make it).
inline constexpr float hit_reference_speed = 5.0f;
inline constexpr float points_per_reference_hit = 100.0f;
inline constexpr float hit_points_exponent = 1.5f;
inline constexpr int points_per_break = 500;
// A hit to the head (the upper neck it rides on, Bone::neck1) counts double; one from a
// vehicle half again. Both together count 2.5 times.
inline constexpr float head_multiplier = 2.0f;
inline constexpr float vehicle_multiplier = 1.5f;
// Road rash: a body sliding along the ground or a wall, not the board, at least this fast
// (metres per second along the surface) scrapes. A long slide on 2026-10-06 slid the body at 3
// to 10 m/s for seconds without a single hit. The skater's road rash is how far it slid: in
// each step, how fast its scraping bodies slid on average, not their sum (a dozen bodies on the
// ground at once would make one metre a dozen). A metre scores what a 5 m/s bruise does.
inline constexpr float scrape_speed = 1.5f;
inline constexpr float points_per_scraped_metre = 100.0f;
inline constexpr float bruising_scrape = 1.0f; // metres one bone slid: it shows as bruised

constexpr Injury injury(float peak) noexcept {
    return peak >= broken_speed ? Injury::broken : peak >= hit_speed ? Injury::hit : Injury::none;
}
// A hit's points by its speed alone.
inline int hit_points(float speed) noexcept {
    if (!(speed >= hit_speed)) return 0;
    return static_cast<int>(
        points_per_reference_hit * std::pow(speed / hit_reference_speed, hit_points_exponent) + 0.5f);
}

struct Impact {
    std::size_t bone{};
    float speed{};    // the hardest step of the contact
    bool vehicle{};   // a vehicle took part in it
};
constexpr bool head(std::size_t bone) noexcept { return bone == skater_body::index(Bone::neck1); }
// What an impact adds on top of its hit points, for the head and for a vehicle.
inline int head_bonus(const Impact& impact) noexcept {
    return head(impact.bone) ? static_cast<int>(hit_points(impact.speed) * (head_multiplier - 1.0f) + 0.5f) : 0;
}
inline int vehicle_bonus(const Impact& impact) noexcept {
    return impact.vehicle ? static_cast<int>(hit_points(impact.speed) * (vehicle_multiplier - 1.0f) + 0.5f) : 0;
}
inline int impact_points(const Impact& impact) noexcept {
    return hit_points(impact.speed) + head_bonus(impact) + vehicle_bonus(impact);
}

// A bail's numbers, so far or in the end.
struct Tally {
    int impacts{};       // hits of at least hit_speed
    int broken{};        // bones
    int hit_points{};    // the hits by their speed
    int head_bonus{};    // what hits to the head added
    int vehicle_bonus{}; // what hits from vehicles added
    float scraped{};     // metres the skater slid along surfaces: the road rash
    int scrape_points{};
    int damage{};        // the hits, their bonuses and the road rash
    int score{};         // the damage and the breaks: the Meat
    float airtime{};     // seconds in the air
};
// A bail's score against the best on the same map so far.
struct Standing {
    int best{};      // the best, this bail included
    bool new_best{}; // this bail set it
};
constexpr Standing standing(int best, int score) noexcept {
    return score > best ? Standing{score, true} : Standing{best, false};
}
// What the overlay draws at one moment: nothing until the bail hurts a bone.
struct View {
    bool visible{};   // the skeleton
    float alpha{};    // 1 while it shows, fading to 0 at the end of the linger
    std::array<Injury, skater_body::count> injuries{};
    std::array<float, skater_body::count> flashes{}; // 1 at a fresh hit, falling to 0
    bool bailing{};   // the bail goes on: the live counter shows
    float card{};     // after it, the card's opacity: the skeleton's (0 = no card)
    Tally tally;
};
// The bail at one moment, for the debug panel: the latest one while riding.
enum class Phase : std::uint8_t { riding, bailing, lingering };
struct Report {
    Phase phase{};
    std::uint64_t bail_ms{}; // how long the bail has lasted, or lasted
    Tally tally;
    bool hit{};              // the bail has a hit: last is it
    Impact last;
};
// A finished bail, for the log and the map's best.
struct Summary {
    std::uint64_t duration_ms{};
    bool shown{}; // it hurt a bone, so it showed (and counts for a best)
    std::array<float, skater_body::count> peaks{};   // each body's hardest hit
    std::array<float, skater_body::count> scraped{}; // and how far it slid, each on its own
    Tally tally;
};

class Tracker {
public:
    // One physics step at `now` (milliseconds). A wipeout starts a bail with the hits just
    // before it; everything else counts only during one. True when this step ended a bail;
    // `ended` then receives it.
    bool step(std::uint64_t now, const Step& step, Summary* ended = nullptr) noexcept;
    View view(std::uint64_t now) const noexcept;
    // view(now).visible, without building the view.
    bool visible(std::uint64_t now) const noexcept;
    Report report(std::uint64_t now) const noexcept;
    bool bailing() const noexcept { return bailing_; }
    void reset() noexcept { *this = {}; }

private:
    struct BoneState {
        float peak{};           // the hardest hit this bail
        std::uint64_t hit_at{}; // its last step with a hit
        std::size_t impact{};   // which impact that step belonged to
        float scraped{};        // metres it slid
        // Outside a bail: its hardest hit of the last wipeout_after_impact_ms, which a bail takes.
        float lead{};
        bool lead_vehicle{};
        std::uint64_t lead_at{};
    };
    void hit(std::size_t bone, float speed, bool vehicle, std::uint64_t now) noexcept;
    Tally tally() const noexcept;
    Injury injury_of(const BoneState& bone) const noexcept;

    bool bailing_{}, hurt_{}; // hurt_: a bone was hit at least at hit_speed, or scraped bruised, this bail
    std::uint64_t started_{}, tumbled_{}, ended_{}, stepped_{};
    std::uint64_t flight_ms_{};  // outside a bail: the last flight, which a bail takes over
    std::uint64_t landed_{};     // when it touched down; 0 while in the air
    std::uint64_t airtime_ms_{};
    float scraped_{}; // metres the skater slid: the road rash
    std::array<BoneState, skater_body::count> bones_{};
    std::array<Impact, max_impacts> impacts_{};
    std::size_t impact_count_{};
    std::size_t last_impact_{}; // the impact the latest hit belonged to
};
}
