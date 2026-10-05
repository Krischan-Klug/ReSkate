// The Hall of Meat model and skeleton, with made-up physics steps and ragdolls.
#include "Extension/Skater/hall_of_meat_model.h"
#include "Extension/Skater/hall_of_meat_skeleton.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace dingosdk;
using namespace dingosdk::hall_of_meat;
using body_bones::Bone;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }
// One physics step in which `bone` hits something at `speed`.
Step hit(Bone bone, float speed, bool wipeout = false) {
    Step step;
    step.wipeout = wipeout;
    step.peaks[body_bones::index(bone)] = speed;
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
    tracker.step(t0 + 16, wipeout());
    check(tracker.bailing(), "a wipeout starts a bail");
    check(tracker.view(t0 + 16).injuries[body_bones::index(Bone::neck1)] == Injury::none,
        "an impact before the bail is not counted");
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
    check(view.injuries[body_bones::index(Bone::left_forearm)] == Injury::hit, "a light hit bruises");
    check(view.injuries[body_bones::index(Bone::neck1)] == Injury::broken, "a hard hit breaks");
    check(view.injuries[body_bones::index(Bone::spine)] == Injury::none, "a graze below the threshold leaves no mark");
    check(view.flashes[body_bones::index(Bone::neck1)] > 0.9f, "a fresh hit flashes");
    check(tracker.view(t0 + 16 + flash_ms).flashes[body_bones::index(Bone::neck1)] == 0, "the flash fades");
}

void the_board_and_bad_values_are_ignored() {
    Tracker tracker;
    auto step = wipeout();
    step.peaks[body_bones::index(Bone::board_root)] = 20.0f;
    step.peaks[body_bones::index(Bone::hips)] = std::nanf("");
    step.peaks[body_bones::index(Bone::spine1)] = -8.0f;
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
    check(summary.duration_ms >= 3000 + settle_ms - 16 && near(summary.peaks[body_bones::index(Bone::hips)], 7.0f),
        "the summary holds the duration and the worst hits");
    const auto end = now - 16;
    check(tracker.view(end + linger_ms - fade_ms / 2).visible, "the skeleton lingers after the bail");
    check(near(tracker.view(end + linger_ms - fade_ms / 2).alpha, 0.5f), "and fades out at the end of the linger");
    check(!tracker.view(end + linger_ms).visible, "then it is gone");
    check(tracker.visible(end + linger_ms - 1) && !tracker.visible(end + linger_ms), "visible() agrees with the view");
    // The next wipeout starts afresh.
    tracker.step(end + linger_ms + 10, wipeout());
    check(tracker.view(end + linger_ms + 10).injuries[body_bones::index(Bone::hips)] == Injury::none, "a new bail starts clean");
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
    check(view.visible && near(view.alpha, 1.0f) && near(view.flashes[body_bones::index(Bone::neck)], 1.0f),
        "a view a moment before the step still shows it");
}

void one_contact_is_one_impact() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::spine, 5.0f, true));
    tracker.step(t0 + 16, hit(Bone::spine, 9.0f)); // the same contact, harder
    tracker.step(t0 + 32, hit(Bone::spine, 2.0f)); // easing off: no hit
    auto tally = tracker.view(t0 + 32).tally;
    check(tally.impacts == 1 && tally.damage == impact_points(9.0f), "a contact counts once, as its hardest step");
    tracker.step(t0 + 32 + impact_gap_ms + 16, hit(Bone::spine, 5.0f)); // hitting the ground again
    tally = tracker.view(t0 + 32 + impact_gap_ms + 16).tally;
    check(tally.impacts == 2 && tally.damage == impact_points(9.0f) + impact_points(5.0f), "a new contact is a new impact");
    check(impact_points(hit_speed - 0.1f) == 0 && impact_points(5.0f) == 100, "only hits score");
    check(impact_points(30.0f) == 1470, "a slam scores more than its speed, less than its energy");
}

void the_meat_adds_up() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::neck1, 10.0f, true)); // the head breaks
    tracker.step(t0 + 16, hit(Bone::left_hand, 5.0f)); // a hand is bruised
    const auto tally = tracker.view(t0 + 16).tally;
    check(tally.broken == 1 && tally.impacts == 2, "one bone broken, two hits");
    check(tally.score == impact_points(10.0f) + impact_points(5.0f) + points_per_break,
        "the Meat is the hits and the breaks");
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
    check(tracker.view(now + 2048).tally.score == impact_points(hit_speed), "airtime scores nothing");

    // A clean landing, then a bail later on the ground: that flight is not the bail's.
    Tracker later;
    for (now = t0; now <= t0 + 1000; now += 16) later.step(now, flying());
    later.step(now, Step{});
    later.step(now + wipeout_after_impact_ms + 16, hit(Bone::hips, hit_speed, true));
    check(later.view(now + wipeout_after_impact_ms + 16).tally.airtime == 0, "a bail on the ground has no airtime");
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

