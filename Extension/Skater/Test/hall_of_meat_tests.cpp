// The Hall of Meat model, with made-up physics steps.
#include "Extension/Skater/hall_of_meat_model.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace dingosdk;
using namespace dingosdk::hall_of_meat;
using skater_body::Bone;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }
constexpr std::uint64_t t0 = 100000; // GetTickCount64() is never near 0

// A step of a skater in a ragdoll that touches nothing: a bail lying still.
Step lying() {
    Step step;
    step.ragdoll = true;
    return step;
}
// A step of a skater standing, or riding: no ragdoll.
Step standing_up() {
    Step step;
    step.ragdoll = false;
    return step;
}
// A ragdoll step in which `bone` hits the ground at `speed`.
Step hit(Bone bone, float speed, bool wipeout = false) {
    auto step = lying();
    step.wipeout = wipeout;
    auto& contact = step.body.bodies[skater_body::index(bone)];
    contact.touching = true;
    contact.impact = speed;
    contact.hit.world = true;
    return step;
}
// The same while riding: no ragdoll yet.
Step riding_hit(Bone bone, float speed) {
    auto step = hit(bone, speed);
    step.ragdoll = false;
    return step;
}
Step wipeout() {
    auto step = lying();
    step.wipeout = true;
    return step;
}
// A step in the air, hitting nothing: in a ragdoll, or on the board.
Step flying(bool ragdoll = true) {
    Step step;
    step.airborne = true;
    step.ragdoll = ragdoll;
    return step;
}
// A ragdoll step in which `bone` slides along the ground at `speed`, hitting nothing hard.
Step slide(Bone bone, float speed, bool wipeout = false) {
    auto step = lying();
    step.wipeout = wipeout;
    auto& contact = step.body.bodies[skater_body::index(bone)];
    contact.touching = true;
    contact.slide = {speed, 0, 0};
    contact.hit.world = true;
    return step;
}
// Steps `step` from `now` on, every 16 ms, for `ms`; returns the time after the last.
std::uint64_t keep(Tracker& tracker, std::uint64_t now, std::uint64_t ms, const Step& step) {
    for (const auto until = now + ms; now < until; now += 16) tracker.step(now, step);
    return now;
}

void impacts_count_only_during_a_bail() {
    Tracker tracker;
    check(!tracker.step(t0, riding_hit(Bone::neck1, 9.0f)), "a step without a bail ends nothing");
    check(tracker.view(t0).phase == Phase::riding, "nothing shows before a bail");
    const auto later = t0 + wipeout_after_impact_ms + 16;
    tracker.step(later, wipeout());
    check(tracker.report(later).phase == Phase::falling, "a wipeout starts a bail");
    check(tracker.view(later).injuries[skater_body::index(Bone::neck1)] == Injury::none,
        "an impact long before the bail is not counted");
}

void the_hit_that_causes_the_wipeout_counts() {
    // A drop from height: the thigh hits, the wipeout comes a step later.
    Tracker tracker;
    tracker.step(t0, riding_hit(Bone::right_upleg, 35.0f));
    tracker.step(t0 + 16, riding_hit(Bone::right_upleg, 12.0f));
    tracker.step(t0 + 32, wipeout());
    const auto view = tracker.view(t0 + 32);
    check(view.phase == Phase::falling && view.injuries[skater_body::index(Bone::right_upleg)] == Injury::broken,
        "the hit before the wipeout breaks");
    check(view.tally.impacts == 1 && view.tally.hit_points == hit_points(35.0f), "at its hardest, once");
    const auto report = tracker.report(t0 + 32);
    check(report.hit && report.last.bone == skater_body::index(Bone::right_upleg) && report.last.speed == 35.0f,
        "and it is the last hit");
}

void nothing_shows_until_a_bone_is_hurt() {
    Tracker tracker;
    tracker.step(t0, wipeout());
    tracker.step(t0 + 16, hit(Bone::spine, hit_speed - 0.5f));
    check(tracker.report(t0 + 16).phase == Phase::falling && tracker.view(t0 + 16).phase == Phase::riding,
        "a bail that hurts nothing shows nothing");
    tracker.step(t0 + 32, hit(Bone::spine, hit_speed));
    const auto view = tracker.view(t0 + 32);
    check(view.phase == Phase::falling && view.card == 0 && view.tally.impacts == 1,
        "the first bruise brings the skeleton and the counter");
    Tracker unhurt;
    unhurt.step(t0, hit(Bone::spine, hit_speed - 0.5f, true));
    Summary summary;
    check(unhurt.step(t0 + 16, standing_up(), &summary) && !summary.shown, "it ends unshown");
    check(unhurt.view(t0 + 32).phase == Phase::riding, "nor does it get a card");
}

