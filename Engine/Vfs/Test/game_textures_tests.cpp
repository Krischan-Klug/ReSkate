// Game textures read from an installed game: dingosdk_game_textures_tests <Skate folder>.
// Without a folder there is nothing to read, and it passes.
#include "Engine/Vfs/game_textures.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/ui_textures.h"
#include <iostream>
#include <stdexcept>

using namespace dingosdk;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void the_overlays_textures_read(const char* game_root) {
    namespace ui = addr::ui_textures;
    vfs::GameTextures textures(game_root);
    for (const auto& texture : {ui::airtime, ui::wipeout, ui::spread_eagle, ui::stopwatch}) {
        const auto image = textures.read(texture.toc, texture.bundle, texture.name, 64);
        check(image.width == 64 && image.height == 64 && image.rgba.size() == 64 * 64 * 4, "a 64 pixel icon");
    }
    const auto broken = textures.read(ui::wipeout_broken.toc, ui::wipeout_broken.bundle, ui::wipeout_broken.name, 64);
    check(broken.width == 64 && broken.height == 60, "a wider icon fits the square, its aspect kept");
    const auto wheel = textures.read(ui::flaming_wheel.toc, ui::flaming_wheel.bundle, ui::flaming_wheel.name, 64);
    check(wheel.width == 64 && wheel.height == 64, "a larger icon is taken from a smaller mip");
    // Streamed: the full size comes from the TOC's own chunk, not the bundle's small mips.
    const auto logo = textures.read(ui::thrasher_logo.toc, ui::thrasher_logo.bundle, ui::thrasher_logo.name, 512);
    check(logo.width == 512 && logo.height == 256, "the streamed logo at full size");
    bool opaque{}, clear{};
    for (std::size_t i = 3; i < logo.rgba.size(); i += 4) (logo.rgba[i] ? opaque : clear) = true;
    check(opaque && clear, "the logo on a clear background");
    bool refused{};
    try { (void)textures.read(ui::airtime.toc, ui::airtime.bundle, "ui/textures/icons/no_such_icon", 64); }
    catch (const std::runtime_error&) { refused = true; }
    check(refused, "a missing texture is refused");
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && *argv[1]) the_overlays_textures_read(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Game texture tests passed.\n";
    return 0;
}
