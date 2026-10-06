#pragma once
#include <cstdint>

// The local player's skater as one verified ownership chain: client -> local player ->
// skater entity -> animation component -> physics core and the objects the core owns.
// The client tick publishes it; hooks on any thread ask whether an object they were
// handed belongs to it, and the chain is resolved again live before they act on it.
namespace dingosdk {
struct LocalSkater {
    std::uintptr_t base{}; // the game's image base the chain was resolved in
    std::uintptr_t client{}, entity{}, player{}, handle{}, component{}, core{}, context{}, selector{}, causes{}, rig{};
    bool operator==(const LocalSkater&) const = default;
};
// Resolves and checks the whole chain now; false while any link differs (no local
// player, a remote skater, a teleport under way, another build's layout...).
bool resolve_local_skater(std::uintptr_t image_base, std::uintptr_t client, std::uintptr_t entity,
    LocalSkater& skater) noexcept;
// Client tick: publish the local skater, or clear it when it does not resolve.
bool publish_local_skater(std::uintptr_t image_base, std::uintptr_t client, std::uintptr_t entity) noexcept;
void clear_local_skater() noexcept;
// Any thread: the published skater, its chain resolved again now and still exactly what
// was published.
bool current_local_skater(LocalSkater& skater) noexcept;
// Any thread: `object` is the published skater's `member`, and the chain still resolves to
// exactly what was published. A retained address alone never names another skater after a
// respawn or level change. `skater` receives the live chain.
bool local_skater_owns(std::uintptr_t object, std::uintptr_t LocalSkater::* member,
    LocalSkater* skater = nullptr) noexcept;
}
