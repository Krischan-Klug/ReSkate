// The skater body model: the bones and their names.
#include "Engine/Game/Skater/skater_body.h"
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace dingosdk::skater_body;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void every_bone_has_its_name() {
    check(index(Bone::hips) == count - 1, "the pelvis is the last body");
    check(std::string_view(names[index(Bone::neck1)]) == "Head", "the upper neck carries the head");
    check(foot(Bone::left_toe) && foot(Bone::right_foot) && !foot(Bone::left_leg), "only toes and feet are feet");
}
}

int main() {
    try {
        every_bone_has_its_name();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skater body tests passed.\n";
    return 0;
}