void hard_hits_break_and_light_ones_bruise() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::left_forearm, hit_speed + 0.1f, true));
    tracker.step(t0 + 16, hit(Bone::neck1, broken_speed + 0.1f));
    tracker.step(t0 + 32, hit(Bone::spine, hit_speed - 0.1f));
    tracker.step(t0 + 48, hit(Bone::left_forearm, 1.0f)); // a later, softer hit keeps the worst
    const auto view = tracker.view(t0 + 48);
    check(view.injuries[skater_body::index(Bone::left_forearm)] == Injury::hit, "a light hit bruises");
    check(view.injuries[skater_body::index(Bone::neck1)] == Injury::broken, "a hard hit breaks");
    check(view.injuries[skater_body::index(Bone::spine)] == Injury::none, "a graze below the threshold leaves no mark");
    check(view.flashes[skater_body::index(Bone::neck1)] > 0.9f, "a fresh hit flashes");
    check(tracker.view(t0 + 16 + flash_ms).flashes[skater_body::index(Bone::neck1)] == 0, "the flash fades");
}

void the_board_and_bad_values_are_ignored() {
    Tracker tracker;
    auto step = wipeout();
    step.body.bodies[skater_body::index(Bone::board_root)].impact = 20.0f;
    step.body.bodies[skater_body::index(Bone::hips)].impact = std::nanf("");
    step.body.bodies[skater_body::index(Bone::spine1)].impact = -8.0f;
    tracker.step(t0, step);
    for (const auto injury : tracker.view(t0).injuries) check(injury == Injury::none, "board, NaN and negative peaks hurt nothing");
}

void a_bail_follows_the_skater() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, 7.0f, true));
    // Rolling: the body keeps hitting the ground; the feet do not count.
    auto now = keep(tracker, t0 + 16, 3000, hit(Bone::spine2, tumbling_speed + 0.5f));
    check(tracker.view(now).phase == Phase::falling && tracker.view(now).card == 0, "tumbling: the counter");
    now = keep(tracker, now, settle_ms - 16, hit(Bone::left_foot, 3.0f));
    check(tracker.view(now - 16).phase == Phase::falling, "not yet lying");
    now = keep(tracker, now, 32, hit(Bone::left_foot, 3.0f));
    check(tracker.view(now).phase == Phase::down && near(tracker.view(now).card, 1.0f), "lying still: the card");
    now = keep(tracker, now, 500, hit(Bone::spine, tumbling_speed + 0.5f));
    check(tracker.view(now).phase == Phase::down, "a light touch on the ground keeps the card");
    tracker.step(now, hit(Bone::neck1, hit_speed + 1.0f));
    check(tracker.view(now).phase == Phase::falling, "a hit throws the body again: the counter");
    now = keep(tracker, now + 16, settle_ms + 16, lying());
    check(tracker.view(now).phase == Phase::down, "and it lies again");
    now = keep(tracker, now, 4000, lying()); // as long as the skater stays down
    check(tracker.view(now).phase == Phase::down, "lying, however long");
    Summary summary;
    check(tracker.step(now, standing_up(), &summary), "standing up ends the bail");
    check(summary.duration_ms == now - t0 && near(summary.peaks[skater_body::index(Bone::hips)], 7.0f) && summary.shown,
        "the summary holds the duration and the worst hits");
    const auto fading = tracker.view(now + fade_ms / 2);
    check(fading.phase == Phase::getting_up && near(fading.alpha, 0.5f) && near(fading.card, fading.alpha),
        "the skeleton and the card fade out together");
    check(fading.tally.impacts == 2, "with the bail's tally");
    check(tracker.view(now + fade_ms).phase == Phase::riding, "then they are gone");
    // The next wipeout starts afresh.
    tracker.step(now + fade_ms + 16, wipeout());
    check(tracker.view(now + fade_ms + 16).injuries[skater_body::index(Bone::hips)] == Injury::none, "a new bail starts clean");
}

