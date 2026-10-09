// Lists every value of Gameplay/SkatePhysicsTuning the trainer can edit, as the game's own
// data names it.
//   dingosdk_trainer_tuning_dump <Skate folder> [--json <file>]
// --json also writes the whole asset (base64) and every curve's points (7 floats each) for the
// C# rebuild of the skater controller (port/, re-tools branch).
#include "Extension/Skater/physics_tuning_model.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {
std::string base64(const std::vector<std::uint8_t> &bytes) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t n = static_cast<std::uint32_t>(bytes[i]) << 16 |
                                (i + 1 < bytes.size() ? static_cast<std::uint32_t>(bytes[i + 1]) << 8 : 0) |
                                (i + 2 < bytes.size() ? bytes[i + 2] : 0);
        out += table[n >> 18 & 63];
        out += table[n >> 12 & 63];
        out += i + 1 < bytes.size() ? table[n >> 6 & 63] : '=';
        out += i + 2 < bytes.size() ? table[n & 63] : '=';
    }
    return out;
}

void write_json(const dingosdk::physics_tuning::Model &model, const char *path) {
    std::ofstream out(path, std::ios::binary);
    out.precision(9);
    out << "{\n  \"asset_size\": " << model.image.size() << ",\n  \"image\": \"" << base64(model.image) << "\",\n  \"curves\": [";
    for (std::size_t i = 0; i < model.curve_slots.size(); ++i) {
        const auto &curve = model.curves[i];
        out << (i ? "," : "") << "\n    {\"slot\": " << model.curve_slots[i] << ", \"name\": \"" << model.curve_names[i] << "\"";
        if (curve) {
            out << ", \"min\": " << curve->min << ", \"max\": " << curve->max << ", \"points\": [";
            for (std::size_t p = 0; p + 0x1c <= curve->points.size(); p += 0x1c) {
                out << (p ? ", " : "") << "[";
                for (std::size_t k = 0; k < 7; ++k) {
                    float value;
                    std::memcpy(&value, curve->points.data() + p + k * 4, 4);
                    out << (k ? ", " : "") << value;
                }
                out << "]";
            }
            out << "]";
        }
        out << "}";
    }
    out << "\n  ]\n}\n";
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: dingosdk_trainer_tuning_dump <Skate folder> [--json <file>]\n";
        return 2;
    }
    try {
        const auto model = dingosdk::physics_tuning::read_game_tuning(argv[1]);
        if (argc >= 4 && std::strcmp(argv[2], "--json") == 0) write_json(model, argv[3]);
        std::size_t unnamed{};
        for (std::size_t i = 0; i < model.fields.size(); ++i) {
            const auto &f = model.fields[i];
            const auto &name = model.field_names[i];
            if (name.empty()) ++unnamed;
            std::cout << (name.empty() ? "?" : name) << '\t' << std::hex << "0x" << f.offset << std::dec << '\t';
            if (f.real) {
                float value;
                std::memcpy(&value, model.image.data() + f.offset, 4);
                std::cout << "real\t" << value;
            } else if (f.flag) {
                std::cout << "flag\t" << static_cast<int>(model.image[f.offset]);
            } else {
                std::int64_t value{};
                std::memcpy(&value, model.image.data() + f.offset, f.size);
                std::cout << "int" << f.size * 8 << '\t' << value;
            }
            std::cout << '\n';
        }
        for (std::size_t i = 0; i < model.curve_slots.size(); ++i) {
            const auto &curve = model.curves[i];
            std::cout << (model.curve_names[i].empty() ? "?" : model.curve_names[i]) << '\t' << std::hex << "0x" << model.curve_slots[i]
                      << std::dec << "\tcurve\t" << (curve ? curve->points.size() / 0x1c : 0) << " points";
            if (curve)
                for (std::size_t p = 0; p + 0x1c <= curve->points.size(); p += 0x1c) {
                    float x, y;
                    std::memcpy(&x, curve->points.data() + p + 0xc, 4);
                    std::memcpy(&y, curve->points.data() + p + 0x14, 4);
                    std::cout << " (" << x << ", " << y << ")";
                }
            std::cout << '\n';
        }
        std::cerr << model.fields.size() << " values (" << unnamed << " unnamed), " << model.curve_slots.size() << " curves\n";
        return unnamed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
