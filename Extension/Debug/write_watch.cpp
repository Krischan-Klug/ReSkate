#include "write_watch.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <exception>
#include <format>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace dingosdk::write_watch {
namespace {
constexpr std::size_t ring_size = 1024;
constexpr DWORD64 slot_bits = 0x3 | (0xfull << 16); // debug register 0: its enable bits, and its kind and length

struct Hit {
    std::atomic<std::uint64_t> sequence{}; // its index + 1 once written
    std::uint64_t code{}, value{};
    std::uint32_t thread{};
};

struct State {
    std::mutex lock; // everything but the handler's fields
    std::string name;
    Resolver resolve;
    std::size_t size{};
    std::map<std::uint64_t, std::uint64_t> writers; // code address -> writes
    std::uint64_t writes{};
    std::optional<std::uint64_t> value;
    PVOID handler{};
    bool armed{};
    // The handler's: the watched address (0 when off), and the one before, whose traps still on their
    // way are swallowed after the watch moved or ended.
    std::atomic<std::uintptr_t> address{}, last_address{};
    std::atomic<std::size_t> watched_size{};
    std::array<Hit, ring_size> ring{};
    std::atomic<std::uint64_t> produced{}, consumed{};
    std::atomic<std::uint64_t> lost{};
};
State& state() { static auto* value = new State; return *value; }

// In the handler only: the address was just written, so it is there.
std::uint64_t read_value(std::uintptr_t address, std::size_t size) noexcept {
    std::uint64_t value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), size);
    return value;
}
// Anywhere else: guarded, the address may be gone.
std::optional<std::uint64_t> peek_value(std::uintptr_t address, std::size_t size) noexcept {
    std::uint64_t value{};
    if (!memory::peek_bytes(address, &value, size)) return std::nullopt;
    return value;
}

// Any thread, inside the exception dispatch: record and continue; nothing that locks or allocates.
LONG CALLBACK on_exception(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    auto* context = info->ContextRecord;
    auto& s = state();
    const auto watched = s.address.load(std::memory_order_acquire);
    const auto last = s.last_address.load(std::memory_order_acquire);
    if (!(context->Dr6 & 1) || !context->Dr0 || (context->Dr0 != watched && context->Dr0 != last))
        return EXCEPTION_CONTINUE_SEARCH;
    if (context->Dr0 == watched) {
        if (s.produced.load(std::memory_order_relaxed) - s.consumed.load(std::memory_order_acquire) < ring_size) {
            const auto claimed = s.produced.fetch_add(1, std::memory_order_acq_rel);
            auto& hit = s.ring[claimed % ring_size];
            hit.code = context->Rip;
            hit.value = read_value(watched, s.watched_size.load(std::memory_order_relaxed));
            hit.thread = GetCurrentThreadId();
            hit.sequence.store(claimed + 1, std::memory_order_release);
        } else {
            s.lost.fetch_add(1, std::memory_order_relaxed);
        }
    }
    context->Dr6 &= ~0xfull;
    return EXCEPTION_CONTINUE_EXECUTION;
}

DWORD64 length_bits(std::size_t size) noexcept {
    switch (size) {
    case 1: return 0;
    case 2: return 1;
    case 8: return 2;
    default: return 3; // 4
    }
}

// Sets (or, for address 0, clears) debug register 0 on every thread of the process. From a thread of
// its own, so the caller's thread is set too. Each thread is suspended only while its context
// changes, and nothing is allocated meanwhile: a thread suspended inside the heap holds the heap.
void set_on_all_threads(std::uintptr_t address, std::size_t size) {
    std::thread([address, size] {
        const auto process = GetCurrentProcessId(), self = GetCurrentThreadId();
        std::vector<DWORD> threads;
        if (const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry))
                if (entry.th32OwnerProcessID == process && entry.th32ThreadID != self) threads.push_back(entry.th32ThreadID);
            CloseHandle(snapshot);
        }
        for (const auto id : threads) {
            const HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, id);
            if (!thread) continue;
            if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
                CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(thread, &context)) {
                    context.Dr7 &= ~slot_bits;
                    context.Dr0 = address;
                    if (address) context.Dr7 |= 1 | (1ull << 16) | (length_bits(size) << 18); // local, on write
                    context.Dr6 = 0;
                    SetThreadContext(thread, &context);
                }
                ResumeThread(thread);
            }
            CloseHandle(thread);
        }
    }).join();
}

