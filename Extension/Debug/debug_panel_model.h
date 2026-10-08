#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The debug panel, the model: the shown source's latest sample and when each of its values
// last changed. No game access, no locking: the caller owns both.
namespace dingosdk::debug_panel {
// A line of a sample: a value to watch, or a section title.
struct Field {
    std::string label, value;
    bool heading{};    // a section title: label only
    bool continuous{}; // a measurement that changes all the time: it never flashes and is logged beside each change
};
inline constexpr std::uint64_t flash_ms = 1500; // how long a changed value stays lit

class Sheet {
public:
    struct Line {
        Field field;
        std::uint64_t changed_at{}; // milliseconds; 0 = not since the sheet began
    };
    // Takes the next sample at `now` (milliseconds). Returns its changes for the log: " | Label
    // old->new" for each changed value, then " | Label value" for each continuous one; empty
    // when nothing changed. A sample with other lines than the last starts the sheet anew.
    std::string update(std::vector<Field> sample, std::uint64_t now);
    const std::vector<Line>& lines() const noexcept { return lines_; }
    // How lit a line still is at `now`: 1 the moment its value changed, falling to 0 over flash_ms.
    static float flash(const Line& line, std::uint64_t now) noexcept;

private:
    std::vector<Line> lines_;
};
}
