#include "hall_of_meat.h"
#include "hall_of_meat_model.h"
#include "local_skater.h"
#include "local_skater_body.h"
#include "local_skater_state.h"
#include "skeleton_mesh.h"
#include "Engine/Core/Log/logging.h"
#include "Extension/Rendering/local_skater_render.h"
#include "Extension/Profile/local_profile_runtime.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <string>
#include <vector>

namespace dingosdk::hall_of_meat {
namespace {
constexpr const char* preference = "HallOfMeat";
constexpr std::string_view best_prefix = "HallOfMeat.Best."; // + the level, lower case

struct State {
    std::atomic<bool> ready{}, enabled{true};
    SRWLOCK lock = SRWLOCK_INIT; // guards the rest; never held across game reads
    Tracker tracker;
    bool summary_pending{};
    Summary summary;
    // The current map's best Meat (known once the client tick named the map), where the last
    // finished bail that showed stands against it, and whether a new best is still to be saved.
    bool best_known{}, best_unsaved{};
    int best{};
    Standing standing;
    std::string level; // client thread only
};
State& state() { static auto* value = new State; return *value; }

// A bail is over (under the lock): it is logged on the client tick, and stands against the map's best.
void finished(State& s, const Summary& ended) noexcept {
    s.summary = ended;
    s.summary_pending = true;
    s.standing = ended.shown && s.best_known ? standing(s.best, ended.tally.score) : Standing{};
    if (s.standing.new_best) {
        s.best = s.standing.best;
        s.best_unsaved = true;
    }
}

// Physics thread, every step of the local skater (local_skater_body.h): what each body touched
// in the step and whether it is a ragdoll, and so where the bail is and what it did.
void observe_step(const skater_body::Step& body_step) noexcept {
    auto& s = state();
    if (!s.enabled.load(std::memory_order_acquire)) return;
    const auto now = GetTickCount64();
    Step step;
    step.wipeout = body_step.wipeout;
    skater_state::SkaterState skater;
    if (skater_state::read(body_step.skater, skater)) {
        step.airborne = skater_state::airborne(skater);
        if (skater_state::mode_known(skater)) step.ragdoll = skater_state::mode(skater) == skater_state::Mode::ragdoll;
    }
    skater_body::read_contacts(body_step.skater, step.body);
    Summary ended;
    AcquireSRWLockExclusive(&s.lock);
    if (s.tracker.step(now, step, &ended)) finished(s, ended);
    ReleaseSRWLockExclusive(&s.lock);
}

// One line per bail, so the thresholds in hall_of_meat_model.h can be checked against
// real falls.
void log_bail(const Summary& summary) {
    std::vector<std::size_t> order;
    for (std::size_t index = 1; index < skater_body::count; ++index)
        if (summary.peaks[index] > 0) order.push_back(index);
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return summary.peaks[a] > summary.peaks[b]; });
    std::string hits;
    for (const auto index : order) {
        const float peak = summary.peaks[index];
        if (peak < hit_speed && !hits.empty()) break;
        hits += std::format("{}{} {:.1f} m/s{}", hits.empty() ? "" : ", ", skater_body::names[index], peak,
            peak >= broken_speed ? " (broken)" : peak >= hit_speed ? " (hit)" : " (hardest, not hurt)");
    }
    std::string scrapes;
    for (std::size_t index = 1; index < skater_body::count; ++index)
        if (summary.scraped[index] >= 0.05f)
            scrapes += std::format("{}{} {:.1f} m", scrapes.empty() ? "" : ", ", skater_body::names[index], summary.scraped[index]);
    const auto& tally = summary.tally;
    logging::log(logging::Level::info, logging::Channel::skater,
        "Hall of Meat: bail over after {:.1f} s, {} Meat ({} damage: {} from {} impacts, head +{}, vehicle +{}, "
        "road rash {:.1f} m +{}; {} broken; {:.1f} s airtime). Hits: {}. Road rash: {}",
        static_cast<double>(summary.duration_ms) / 1000.0, tally.score, tally.damage, tally.hit_points, tally.impacts,
        tally.head_bonus, tally.vehicle_bonus, static_cast<double>(tally.scraped), tally.scrape_points, tally.broken,
        static_cast<double>(tally.airtime), hits.empty() ? std::string("no body contact") : hits,
        scrapes.empty() ? std::string("none") : scrapes);
}

std::string best_key(std::string_view level) {
    std::string key(best_prefix);
    for (const char c : level) key += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    return key;
}
int saved_best(std::string_view level) noexcept {
    const auto value = profile_runtime::local_value(best_key(level));
    if (!value || !value->is_number()) return 0;
    const auto best = value->get<double>();
    return std::isfinite(best) && best > 0 && best < 1e9 ? static_cast<int>(best) : 0;
}

overlay::MeatInjury to_overlay(Injury injury) {
    return injury == Injury::broken ? overlay::MeatInjury::broken
         : injury == Injury::hit    ? overlay::MeatInjury::hit
                                    : overlay::MeatInjury::none;
}
}

