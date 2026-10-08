// The Hall of Meat model, with made-up physics steps, and its score card.
#include "Extension/Skater/hall_of_meat_card.h"
#include "Extension/Skater/hall_of_meat_model.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
// A ragdoll step at `speed`, touching nothing.
Step moving(float speed) {
    auto step = lying();
    step.velocity = game::Vec3{speed, 0, 0};
    return step;
}
float seconds(std::uint64_t ms) { return static_cast<float>(ms) / 1000.0f; }
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
    check(tracker.report(later).phase == Phase::bailing, "a wipeout starts a bail");
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
    check(view.phase == Phase::bailing && view.injuries[skater_body::index(Bone::right_upleg)] == Injury::broken,
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
    check(tracker.report(t0 + 16).phase == Phase::bailing && tracker.view(t0 + 16).phase == Phase::riding,
        "a bail that hurts nothing shows nothing");
    tracker.step(t0 + 32, hit(Bone::spine, hit_speed));
    const auto view = tracker.view(t0 + 32);
    check(view.phase == Phase::bailing && near(view.alpha, 1.0f) && view.tally.impacts == 1,
        "the first bruise brings the skeleton and the card");
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

void a_flump_starts_no_bail() {
    // Rolling along the ground and jumping off a height while rolling: a ragdoll slamming the ground,
    // which the game never wipes out.
    Tracker tracker;
    auto now = keep(tracker, t0, 1500, flying());
    for (int i = 0; i < 60; ++i, now += 16) {
        auto step = hit(i % 2 ? Bone::left_arm : Bone::spine, broken_speed + 5.0f);
        step.velocity = game::Vec3{8, -6, 0};
        tracker.step(now, step);
    }
    check(tracker.view(now).phase == Phase::riding && tracker.view(now).tally.score == 0, "nothing shows, nothing counts");
    check(!tracker.step(now, standing_up()), "and no bail ends");
}

void a_break_pulses_the_screen_edge() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::left_hand, hit_speed + 1.0f, true));
    check(tracker.view(t0 + 16).break_pulse == 0.0f, "a bruise does not pulse");
    tracker.step(t0 + 16, hit(Bone::left_forearm, broken_speed + 1.0f));
    const auto broke = t0 + 16;
    check(near(tracker.view(broke + break_rise_ms / 2).break_pulse, 0.5f), "a break pulses up");
    check(near(tracker.view(broke + break_rise_ms).break_pulse, 1.0f), "to its full strength");
    check(near(tracker.view(broke + break_rise_ms + break_hold_ms - 16).break_pulse, 1.0f), "holds it");
    check(near(tracker.view(broke + break_rise_ms + break_hold_ms + break_fall_ms / 2).break_pulse, 0.5f), "and fades");
    check(tracker.view(broke + break_effect_ms).break_pulse == 0.0f, "then it is gone");
    tracker.step(broke + break_effect_ms, hit(Bone::left_forearm, broken_speed + 5.0f));
    check(tracker.view(broke + break_effect_ms + break_rise_ms).break_pulse == 0.0f, "a broken bone hit again does not pulse");
    tracker.step(broke + break_effect_ms + 16, hit(Bone::right_leg, broken_speed + 1.0f));
    check(near(tracker.view(broke + break_effect_ms + 16 + break_rise_ms).break_pulse, 1.0f), "the next break pulses again");
}

