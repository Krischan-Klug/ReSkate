#include "overlay_internal.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Game/UI/image_layout.h"
#include "Engine/Vfs/game_textures.h"
#include <filesystem>
#include <future>
#include <set>

// Game images in the overlay (overlay.h add_game_images): read and decoded in the background,
// then placed in the ImGui font atlas each time the overlay builds one, like the chat emotes.
// Only ImGui's own fenced upload is used: no game GPU resources are borrowed.

namespace dingosdk::overlay {
namespace {
struct Decoded {
    std::string key;
    frostbite::Image image;
    frostbite::ImageRegion body;
    float slice{};
};
struct Reads {
    std::mutex mutex;
    std::set<std::string, std::less<>> keys; // added, each read once
    std::vector<std::future<std::vector<Decoded>>> pending;
    std::vector<Decoded> decoded; // kept: every atlas the overlay builds (one per swapchain) needs them
};
Reads& reads() {
    static auto* state = new Reads;
    return *state;
}

std::filesystem::path game_root() {
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw std::runtime_error("the game's folder is unknown");
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

// White, with the texture's own alpha or its brightness as alpha.
void recolour(frostbite::Image& image, GameImageColours colours) {
    if (colours == GameImageColours::original) return;
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        auto* p = image.rgba.data() + i;
        if (colours == GameImageColours::brightness) p[3] = std::max({p[0], p[1], p[2]});
        p[0] = p[1] = p[2] = 255;
    }
}

std::vector<Decoded> decode(const std::vector<GameImage>& images) {
    std::vector<Decoded> result;
    try {
        vfs::GameTextures textures(game_root());
        for (const auto& request : images) {
            try {
                auto image = frostbite::crop(textures.read(request.toc, request.bundle, request.name, request.side), request.region);
                recolour(image, request.colours);
                result.push_back({request.key, std::move(image), request.body, request.slice});
            } catch (const std::exception& error) {
                logging::log(logging::Level::warning, logging::Channel::graphics, "Game image {} is unavailable: {}.",
                    request.key, error.what());
            }
        }
    } catch (const std::exception& error) {
        logging::log(logging::Level::warning, logging::Channel::graphics, "Game images are unavailable: {}.", error.what());
    }
    return result;
}
} // namespace

void add_game_images(std::vector<GameImage> images) {
    auto& state = reads();
    std::lock_guard lock(state.mutex);
    std::erase_if(images, [&](const GameImage& image) { return image.key.empty() || !state.keys.insert(image.key).second; });
    if (images.empty()) return;
    state.pending.push_back(std::async(std::launch::async, [images = std::move(images)] { return decode(images); }));
}
} // namespace dingosdk::overlay

