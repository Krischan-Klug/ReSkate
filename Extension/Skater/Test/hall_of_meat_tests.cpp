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
// One physics step in which `bone` hits the ground at `speed`.
Step hit(Bone bone, float speed, bool wipeout = false) {
    Step step;
    step.wipeout = wipeout;
    auto& contact = step.body.bodies[skater_body::index(bone)];
    contact.touching = true;
    contact.impact = speed;
    contact.hit.world = true;
    return step;
}
Step wipeout() {
    Step step;
    step.wipeout = true;
    return step;
}
constexpr std::uint64_t t0 = 100000; // GetTickCount64() is never near 0

void impacts_count_only_during_a_bail() {
    Tracker tracker;
    check(!tracker.step(t0, hit(Bone::neck1, 9.0f)), "a step without a bail ends nothing");
    check(!tracker.view(t0).visible, "nothing shows before a bail");
    const auto later = t0 + wipeout_after_impact_ms + 16;
    tracker.step(later, wipeout());
    check(tracker.bailing(), "a wipeout starts a bail");
    check(tracker.view(later).injuries[skater_body::index(Bone::neck1)] == Injury::none,
        "an impact long before the bail is not counted");
}

void the_hit_that_causes_the_wipeout_counts() {
    // A drop from height: the thigh hits, the wipeout comes a step later.
    Tracker tracker;
    tracker.step(t0, hit(Bone::right_upleg, 35.0f));
    tracker.step(t0 + 16, hit(Bone::right_upleg, 12.0f));
    tracker.step(t0 + 32, wipeout());
    const auto view = tracker.view(t0 + 32);
    check(view.visible && view.injuries[skater_body::index(Bone::right_upleg)] == Injury::broken, "the hit before the wipeout breaks");
    check(view.tally.impacts == 1 && view.tally.hit_points == hit_points(35.0f), "at its hardest, once");
    const auto report = tracker.report(t0 + 32);
    check(report.hit && report.last.bone == skater_body::index(Bone::right_upleg) && report.last.speed == 35.0f,
        "and it is the last hit");
}

void nothing_shows_until_a_bone_is_hurt() {
    Tracker tracker;
    tracker.step(t0, wipeout());
    tracker.step(t0 + 16, hit(Bone::spine, hit_speed - 0.5f)); // grazes keep the body tumbling
    auto view = tracker.view(t0 + 16);
    check(tracker.bailing() && !view.visible && !view.bailing, "a bail that hurts nothing shows nothing");
    tracker.step(t0 + 32, hit(Bone::spine, hit_speed));
    view = tracker.view(t0 + 32);
    check(view.visible && view.bailing && view.tally.impacts == 1, "the first bruise brings the skeleton and the counter");
    Tracker unhurt;
    unhurt.step(t0, hit(Bone::spine, hit_speed - 0.5f, true));
    auto now = t0;
    while (!unhurt.step(now += 16, Step{})) {}
    check(unhurt.view(now).card == 0.0f, "nor does it get a card");
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

void a_bail_ends_once_the_body_settles_then_lingers() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, 7.0f, true));
    auto now = t0;
    // Rolling: the body keeps hitting the ground, the feet do not count.
    for (; now < t0 + 3000; now += 16) check(!tracker.step(now, hit(Bone::spine2, tumbling_speed + 0.5f)), "tumbling keeps the bail open");
    check(tracker.bailing(), "still bailing while tumbling");
    Summary summary;
    bool ended = false;
    for (; now < t0 + 3000 + settle_ms + 100 && !ended; now += 16) ended = tracker.step(now, hit(Bone::left_foot, 5.0f), &summary);
    check(ended && !tracker.bailing(), "foot contacts alone let the bail settle");
    check(summary.duration_ms >= 3000 + settle_ms - 16 && near(summary.peaks[skater_body::index(Bone::hips)], 7.0f),
        "the summary holds the duration and the worst hits");
    const auto end = now - 16;
    check(tracker.view(end + linger_ms - fade_ms / 2).visible, "the skeleton lingers after the bail");
    check(near(tracker.view(end + linger_ms - fade_ms / 2).alpha, 0.5f), "and fades out at the end of the linger");
    check(!tracker.view(end + linger_ms).visible, "then it is gone");
    check(tracker.visible(end + linger_ms - 1) && !tracker.visible(end + linger_ms), "visible() agrees with the view");
    // The next wipeout starts afresh.
    tracker.step(end + linger_ms + 10, wipeout());
    check(tracker.view(end + linger_ms + 10).injuries[skater_body::index(Bone::hips)] == Injury::none, "a new bail starts clean");
}

void a_bail_never_outlasts_its_cap() {
    Tracker tracker;
    bool ended = false;
    std::uint64_t now = t0;
    for (; now <= t0 + longest_bail_ms && !ended; now += 16) ended = tracker.step(now, hit(Bone::spine, 3.0f, true));
    check(ended, "an endless wipeout request still ends at the cap");
}

