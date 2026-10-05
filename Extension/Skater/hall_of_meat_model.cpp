#include "hall_of_meat_model.h"
#include <algorithm>
#include <cmath>

namespace dingosdk::hall_of_meat {
namespace {
// The physics thread steps and the render thread draws, each reading the clock itself:
// a moment ago may be a moment ahead.
std::uint64_t elapsed(std::uint64_t now, std::uint64_t since) noexcept { return now > since ? now - since : 0; }
// 1 until the last fade_ms of `duration`, then down to 0 at its end.
float fading(std::uint64_t since, std::uint64_t duration) noexcept {
    if (since >= duration) return 0.0f;
    return std::min(1.0f, static_cast<float>(duration - since) / static_cast<float>(fade_ms));
}
}

bool Tracker::step(std::uint64_t now, const Step& step, Summary* ended) noexcept {
    const auto flown = step.airborne && stepped_ ? std::min(elapsed(now, stepped_), longest_step_ms) : 0;
    stepped_ = now;
    if (step.airborne) landed_ = 0;
    else if (!landed_) landed_ = now;
    if (!bailing_) {
        if (step.airborne) flight_ms_ += flown;
        else if (elapsed(now, landed_) > wipeout_after_impact_ms) flight_ms_ = 0; // landed, and stayed up
        if (!step.wipeout) return false;
        const auto flight = flight_ms_;
        *this = {};
        bailing_ = true;
        started_ = tumbled_ = stepped_ = now;
        airtime_ms_ = flight; // this step's share is in it
    } else {
        airtime_ms_ += flown;
    }
    if (step.wipeout) tumbled_ = now;
    for (std::size_t index = 1; index < body_bones::count; ++index) { // 0 is the board
        const float peak = step.peaks[index];
        if (!std::isfinite(peak) || peak <= 0) continue;
        auto& bone = bones_[index];
        bone.peak = std::max(bone.peak, peak);
        if (peak >= tumbling_speed && !body_bones::foot(static_cast<body_bones::Bone>(index))) tumbled_ = now;
        if (peak < hit_speed) continue;
        hurt_ = true;
        if (bone.hit_at && elapsed(now, bone.hit_at) <= impact_gap_ms) {
            auto& impact = impacts_[bone.impact];
            impact.speed = std::max(impact.speed, peak);
        } else if (impact_count_ < max_impacts) {
            bone.impact = impact_count_;
            impacts_[impact_count_++] = {index, peak};
        }
        bone.hit_at = now;
    }
    if (elapsed(now, tumbled_) < settle_ms && elapsed(now, started_) < longest_bail_ms) return false;
    bailing_ = false;
    ended_ = now;
    if (ended) {
        ended->duration_ms = elapsed(now, started_);
        ended->shown = hurt_;
        for (std::size_t index = 0; index < body_bones::count; ++index) ended->peaks[index] = bones_[index].peak;
        ended->tally = tally();
    }
    return true;
}

Tally Tracker::tally() const noexcept {
    Tally result;
    for (std::size_t index = 0; index < impact_count_; ++index) result.damage += impact_points(impacts_[index].speed);
    result.impacts = static_cast<int>(impact_count_);
    for (const auto& bone : bones_)
        if (injury(bone.peak) == Injury::broken) ++result.broken;
    result.airtime = static_cast<float>(airtime_ms_) / 1000.0f;
    result.score = result.damage + result.broken * points_per_break;
    return result;
}

bool Tracker::visible(std::uint64_t now) const noexcept {
    return hurt_ && (bailing_ || elapsed(now, ended_) < linger_ms);
}

View Tracker::view(std::uint64_t now) const noexcept {
    View view;
    if (!visible(now)) return view;
    view.visible = true;
    view.bailing = bailing_;
    view.alpha = bailing_ ? 1.0f : fading(elapsed(now, ended_), linger_ms);
    view.card = bailing_ ? 0.0f : view.alpha;
    view.tally = tally();
    for (std::size_t index = 0; index < body_bones::count; ++index) {
        const auto& bone = bones_[index];
        view.injuries[index] = injury(bone.peak);
        const auto since_hit = elapsed(now, bone.hit_at);
        if (bone.hit_at && since_hit < flash_ms)
            view.flashes[index] = 1.0f - static_cast<float>(since_hit) / static_cast<float>(flash_ms);
    }
    return view;
}
}