void getting_up_before_the_body_lies_still_shows_the_card() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::left_hand, broken_speed + 1.0f, true));
    const auto now = keep(tracker, t0 + 16, settle_ms / 2, hit(Bone::spine, tumbling_speed + 0.5f));
    tracker.step(now, standing_up());
    const auto view = tracker.view(now);
    check(view.phase == Phase::getting_up && near(view.card, 1.0f) && view.tally.broken == 1,
        "the result shows as it fades");
}

void a_ragdoll_in_the_air_starts_the_bail() {
    // Thrown off the board from height: the ragdoll begins in the air, the wipeout comes with the impact.
    Tracker tracker;
    auto now = keep(tracker, t0, 500, flying(false));
    now = keep(tracker, now, 2000, flying());
    check(tracker.report(now).phase == Phase::falling, "the ragdoll starts the bail in the air");
    tracker.step(now, hit(Bone::neck1, 20.0f, true));
    check(tracker.view(now).phase == Phase::falling && tracker.view(now).tally.impacts == 1, "the impact is the same bail's");
    check(std::abs(tracker.view(now).tally.airtime - 2.5f) < 0.02f, "with the whole flight's airtime");
    now = keep(tracker, now + 16, settle_ms + 16, lying());
    Summary summary;
    check(tracker.step(now, standing_up(), &summary) && summary.tally.impacts == 1, "and ends when the skater stands up");
}

void a_stumble_without_a_ragdoll_ends() {
    Tracker tracker;
    auto stumble = wipeout();
    stumble.ragdoll = false;
    tracker.step(t0, stumble);
    const auto now = keep(tracker, t0 + 16, ragdoll_wait_ms - 32, standing_up());
    check(tracker.report(now).phase == Phase::falling, "the ragdoll is given a moment to begin");
    check(tracker.step(t0 + ragdoll_wait_ms, standing_up()), "then the bail ends");
}

void an_unknown_skater_state_ends_nothing() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, 7.0f, true));
    const auto now = keep(tracker, t0 + 16, 2000, Step{}); // the skater state could not be read
    check(tracker.report(now).phase == Phase::down, "the bail goes on");
    check(tracker.step(now, standing_up()), "until the skater is seen standing");
    Tracker riding;
    riding.step(t0, Step{});
    check(riding.report(t0).phase == Phase::riding, "nor does it start one");
}

void a_lost_skater_ends_the_bail() {
    Tracker tracker;
    check(!tracker.lose(t0), "no bail, nothing to end");
    tracker.step(t0, hit(Bone::hips, 7.0f, true));
    Summary summary;
    check(tracker.lose(t0 + 500, &summary) && summary.duration_ms == 500 && summary.shown, "a respawn ends the bail");
    check(tracker.view(t0 + 500).phase == Phase::getting_up, "and it fades out");
    check(!tracker.lose(t0 + 516), "once");
    // A teleport in mid-air: the flight before it is not the next bail's.
    Tracker teleported;
    const auto now = keep(teleported, t0, 1000, flying(false));
    teleported.lose(now);
    teleported.step(now + 16, hit(Bone::hips, 7.0f, true));
    check(teleported.view(now + 16).tally.airtime == 0, "a flight before a teleport is forgotten");
}

void a_reading_clock_behind_the_physics_one_is_harmless() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::neck, hit_speed + 0.5f, true));
    const auto view = tracker.view(t0 - 5);
    check(view.phase == Phase::falling && near(view.alpha, 1.0f) && near(view.flashes[skater_body::index(Bone::neck)], 1.0f),
        "a view a moment before the step still shows it");
}

void one_contact_is_one_impact() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::spine, 5.0f, true));
    tracker.step(t0 + 16, hit(Bone::spine, 9.0f)); // the same contact, harder
    tracker.step(t0 + 32, hit(Bone::spine, 2.0f)); // easing off: no hit
    auto tally = tracker.view(t0 + 32).tally;
    check(tally.impacts == 1 && tally.damage == hit_points(9.0f), "a contact counts once, as its hardest step");
    tracker.step(t0 + 32 + impact_gap_ms + 16, hit(Bone::spine, 5.0f)); // hitting the ground again
    tally = tracker.view(t0 + 32 + impact_gap_ms + 16).tally;
    check(tally.impacts == 2 && tally.damage == hit_points(9.0f) + hit_points(5.0f), "a new contact is a new impact");
    check(hit_points(hit_speed - 0.1f) == 0 && hit_points(5.0f) == 100, "only hits score");
    check(hit_points(30.0f) == 1470, "a slam scores more than its speed, less than its energy");
}