void a_reading_clock_behind_the_physics_one_is_harmless() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::neck, hit_speed + 0.5f, true));
    const auto view = tracker.view(t0 - 5);
    check(view.visible && near(view.alpha, 1.0f) && near(view.flashes[skater_body::index(Bone::neck)], 1.0f),
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

// One physics step in which `bone` slides along the ground at `speed`, hitting nothing hard.
Step slide(Bone bone, float speed, bool wipeout = false) {
    Step step;
    step.wipeout = wipeout;
    auto& contact = step.body.bodies[skater_body::index(bone)];
    contact.touching = true;
    contact.slide = {speed, 0, 0};
    contact.hit.world = true;
    return step;
}

void sliding_along_the_ground_is_road_rash() {
    Tracker tracker;
    tracker.step(t0, slide(Bone::hips, 5.0f, true));
    auto now = t0;
    for (int i = 0; i < 50; ++i) tracker.step(now += 20, slide(Bone::hips, 5.0f)); // a second at 5 m/s
    const auto view = tracker.view(now);
    check(tracker.bailing(), "a sliding body has not settled");
    check(std::abs(view.tally.scraped - 5.0f) < 1e-3f && view.tally.scrape_points == 500, "5 m of road rash");
    check(view.visible && view.injuries[skater_body::index(Bone::hips)] == Injury::hit, "it bruises and shows");
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
    check(report.phase == Phase::bailing && report.bail_ms == 500, "bailing");
    check(report.hit && report.last.bone == skater_body::index(Bone::left_hand) && report.tally.impacts == 1, "the hand's hit");
    auto now = t0;
    while (!tracker.step(now += 16, Step{})) {}
    report = tracker.report(now + 100);
    check(report.phase == Phase::lingering && report.bail_ms == now - t0, "lingering, with how long it lasted");
    check(tracker.report(now + linger_ms).phase == Phase::riding, "then riding again");
}

// One physics step in the air, hitting nothing.
Step flying() {
    Step step;
    step.airborne = true;
    return step;
}

void airtime_is_the_bails_time_in_the_air() {
    // A fall from height: 4 s in the air, the wipeout a step after the impact.
    Tracker tracker;
    std::uint64_t now = t0;
    for (; now <= t0 + 4000; now += 16) tracker.step(now, flying());
    tracker.step(now, Step{});                              // the touch-down
    tracker.step(now + 16, hit(Bone::hips, hit_speed, true)); // the impact's wipeout
    tracker.step(now + 32, flying());                       // the ragdoll bounces: +16 ms
    tracker.step(now + 48, Step{});                         // and lies
    tracker.step(now + 2048, flying());                     // after a pause: longest_step_ms at most
    check(std::abs(tracker.view(now + 2048).tally.airtime - (4000 + 16 + longest_step_ms) / 1000.0f) < 1e-4f,
        "the flight before the wipeout and the bail's own flights count");
    check(tracker.view(now + 2048).tally.score == hit_points(hit_speed), "airtime scores nothing");

    // A clean landing, then a bail later on the ground: that flight is not the bail's.
    Tracker later;
    for (now = t0; now <= t0 + 1000; now += 16) later.step(now, flying());
    later.step(now, Step{});
    later.step(now + wipeout_after_impact_ms + 16, hit(Bone::hips, hit_speed, true));
    check(later.view(now + wipeout_after_impact_ms + 16).tally.airtime == 0, "a bail on the ground has no airtime");
}

void a_bail_lasts_through_its_flights() {
    // The first impact throws the body off a ledge: seconds in the air, then the second one.
    Tracker tracker;
    std::uint64_t now = t0;
    for (; now < t0 + 1000; now += 16) tracker.step(now, flying());
    tracker.step(now, hit(Bone::hips, 9.0f, true));
    const auto thrown = now;
    for (now += 16; now < thrown + 3 * settle_ms; now += 16)
        check(!tracker.step(now, flying()), "a body in the air has not settled");
    tracker.step(now, hit(Bone::neck1, 12.0f, true)); // the second impact, wiping out again
    check(tracker.bailing(), "still the same bail");
    const auto tally = tracker.view(now).tally;
    check(tally.impacts == 2 && tally.broken == 2, "both impacts count");
    check(std::abs(tally.airtime - static_cast<float>(1000 + now - thrown - 16) / 1000.0f) < 0.02f,
        "the flight before the bail and the one inside it add up");
    Summary summary;
    while (!tracker.step(now += 16, Step{}, &summary)) {}
    check(summary.tally.impacts == 2 && std::abs(summary.tally.airtime - tally.airtime) < 1e-4f,
        "the airtime holds until the bail is over");
}

void a_bail_sets_a_best_only_by_beating_it() {
    check(standing(0, 300).new_best && standing(0, 300).best == 300, "the first bail on a map sets its best");
    check(!standing(500, 300).new_best && standing(500, 300).best == 500, "a smaller bail keeps it");
    check(!standing(500, 500).new_best, "matching it does not beat it");
}

void the_card_follows_the_bail() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::left_hand, broken_speed + 1.0f, true));
    auto now = t0;
    Summary summary;
    while (!tracker.step(now += 16, Step{}, &summary)) {}
    check(tracker.view(now).bailing == false && near(tracker.view(now).card, 1.0f), "the card comes with the bail's end");
    const auto fading = tracker.view(now + linger_ms - fade_ms / 2);
    check(fading.visible && near(fading.card, fading.alpha) && fading.tally.broken == 1,
        "the card fades with the skeleton, with its tally");
    check(tracker.view(now + linger_ms).card == 0.0f, "and goes with it");
    check(summary.tally.broken == 1 && summary.shown, "the summary carries the tally, and that it showed");
}
}

int main() {
    try {
        impacts_count_only_during_a_bail();
        the_hit_that_causes_the_wipeout_counts();
        nothing_shows_until_a_bone_is_hurt();
        hard_hits_break_and_light_ones_bruise();
        the_board_and_bad_values_are_ignored();
        a_bail_ends_once_the_body_settles_then_lingers();
        a_bail_never_outlasts_its_cap();
        a_reading_clock_behind_the_physics_one_is_harmless();
        one_contact_is_one_impact();
        the_meat_adds_up();
        a_vehicle_adds_half_again();
        sliding_along_the_ground_is_road_rash();
        the_report_follows_the_bail();
        airtime_is_the_bails_time_in_the_air();
        a_bail_lasts_through_its_flights();
        a_bail_sets_a_best_only_by_beating_it();
        the_card_follows_the_bail();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Hall of Meat tests passed.\n";
    return 0;
}
