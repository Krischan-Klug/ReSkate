// The debug panel's sheet: what changed between samples, what flashes and what is logged.
#include "Extension/Debug/debug_panel_model.h"
#include <iostream>
#include <stdexcept>

using namespace dingosdk::debug_panel;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

std::vector<Field> sample(std::string state, std::string height) {
    return {{"SKATER", {}, true}, {"State", std::move(state)}, {"Height", std::move(height), false, true}};
}

void the_first_sample_changes_nothing() {
    Sheet sheet;
    check(sheet.update(sample("ground", "0.00 m"), 100).empty(), "the first sample is not a change");
    for (const auto& line : sheet.lines()) check(Sheet::flash(line, 100) == 0, "nothing flashes yet");
}

void a_changed_value_flashes_and_is_logged_with_the_measurements() {
    Sheet sheet;
    sheet.update(sample("ground", "0.00 m"), 100);
    check(sheet.update(sample("ground", "0.40 m"), 200).empty(), "a measurement alone is not a change");
    check(sheet.update(sample("air", "1.20 m"), 300) == " | State ground->air | Height 1.20 m", "the change, then the measurement");
    const auto& state = sheet.lines()[1];
    check(Sheet::flash(state, 300) == 1.0f, "lit the moment it changed");
    check(Sheet::flash(state, 300 + flash_ms / 2) == 0.5f, "fading");
    check(Sheet::flash(state, 300 + flash_ms) == 0.0f, "out after flash_ms");
    check(Sheet::flash(sheet.lines()[2], 300) == 0.0f, "a measurement never flashes");
    sheet.update(sample("air", "2.00 m"), 400);
    check(sheet.lines()[1].changed_at == 300, "an unchanged value keeps when it changed");
}

void other_lines_start_the_sheet_anew() {
    Sheet sheet;
    sheet.update(sample("ground", "0.00 m"), 100);
    check(sheet.update({{"Bone", "Head"}}, 200).empty(), "another source's lines are not changes");
    check(sheet.lines().size() == 1 && sheet.lines()[0].changed_at == 0, "the sheet starts anew");
}
}

int main() {
    try {
        the_first_sample_changes_nothing();
        a_changed_value_flashes_and_is_logged_with_the_measurements();
        other_lines_start_the_sheet_anew();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Debug panel tests passed.\n";
    return 0;
}
