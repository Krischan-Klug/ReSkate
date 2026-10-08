#pragma once
#include <cstddef>
#include <cstdint>

// Taking over fields of the game's UI models. The game keeps writing its models from its own
// logic, on its own threads: a hook on the UI model's write of one value
// (Engine/Game/Build/20260929/ui_model.h) writes the taker's value instead whenever the game writes
// a taken-over field, and keeps the game's to give back, so whatever shows the model only ever sees
// the taker's. Taking over, writing and giving back happen on the client thread under the game's
// model lock (game::ModelWriteLock), which the game's writers take too: none of theirs comes in between.
namespace dingosdk::native_ui {
// Startup, with the game's image base: hooks the UI model's write when it matches this build.
bool start_model_takeover(std::uintptr_t base) noexcept;
bool model_takeover_available() noexcept;

// The value bytes of a field as the model's write takes them.
using FieldValue = std::uint64_t;

// One field a feature may take over, kept for the feature's lifetime (at most 32 in all).
class TakenField {
public:
    explicit TakenField(std::size_t size); // the field's value size: 1, 2, 4 or 8 bytes
    ~TakenField();
    TakenField(const TakenField&) = delete;
    TakenField& operator=(const TakenField&) = delete;

    // Under the model lock. The field's handle in the current model (0: none); a new one gives
    // up what was taken, as a new model holds the game's own state.
    void bind(std::uint64_t handle) noexcept;
    // Under the model lock. From now on the game's writes write `ours` instead; the first take
    // keeps `now` (what the model holds) as what the game means, until it writes otherwise.
    void take(FieldValue ours, FieldValue now) noexcept;
    bool taken() const noexcept;
    // Under the model lock. No longer taken: what the game last meant.
    FieldValue give_back() noexcept;

private:
    std::size_t slot_;
};

// How often the game wrote a taken-over field with another value than the taker's, held back.
std::uint64_t held_back() noexcept;
}