void the_meat_adds_up() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::neck1, 10.0f, true)); // the head breaks
    tracker.step(t0 + 16, hit(Bone::left_hand, 5.0f)); // a hand is bruised
    const auto tally = tracker.view(t0 + 16).tally;
    check(tally.broken == 1 && tally.impacts == 2, "one bone broken, two hits");
    check(tally.hit_points == hit_points(10.0f) + hit_points(5.0f) && tally.head_bonus == hit_points(10.0f),
        "a hit to the head counts double");
    check(tally.score == 2 * hit_points(10.0f) + hit_points(5.0f) + points_per_break, "the Meat is the hits and the breaks");
}

void a_vehicle_adds_half_again() {
    Tracker tracker;
    auto car = hit(Bone::spine, 10.0f, true);
    car.body.bodies[skater_body::index(Bone::spine)].hit.vehicle = true;
    tracker.step(t0, car);
    tracker.step(t0 + 16, hit(Bone::spine, 12.0f)); // the same contact, harder: still the car's
    const auto tally = tracker.view(t0 + 16).tally;
    check(tally.impacts == 1 && tally.vehicle_bonus == static_cast<int>(hit_points(12.0f) * 0.5f + 0.5f), "half again");
    check(impact_points({skater_body::index(Bone::neck1), 10.0f, true}) ==
              hit_points(10.0f) * 2 + static_cast<int>(hit_points(10.0f) * 0.5f + 0.5f),
        "the head and a vehicle together");
}

void sliding_along_the_ground_is_road_rash() {
    Tracker tracker;
    tracker.step(t0, slide(Bone::hips, 5.0f, true));
    auto now = t0;
    for (int i = 0; i < 50; ++i) tracker.step(now += 20, slide(Bone::hips, 5.0f)); // a second at 5 m/s
    const auto view = tracker.view(now);
    check(view.phase == Phase::falling, "a sliding body does not lie still");
    check(std::abs(view.tally.scraped - 5.0f) < 1e-3f && view.tally.scrape_points == 500, "5 m of road rash");
    check(view.injuries[skater_body::index(Bone::hips)] == Injury::hit, "it bruises and shows");
    check(view.tally.score == 500 && view.tally.impacts == 0, "it scores without a hit");

    // The whole body slides as one: the road rash is how far it went, not the bodies' sum.
    Tracker body;
    auto all = slide(Bone::hips, 4.0f, true);
    for (const auto bone : {Bone::spine, Bone::spine1, Bone::left_upleg})
        all.body.bodies[skater_body::index(bone)] = all.body.bodies[skater_body::index(Bone::hips)];
    all.body.bodies[skater_body::index(Bone::left_hand)] = slide(Bone::left_hand, 8.0f).body.bodies[skater_body::index(Bone::left_hand)];
    body.step(t0, all);
    all.wipeout = false;
    for (now = t0; now < t0 + 1000;) body.step(now += 20, all);
    const auto tally = body.view(now).tally;
    check(std::abs(tally.scraped - 4.8f) < 1e-3f, "the average of the scraping bodies' slides, over a second");

    Tracker board;
    auto on_board = slide(Bone::spine, 5.0f, true);
    auto& contact = on_board.body.bodies[skater_body::index(Bone::spine)];
    contact.hit = {};
    contact.hit.board = true;
    board.step(t0, on_board);
    board.step(t0 + 1000, on_board);
    board.step(t0 + 1020, slide(Bone::left_foot, 5.0f));
    board.step(t0 + 1040, slide(Bone::spine, scrape_speed - 0.1f));
    check(board.view(t0 + 1040).tally.scraped == 0.0f, "the board, the feet and a slow slip do not scrape");
}

void the_report_follows_the_bail() {
    Tracker tracker;
    check(tracker.report(t0).phase == Phase::riding && !tracker.report(t0).hit, "riding, no hit");
    tracker.step(t0, hit(Bone::left_hand, 6.0f, true));
    auto report = tracker.report(t0 + 500);
    check(report.phase == Phase::falling && report.bail_ms == 500, "falling");
    check(report.hit && report.last.bone == skater_body::index(Bone::left_hand) && report.tally.impacts == 1, "the hand's hit");
    auto now = keep(tracker, t0 + 16, settle_ms + 16, lying());
    check(tracker.report(now).phase == Phase::down, "down");
    tracker.step(now, standing_up());
    report = tracker.report(now + 100);
    check(report.phase == Phase::getting_up && report.bail_ms == now - t0, "getting up, with how long the bail lasted");
    check(tracker.report(now + fade_ms).phase == Phase::riding, "then riding again");
}

