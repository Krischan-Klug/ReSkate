#include "debug_panel.h"
#include "Engine/Core/Log/logging.h"
#include <Windows.h>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

namespace dingosdk::debug_panel {
namespace {
struct State {
    std::shared_mutex lock;
    std::vector<Source> sources;
    std::ptrdiff_t shown = -1; // into sources
    Sheet sheet;
};
State& state() { static auto* value = new State; return *value; }

std::ptrdiff_t find(const State& s, std::string_view id) {
    const auto at = std::find_if(s.sources.begin(), s.sources.end(), [id](const Source& source) { return source.id == id; });
    return at == s.sources.end() ? -1 : at - s.sources.begin();
}
}

void add(Source source) {
    auto& s = state();
    std::unique_lock lock(s.lock);
    if (source.sample && find(s, source.id) < 0) s.sources.push_back(std::move(source));
}

std::vector<overlay::DebugSource> sources() {
    auto& s = state();
    std::shared_lock lock(s.lock);
    std::vector<overlay::DebugSource> result;
    for (const auto& source : s.sources) result.push_back({source.id, source.title});
    return result;
}

bool select(std::string_view id) {
    auto& s = state();
    std::unique_lock lock(s.lock);
    const auto index = id.empty() ? -1 : find(s, id);
    if (!id.empty() && index < 0) return false;
    if (index != s.shown) {
        s.shown = index;
        s.sheet = {};
    }
    return true;
}

std::string selected() {
    auto& s = state();
    std::shared_lock lock(s.lock);
    return s.shown < 0 ? std::string{} : s.sources[static_cast<std::size_t>(s.shown)].id;
}

void on_client_tick() noexcept {
    auto& s = state();
    try {
        std::ptrdiff_t shown;
        Source source;
        {
            std::shared_lock lock(s.lock);
            if (s.shown < 0) return;
            shown = s.shown;
            source = s.sources[static_cast<std::size_t>(shown)];
        }
        auto sample = source.sample(); // outside the lock: it reads the game
        std::string changes;
        {
            std::unique_lock lock(s.lock);
            if (s.shown != shown) return; // another source was picked meanwhile
            changes = s.sheet.update(std::move(sample), GetTickCount64());
        }
        if (!changes.empty())
            logging::log(logging::Level::info, logging::Channel::diagnostics, "{}:{}", source.title, changes);
    } catch (...) { /* A lost sample is never worth the client tick. */ }
}

overlay::DebugPanel panel() {
    auto& s = state();
    std::shared_lock lock(s.lock);
    overlay::DebugPanel result;
    if (s.shown < 0) return result;
    result.title = s.sources[static_cast<std::size_t>(s.shown)].title;
    const auto now = GetTickCount64();
    for (const auto& line : s.sheet.lines())
        result.fields.push_back({line.field.label, line.field.value, Sheet::flash(line, now), line.field.heading});
    return result;
}
}
