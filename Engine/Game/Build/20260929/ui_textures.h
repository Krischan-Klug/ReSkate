#pragma once
#include <string_view>

namespace dingosdk::game::build::v20260929::ui_textures {
// Supported SHA-256 fbce74d5e28ef525dbba2cb4adbebc13405bdbd88f31bc940bca45e4ae88b8f9.
// Icons and logos skate. ships, read from the installed game's data for the overlay
// (Engine/Vfs/game_textures.h): where each texture is, by superbundle TOC, bundle and name.
// Found with a scan of every texture the game ships (78,566 in 6,056 bundles) on 2026-10-06.
struct Texture {
    std::string_view toc, bundle, name;
};

// skate.'s scoring icons: white on clear, 64 pixels (the wipeout's broken one 68 x 64).
inline constexpr std::string_view scoring_toc = "Win32/globals.toc";
inline constexpr std::string_view scoring_bundle = "win32/configurations/gameconfigurations/delmargameconfiguration";
inline constexpr Texture airtime{scoring_toc, scoring_bundle, "ui/textures/icons/scoring/img_icon_scoring_airtime_64_64_64"};
inline constexpr Texture wipeout{scoring_toc, scoring_bundle, "ui/textures/icons/scoring/img_icon_scoring_wipeout_64_64_64"};
inline constexpr Texture wipeout_broken{scoring_toc, scoring_bundle,
    "ui/textures/icons/scoring/img_icon_scoring_wipeout_broken_64_68_64"};
inline constexpr Texture spread_eagle{scoring_toc, scoring_bundle, "ui/textures/icons/scoring/img_icon_scoring_spreadeagle_64_64_64"};

// The stopwatch of the game's timers, white on clear.
inline constexpr Texture stopwatch{"Win32/levels/game/dingolevel_root/dingolevel_root.toc",
    "win32/levels/game/dingolevel_root/dingolevel_root", "ui/textures/icons/generic/img_generic_stopwatch_64_64_64"};
// The speed line challenge's flaming wheel: white and black on clear.
inline constexpr Texture flaming_wheel{"Win32/levels/game/bam_levelroot/bam_levelroot.toc",
    "win32/levels/game/bam_levelroot/bam_levelroot", "ui/textures/icons/activities/challenges/icon/img_icon_speedlinechallenge_256_128_128"};

// The classic THRASHER MAGAZINE logo of the Thrasher tee (512 x 256, red and white on clear).
// Streamed: its bundle keeps only the small mips.
inline constexpr Texture thrasher_logo{"Win32/items.toc",
    "win32/characters/maincharacters/generic/cas/clothing/licensed/thrasher/apparel/top/shirt/tshirttight/2025/colorways/"
    "thrasher_shirt_tshirttight_00001_ap_cas_main_bundlereftable",
    "characters/materials/logo/licensed/thrasher/logo_thrasher_2x1_002_co"};
}
