#include "game_speed.h"
#include "Engine/Game/Multiplayer/session_tools.h"
#include <format>

namespace dingosdk {
bool GameSpeed::set(float speed) {
    if (!(speed < 1.0f) || multiplayer_session_active()) {
        release();
        return false;
    }
    if (time_scale_.hold(std::format("{:.3f}", speed))) return true;
    release();
    return false;
}

void GameSpeed::release() { time_scale_.release(); }
}
