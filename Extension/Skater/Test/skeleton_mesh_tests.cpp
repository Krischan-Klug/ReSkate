// The skater's skeleton mesh read from an installed game: dingosdk_skeleton_mesh_tests <Skate folder>.
// Without a folder there is nothing to read, and it passes.
#include "Extension/Skater/skeleton_mesh.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace dingosdk;
using namespace dingosdk::skater_skeleton;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void the_games_skeleton_reads_and_poses(const char* game_root) {
    const auto mesh = read_mesh(game_root);
    check(mesh.vertices.size() == 12222 && mesh.triangles.size() == 13817 * 3, "the level of detail's vertices and triangles");
    for (const auto part : mesh.parts) check(part >= 1 && part < skater_body::count, "every vertex on a body of the skater");
    check(mesh.bone_count == 386, "the render skeleton's bones");
    // Skinning matrices that leave every bone where it is leave the mesh as it was made.
    const std::vector<LinearTransform> unmoved(mesh.bone_count, LinearTransform{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
    Posed posed;
    check(pose(mesh, unmoved, posed), "posed");
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
        for (std::size_t axis = 0; axis < 3; ++axis)
            check(std::abs(posed.positions[i][axis] - mesh.vertices[i].position[axis]) < 1e-3f, "unmoved bones keep the mesh");
    std::cout << "Read " << mesh.vertices.size() << " vertices, " << mesh.triangles.size() / 3 << " triangles.\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && *argv[1]) the_games_skeleton_reads_and_poses(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skeleton mesh tests passed.\n";
    return 0;
}