void airtime_is_the_bails_time_in_the_air() {
    // A fall from height on the board: 4 s in the air, the wipeout a step after the impact.
    Tracker tracker;
    std::uint64_t now = t0;
    for (; now <= t0 + 4000; now += 16) tracker.step(now, flying(false));
    tracker.step(now, standing_up());                       // the touch-down
    tracker.step(now + 16, hit(Bone::hips, hit_speed, true)); // the impact's wipeout
    tracker.step(now + 32, flying());                       // the ragdoll bounces: +16 ms
    tracker.step(now + 48, lying());                        // and lies
    tracker.step(now + 2048, flying());                     // after a pause: longest_step_ms at most
    check(std::abs(tracker.view(now + 2048).tally.airtime - (4000 + 16 + longest_step_ms) / 1000.0f) < 1e-4f,
        "the flight before the wipeout and the bail's own flights count");
    check(tracker.view(now + 2048).tally.score == hit_points(hit_speed), "airtime scores nothing");

    // A clean landing, then a bail later on the ground: that flight is not the bail's.
    Tracker later;
    for (now = t0; now <= t0 + 1000; now += 16) later.step(now, flying(false));
    later.step(now, standing_up());
    later.step(now + wipeout_after_impact_ms + 16, hit(Bone::hips, hit_speed, true));
    check(later.view(now + wipeout_after_impact_ms + 16).tally.airtime == 0, "a bail on the ground has no airtime");
}

void a_bail_lasts_through_its_flights() {
    // The first impact throws the body off a ledge: seconds in the air, then the second one.
    Tracker tracker;
    auto now = keep(tracker, t0, 1000, flying(false));
    tracker.step(now, hit(Bone::hips, 9.0f, true));
    const auto thrown = now;
    now = keep(tracker, now + 16, 3000, flying());
    check(tracker.view(now).phase == Phase::falling, "a body in the air does not lie still");
    tracker.step(now, hit(Bone::neck1, 12.0f, true)); // the second impact, wiping out again
    const auto tally = tracker.view(now).tally;
    check(tracker.view(now).phase == Phase::falling && tally.impacts == 2 && tally.broken == 2, "the same bail, both impacts");
    check(std::abs(tally.airtime - static_cast<float>(1000 + now - thrown - 16) / 1000.0f) < 0.02f,
        "the flight before the bail and the one inside it add up");
    Summary summary;
    tracker.step(now + 16, standing_up(), &summary);
    check(summary.tally.impacts == 2 && std::abs(summary.tally.airtime - tally.airtime) < 1e-4f,
        "the airtime holds until the bail is over");
}

void a_bail_sets_a_best_only_by_beating_it() {
    check(standing(0, 300).new_best && standing(0, 300).best == 300, "the first bail on a map sets its best");
    check(!standing(500, 300).new_best && standing(500, 300).best == 500, "a smaller bail keeps it");
    check(!standing(500, 500).new_best, "matching it does not beat it");
}
}

int main() {
    try {
        impacts_count_only_during_a_bail();
        the_hit_that_causes_the_wipeout_counts();
        nothing_shows_until_a_bone_is_hurt();
        hard_hits_break_and_light_ones_bruise();
        the_board_and_bad_values_are_ignored();
        a_bail_follows_the_skater();
        getting_up_before_the_body_lies_still_shows_the_card();
        a_ragdoll_in_the_air_starts_the_bail();
        a_stumble_without_a_ragdoll_ends();
        an_unknown_skater_state_ends_nothing();
        a_lost_skater_ends_the_bail();
        a_reading_clock_behind_the_physics_one_is_harmless();
        one_contact_is_one_impact();
        the_meat_adds_up();
        a_vehicle_adds_half_again();
        sliding_along_the_ground_is_road_rash();
        the_report_follows_the_bail();
        airtime_is_the_bails_time_in_the_air();
        a_bail_lasts_through_its_flights();
        a_bail_sets_a_best_only_by_beating_it();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Hall of Meat tests passed.\n";
    return 0;
}
