#include "skater_body_debug.h"
#include "local_skater_body.h"
#include <Windows.h>
#include <atomic>
#include <format>
#include <mutex>
#include <string>
#include <utility>

namespace dingosdk::skater_body {
namespace {
using debug_panel::Field;
using debug_panel::yes_no;
// The physics steps are held for the panel only while it samples them: a sample keeps them
// coming this long.
constexpr std::uint64_t lease_ms = 500;

// Every physics step since the panel's last sample, held as one (skater_body.h hold): a hit
// that lasts one step still shows, at its peak.
struct Steps {
    std::atomic<std::uint64_t> sampled_at{}; // GetTickCount64() of the last sample
    std::mutex lock;
    Contacts held;
    std::uint64_t held_at{}; // the latest step held; 0 = none since the last sample
    Contacts last;           // the last sample's, client thread only
    std::uint64_t last_at{}; // its latest step
};
Steps& steps() { static auto* value = new Steps; return *value; }

// Physics thread, in the step, while the panel samples.
void observe(const Step& step) noexcept {
    auto& s = steps();
    if (GetTickCount64() - s.sampled_at.load(std::memory_order_acquire) > lease_ms) return;
    Contacts contacts;
    if (!read_contacts(step.skater, contacts)) return;
    std::lock_guard guard(s.lock);
    hold(s.held, contacts);
    s.held_at = GetTickCount64();
}

// The steps held since the last sample. A sample between two steps repeats the last one; once
// the steps stop (the game paused, no skater) it reads the contacts as they are.
bool take(Contacts& contacts) {
    auto& s = steps();
    const auto now = GetTickCount64();
    s.sampled_at.store(now, std::memory_order_release);
    add_step_observer(&observe); // once; it stays, idle while the panel does not sample
    {
        std::lock_guard guard(s.lock);
        if (s.held_at) {
            s.last = std::exchange(s.held, {});
            s.last_at = std::exchange(s.held_at, 0);
        }
    }
    if (s.last_at && now - s.last_at <= lease_ms) {
        contacts = s.last;
        return true;
    }
    LocalSkater skater;
    return current_local_skater(skater) && read_contacts(skater, contacts);
}

// What the body hit in the steps, of every kind: "world, vehicle", or "none".
std::string kinds(const Contacts& contacts) {
    HitKinds any;
    for (std::size_t body = 1; body < count; ++body) {
        const auto& hit = contacts.bodies[body].hit;
        any = {any.board || hit.board, any.vehicle || hit.vehicle, any.world || hit.world,
            any.kind_5 || hit.kind_5, any.kind_11 || hit.kind_11};
    }
    std::string result;
    const auto add = [&](bool set, std::string_view name) {
        if (set) result += (result.empty() ? "" : ", ") + std::string(name);
    };
    add(any.world, "world");
    add(any.vehicle, "vehicle");
    add(any.board, "board");
    add(any.kind_5, "kind 5");
    add(any.kind_11, "kind 11");
    return result.empty() ? "none" : result;
}

std::vector<Field> sample() {
    Contacts contacts;
    const bool known = take(contacts);
    const auto value = [](bool readable, std::string text) { return readable ? std::move(text) : std::string("-"); };
    int touching{};
    for (std::size_t body = 1; body < count; ++body) touching += contacts.bodies[body].touching ? 1 : 0;
    const std::size_t body = hardest(contacts);
    const bool hit = known && body > 0;
    const auto& b = contacts.bodies[body];
    std::vector<Field> fields;
    fields.push_back({"CONTACT", {}, true});
    fields.push_back({"Touching", value(known, touching ? std::format("{} bones", touching) : "none")});
    fields.push_back({"Sensitive", value(known, yes_no(contacts.sensitive))});
    fields.push_back({"Feet on board", value(known, yes_no(contacts.feet_on_board))});
    fields.push_back({"Hit", value(known, kinds(contacts))});
    fields.push_back({"HARDEST HIT", {}, true});
    fields.push_back({"Bone", value(hit, names[body])});
    fields.push_back({"Impact", value(hit, std::format("{:.0f} m/s", b.impact))});
    fields.push_back({"Speed", value(hit, std::format("{:.1f} m/s", game::length(b.velocity))), false, true});
    fields.push_back({"Slide", value(hit, std::format("{:.1f} m/s", game::length(b.slide))), false, true});
    return fields;
}
}

debug_panel::Source debug_source() { return {"skaterbody", "Skater body", &sample}; }
}
