// The skater body model: the bones' names and the contacts of its physics steps.
#include "Engine/Game/Skater/skater_body.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace dingosdk::skater_body;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }

void every_bone_has_its_name() {
    check(index(Bone::hips) == count - 1, "the pelvis is the last body");
    check(std::string_view(name(Bone::neck1)) == "Head", "the upper neck carries the head");
    check(foot(Bone::left_toe) && foot(Bone::right_foot) && !foot(Bone::left_leg), "only toes and feet are feet");
}

void the_hardest_hit_is_found() {
    Contacts contacts;
    check(hardest(contacts) == 0, "no hit: the board's root");
    contacts.bodies[index(Bone::left_hand)].impact = 3.0f;
    contacts.bodies[index(Bone::hips)].impact = 7.5f;
    check(hardest(contacts) == index(Bone::hips), "the hardest wins");
}

void held_steps_keep_the_peaks() {
    const auto head = index(Bone::neck1);
    Contacts first, second, third;
    first.any = true;
    first.bodies[head] = {true, 0.0f, true, {1, 0, 0}, 9.0f, false, {0, 1, 0}, {}, {5, 5, 5}, {false, true}};
    second.bodies[head] = {false, 0.1f, true, {2, 0, 0}, 3.0f, false, {0, 0, 1}, {}, {6, 6, 6}, {false, false, true}};
    Contacts held;
    hold(held, first);
    hold(held, second);
    const auto& h = held.bodies[head];
    check(held.any && h.touching, "a flag of any step stays set");
    check(h.impact == 9.0f && h.velocity[0] == 1.0f && h.point[0] == 5.0f, "the hardest contact keeps its motion");
    check(h.hit.vehicle && h.hit.world, "every kind hit stays");
    check(near(h.since_contact, 0.1f), "since the last contact is the latest step's");
    third.bodies[head].velocity = {4, 0, 0};
    Contacts quiet;
    hold(quiet, second);
    hold(quiet, third);
    check(quiet.bodies[index(Bone::hips)].impact == 0 && quiet.bodies[head].impact == 3.0f, "an impact outlasts quiet steps");
    Contacts calm;
    hold(calm, third);
    hold(calm, Contacts{});
    check(calm.bodies[head].velocity[0] == 0.0f, "without impacts the latest motion shows");
}

}

int main() {
    try {
        every_bone_has_its_name();
        the_hardest_hit_is_found();
        held_steps_keep_the_peaks();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skater body tests passed.\n";
    return 0;
}
