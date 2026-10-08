#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// A hardware write watch for live reverse engineering: "who writes this field?", which reading the
// code often cannot answer. Debug register 0 of every thread of the game is set on one address, so
// each write to it traps once; the handler only records the writer's code address, its thread and
// the value now there, and the client tick logs them (Diagnostics channel): each writer once, and
// every change of the value with the writer that made it, as module+offset and, for Skate.exe, the
// Ghidra address. The trap comes right after the writing instruction: the code address logged is
// the one after it. Debugging only: threads started after arming are not watched, and every write
// costs an exception while it is armed.
namespace dingosdk::write_watch {
// The address to watch now; 0 while it cannot be resolved.
using Resolver = std::function<std::uintptr_t()>;

// Watches `size` bytes (1, 2, 4 or 8, aligned to the size) at the address `resolve` gives. While it
// is armed, the client tick resolves it again and moves the watch when it changed (the object was
// rebuilt). Any thread. False, with `error`, when it does not resolve or is not aligned.
bool arm(std::string name, Resolver resolve, std::size_t size, std::string& error);
void disarm();

struct Summary {
    bool armed{};
    std::string name;
    std::uintptr_t address{};
    std::size_t writers{};          // distinct code addresses that wrote
    std::uint64_t writes{};         // writes seen
    std::optional<std::uint64_t> value; // the latest written, as raw bytes
};
Summary summary();

// Client thread, every tick: logs what the handler recorded and follows the address.
void on_client_tick() noexcept;
}
