#pragma once
#include "Engine/Core/Log/logging.h"
#include <atomic>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace dingosdk::debug_tool {
struct Probe {
    const char *id, *system, *title, *description;
    std::atomic<bool> enabled{};
    bool (*available)() = nullptr;
    // Called when the probe is switched, before `enabled` changes to on and after it changed to off.
    void (*switched)(bool on) = nullptr;
    std::string (*status)() = nullptr;
};

// Each system's probes, in the DEBUG tab's order.
std::span<Probe> skater_probes();
bool start_skater_probes(std::uintptr_t image_base) noexcept;

template<class... Args>
void report(const Probe& probe, std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        logging::write(logging::Level::info, logging::Channel::diagnostics,
            std::string("[") + probe.id + "] " + std::format(format, std::forward<Args>(args)...));
    } catch (...) {}
}
}
