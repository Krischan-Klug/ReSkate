#include "debug_panel_model.h"
#include <algorithm>
#include <format>

namespace dingosdk::debug_panel {
std::string Sheet::update(std::vector<Field> sample, std::uint64_t now) {
    const bool same_lines = sample.size() == lines_.size() &&
        std::equal(sample.begin(), sample.end(), lines_.begin(), [](const Field& field, const Line& line) {
            return field.label == line.field.label && field.heading == line.field.heading;
        });
    std::vector<Line> next;
    next.reserve(sample.size());
    std::string changes, measured;
    for (std::size_t i = 0; i < sample.size(); ++i) {
        Line line{std::move(sample[i])};
        if (same_lines) {
            const auto& before = lines_[i];
            line.changed_at = before.changed_at;
            if (line.field.continuous) {
                measured += std::format(" | {} {}", line.field.label, line.field.value);
            } else if (!line.field.heading && line.field.value != before.field.value) {
                line.changed_at = now;
                changes += std::format(" | {} {}->{}", line.field.label, before.field.value, line.field.value);
            }
        }
        next.push_back(std::move(line));
    }
    lines_ = std::move(next);
    return changes.empty() ? changes : changes + measured;
}

float Sheet::flash(const Line& line, std::uint64_t now) noexcept {
    if (!line.changed_at || now < line.changed_at || now - line.changed_at >= flash_ms) return 0.0f;
    return 1.0f - static_cast<float>(now - line.changed_at) / static_cast<float>(flash_ms);
}
}
