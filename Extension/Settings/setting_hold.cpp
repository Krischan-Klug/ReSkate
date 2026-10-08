#include "setting_hold.h"
#include "named_settings.h"

namespace dingosdk {
namespace {
bool failed(const std::string& result) { return result.starts_with("error: "); }
}

bool SettingHold::hold(std::string_view value) {
    if (yielded_) return false;
    const auto now = read_named_setting(name_);
    if (!now) return false;
    if (before_ && *now != ours_) { // changed by someone else: theirs now, ours no longer
        before_.reset();
        yielded_ = true;
        return false;
    }
    if (failed(change_named_setting(name_, value, false))) return false;
    const auto after = read_named_setting(name_);
    if (!after) return false;
    if (!before_) before_ = *now;
    ours_ = *after;
    return true;
}

void SettingHold::release() {
    yielded_ = false;
    if (!before_) return;
    const auto now = read_named_setting(name_);
    if (now && *now == ours_) (void)change_named_setting(name_, *before_, false);
    before_.reset();
}
}
