#include "model_takeover.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/ui_model.h"
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace dingosdk::native_ui {
namespace {
using Address = std::uintptr_t;
using ModelWrite = std::uint64_t (*)(Address manager, std::uint64_t handle, Address type, const void* value,
                                     std::uint8_t flag);
constexpr std::size_t slot_count = 32;

// One taken-over field, as the hook sees it on whichever thread the game writes from.
struct Slot {
    std::atomic<bool> used{};
    std::size_t size{};
    std::atomic<std::uint64_t> handle{};
    std::atomic<bool> taken{};
    std::atomic<FieldValue> ours{}, meant{};
};
struct Hook {
    std::atomic<ModelWrite> original{};
    std::atomic<bool> ready{};
    std::array<Slot, slot_count> slots;
    std::mutex slots_mutex; // guards claiming a slot
    std::atomic<std::uint64_t> held_back{};
};
Hook& hook() {
    static auto* value = new Hook;
    return *value;
}

std::uint64_t model_write(Address manager, std::uint64_t handle, Address type, const void* value, std::uint8_t flag) {
    auto& h = hook();
    FieldValue mine{};
    if (handle && value) {
        for (auto& slot : h.slots) {
            if (!slot.taken.load(std::memory_order_acquire) || handle != slot.handle.load(std::memory_order_acquire)) continue;
            FieldValue meant{};
            std::memcpy(&meant, value, slot.size);
            slot.meant.store(meant, std::memory_order_release);
            mine = slot.ours.load(std::memory_order_acquire);
            if (meant != mine) h.held_back.fetch_add(1, std::memory_order_relaxed);
            value = &mine;
            break;
        }
    }
    return h.original.load(std::memory_order_acquire)(manager, handle, type, value, flag);
}
} // namespace

bool start_model_takeover(std::uintptr_t base) noexcept {
    auto& h = hook();
    if (h.ready.load(std::memory_order_acquire)) return true;
    const auto& contract = addr::ui_model::model_write;
    std::array<unsigned char, 32> code{};
    auto status = HookNotFound;
    if (memory::peek(base + contract.rva, code) && code == contract.bytes) {
        auto* target = reinterpret_cast<void*>(base + contract.rva);
        void* relay{};
        status = hook_prepare(target, reinterpret_cast<void*>(&model_write), &relay);
        if (status == HookOk && !relay) status = HookUnsupportedFunction;
        if (status == HookOk) {
            h.original.store(reinterpret_cast<ModelWrite>(relay), std::memory_order_release); // before the hook is live
            status = hook_enable(target);
        }
        if (status != HookOk) (void)hook_remove(target);
    }
    if (status != HookOk) {
        logging::log(logging::Level::warning, logging::Channel::ui,
            "Native UI: its models cannot be taken over, hooking the UI model's write failed ({}).", hook_status_string(status));
        return false;
    }
    h.ready.store(true, std::memory_order_release);
    return true;
}

bool model_takeover_available() noexcept { return hook().ready.load(std::memory_order_acquire); }

TakenField::TakenField(std::size_t size) {
    if (size != 1 && size != 2 && size != 4 && size != 8) throw std::invalid_argument("A taken-over field holds 1 to 8 bytes.");
    auto& h = hook();
    std::lock_guard lock(h.slots_mutex);
    for (slot_ = 0; slot_ < slot_count; ++slot_) {
        auto& slot = h.slots[slot_];
        if (slot.used.load(std::memory_order_acquire)) continue;
        slot.size = size;
        slot.handle.store(0, std::memory_order_release);
        slot.taken.store(false, std::memory_order_release);
        slot.used.store(true, std::memory_order_release);
        return;
    }
    throw std::length_error("Every taken-over field's place is in use.");
}

TakenField::~TakenField() {
    auto& slot = hook().slots[slot_];
    slot.taken.store(false, std::memory_order_release);
    slot.handle.store(0, std::memory_order_release);
    slot.used.store(false, std::memory_order_release);
}

void TakenField::bind(std::uint64_t handle) noexcept {
    auto& slot = hook().slots[slot_];
    if (slot.handle.load(std::memory_order_acquire) == handle) return;
    slot.taken.store(false, std::memory_order_release);
    slot.handle.store(handle, std::memory_order_release);
}

void TakenField::take(FieldValue ours, FieldValue now) noexcept {
    auto& slot = hook().slots[slot_];
    if (!slot.taken.load(std::memory_order_acquire)) slot.meant.store(now, std::memory_order_release);
    slot.ours.store(ours, std::memory_order_release);
    slot.taken.store(true, std::memory_order_release);
}

bool TakenField::taken() const noexcept { return hook().slots[slot_].taken.load(std::memory_order_acquire); }

FieldValue TakenField::give_back() noexcept {
    auto& slot = hook().slots[slot_];
    slot.taken.store(false, std::memory_order_release);
    return slot.meant.load(std::memory_order_acquire);
}

std::uint64_t held_back() noexcept { return hook().held_back.load(std::memory_order_relaxed); }
}
