#include "hall_of_meat_model.h"
#include <algorithm>
#include <cmath>

namespace dingosdk::hall_of_meat {
namespace {
// The physics thread steps and the render thread draws, each reading the clock itself:
// a moment ago may be a moment ahead.
std::uint64_t elapsed(std::uint64_t now, std::uint64_t since) noexcept { return now > since ? now - since : 0; }
float speed_of(float value) noexcept { return std::isfinite(value) && value > 0 ? value : 0.0f; }
bool is_foot(std::size_t bone) noexcept { return skater_body::foot(static_cast<Bone>(bone)); }
// Sliding along anything but the board scrapes: the feet ride on it all the time.
bool scrapes(const skater_body::HitKinds& hit) noexcept {
    return hit.world || hit.vehicle || hit.kind_5 || hit.kind_11;
}
}

bool Tracker::step(std::uint64_t now, const Step& step, Summary* ended) noexcept {
    const auto step_ms = stepped_ ? std::min(elapsed(now, stepped_), longest_step_ms) : 0;
    const auto flown = step.airborne ? step_ms : 0;
    stepped_ = now;
    if (step.airborne) landed_ = 0;
    else if (!landed_) landed_ = now;
    const bool ragdoll = step.ragdoll.value_or(false);
    if (phase_ == Phase::riding) {
        if (step.airborne) flight_ms_ += flown;
        else if (elapsed(now, landed_) > wipeout_after_impact_ms) flight_ms_ = 0; // landed, and stayed up
        // Each body's hardest recent hit, for a bail that starts a few steps after it.
        for (std::size_t index = 1; index < skater_body::count; ++index) { // 0 is the board
            const auto& contact = step.body.bodies[index];
            const float speed = speed_of(contact.impact);
            auto& lead = bones_[index].lead;
            if (speed <= 0 || (lead.at && elapsed(now, lead.at) <= wipeout_after_impact_ms && speed <= lead.speed)) continue;
            lead = {speed, contact.hit.vehicle, now};
        }
        if (!step.wipeout && !ragdoll) return false;
        begin(now);
    } else {
        airtime_ms_ += flown;
    }
    ragdolled_ = ragdolled_ || ragdoll;

    // Whether the body still tumbles, and whether something hit it hard.
    bool tumbling = step.wipeout || step.airborne, struck = false;
    const float step_seconds = static_cast<float>(step_ms) / 1000.0f;
    float sliding{}; // the scraping bodies' slides, summed
    int scraping{};
    for (std::size_t index = 1; index < skater_body::count; ++index) {
        const auto& contact = step.body.bodies[index];
        const float speed = speed_of(contact.impact);
        if (speed >= tumbling_speed && !is_foot(index)) tumbling = true;
        if (speed > 0 && hit(index, speed, contact.hit.vehicle, now)) struck = true;
        const float slide = game::length(contact.slide);
        if (!contact.touching || is_foot(index) || !scrapes(contact.hit) || !std::isfinite(slide) || slide < scrape_speed)
            continue;
        auto& bone = bones_[index];
        bone.scraped += slide * step_seconds;
        if (bone.scraped >= bruising_scrape) hurt_ = true;
        sliding += slide;
        ++scraping;
    }
    if (scraping) {
        scraped_ += sliding / static_cast<float>(scraping) * step_seconds;
        tumbling = true;
    }
    if (tumbling) tumbled_ = now;

    // The skater stood up, or never went down.
    const bool stood_up = ragdolled_ ? step.ragdoll.has_value() && !*step.ragdoll : elapsed(now, started_) >= ragdoll_wait_ms;
    if (stood_up) return end(now, ended);
    if (phase_ == Phase::falling && elapsed(now, tumbled_) >= settle_ms) phase_ = Phase::down;
    else if (phase_ == Phase::down && (struck || step.airborne)) phase_ = Phase::falling;
    return false;
}

bool Tracker::lose(std::uint64_t now, Summary* ended) noexcept {
    // Nothing from before the skater went carries over to the one that comes back.
    flight_ms_ = landed_ = stepped_ = 0;
    for (auto& bone : bones_) bone.lead = {};
    return phase_ != Phase::riding && end(now, ended);
}

// A bail from `now`, with the flight it came from and the hits just before it.
void Tracker::begin(std::uint64_t now) noexcept {
    const auto flight = flight_ms_;
    const auto before = bones_;
    *this = {};
    phase_ = Phase::falling;
    started_ = tumbled_ = stepped_ = now;
    airtime_ms_ = flight; // this step's share is in it
    for (std::size_t index = 1; index < skater_body::count; ++index) {
        const auto& lead = before[index].lead;
        if (lead.at && elapsed(now, lead.at) <= wipeout_after_impact_ms) hit(index, lead.speed, lead.vehicle, lead.at);
    }
}

// A hit of one bone at `at`: a new impact, or the harder step of the contact it belongs to.
bool Tracker::hit(std::size_t index, float speed, bool vehicle, std::uint64_t at) noexcept {
    auto& bone = bones_[index];
    bone.peak = std::max(bone.peak, speed);
    if (speed < hit_speed) return false;
    hurt_ = true;
    if (bone.hit_at && elapsed(at, bone.hit_at) <= impact_gap_ms) {
        auto& impact = impacts_[bone.impact];
        impact.speed = std::max(impact.speed, speed);
        impact.vehicle = impact.vehicle || vehicle;
        last_impact_ = bone.impact;
    } else if (impact_count_ < max_impacts) {
        bone.impact = last_impact_ = impact_count_;
        impacts_[impact_count_++] = {index, speed, vehicle};
    }
    bone.hit_at = at;
    return true;
}

bool Tracker::end(std::uint64_t now, Summary* ended) noexcept {
    phase_ = Phase::riding;
    ended_ = now;
    if (ended) {
        ended->duration_ms = elapsed(now, started_);
        ended->shown = hurt_;
        for (std::size_t index = 0; index < skater_body::count; ++index) {
            ended->peaks[index] = bones_[index].peak;
            ended->scraped[index] = bones_[index].scraped;
        }
        ended->tally = tally();
    }
    return true;
}

Phase Tracker::phase(std::uint64_t now) const noexcept {
    if (phase_ != Phase::riding) return phase_;
    return ended_ && elapsed(now, ended_) < fade_ms ? Phase::getting_up : Phase::riding;
}

Injury Tracker::injury_of(const BoneState& bone) const noexcept {
    const auto by_hits = injury(bone.peak);
    return by_hits == Injury::none && bone.scraped >= bruising_scrape ? Injury::hit : by_hits;
}

Tally Tracker::tally() const noexcept {
    Tally result;
    for (std::size_t index = 0; index < impact_count_; ++index) {
        const auto& impact = impacts_[index];
        result.hit_points += hit_points(impact.speed);
        result.head_bonus += head_bonus(impact);
        result.vehicle_bonus += vehicle_bonus(impact);
    }
    result.impacts = static_cast<int>(impact_count_);
    for (const auto& bone : bones_)
        if (injury(bone.peak) == Injury::broken) ++result.broken;
    result.scraped = scraped_;
    result.scrape_points = static_cast<int>(result.scraped * points_per_scraped_metre + 0.5f);
    result.damage = result.hit_points + result.head_bonus + result.vehicle_bonus + result.scrape_points;
    result.score = result.damage + result.broken * points_per_break;
    result.airtime = static_cast<float>(airtime_ms_) / 1000.0f;
    return result;
}

View Tracker::view(std::uint64_t now) const noexcept {
    View view;
    const auto phase = this->phase(now);
    if (phase == Phase::riding || !hurt_) return view;
    view.phase = phase;
    view.alpha = phase == Phase::getting_up
        ? 1.0f - static_cast<float>(elapsed(now, ended_)) / static_cast<float>(fade_ms) : 1.0f;
    view.card = phase == Phase::falling ? 0.0f : view.alpha;
    view.tally = tally();
    for (std::size_t index = 0; index < skater_body::count; ++index) {
        const auto& bone = bones_[index];
        view.injuries[index] = injury_of(bone);
        const auto since_hit = elapsed(now, bone.hit_at);
        if (bone.hit_at && since_hit < flash_ms)
            view.flashes[index] = 1.0f - static_cast<float>(since_hit) / static_cast<float>(flash_ms);
    }
    return view;
}

Report Tracker::report(std::uint64_t now) const noexcept {
    Report report;
    report.phase = phase(now);
    report.bail_ms = phase_ != Phase::riding ? elapsed(now, started_) : elapsed(ended_, started_);
    report.tally = tally();
    report.hit = impact_count_ > 0;
    if (report.hit) report.last = impacts_[last_impact_];
    return report;
}
}
