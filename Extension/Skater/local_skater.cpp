#include "local_skater.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/engine.h"
#include "Engine/Game/Build/20260929/no_bail.h"
#include <Windows.h>

namespace dingosdk {
namespace {
constexpr std::uintptr_t highest = memory::highest_user_address;
// Hooks resolve the chain (some 35 fields) on every physics step they act on: guarded
// same-process copies, not a system call each.
template<class T> bool read(std::uintptr_t address, T& value) noexcept { return memory::peek(address, value); }
std::uintptr_t pointer(std::uintptr_t object, std::uintptr_t offset = 0) noexcept {
    std::uintptr_t value{};
    if (object < 0x10000 || object > highest - offset || !read(object + offset, value) ||
        value < 0x10000 || value > highest - 0x1000100) return 0;
    return value;
}
struct Published {
    SRWLOCK lock = SRWLOCK_INIT; // only copies; never held across game reads
    std::uintptr_t base{};
    LocalSkater skater;
};
Published& published() { static auto* value = new Published; return *value; }
// The published skater, copied out of the lock; 0 while none is.
std::uintptr_t published_skater(LocalSkater& skater) noexcept {
    auto& p = published();
    AcquireSRWLockShared(&p.lock);
    const auto base = p.base;
    skater = p.skater;
    ReleaseSRWLockShared(&p.lock);
    return base;
}
// The chain still resolves to exactly what was published.
bool still_resolves(std::uintptr_t base, const LocalSkater& expected) noexcept {
    LocalSkater live;
    return resolve_local_skater(base, expected.client, expected.entity, live) && live == expected;
}
}

bool resolve_local_skater(std::uintptr_t base, std::uintptr_t client, std::uintptr_t entity, LocalSkater& o) noexcept {
    unsigned state{}, manager_offset{};
    if (!base || pointer(client) != base + addr::engine::client_vtable || !read(client + 0xc4, state) ||
        (state != 13 && state != 21) || pointer(entity) != base + addr::engine::skater_entity_vtable ||
        !read(base + addr::engine::context_player_manager_offset, manager_offset) || manager_offset > 0x1000000) return false;
    const auto context = pointer(client, 8);
    const auto manager = pointer(context, manager_offset);
    const auto begin = pointer(manager, 0x4c8), end = pointer(manager, 0x4d0);
    if (!context || pointer(entity, 0x20) != context || pointer(manager) != base + addr::engine::local_player_manager_vtable ||
        !begin || end != begin + 8) return false;
    o.player = pointer(begin);
    std::uint8_t local{}, remote{}, teleport{};
    if (pointer(o.player) != base + addr::engine::local_player_vtable || pointer(o.player, 0x78) != context ||
        !read(o.player + 0x45, local) || local != 1 || !read(o.player + 0x44, remote) || remote ||
        pointer(o.player, 0xb8) != entity || pointer(entity, 0xf8) != o.player ||
        !read(entity + 0x7e0, teleport) || teleport) return false;
    o.handle = pointer(o.player, 0xb0);
    const auto collection = pointer(entity, 0x70);
    o.component = pointer(entity, 0x628);
    o.core = pointer(o.component, 0x70);
    o.context = pointer(o.core, 0x3c0);
    o.selector = pointer(o.core, 0x440);
    o.causes = pointer(o.core, 0x428);
    o.rig = pointer(o.core, 0x438);
    if (pointer(o.handle) != entity + 8 || pointer(collection) != entity ||
        pointer(o.component) != base + addr::engine::skater_component_vtable || pointer(o.component, 0x18) != collection ||
        pointer(o.core) != base + addr::no_bail::bail_core_vtable || !o.context || !o.selector ||
        pointer(o.selector, 8) != o.context || pointer(o.causes, 0x20) != o.context || pointer(o.rig) != o.context ||
        pointer(o.rig, 0x4630) != o.core)
        return false;
    o.base = base;
    o.client = client;
    o.entity = entity;
    return true;
}

bool publish_local_skater(std::uintptr_t base, std::uintptr_t client, std::uintptr_t entity) noexcept {
    LocalSkater skater;
    const bool resolved = resolve_local_skater(base, client, entity, skater);
    auto& p = published();
    AcquireSRWLockExclusive(&p.lock);
    p.base = resolved ? base : 0;
    p.skater = resolved ? skater : LocalSkater{};
    ReleaseSRWLockExclusive(&p.lock);
    return resolved;
}

void clear_local_skater() noexcept {
    auto& p = published();
    AcquireSRWLockExclusive(&p.lock);
    p.base = 0;
    p.skater = {};
    ReleaseSRWLockExclusive(&p.lock);
}

bool current_local_skater(LocalSkater& skater) noexcept {
    LocalSkater expected;
    const auto base = published_skater(expected);
    if (!base || !still_resolves(base, expected)) return false;
    skater = expected;
    return true;
}
bool local_skater_owns(std::uintptr_t object, std::uintptr_t LocalSkater::* member, LocalSkater* skater) noexcept {
    LocalSkater expected;
    const auto base = published_skater(expected);
    // Compare first: hooks ask about every skater on every step, and most are not ours.
    if (!base || !object || object != expected.*member || !still_resolves(base, expected)) return false;
    if (skater) *skater = expected;
    return true;
}
}
