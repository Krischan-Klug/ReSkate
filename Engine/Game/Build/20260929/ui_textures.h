#pragma once
#include <string_view>

namespace dingosdk::game::build::v20260929::ui_textures {
// Supported SHA-256 fbce74d5e28ef525dbba2cb4adbebc13405bdbd88f31bc940bca45e4ae88b8f9.
// Icons and logos skate. ships, read from the installed game's data for the overlay
// (Engine/Vfs/game_textures.h): where each texture is, by superbundle TOC, bundle and name.
// Found with a scan of every texture the game ships (78,566 in 6,056 bundles) on 2026-10-06.

// The part of a texture that is the picture, as fractions of its width and height.
struct Region {
    float left{}, top{}, right{1}, bottom{1};
};
struct Texture {
    std::string_view toc, bundle, name;
    Region region{};
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

// The THRASHER wordmark: the THRASHER / SKATEBOARD MAGAZINE logo of the Thrasher tee (512 x 256, grey on
// clear), its wordmark alone (pixels 13 to 498 across, 39 to 192 down; the subtitle is below). Drawn in
// true proportion, unlike the logos squeezed for a garment. Streamed: its bundle keeps only the small mips.
inline constexpr Texture thrasher_wordmark{"Win32/items.toc",
    "win32/characters/maincharacters/generic/cas/clothing/licensed/thrasher/apparel/top/shirt/tshirtrelaxed/2023/colorways/"
    "thrasher_shirt_tshirtrelaxed_00004_ap_cas_main_bundlereftable",
    "characters/materials/logo/licensed/thrasher/logo_thrasher_2x1_011_co", {13.0f / 512, 39.0f / 256, 499.0f / 512, 193.0f / 256}};
}