bool start() noexcept {
    auto& s = state();
    if (s.ready.load(std::memory_order_acquire)) return true;
    if (!skater_body::available()) {
        logging::write(logging::Level::warning, logging::Channel::skater,
            "Hall of Meat is unavailable: it needs the skater body, which did not start.");
        return false;
    }
    s.enabled.store(profile_runtime::local_preference(preference).value_or(true), std::memory_order_release);
    skater_skeleton::prepare();
    skater_body::add_step_observer(&observe_step);
    s.ready.store(true, std::memory_order_release);
    logging::log(logging::Level::info, logging::Channel::skater, "Hall of Meat ready ({}).",
        s.enabled.load() ? "on" : "off");
    return true;
}

void on_client_tick() noexcept {
    auto& s = state();
    if (!s.ready.load(std::memory_order_acquire)) return;
    try {
        LocalSkater skater;
        const bool gone = !current_local_skater(skater);
        const auto now = GetTickCount64();
        Summary summary, ended;
        bool pending{}, unsaved{};
        int best{};
        AcquireSRWLockExclusive(&s.lock);
        if (gone && s.tracker.lose(now, &ended)) finished(s, ended);
        std::swap(pending, s.summary_pending);
        if (pending) summary = s.summary;
        std::swap(unsaved, s.best_unsaved);
        best = s.best;
        ReleaseSRWLockExclusive(&s.lock);
        if (pending) log_bail(summary);
        if (unsaved && !s.level.empty()) profile_runtime::set_local_values({{best_key(s.level), static_cast<double>(best)}});
    } catch (...) { /* A lost log line or best is never worth the client tick. */ }
}

void set_level(std::string_view level) noexcept {
    auto& s = state();
    if (!s.ready.load(std::memory_order_acquire) || level == s.level) return;
    try {
        s.level = level;
        const int best = level.empty() ? 0 : saved_best(level);
        AcquireSRWLockExclusive(&s.lock);
        s.best_known = !level.empty();
        s.best = best;
        s.best_unsaved = false;
        s.standing = {};
        ReleaseSRWLockExclusive(&s.lock);
    } catch (...) { /* The map's best stays unknown: no card shows one. */ }
}

bool enabled() noexcept {
    const auto& s = state();
    return s.ready.load(std::memory_order_acquire) && s.enabled.load(std::memory_order_acquire);
}

void set_enabled(bool enabled) noexcept {
    auto& s = state();
    s.enabled.store(enabled, std::memory_order_release);
    if (!enabled) {
        AcquireSRWLockExclusive(&s.lock);
        s.tracker.reset();
        s.summary_pending = false;
        ReleaseSRWLockExclusive(&s.lock);
    }
    profile_runtime::set_local_preference(preference, enabled);
}

Report report() noexcept {
    auto& s = state();
    if (!s.ready.load(std::memory_order_acquire)) return {};
    AcquireSRWLockShared(&s.lock);
    const auto result = s.tracker.report(GetTickCount64());
    ReleaseSRWLockShared(&s.lock);
    return result;
}

overlay::MeatFrame frame() {
    auto& s = state();
    if (!enabled()) return {};
    const auto now = GetTickCount64();
    View view;
    Standing against; // the card's: the bail against the map's best before it
    AcquireSRWLockExclusive(&s.lock);
    view = s.tracker.view(now);
    if (view.phase == Phase::down) against = s.best_known ? standing(s.best, view.tally.score) : Standing{};
    else if (view.phase == Phase::getting_up) against = s.standing; // the best already counts this bail
    ReleaseSRWLockExclusive(&s.lock);

    overlay::MeatFrame result;
    auto& tally = result.tally;
    tally.live = view.phase == Phase::falling;
    tally.card = view.card;
    if (tally.live || tally.card > 0) {
        tally.score = view.tally.score;
        tally.damage = view.tally.damage;
        tally.impacts = view.tally.impacts;
        tally.broken = view.tally.broken;
        tally.road_rash = view.tally.scraped;
        tally.airtime = view.tally.airtime;
        if (!tally.live) {
            tally.best = against.best;
            tally.new_best = against.new_best;
        }
    }
    // The skeleton as the renderer drew the skater in the latest picture, seen by its camera.
    const auto mesh = skater_skeleton::mesh();
    skater_render::Picture picture;
    skater_skeleton::Posed posed;
    if (!mesh || view.phase == Phase::riding || !skater_render::latest(picture) || !skater_skeleton::pose(*mesh, picture.skin, posed))
        return result;
    auto& skeleton = result.skeleton;
    skeleton.frame.camera = picture.camera;
    skeleton.frame.vertical_fov = picture.vertical_fov;
    skeleton.frame.positions = std::move(posed.positions);
    skeleton.frame.normals = std::move(posed.normals);
    skeleton.frame.triangles = {mesh, &mesh->triangles}; // the mesh's own, shared
    skeleton.frame.parts = {mesh, &mesh->parts};
    skeleton.alpha = view.alpha;
    for (std::size_t body = 0; body < skater_body::count; ++body) {
        skeleton.injuries[body] = to_overlay(view.injuries[body]);
        skeleton.flashes[body] = view.flashes[body];
    }
    return result;
}
}
