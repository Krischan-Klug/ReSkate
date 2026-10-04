#include "hall_of_meat.h"
#include "hall_of_meat_model.h"
#include "hall_of_meat_skeleton.h"
#include "no_bail.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/body_impacts.h"
#include "Engine/Game/Skater/body_bones.h"
#include "Engine/Game/UI/game_view.h"
#include "Extension/Profile/local_profile_runtime.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <vector>

namespace dingosdk::hall_of_meat {
namespace {
using namespace addr::body_impacts;
constexpr const char* preference = "HallOfMeat";
constexpr std::string_view best_prefix = "HallOfMeat.Best."; // + the level, lower case
static_assert(body_bone_count == body_bones::count);
static_assert(sizeof(Frame) == body_frame_size);

struct State {
    std::uintptr_t base{};
    std::atomic<bool> ready{}, enabled{true}, unreadable_logged{};
    SRWLOCK lock = SRWLOCK_INIT; // guards the rest; never held across game reads
    Tracker tracker;
    bool summary_pending{};
    Summary summary;
    // The current map's best Meat (known once the client tick named the map), where the last
    // bail that showed stands against it, and whether a new best is still to be saved.
    bool best_known{}, best_unsaved{};
    int best{};
    Standing standing;
    std::string level; // client thread only
    // The ragdoll as the latest physics step left it, while the skeleton shows.
    Ragdoll ragdoll;
    std::uint64_t ragdoll_at{}; // GetTickCount64(); 0 = none
};
State& state() { static auto* value = new State; return *value; }

// Each body bone's peak impact speed in this physics step, read from the rig's contact
// struct (body_impacts.h). Zeros when the struct is not readable.
Peaks read_peaks(std::uintptr_t rig) noexcept {
    Peaks peaks{};
    std::uintptr_t holder{}, contacts{};
    if (!memory::peek(rig + contact_holder_offset, holder) || !memory::peek(holder + contact_struct_offset, contacts))
        return peaks;
    std::array<unsigned char, body_bone_count * bone_record_size> records{};
    if (!memory::peek_bytes(contacts + bone_records_offset, records.data(), records.size())) return peaks;
    for (std::size_t index = 0; index < body_bone_count; ++index) {
        float ordinary{}, feet{};
        std::memcpy(&ordinary, records.data() + index * bone_record_size + bone_peak_offset, sizeof(float));
        std::memcpy(&feet, records.data() + index * bone_record_size + bone_foot_peak_offset, sizeof(float));
        const float peak = std::max(ordinary, feet);
        peaks[index] = std::isfinite(peak) && peak > 0 && peak < 1000 ? peak : 0;
    }
    return peaks;
}

// The ragdoll this physics step left on the rig (body_impacts.h): each body's joint in the
// world, and the tree its bodies hang in. False when it is not readable or not sane.
bool read_ragdoll(std::uintptr_t rig, Ragdoll& ragdoll) noexcept {
    std::array<Frame, body_bone_count> frames{}, offsets{};
    std::array<std::int32_t, body_bone_count> parents{};
    if (!memory::peek(rig + body_frames_offset, frames) || !memory::peek(rig + joint_offsets_offset, offsets) ||
        !memory::peek(rig + body_parents_offset, parents))
        return false;
    for (std::size_t body = 0; body < body_bone_count; ++body) {
        if (parents[body] < -1 || parents[body] >= static_cast<int>(body_bone_count)) return false;
        ragdoll.parents[body] = static_cast<std::int8_t>(parents[body]);
        const auto& offset = offsets[body][frame_position_offset / sizeof(offsets[body][0])];
        ragdoll.joints[body] = place({offset[0], offset[1], offset[2]}, frames[body]);
        for (const float value : ragdoll.joints[body])
            if (!std::isfinite(value) || std::abs(value) > 1e7f) return false;
    }
    return valid_tree(ragdoll.parents);
}

// Physics thread, every step of the local skater (No Bail's skeleton hook): the step's
// impacts and, while the skeleton shows, the ragdoll it left. Both come from this one step
// on this one thread, so the skeleton is the ragdoll the game simulates, never a mix.
void observe_step(const LocalSkeletonStep& skeleton_step) noexcept {
    auto& s = state();
    if (!s.enabled.load(std::memory_order_acquire)) return;
    const auto rig = skeleton_step.skater.rig;
    const auto now = GetTickCount64();
    Step step;
    step.wipeout = skeleton_step.wipeout;
    step.peaks = read_peaks(rig);
    Summary ended;
    AcquireSRWLockExclusive(&s.lock);
    if (s.tracker.step(now, step, &ended)) {
        s.summary = ended;
        s.summary_pending = true;
        s.standing = ended.shown && s.best_known ? standing(s.best, ended.tally.score) : Standing{};
        if (s.standing.new_best) {
            s.best = s.standing.best;
            s.best_unsaved = true;
        }
    }
    const bool showing = s.tracker.visible(now);
    ReleaseSRWLockExclusive(&s.lock);
    if (!showing) return;
    Ragdoll ragdoll;
    if (!read_ragdoll(rig, ragdoll)) {
        if (!s.unreadable_logged.exchange(true))
            logging::write(logging::Level::warning, logging::Channel::skater,
                "Hall of Meat: this skater's ragdoll could not be read; the skeleton stays hidden.");
        return;
    }
    AcquireSRWLockExclusive(&s.lock);
    s.ragdoll = ragdoll;
    s.ragdoll_at = now;
    ReleaseSRWLockExclusive(&s.lock);
}

// One line per bail, so the thresholds in hall_of_meat_model.h can be checked against
// real falls.
void log_bail(const Summary& summary) {
    std::vector<std::size_t> order;
    for (std::size_t index = 1; index < body_bones::count; ++index)
        if (summary.peaks[index] > 0) order.push_back(index);
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return summary.peaks[a] > summary.peaks[b]; });
    std::string hits;
    for (const auto index : order) {
        const float peak = summary.peaks[index];
        if (peak < hit_speed && !hits.empty()) break;
        hits += std::format("{}{} {:.1f} m/s{}", hits.empty() ? "" : ", ", body_bones::names[index], peak,
            peak >= broken_speed ? " (broken)" : peak >= hit_speed ? " (hit)" : " (hardest, not hurt)");
    }
    const auto& tally = summary.tally;
    logging::log(logging::Level::info, logging::Channel::skater,
        "Hall of Meat: bail over after {:.1f} s, {} Meat ({} damage from {} impacts, {} broken): {}",
        static_cast<double>(summary.duration_ms) / 1000.0, tally.score, tally.damage, tally.impacts, tally.broken,
        hits.empty() ? std::string("no body contact") : hits);
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

bool start(std::uintptr_t base) noexcept {
    auto& s = state();
    if (s.ready.load(std::memory_order_acquire)) return s.base == base;
    for (const auto& contract : {contacts_before_skeleton_contract, contact_holder_contract, contact_struct_contract,
             bone_record_reset_contract, bone_peak_contract, body_frames_contract, joint_offsets_contract,
             body_parents_contract}) {
        std::array<unsigned char, 32> actual{};
        if (!memory::peek(base + contract.rva, actual) || actual != contract.bytes) {
            logging::write(logging::Level::warning, logging::Channel::skater,
                "Hall of Meat is unavailable: the native body impact contract did not match.");
            return false;
        }
    }
    if (!no_bail_available()) {
        logging::write(logging::Level::warning, logging::Channel::skater,
            "Hall of Meat is unavailable: it observes the skater through No Bail's hooks, which did not start.");
        return false;
    }
    s.base = base;
    s.enabled.store(profile_runtime::local_preference(preference).value_or(true), std::memory_order_release);
    set_local_skeleton_observer(&observe_step);
    s.ready.store(true, std::memory_order_release);
    logging::log(logging::Level::info, logging::Channel::skater, "Hall of Meat ready ({}).",
        s.enabled.load() ? "on" : "off");
    return true;
}

void on_client_tick(std::string_view level) noexcept {
    auto& s = state();
    if (!s.ready.load(std::memory_order_acquire)) return;
    try {
        if (level != s.level) {
            s.level = level;
            const int best = level.empty() ? 0 : saved_best(level);
            AcquireSRWLockExclusive(&s.lock);
            s.best_known = !level.empty();
            s.best = best;
            s.best_unsaved = false;
            s.standing = {};
            ReleaseSRWLockExclusive(&s.lock);
        }
        Summary summary;
        bool finished{}, unsaved{};
        int best{};
        AcquireSRWLockExclusive(&s.lock);
        std::swap(finished, s.summary_pending);
        if (finished) summary = s.summary;
        std::swap(unsaved, s.best_unsaved);
        best = s.best;
        ReleaseSRWLockExclusive(&s.lock);
        if (finished) log_bail(summary);
        if (unsaved && !s.level.empty()) profile_runtime::set_local_values({{best_key(s.level), static_cast<double>(best)}});
    } catch (...) { /* A lost log line or best is never worth the client tick. */ }
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
        s.ragdoll_at = 0;
        ReleaseSRWLockExclusive(&s.lock);
    }
    profile_runtime::set_local_preference(preference, enabled);
}