void a_break_slows_the_game_with_its_red_edge() {
    Tracker tracker;
    check(tracker.game_speed(t0) == 1.0f, "full speed without a bail");
    tracker.step(t0, hit(Bone::left_hand, hit_speed + 1.0f, true));
    check(tracker.game_speed(t0 + 16) == 1.0f, "a bruise keeps the speed");
    tracker.step(t0 + 16, hit(Bone::left_forearm, broken_speed + 1.0f));
    const auto broke = t0 + 16;
    check(tracker.game_speed(broke) == 1.0f && near(tracker.game_speed(broke + break_rise_ms), break_game_speed),
        "a break slows the game down");
    check(near(tracker.game_speed(broke + break_rise_ms + break_hold_ms - 16), break_game_speed), "holds it slow");
    check(near(tracker.game_speed(broke + break_rise_ms + break_hold_ms + break_fall_ms / 2), (1.0f + break_game_speed) / 2),
        "then lets it go");
    check(tracker.game_speed(broke + break_effect_ms) == 1.0f, "back to full speed as the red edge is gone");
    for (auto at = broke; at < broke + break_effect_ms; at += 10)
        check(near(tracker.game_speed(at), 1.0f - (1.0f - break_game_speed) * tracker.view(at).break_pulse),
            "the slow motion and the red edge are one");
    // A further break while it lets go slows the game again, with no jump on the way.
    const auto again = broke + break_rise_ms + break_hold_ms + break_fall_ms / 2;
    tracker.step(again, hit(Bone::right_leg, broken_speed + 1.0f));
    float previous = tracker.game_speed(again - 1), steepest = 0;
    for (auto at = again; at < again + break_rise_ms + 16; ++at) {
        const float speed = tracker.game_speed(at);
        steepest = std::max(steepest, std::abs(speed - previous));
        previous = speed;
    }
    check(steepest < 0.02f, "the speed never jumps");
    check(near(tracker.game_speed(again + break_rise_ms), break_game_speed), "the further break slows it down again");
    tracker.lose(again + 16);
    check(tracker.game_speed(again + 32) == 1.0f, "a skater gone takes the slow motion with them");
}

void a_break_just_before_the_bail_counts_from_the_bail() {
    // The wipeout comes a few steps after the impact that broke the bone: the slow motion and the
    // red edge run from the step that saw the break, not from the impact.
    Tracker tracker;
    tracker.step(t0, riding_hit(Bone::right_upleg, broken_speed + 5.0f));
    const auto wiped_out = t0 + wipeout_after_impact_ms - 16;
    tracker.step(wiped_out, wipeout());
    check(near(tracker.game_speed(wiped_out + break_rise_ms), break_game_speed), "the slow motion at its full strength");
    check(near(tracker.view(wiped_out + break_rise_ms).break_pulse, 1.0f), "the red edge is at its full strength");
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

void a_bail_lasts_until_the_skater_gets_up() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, hit_speed + 1.0f, true));
    // Rolling, then lying however long: the bail goes on, its card with it.
    auto now = keep(tracker, t0 + 16, 3000, hit(Bone::spine2, 2.0f));
    now = keep(tracker, now, 6000, lying());
    tracker.step(now, hit(Bone::neck1, hit_speed + 1.0f)); // a car finds the body on the ground
    now = keep(tracker, now + 16, 1000, lying());
    const auto down = tracker.view(now);
    check(down.phase == Phase::bailing && near(down.alpha, 1.0f) && down.tally.impacts == 2, "the bail and its card go on");
    Summary summary;
    check(tracker.step(now, standing_up(), &summary), "standing up ends the bail");
    check(near(summary.tally.seconds, seconds(now - t0)) && near(summary.peaks[skater_body::index(Bone::hips)], hit_speed + 1.0f) && summary.shown,
        "the summary holds the duration and the worst hits");
    const auto lingering = tracker.view(now + linger_ms - 16);
    check(lingering.phase == Phase::getting_up && near(lingering.alpha, 1.0f), "the skeleton and the card stay a while");
    const auto fading = tracker.view(now + linger_ms + fade_ms / 2);
    check(fading.phase == Phase::getting_up && near(fading.alpha, 0.5f), "then fade out together");
    check(fading.tally.impacts == 2, "with the bail's tally");
    check(tracker.view(now + linger_ms + fade_ms).phase == Phase::riding, "then they are gone");
    // The next wipeout starts afresh.
    const auto later = now + linger_ms + fade_ms + 16;
    tracker.step(later, wipeout());
    check(tracker.view(later).injuries[skater_body::index(Bone::hips)] == Injury::none, "a new bail starts clean");
}