void a_point_is_placed_in_its_frame() {
    // A frame turned 90 degrees about +Z (x -> y), standing at (1, 2, 3).
    const Frame frame{{{0, 1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 1, 0}, {1, 2, 3, 1}}};
    const auto point = place({2, 0, 1}, frame);
    check(near(point[0], 1) && near(point[1], 4) && near(point[2], 4), "offsets turn with the frame and move with it");
}

void only_a_tree_of_bodies_is_a_ragdoll() {
    Parents parents{};
    parents.fill(-1);
    check(valid_tree(parents), "loose bodies are a forest, which is fine");
    parents[2] = 1;
    parents[1] = 2;
    check(!valid_tree(parents), "a cycle is refused");
    parents[1] = 30;
    check(!valid_tree(parents), "a parent outside the bodies is refused");
}

// Pelvis -> spine -> neck -> head, and pelvis -> thigh -> foot, laid out along the axes.
Ragdoll small_ragdoll() {
    Ragdoll ragdoll;
    ragdoll.parents.fill(-1);
    const auto at = [&](Bone bone, Vec3 position, Bone parent) {
        ragdoll.joints[body_bones::index(bone)] = position;
        ragdoll.parents[body_bones::index(bone)] = static_cast<std::int8_t>(body_bones::index(parent));
    };
    ragdoll.joints[body_bones::index(Bone::hips)] = {0, 1, 0};
    ragdoll.parents[body_bones::index(Bone::hips)] = 0; // the board, which is not drawn
    at(Bone::spine, {0, 1.5f, 0}, Bone::hips);
    at(Bone::neck, {0, 2, 0}, Bone::spine);
    at(Bone::neck1, {0, 2.1f, 0}, Bone::neck);
    at(Bone::right_upleg, {0.2f, 1, 0}, Bone::hips);
    at(Bone::right_foot, {0.2f, 0, 0}, Bone::right_upleg);
    return ragdoll;
}

void bones_follow_the_ragdoll_tree() {
    Skull skull;
    const auto segments = bones(small_ragdoll(), skull);
    const auto has = [&](Bone owner, Vec3 from, Vec3 to) {
        return std::any_of(segments.begin(), segments.end(), [&](const Segment& segment) {
            return segment.body == body_bones::index(owner) && near(segment.from[0], from[0]) &&
                near(segment.from[1], from[1]) && near(segment.to[0], to[0]) && near(segment.to[1], to[1]);
        });
    };
    check(has(Bone::hips, {0, 1, 0}, {0, 1.5f, 0}) && has(Bone::hips, {0, 1, 0}, {0.2f, 1, 0}),
        "a body's bone runs to each of its children");
    check(has(Bone::spine, {0, 1.5f, 0}, {0, 2, 0}) && has(Bone::right_upleg, {0.2f, 1, 0}, {0.2f, 0, 0}),
        "every joint is the parent's bone end");
    check(has(Bone::right_foot, {0.2f, 0, 0}, {0.2f, -0.5f, 0}), "a chain's end continues half its last bone");
    check(std::none_of(segments.begin(), segments.end(), [](const Segment& segment) { return segment.body == 0; }),
        "the board draws nothing");
    check(near(skull.radius, skull_radius) && near(skull.centre[1], 2.1f + skull_offset),
        "the skull sits on the head body, along the neck");
}
}

int main() {
    try {
        impacts_count_only_during_a_bail();
        nothing_shows_until_a_bone_is_hurt();
        hard_hits_break_and_light_ones_bruise();
        the_board_and_bad_values_are_ignored();
        a_bail_ends_once_the_body_settles_then_lingers();
        a_bail_never_outlasts_its_cap();
        a_reading_clock_behind_the_physics_one_is_harmless();
        one_contact_is_one_impact();
        the_meat_adds_up();
        airtime_is_the_bails_time_in_the_air();
        a_bail_sets_a_best_only_by_beating_it();
        the_card_follows_the_bail();
        a_point_is_placed_in_its_frame();
        only_a_tree_of_bodies_is_a_ragdoll();
        bones_follow_the_ragdoll_tree();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Hall of Meat tests passed.\n";
    return 0;
}
