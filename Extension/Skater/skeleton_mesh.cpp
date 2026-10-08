#include "skeleton_mesh.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/skater_skeleton.h"
#include "Engine/Resource/ebx_document.h"
#include "Engine/Vfs/game_bundles.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace dingosdk::skater_skeleton {
namespace {
namespace build = addr::skater_skeleton;

struct Loader {
    std::once_flag started;
    std::atomic<std::shared_ptr<const Mesh>> mesh;
};
Loader& loader() { static auto* value = new Loader; return *value; }

void require(bool valid, const std::string& message) {
    if (!valid) throw std::runtime_error(message);
}
vfs::GameBundle bundle(const vfs::GameData& data, const frostbite::TocDocument& toc, std::string_view name) {
    auto found = data.read_bundle(toc, name);
    require(found.has_value(), "the bundle " + std::string(name) + " is missing");
    return std::move(*found);
}
std::vector<std::byte> asset(const vfs::GameData& data, const vfs::GameBundle& bundle, frostbite::AssetKind kind,
    std::string_view name) {
    std::size_t index{};
    require(bundle.find(kind, name, &index) && bundle.payload(kind, index), "the asset " + std::string(name) + " is missing");
    return data.read(*bundle.payload(kind, index));
}
// A geometry chunk: in the bundle when it has it, else in its superbundle's TOC.
std::vector<std::byte> chunk(const vfs::GameData& data, const vfs::GameBundle& bundle, const frostbite::TocDocument& toc,
    const frostbite::Guid& id) {
    std::size_t index{};
    if (bundle.find_chunk(id, &index) && bundle.payload(frostbite::AssetKind::chunk, index))
        return data.read(*bundle.payload(frostbite::AssetKind::chunk, index));
    const auto found = std::find_if(toc.chunks.begin(), toc.chunks.end(),
        [&](const frostbite::TocChunk& candidate) { return candidate.guid == id && !candidate.removed; });
    require(found != toc.chunks.end(), "the mesh's geometry chunk is missing");
    return data.read({found->location, found->offset, found->size});
}
}

Mesh read_mesh(const std::filesystem::path& game_root) {
    const vfs::GameData data(game_root);
    const auto skeleton_toc = data.read_toc(build::skeleton_toc);
    const auto skeleton_document =
        frostbite::ebx::read_document(asset(data, bundle(data, skeleton_toc, build::skeleton_bundle), frostbite::AssetKind::ebx,
            build::skeleton_asset));
    const auto* root = skeleton_document.root();
    require(root && root->object, "the render skeleton is empty");
    const auto skeleton = frostbite::read_skeleton(*root->object);
    const auto mesh_toc = data.read_toc(build::mesh_toc);
    const auto mesh_bundle = bundle(data, mesh_toc, build::mesh_bundle);
    const auto resource = asset(data, mesh_bundle, frostbite::AssetKind::resource, build::mesh_asset);
    const auto lod = frostbite::read_mesh_lod(resource, build::mesh_lod);
    const auto geometry = chunk(data, mesh_bundle, mesh_toc, lod.chunk);
    return bind(skeleton, frostbite::read_skinned_mesh(resource, build::mesh_lod, geometry));
}

void prepare() noexcept {
    try {
        std::call_once(loader().started, [] {
            std::thread([] {
                const auto started = GetTickCount64();
                try {
                    std::vector<wchar_t> exe(32768);
                    const auto length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
                    require(length && length < exe.size(), "the game's folder is unknown");
                    auto read = std::make_shared<const Mesh>(read_mesh(std::filesystem::path(exe.data()).parent_path()));
                    logging::log(logging::Level::info, logging::Channel::skater,
                        "Skater skeleton: read the game's own ({} vertices, {} triangles) in {} ms.", read->vertices.size(),
                        read->triangles.size() / 3, GetTickCount64() - started);
                    loader().mesh.store(std::move(read));
                } catch (const std::exception& failure) {
                    logging::log(logging::Level::warning, logging::Channel::skater,
                        "Skater skeleton is unavailable: {}.", failure.what());
                }
            }).detach();
        });
    } catch (...) {
        logging::write(logging::Level::warning, logging::Channel::skater,
            "Skater skeleton is unavailable: its reading could not start.");
    }
}

std::shared_ptr<const Mesh> mesh() noexcept { return loader().mesh.load(); }
}