overlay::MeatFrame frame() {
    auto& s = state();
    if (!enabled()) return {};
    const auto now = GetTickCount64();
    View view;
    Standing standing;
    Ragdoll ragdoll;
    std::uint64_t ragdoll_at{};
    AcquireSRWLockExclusive(&s.lock);
    view = s.tracker.view(now);
    standing = s.standing;
    ragdoll = s.ragdoll;
    ragdoll_at = s.ragdoll_at;
    ReleaseSRWLockExclusive(&s.lock);

    overlay::MeatFrame result;
    auto& tally = result.tally;
    tally.live = view.bailing;
    tally.card = view.card;
    if (tally.live || tally.card > 0) {
        tally.score = view.tally.score;
        tally.damage = view.tally.damage;
        tally.impacts = view.tally.impacts;
        tally.broken = view.tally.broken;
        if (!tally.live) {
            tally.best = standing.best;
            tally.new_best = standing.new_best;
        }
    }
    // Physics steps stop on a level change, in a pause, or once the skater is gone.
    if (!view.visible || !ragdoll_at || now - ragdoll_at > 250) return result;
    auto camera = latest_game_view();
    if (!camera) return result;
    read_live_game_view(s.base, *camera);
    auto& skeleton = result.skeleton;
    skeleton.camera = camera->world;
    skeleton.vertical_fov = camera->vertical_fov;
    skeleton.alpha = view.alpha;
    Skull skull;
    for (const auto& bone : bones(ragdoll, skull))
        skeleton.bones.push_back({bone.from, bone.to, to_overlay(view.injuries[bone.body]), view.flashes[bone.body]});
    const auto head = body_bones::index(body_bones::Bone::neck1);
    skeleton.skull = {skull.centre, skull.radius, to_overlay(view.injuries[head]), view.flashes[head]};
    return result;
}
}
