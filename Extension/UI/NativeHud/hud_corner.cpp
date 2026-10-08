#include "hud_corner.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Abi/native_data.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/engine.h"
#include "Engine/Game/Build/20260929/hud_corner.h"
#include "Extension/UI/NativeMenu/native_menu_data.h"
#include "Extension/UI/NativeUi/model_takeover.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>

namespace dingosdk::hud_corner {
namespace {
namespace corner = addr::hud_corner;
namespace menu_data = multiplayer::menu_data;
using Address = std::uintptr_t;
using Clock = std::chrono::steady_clock;
using native_ui::FieldValue;

// Our stack item: counted beside the d-pad (above its priority, within what the stack counts).
constexpr std::uint32_t cover_key = 0x52534843; // "RSHC": apart from the game's own keys (0x8000....)
constexpr std::uint64_t cover_id = 0x5253484300000000ULL;
constexpr std::int32_t cover_priority = corner::dpad_priority + 1;
static_assert(cover_priority < corner::score_priority);

// The score HUD's fields that hold it down, their sizes and the values that do.
struct Switch {
    std::uint32_t field;
    std::size_t size;
    FieldValue down;
};
constexpr std::array<Switch, 2> score_switches{{
    {corner::hud_widget_active, 1, 0},
    {corner::extra_info_style, 4, static_cast<std::uint32_t>(corner::extra_info_none)},
}};
constexpr std::size_t hud_switch = 0, style_switch = 1;

struct Shared {
    std::atomic<Address> base{};
    std::mutex mutex; // guards claims
    std::vector<Claim> claims;
    // Client thread only.
    std::unique_ptr<std::array<std::unique_ptr<native_ui::TakenField>, score_switches.size()>> score; // once started
    Address manager{};
    menu_data::Value stack;       // the d-pad's stack, once found
    menu_data::Value cover;       // our stack item, while a feature covers the corner
    menu_data::Value score_model; // ScoringHUDViewModel, once found
    std::array<std::uint16_t, score_switches.size()> score_offsets{};
    Clock::time_point next_stack_scan, next_score_scan;
    bool warned{}, slip_logged{};
};
Shared& shared() {
    static auto* value = new Shared;
    return *value;
}

// Runs `use(context)` on the game's UI model registry under its model lock, which the game's
// writers take too. False while there is no registry.
template <class Use> bool with_ui_models(Address base, Use&& use) {
    if (!base) return false;
    const auto ui = menu_data::read<Address>(base + addr::engine::ui_manager);
    const auto manager = ui ? menu_data::read<Address>(ui + 0x140) : 0;
    if (!manager) return false;
    const menu_data::Context context(base, manager);
    game::ModelWriteLock lock(manager);
    use(context);
    return true;
}

Cover wanted(Shared& s) {
    std::lock_guard lock(s.mutex);
    auto result = Cover::none;
    for (const auto& claim : s.claims) result = std::max(result, claim.cover);
    return result;
}
bool score_held(const Shared& s) {
    return s.score && std::any_of(s.score->begin(), s.score->end(), [](const auto& field) { return field->taken(); });
}

template <class T> T at(const menu_data::Context& context, const menu_data::Value& value) {
    return menu_data::read<T>(context.address(value));
}
std::string asset_name(Address asset) { return asset ? menu_data::string(menu_data::read<Address>(asset + 0x18), 256) : std::string{}; }
std::string widget_of(const menu_data::Context& context, const menu_data::Value& item) {
    return asset_name(at<Address>(context, context.path(item, {corner::item_content, corner::content_blueprint})));
}
menu_data::Value items_of(const menu_data::Context& context, const menu_data::Value& stack) {
    return context.field(stack, corner::stack_items);
}
// Whether `next` says it is time to look again (at most once a second while something is not found).
bool time_to_look(Clock::time_point& next) {
    const auto now = Clock::now();
    if (now < next) return false;
    next = now + std::chrono::seconds(1);
    return true;
}

// A new registry holds the game's own state: our item and what was taken over went with the old one.
void follow_registry(Shared& s, const menu_data::Context& context) {
    if (s.manager == context.manager) return;
    s.manager = context.manager;
    s.stack = {};
    s.cover = {};
    s.score_model = {};
    if (s.score)
        for (auto& field : *s.score) field->bind(0);
}

// The stack the d-pad is in.
bool find_stack(Shared& s, const menu_data::Context& context) {
    if (s.stack.handle && context.type_of(s.stack.handle) == s.stack.type) return true;
    s.stack = {};
    if (!time_to_look(s.next_stack_scan)) return false;
    for (const auto& root : context.roots({corner::hud_view_schema})) {
        const auto stack = context.field(root.model, corner::dpad_scoring_stack);
        const auto items = items_of(context, stack);
        unsigned count{}, stride{};
        (void)context.array(items, 16, count, stride);
        for (unsigned index = 0; index < count; ++index)
            if (widget_of(context, context.element(items, index)) == corner::dpad_widget) {
                s.stack = stack;
                return true;
            }
    }
    return false;
}

// Puts our item into the d-pad's stack, or takes it out; the game's items stay as they are. Our
// item: empty content, our key, the stack's transition as a push gives it.
void cover_dpad(Shared& s, const menu_data::Context& context, bool covered) {
    const auto field = items_of(context, s.stack);
    unsigned count{}, stride{};
    auto items = context.array(field, 16, count, stride);
    menu_data::require(!count || stride == menu_data::stack_item.size, "the stack's items differ");
    const auto key_at = context.member(context.type(menu_data::stack_item), corner::item_key).offset;
    unsigned ours = count;
    for (unsigned index = 0; index < count; ++index) {
        std::uint32_t key{};
        std::memcpy(&key, items.data() + std::size_t{index} * stride + key_at, sizeof key);
        if (key == cover_key) ours = index;
    }
    items.resize(std::size_t{count} * menu_data::stack_item.size);
    if (covered && ours == count) {
        if (!s.cover.handle) {
            s.cover = context.create(menu_data::stack_item, cover_id);
            context.set(context.field(s.cover, corner::item_key), cover_key);
            context.set(context.field(s.cover, corner::item_priority), cover_priority);
            context.copy(context.field(s.cover, corner::item_transition),
                         context.address(context.field(s.stack, corner::default_transition)));
        }
        std::vector<std::byte> bytes(menu_data::stack_item.size);
        menu_data::require(memory::read_bytes(context.address(s.cover), bytes.data(), bytes.size()), "our stack item is unavailable");
        items.insert(items.end(), bytes.begin(), bytes.end());
        context.array(field, items, count + 1);
    } else if (!covered && ours < count) {
        const auto begin = items.begin() + static_cast<std::ptrdiff_t>(std::size_t{ours} * stride);
        items.erase(begin, begin + stride);
        context.array(field, items, count - 1);
    }
    if (!covered && s.cover.handle) {
        context.destroy(s.cover);
        s.cover = {};
    }
}

// ScoringHUDViewModel, its switches bound to the model's fields.
bool find_score(Shared& s, const menu_data::Context& context) {
    if (s.score_model.handle && context.type_of(s.score_model.handle) == s.score_model.type) return true;
    s.score_model = {};
    for (auto& field : *s.score) field->bind(0);
    if (!time_to_look(s.next_score_scan)) return false;
    const auto roots = context.roots({corner::score_view_schema});
    if (roots.empty()) return false;
    const auto model = roots.front().model;
    for (std::size_t index = 0; index < score_switches.size(); ++index) {
        const auto member = context.member(model.type, score_switches[index].field);
        menu_data::require(menu_data::size(member.type) == score_switches[index].size, "a score HUD field's size differs");
        s.score_offsets[index] = member.offset;
        (*s.score)[index]->bind(context.field(model, score_switches[index].field).handle);
    }
    s.score_model = model;
    return true;
}
FieldValue score_value(const Shared& s, const menu_data::Context& context, std::size_t index) {
    FieldValue now{};
    menu_data::require(memory::peek_bytes(context.address(s.score_model) + s.score_offsets[index], &now, score_switches[index].size),
                       "a score HUD field is unavailable");
    return now;
}

// Holds the score HUD down (each switch taken over at its value that does), or gives it back.
void hold_score(Shared& s, const menu_data::Context& context, bool held) {
    for (std::size_t index = 0; index < score_switches.size(); ++index) {
        const auto& sw = score_switches[index];
        auto& field = *(*s.score)[index];
        const auto now = score_value(s, context, index);
        if (held) {
            if (field.taken() && now != sw.down && !std::exchange(s.slip_logged, true))
                logging::write(logging::Level::warning, logging::Channel::ui,
                    "HUD corner: the game wrote the score HUD past the model write hook.");
            field.take(sw.down, now);
            if (now != sw.down) context.publish(context.field(s.score_model, sw.field), &sw.down);
        } else if (field.taken()) {
            const auto meant = field.give_back(); // what the game wrote while it was held
            if (now != meant) context.publish(context.field(s.score_model, sw.field), &meant);
        }
    }
}
} // namespace

std::string_view name(Cover cover) noexcept {
    switch (cover) {
    case Cover::none: return "none";
    case Cover::dpad: return "dpad";
    case Cover::all: return "all";
    }
    return "?";
}

void start(std::uintptr_t base) noexcept {
    auto& s = shared();
    s.base.store(base, std::memory_order_release);
    try {
        if (!native_ui::start_model_takeover(base)) return;
        auto fields = std::make_unique<std::array<std::unique_ptr<native_ui::TakenField>, score_switches.size()>>();
        for (std::size_t index = 0; index < score_switches.size(); ++index)
            (*fields)[index] = std::make_unique<native_ui::TakenField>(score_switches[index].size);
        s.score = std::move(fields);
    } catch (...) { /* The d-pad can still be covered; the score HUD is not. */ }
}

void set_cover(std::string_view owner, Cover cover) {
    auto& s = shared();
    std::lock_guard lock(s.mutex);
    const auto found = std::find_if(s.claims.begin(), s.claims.end(), [&](const Claim& c) { return c.owner == owner; });
    if (cover == Cover::none) {
        if (found != s.claims.end()) s.claims.erase(found);
    } else if (found != s.claims.end()) {
        found->cover = cover;
    } else {
        s.claims.push_back({std::string(owner), cover});
    }
}

std::vector<Claim> claims() {
    auto& s = shared();
    std::lock_guard lock(s.mutex);
    return s.claims;
}

State state() noexcept {
    auto& s = shared();
    State result;
    try {
        with_ui_models(s.base.load(std::memory_order_acquire), [&](const menu_data::Context& context) {
            follow_registry(s, context);
            if (find_stack(s, context)) {
                result.stack_found = true;
                result.target_index = at<std::int32_t>(context, context.field(s.stack, corner::target_index));
                result.stack_active = at<std::uint8_t>(context, context.field(s.stack, corner::is_active)) != 0;
                const auto items = items_of(context, s.stack);
                unsigned count{}, stride{};
                (void)context.array(items, 16, count, stride);
                for (unsigned index = 0; index < count; ++index) {
                    const auto item = context.element(items, index);
                    result.items.push_back({widget_of(context, item), at<std::int32_t>(context, context.field(item, corner::item_priority)),
                        at<std::uint8_t>(context, context.field(item, corner::is_active)) != 0,
                        at<std::uint32_t>(context, context.field(item, corner::item_key)) == cover_key});
                }
            }
            if (s.score && find_score(s, context)) {
                result.score_found = true;
                result.score_shown = score_value(s, context, hud_switch) != 0;
                result.extra_info_style = static_cast<int>(static_cast<std::int32_t>(score_value(s, context, style_switch)));
                result.score_held = score_held(s);
            }
        });
    } catch (...) {
        s.stack = {};
        s.score_model = {};
    }
    return result;
}

void on_client_tick() noexcept {
    auto& s = shared();
    try {
        const auto cover = wanted(s);
        if (cover == Cover::none && !s.cover.handle && !score_held(s)) return; // the game's own corner, untouched
        with_ui_models(s.base.load(std::memory_order_acquire), [&](const menu_data::Context& context) {
            follow_registry(s, context);
            if (find_stack(s, context)) cover_dpad(s, context, cover != Cover::none);
            if (s.score && (cover == Cover::all || score_held(s)) && find_score(s, context)) hold_score(s, context, cover == Cover::all);
            s.warned = false;
        });
    } catch (const std::exception& error) {
        s.stack = {};
        s.score_model = {};
        if (!std::exchange(s.warned, true))
            logging::log(logging::Level::warning, logging::Channel::ui, "HUD corner: cannot cover it: {}.", error.what());
    } catch (...) {
        s.stack = {};
        s.score_model = {};
    }
}
}
