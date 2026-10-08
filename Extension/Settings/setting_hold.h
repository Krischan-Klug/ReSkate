#pragma once
#include <optional>
#include <string>
#include <string_view>

// A native setting (named_settings.h) held at a value of ReSkate's for a while, then given back as
// it was before: unless something else changed it meanwhile (a player's console, the trainer), in
// which case that newer value stays. ReSkate's own change, so the multiplayer lock on players'
// changes (Engine/Game/Settings/multiplayer_settings_lock.h) does not apply: a holder that must not
// hold during a session checks that itself. Game update thread only.
namespace dingosdk {
class SettingHold {
public:
    explicit SettingHold(std::string name) : name_(std::move(name)) {}
    SettingHold(const SettingHold&) = delete;
    SettingHold& operator=(const SettingHold&) = delete;

    // Holds the setting at `value`, keeping what it was the first time; false when it cannot be
    // changed now (then nothing new is held), or once something else changed it while it was held:
    // then it is theirs until the next release().
    bool hold(std::string_view value);
    // Gives the setting back (unless it is someone else's now), and may hold it again.
    void release();
    bool held() const noexcept { return before_.has_value(); }

private:
    std::string name_;
    std::optional<std::string> before_; // its value before the hold
    std::string ours_;                  // its value as we hold it, as the engine reports it
    bool yielded_{};                    // something else changed it while it was held
};
}