// "Skate.exe+0x47d21a5 (Ghidra 0x1447d21a5)", or "<module>+0x…".
std::string describe_code(std::uint64_t code) {
    HMODULE module{};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(code), &module))
        return std::format("{:#x}", code);
    char path[MAX_PATH]{};
    GetModuleFileNameA(module, path, MAX_PATH);
    std::string name(path);
    if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos) name = name.substr(slash + 1);
    const auto offset = code - reinterpret_cast<std::uint64_t>(module);
    if (module == GetModuleHandleA(nullptr)) return std::format("{}+{:#x} (Ghidra {:#x})", name, offset, 0x140000000 + offset);
    return std::format("{}+{:#x}", name, offset);
}

std::string describe_value(std::optional<std::uint64_t> raw, std::size_t size) {
    if (!raw) return "unreadable";
    const auto value = *raw;
    if (size == 4) return std::format("{:#010x} (float {})", value, std::bit_cast<float>(static_cast<std::uint32_t>(value)));
    if (size == 8) return std::format("{:#018x} (double {})", value, std::bit_cast<double>(value));
    return std::format("{:#x}", value);
}

void log(const std::string& text) { logging::log(logging::Level::info, logging::Channel::diagnostics, "Write watch: {}", text); }

// Under the lock: the watch on `address` from now, or none for 0.
void watch(State& s, std::uintptr_t address) {
    if (const auto previous = s.address.load(); previous) s.last_address.store(previous, std::memory_order_release);
    s.address.store(0, std::memory_order_release);
    set_on_all_threads(address, s.size);
    s.watched_size.store(s.size, std::memory_order_relaxed);
    s.consumed.store(s.produced.load());
    s.address.store(address, std::memory_order_release);
    s.value = address ? peek_value(address, s.size) : std::nullopt;
}

void log_summary(const State& s) {
    std::string writers;
    for (const auto& [code, count] : s.writers) writers += std::format("; {} x{}", describe_code(code), count);
    log(std::format("{}: {} writes by {} writer(s){}", s.name, s.writes, s.writers.size(), writers));
}
}

bool arm(std::string name, Resolver resolve, std::size_t size, std::string& error) {
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        error = "size must be 1, 2, 4 or 8";
        return false;
    }
    const auto address = resolve ? resolve() : 0;
    if (!address || address % size || !peek_value(address, size)) {
        error = !address ? "nothing to watch right now"
              : address % size ? std::format("{:#x} is not aligned to {} bytes", address, size)
                               : std::format("{:#x} cannot be read", address);
        return false;
    }
    auto& s = state();
    std::lock_guard lock(s.lock);
    if (s.armed) log_summary(s);
    if (!s.handler) s.handler = AddVectoredExceptionHandler(1, &on_exception); // stays: it swallows late traps
    s.name = std::move(name);
    s.resolve = std::move(resolve);
    s.size = size;
    s.writers.clear();
    s.writes = 0;
    try {
        watch(s, address);
    } catch (const std::exception& e) {
        s.armed = false;
        error = std::string("could not set the watch: ") + e.what();
        return false;
    }
    s.armed = true;
    log(std::format("watching {} at {:#x} ({} bytes), now {}", s.name, address, size, describe_value(s.value, size)));
    return true;
}

void disarm() {
    auto& s = state();
    std::lock_guard lock(s.lock);
    if (!s.armed) return;
    watch(s, 0);
    s.armed = false;
    log_summary(s);
}

Summary summary() {
    auto& s = state();
    std::lock_guard lock(s.lock);
    return {s.armed, s.name, s.address.load(), s.writers.size(), s.writes, s.value};
}

void on_client_tick() noexcept {
    auto& s = state();
    try {
        std::lock_guard lock(s.lock);
        if (!s.armed) return;
        for (auto index = s.consumed.load(); index < s.produced.load(std::memory_order_acquire); ++index) {
            auto& hit = s.ring[index % ring_size];
            if (hit.sequence.load(std::memory_order_acquire) != index + 1) break; // still being written
            ++s.writes;
            if (s.writers[hit.code]++ == 0)
                log(std::format("{} written by {} (thread {}): {}", s.name, describe_code(hit.code), hit.thread,
                    describe_value(hit.value, s.size)));
            if (hit.value != s.value) {
                log(std::format("{} changed {} -> {} by {} (thread {})", s.name, describe_value(s.value, s.size),
                    describe_value(hit.value, s.size), describe_code(hit.code), hit.thread));
                s.value = hit.value;
            }
            s.consumed.store(index + 1, std::memory_order_release);
        }
        if (const auto lost = s.lost.exchange(0)) log(std::format("{}: {} writes not recorded (too many at once)", s.name, lost));
        const auto now = s.resolve ? s.resolve() : 0;
        if (now && now != s.address.load()) {
            log(std::format("{} moved {:#x} -> {:#x}; watching the new one", s.name, s.address.load(), now));
            watch(s, now);
            log(std::format("{} now {}", s.name, describe_value(s.value, s.size)));
        }
    } catch (...) { /* A lost log line is never worth the client tick. */ }
}
}