void a_rest_ends_the_counting() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, hit_speed + 1.0f, true));
    auto now = keep(tracker, t0 + 16, 1000, moving(4.0f));
    const auto went_still = now;
    now = keep(tracker, now, rest_ms - 16, moving(0.2f));
    check(tracker.report(now - 16).phase == Phase::bailing, "not resting yet");
    now = keep(tracker, now, 32, moving(0.1f));
    check(tracker.report(now).phase == Phase::down, "a still body rests");
    check(near(tracker.view(now).tally.seconds, seconds(went_still - t0)), "its time stopped when it went still");
    auto twitch = hit(Bone::left_hand, hit_speed + 1.0f);
    twitch.velocity = game::Vec3{0.3f, 0, 0}; // a bone twitches, the body barely moves
    tracker.step(now, twitch);
    check(tracker.report(now).phase == Phase::down && tracker.view(now).tally.impacts == 1, "a twitch at rest scores nothing");
    const auto rested = tracker.view(now).tally;
    const auto later = now + 3000;
    check(near(tracker.view(later).tally.seconds, seconds(went_still - t0)), "however long it lies");
    // Getting up starts inside the ragdoll and moves the body fast again: nothing more counts.
    auto getting_up = hit(Bone::left_foot, hit_speed + 1.0f);
    getting_up.velocity = game::Vec3{0, 3.0f, 2.0f};
    now = keep(tracker, later, 600, getting_up);
    const auto final = tracker.view(now).tally;
    check(tracker.report(now).phase == Phase::down && final.score == rested.score && final.impacts == rested.impacts &&
              near(final.seconds, rested.seconds) && near(final.top_speed, rested.top_speed),
        "getting up adds no time and no points");
    Summary summary;
    tracker.step(now, standing_up(), &summary);
    check(summary.tally.score == rested.score && near(summary.tally.seconds, seconds(went_still - t0)),
        "the bail ends as it was when the body came to rest");
    check(tracker.view(now + linger_ms + fade_ms / 2).tally.score == rested.score, "and fades out so");
    Tracker unknown;
    unknown.step(t0, hit(Bone::hips, hit_speed + 1.0f, true));
    check(unknown.report(keep(unknown, t0 + 16, 2000, lying())).phase == Phase::bailing, "an unknown speed never rests");
}

void a_fall_from_height_bails_at_its_wipeout() {
    // Thrown off the board from height: the ragdoll begins in the air, the wipeout comes with the impact.
    Tracker tracker;
    auto now = keep(tracker, t0, 500, flying(false));
    now = keep(tracker, now, 2000, flying());
    check(tracker.report(now).phase == Phase::riding, "a ragdoll in the air is no bail yet");
    tracker.step(now, hit(Bone::neck1, 20.0f, true));
    check(tracker.view(now).phase == Phase::bailing && tracker.view(now).tally.impacts == 1, "the wipeout starts it with the impact");
    check(std::abs(tracker.view(now).tally.airtime - 2.5f) < 0.02f, "and the whole flight's airtime");
    now = keep(tracker, now + 16, 1000, lying());
    Summary summary;
    check(tracker.step(now, standing_up(), &summary) && summary.tally.impacts == 1, "and ends when the skater stands up");
}

void a_stumble_without_a_ragdoll_ends() {
    Tracker tracker;
    auto stumble = wipeout();
    stumble.ragdoll = false;
    tracker.step(t0, stumble);
    const auto now = keep(tracker, t0 + 16, ragdoll_wait_ms - 32, standing_up());
    check(tracker.report(now).phase == Phase::bailing, "the ragdoll is given a moment to begin");
    check(tracker.step(t0 + ragdoll_wait_ms, standing_up()), "then the bail ends");
}

void an_unknown_skater_state_ends_nothing() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::hips, hit_speed + 1.0f, true));
    const auto now = keep(tracker, t0 + 16, 2000, Step{}); // the skater state could not be read
    check(tracker.report(now).phase == Phase::bailing, "the bail goes on");
    check(tracker.step(now, standing_up()), "until the skater is seen standing");
    Tracker riding;
    riding.step(t0, Step{});
    check(riding.report(t0).phase == Phase::riding, "nor does it start one");
}

