// How numbers show in the overlay: imperial units and grouped thousands.
#include "Engine/Game/UI/display_format.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace dingosdk::display_format;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-3f; }

void units_convert() {
    check(near(feet(1.0f), 3.28084f) && near(feet(10.0f), 32.8084f), "metres in feet");
    check(near(mph(1.0f), 2.23694f) && near(mph(10.0f), 22.3694f), "metres per second in miles per hour");
}

void thousands_are_grouped() {
    check(grouped(0) == "0" && grouped(999) == "999" && grouped(1000) == "1,000", "below and at a thousand");
    check(grouped(43631) == "43,631" && grouped(1234567) == "1,234,567", "every three digits");
    check(grouped(-1234) == "-1,234", "a negative number");
}
}

int main() {
    try {
        units_convert();
        thousands_are_grouped();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Display format tests passed.\n";
    return 0;
}
