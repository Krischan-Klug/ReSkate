#pragma once
#include "Extension/Debug/debug_panel.h"

// The skater's body as a debug panel source (Extension/Debug/debug_panel.h): the contacts of
// every physics step since the last sample (local_skater_body.h), held at their peaks: what
// the body touches, and the hardest hit with how fast that bone moved and slid.
namespace dingosdk::skater_body {
debug_panel::Source debug_source();
}