void a_lost_skater_ends_the_bail() {
    Tracker tracker;
    check(!tracker.lose(t0), "no bail, nothing to end");
    tracker.step(t0, hit(Bone::hips, hit_speed + 1.0f, true));
    Summary summary;
    check(tracker.lose(t0 + 500, &summary) && near(summary.tally.seconds, 0.5f) && summary.shown, "a respawn ends the bail");
    check(tracker.view(t0 + 500).phase == Phase::getting_up, "and it fades out");
    check(!tracker.lose(t0 + 516), "once");
    // A teleport in mid-air: the flight before it is not the next bail's.
    Tracker teleported;
    const auto now = keep(teleported, t0, 1000, flying(false));
    teleported.lose(now);
    teleported.step(now + 16, hit(Bone::hips, hit_speed + 1.0f, true));
    check(teleported.view(now + 16).tally.airtime == 0, "a flight before a teleport is forgotten");
}

void a_reading_clock_behind_the_physics_one_is_harmless() {
    Tracker tracker;
    tracker.step(t0, hit(Bone::neck, hit_speed + 0.5f, true));
    const auto view = tracker.view(t0 - 5);
    check(view.phase == Phase::bailing && near(view.alpha, 1.0f) && near(view.flashes[skater_body::index(Bone::neck)], 1.0f),
        "a view a moment before the step still shows it");
}

void one_contact_is_one_impact() {
    Tracker tracker;
    constexpr float light = hit_speed + 0.5f, harder = broken_speed - 1.0f;
    tracker.step(t0, hit(Bone::spine, light, true));
    tracker.step(t0 + 16, hit(Bone::spine, harder)); // the same contact, harder
    tracker.step(t0 + 32, hit(Bone::spine, 2.0f)); // easing off: no hit
    auto tally = tracker.view(t0 + 32).tally;
    check(tally.impacts == 1 && tally.damage == hit_points(harder), "a contact counts once, as its hardest step");
    tracker.step(t0 + 32 + impact_gap_ms + 16, hit(Bone::spine, light)); // hitting the ground again
    tally = tracker.view(t0 + 32 + impact_gap_ms + 16).tally;
    check(tally.impacts == 2 && tally.damage == hit_points(harder) + hit_points(light), "a new contact is a new impact");
    check(hit_points(hit_speed - 0.1f) == 0 && hit_points(hit_speed) == 100, "only hits score, the lightest 100");
    check(hit_points(30.0f) == 887, "a slam scores more than its speed, less than its energy");
}

void the_meat_adds_up() {
    Tracker tracker;
    constexpr float breaks = broken_speed + 1.0f, bruises = hit_speed + 1.0f;
    tracker.step(t0, hit(Bone::neck1, breaks, true)); // the head breaks
    tracker.step(t0 + 16, hit(Bone::left_hand, bruises)); // a hand is bruised
    const auto tally = tracker.view(t0 + 16).tally;
    check(tally.broken == 1 && tally.impacts == 2, "one bone broken, two hits");
    check(tally.hit_points == hit_points(breaks) + hit_points(bruises) && tally.head_bonus == hit_points(breaks),
        "a hit to the head counts double");
    check(tally.score == 2 * hit_points(breaks) + hit_points(bruises) + points_per_break + tally.time_points,
        "the Meat is the hits, the breaks and the time");
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
    check(view.phase == Phase::bailing, "a sliding bail goes on");
    check(std::abs(view.tally.scraped - 5.0f) < 1e-3f && view.tally.scrape_points == static_cast<int>(5.0f * points_per_scraped_metre + 0.5f), "5 m of road rash");
    check(view.injuries[skater_body::index(Bone::hips)] == Injury::hit, "it bruises and shows");
    check(view.tally.score == view.tally.scrape_points + view.tally.time_points && view.tally.impacts == 0, "it scores without a hit");

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
    tracker.step(t0, hit(Bone::left_hand, hit_speed + 1.0f, true));
    auto report = tracker.report(t0 + 500);
    check(report.phase == Phase::bailing && near(report.tally.seconds, 0.5f), "bailing");
    check(report.hit && report.last.bone == skater_body::index(Bone::left_hand) && report.tally.impacts == 1, "the hand's hit");
    const auto now = keep(tracker, t0 + 16, 1000, lying());
    tracker.step(now, standing_up());
    report = tracker.report(now + 100);
    check(report.phase == Phase::getting_up && near(report.tally.seconds, seconds(now - t0)), "getting up, with how long the bail lasted");
    check(tracker.report(now + linger_ms + fade_ms).phase == Phase::riding, "then riding again");
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
    const auto air = tracker.view(now + 2048).tally;
    check(air.airtime_points == static_cast<int>(air.airtime * points_per_air_second + 0.5f), "airtime scores by the second");

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
    tracker.step(now, hit(Bone::hips, broken_speed + 1.0f, true));
    const auto thrown = now;
    now = keep(tracker, now + 16, 3000, flying());
    check(tracker.view(now).phase == Phase::bailing, "a body in the air is still bailing");
    tracker.step(now, hit(Bone::neck1, broken_speed + 1.0f, true)); // the second impact, wiping out again
    const auto tally = tracker.view(now).tally;
    check(tracker.view(now).phase == Phase::bailing && tally.impacts == 2 && tally.broken == 2, "the same bail, both impacts");
    check(std::abs(tally.airtime - static_cast<float>(1000 + now - thrown - 16) / 1000.0f) < 0.02f,
        "the flight before the bail and the one inside it add up");
    Summary summary;
    tracker.step(now + 16, standing_up(), &summary);
    check(summary.tally.impacts == 2 && std::abs(summary.tally.airtime - tally.airtime) < 1e-4f,
        "the airtime holds until the bail is over");
}

