#include "overlay_internal.h"
#include "Engine/Core/Log/logging.h"
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

// The part of `image` that `region` names (fractions of its width and height).
frostbite::Image cropped(const frostbite::Image& image, const std::array<float, 4>& region) {
    const auto [left, top, right, bottom] = region;
    if (!(left >= 0 && top >= 0 && right <= 1 && bottom <= 1 && left < right && top < bottom))
        throw std::runtime_error("its region is not inside it");
    const auto x0 = static_cast<std::uint32_t>(left * static_cast<float>(image.width) + 0.5f);
    const auto y0 = static_cast<std::uint32_t>(top * static_cast<float>(image.height) + 0.5f);
    const auto x1 = std::max(x0 + 1, static_cast<std::uint32_t>(right * static_cast<float>(image.width) + 0.5f));
    const auto y1 = std::max(y0 + 1, static_cast<std::uint32_t>(bottom * static_cast<float>(image.height) + 0.5f));
    if (x1 > image.width || y1 > image.height) throw std::runtime_error("its region is not inside it");
    frostbite::Image result{x1 - x0, y1 - y0, std::vector<std::uint8_t>(std::size_t{x1 - x0} * (y1 - y0) * 4)};
    for (std::uint32_t y = y0; y < y1; ++y)
        std::memcpy(result.rgba.data() + std::size_t{y - y0} * result.width * 4,
                    image.rgba.data() + (std::size_t{y} * image.width + x0) * 4, std::size_t{result.width} * 4);
    return result;
}

std::vector<Decoded> decode(const std::vector<GameImage>& images) {
    std::vector<Decoded> result;
    try {
        vfs::GameTextures textures(game_root());
        for (const auto& request : images) {
            try {
                auto image = cropped(textures.read(request.toc, request.bundle, request.name, request.side), request.region);
                if (request.silhouette)
                    for (std::size_t i = 0; i < image.rgba.size(); i += 4) image.rgba[i] = image.rgba[i + 1] = image.rgba[i + 2] = 255;
                result.push_back({request.key, std::move(image)});
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
struct Loaded {
    ImFontAtlas* atlas{};
    bool filled{};
    std::map<std::string, std::pair<std::size_t, int>, std::less<>> images; // key: its decoded image, its rect
};
Loaded& loaded() {
    static Loaded state;
    return state;
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
            const auto& image = state.decoded[index].image;
            l.images[state.decoded[index].key] = {index, atlas.AddCustomRectRegular(static_cast<int>(image.width),
                static_cast<int>(image.height))};
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
            const auto& image = state.decoded[placed.first].image;
            const auto* rect = atlas.GetCustomRectByIndex(placed.second);
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
    const auto& l = loaded();
    if (!l.filled || !l.atlas || ImGui::GetIO().Fonts != l.atlas || !l.atlas->TexID) return false;
    const auto found = l.images.find(key);
    if (found == l.images.end()) return false;
    const auto* rect = l.atlas->GetCustomRectByIndex(found->second.second);
    if (!rect || !rect->IsPacked() || !rect->Width || !rect->Height) return false;
    ImVec2 uv0, uv1;
    l.atlas->CalcCustomRectUV(rect, &uv0, &uv1);
    // Fitted into the box, centred, its aspect kept.
    const float scale = std::min((max.x - min.x) / rect->Width, (max.y - min.y) / rect->Height);
    const ImVec2 size(rect->Width * scale, rect->Height * scale);
    const ImVec2 at(min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f);
    draw->AddImage(l.atlas->TexID, at, ImVec2(at.x + size.x, at.y + size.y), uv0, uv1, tint);
    return true;
}
} // namespace dingosdk::overlay::detail