namespace dingosdk::overlay::detail {
namespace {
// The images in the atlas being built or drawn with: render thread only.
struct Placed {
    std::size_t decoded{}; // its index in Reads::decoded
    int rect{};            // its custom rect in the atlas
    frostbite::ImageRegion body;
    float slice{};
};
struct Loaded {
    ImFontAtlas* atlas{};
    bool filled{};
    std::map<std::string, Placed, std::less<>> images;
};
Loaded& loaded() {
    static Loaded state;
    return state;
}

// An image in the atlas the overlay draws with: what it is, where, and its texture coordinates.
struct Found {
    const Placed* placed{};
    const ImFontAtlasCustomRect* rect{};
    ImVec2 uv0, uv1;
};
bool find(std::string_view key, Found& found) {
    const auto& l = loaded();
    if (!l.filled || !l.atlas || ImGui::GetIO().Fonts != l.atlas || !l.atlas->TexID) return false;
    const auto image = l.images.find(key);
    if (image == l.images.end()) return false;
    found.placed = &image->second;
    found.rect = l.atlas->GetCustomRectByIndex(image->second.rect);
    if (!found.rect || !found.rect->IsPacked() || !found.rect->Width || !found.rect->Height) return false;
    l.atlas->CalcCustomRectUV(found.rect, &found.uv0, &found.uv1);
    return true;
}
} // namespace

std::size_t reserve_game_images(ImFontAtlas& atlas, std::chrono::milliseconds wait) noexcept {
    auto& l = loaded();
    l = {};
    try {
        auto& state = reads();
        std::lock_guard lock(state.mutex);
        const auto deadline = std::chrono::steady_clock::now() + wait;
        // Reads still running stay pending for the next atlas.
        std::erase_if(state.pending, [&](auto& pending) {
            if (pending.wait_until(deadline) != std::future_status::ready) return false;
            for (auto& image : pending.get()) state.decoded.push_back(std::move(image));
            return true;
        });
        if (state.decoded.empty()) return 0;
        atlas.TexDesiredWidth = std::max(atlas.TexDesiredWidth, 2048);
        atlas.Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        for (std::size_t index = 0; index < state.decoded.size(); ++index) {
            const auto& decoded = state.decoded[index];
            l.images[decoded.key] = {index, atlas.AddCustomRectRegular(static_cast<int>(decoded.image.width),
                static_cast<int>(decoded.image.height)), decoded.body, decoded.slice};
        }
        l.atlas = &atlas;
        return l.images.size();
    } catch (...) {
        l = {};
        return 0;
    }
}

void fill_game_images(ImFontAtlas& atlas) noexcept {
    auto& l = loaded();
    if (l.atlas != &atlas || l.images.empty()) return;
    try {
        unsigned char* pixels{};
        int width{}, height{};
        atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels || width <= 0 || height <= 0) {
            l = {};
            return;
        }
        auto& state = reads();
        std::lock_guard lock(state.mutex);
        for (const auto& [key, placed] : l.images) {
            const auto& image = state.decoded[placed.decoded].image;
            const auto* rect = atlas.GetCustomRectByIndex(placed.rect);
            if (!rect || !rect->IsPacked() || rect->X + image.width > static_cast<unsigned>(width) ||
                rect->Y + image.height > static_cast<unsigned>(height)) {
                l = {};
                return;
            }
            for (std::uint32_t y = 0; y < image.height; ++y)
                std::memcpy(pixels + ((static_cast<std::size_t>(rect->Y) + y) * width + rect->X) * 4,
                            image.rgba.data() + static_cast<std::size_t>(y) * image.width * 4,
                            static_cast<std::size_t>(image.width) * 4);
        }
        atlas.TexPixelsUseColors = true;
        l.filled = true;
    } catch (...) {
        l = {};
    }
}

bool draw_game_image(ImDrawList* draw, std::string_view key, ImVec2 min, ImVec2 max, ImU32 tint) {
    Found found;
    if (!find(key, found)) return false;
    // Fitted into the box, centred, its aspect kept.
    const auto* rect = found.rect;
    const float scale = std::min((max.x - min.x) / rect->Width, (max.y - min.y) / rect->Height);
    const ImVec2 size(rect->Width * scale, rect->Height * scale);
    const ImVec2 at(min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f);
    draw->AddImage(loaded().atlas->TexID, at, ImVec2(at.x + size.x, at.y + size.y), found.uv0, found.uv1, tint);
    return true;
}

bool draw_game_shape(ImDrawList* draw, std::string_view key, ImVec2 min, ImVec2 max, ImU32 tint) {
    Found found;
    if (!find(key, found)) return false;
    const auto& body = found.placed->body;
    const auto whole = image_layout::around_body({min.x, min.y, max.x, max.y}, {body.left, body.top, body.right, body.bottom});
    draw->AddImage(loaded().atlas->TexID, ImVec2(whole.left, whole.top), ImVec2(whole.right, whole.bottom), found.uv0,
                   found.uv1, tint);
    return true;
}

bool draw_game_panel(ImDrawList* draw, std::string_view key, ImVec2 min, ImVec2 max, float edge, ImU32 tint) {
    Found found;
    if (!find(key, found)) return false;
    const float slice = found.placed->slice;
    const image_layout::Box uv{found.uv0.x, found.uv0.y, found.uv1.x, found.uv1.y};
    for (const auto& patch : image_layout::nine_slice({min.x, min.y, max.x, max.y}, edge, uv,
             slice * (uv.right - uv.left), slice * (uv.bottom - uv.top)))
        draw->AddImage(loaded().atlas->TexID, ImVec2(patch.at.left, patch.at.top), ImVec2(patch.at.right, patch.at.bottom),
                       ImVec2(patch.uv.left, patch.uv.top), ImVec2(patch.uv.right, patch.uv.bottom), tint);
    return true;
}
} // namespace dingosdk::overlay::detail