void the_card_shows_the_bail() {
    const auto images = card_images();
    check(images.size() == 13, "eight icons, the logo and four shapes");
    const auto added = [&](const std::string& key) {
        return std::count_if(images.begin(), images.end(), [&](const auto& image) { return image.key == key; }) == 1;
    };
    for (const auto& image : images) {
        check(image.colours != overlay::GameImageColours::original && image.side > 0 && !image.name.empty(),
            "white to be tinted, from a texture");
        check(added(image.key), "each under its own key");
    }
    Tracker tracker;
    check(score_card(tracker.view(t0), {}).opacity == 0 && score_card(tracker.view(t0), {}).rows.empty(), "no card while riding");
    tracker.step(t0, hit(Bone::neck1, broken_speed + 1.0f, true)); // the head breaks
    tracker.step(t0 + 16, hit(Bone::left_hand, hit_speed + 1.0f));
    const auto view = tracker.view(t0 + 1500);
    auto card = score_card(view, standing(0, view.tally.score));
    check(near(card.opacity, 1.0f) && card.total == view.tally.score && card.title == "Hall of Meat" && added(card.logo) &&
              card.logo_clear < 1,
        "the Meat under the logo, its title in the arch, every stat counted");
    const auto& skin = card.skin;
    check(added(skin.row) && added(skin.panel) && added(skin.scratches) && added(skin.underline),
        "drawn with the shapes it adds");
    check(card.rows.size() == 1 && card.rows[0].key == "time" && card.rows[0].value == "1.5 s" &&
              card.rows[0].points == static_cast<int>(1.5f * points_per_second + 0.5f),
        "a small bail shows its time alone");
    check(card.badge == "NEW BEST" && card.highlight, "the map's first bail sets its best");
    card = score_card(view, standing(99999, view.tally.score));
    check(card.badge == "BEST 99,999" && !card.highlight, "a smaller one shows the best");
    check(score_card(view, {}).badge.empty(), "an unknown best shows none");
    tracker.step(t0 + 1500, standing_up());
    check(near(score_card(tracker.view(t0 + 1500 + linger_ms / 2), {}).opacity, 1.0f), "the card stays once the skater gets up");
    check(near(score_card(tracker.view(t0 + 1500 + linger_ms + fade_ms / 2), {}).opacity, 0.5f), "then fades");

    // A big one: off a roof for 3.5 s, five bones broken at 20 m/s, a slide of 5 m.
    Tracker big;
    auto falling = flying(false);
    falling.velocity = game::Vec3{0, -10, 0};
    auto now = keep(big, t0, 3500, falling);
    for (const auto bone : {Bone::hips, Bone::spine, Bone::left_leg, Bone::right_leg, Bone::left_arm}) {
        auto impact = hit(bone, broken_speed + 1.0f, true);
        impact.velocity = game::Vec3{20, 0, 0};
        big.step(now += 16, impact);
    }
    auto sliding = slide(Bone::spine1, 5.0f);
    sliding.velocity = game::Vec3{5, 0, 0};
    sliding.spin = 3.0f * radians_per_rotation; // tumbling over three times a second
    now = keep(big, now + 16, 1000, sliding);
    card = score_card(big.view(now), {});
    std::vector<std::string> keys;
    for (const auto& row : card.rows) keys.push_back(row.key);
    check(keys == std::vector<std::string>{"time", "hits", "broken", "road_rash", "airtime", "fall", "speed", "rotations"},
        "a big bail shows every stat, in its order");
    check(card.rows[2].value == "5 broken" && card.rows[2].points == 5 * points_per_break, "the broken bones");
    check(card.rows[6].value == "44.7 MPH" && card.rows[6].points == static_cast<int>(20.0f * points_per_speed + 0.5f),
        "the top speed, in miles per hour");
    check(card.rows[7].value == "3.0 rotations", "the rotations");
}

void the_fall_and_the_top_speed_score() {
    // Off a roof on the board: falling at 10 m/s for 2 s, then the impact's wipeout at 20 m/s.
    Tracker tracker;
    auto falling = flying(false);
    falling.velocity = game::Vec3{3, -10, 0};
    auto now = keep(tracker, t0, 2000, falling);
    auto impact = hit(Bone::hips, hit_speed + 2.0f, true);
    impact.velocity = game::Vec3{12, -16, 0};
    tracker.step(now, impact);
    auto rolling = lying();
    rolling.velocity = game::Vec3{0, 2, 4}; // rolling on, bouncing up: it goes no further down
    now = keep(tracker, now + 16, 500, rolling);
    const auto t = tracker.view(now).tally;
    check(std::abs(t.fallen - 19.84f) < 0.01f, "the flight's fall is the bail's");
    check(t.fall_points == static_cast<int>(t.fallen * points_per_metre_fallen + 0.5f), "a metre fallen scores");
    check(near(t.top_speed, 20.0f) && t.speed_points == static_cast<int>(20.0f * points_per_speed + 0.5f), "the top speed scores");
    check(t.score == t.damage + t.broken * points_per_break + t.time_points + t.airtime_points + t.fall_points +
              t.speed_points + t.rotation_points,
        "the Meat is every stat's points");
}

void the_body_turning_over_scores() {
    Tracker tracker;
    auto spinning = hit(Bone::hips, hit_speed + 1.0f, true);
    spinning.spin = -1.0f; // not a speed: nothing
    tracker.step(t0, spinning);
    auto tumbling = moving(4.0f);
    tumbling.spin = 2.0f * radians_per_rotation; // twice a second, any way round
    auto now = keep(tracker, t0 + 16, 1504, tumbling);
    auto t = tracker.view(now).tally;
    check(std::abs(t.rotations - 3.0f) < 0.01f, "three rotations in a second and a half");
    check(t.rotation_points == static_cast<int>(t.rotations * points_per_rotation + 0.5f), "a rotation scores");
    auto lying_turning = moving(0.1f);
    lying_turning.spin = radians_per_rotation; // lying still: the body comes to rest, whatever turns
    now = keep(tracker, now, rest_ms + 32, lying_turning);
    check(tracker.report(now).phase == Phase::down, "the body rests");
    const auto rested = tracker.view(now).tally.rotations;
    now = keep(tracker, now, 1000, tumbling); // getting up
    check(near(tracker.view(now).tally.rotations, rested), "after the rest nothing turns any more");
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
        a_break_pulses_the_screen_edge();
        a_break_slows_the_game_with_its_red_edge();
        a_break_just_before_the_bail_counts_from_the_bail();
        a_flump_starts_no_bail();
        the_board_and_bad_values_are_ignored();
        a_bail_lasts_until_the_skater_gets_up();
        a_rest_ends_the_counting();
        a_fall_from_height_bails_at_its_wipeout();
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
        the_fall_and_the_top_speed_score();
        the_body_turning_over_scores();
        a_bail_sets_a_best_only_by_beating_it();
        the_card_shows_the_bail();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Hall of Meat tests passed.\n";
    return 0;
}
