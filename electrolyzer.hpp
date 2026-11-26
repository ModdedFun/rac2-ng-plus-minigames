// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "openrac/mod.hpp"

#include <cstdint>

namespace minigames {

inline void complete_electrolyzer(openrac::Mod& mod) {
    const int level = mod.loaded_level();
    // PAL's board controller is present in these overlays.
    switch (level) {
    case 2: case 3: case 4: case 9: case 12: case 17: case 20: case 30:
        break;
    default:
        return;
    }
    const auto memory = mod.memory();
    const auto mode = mod.relocate_level(level, 0x001A8F80u);
    if (!mode || memory.read<int>(mode) != 7) return;

    const auto table = mod.relocate(0x001B1B5Cu);
    const auto first = memory.read<openrac::gaddr>(table);
    const auto end = memory.read<openrac::gaddr>(table + 8u);
    if (!first || end <= first || (end - first) % 0x100u || end > 0x02000000u) return;
    for (auto moby = first; moby != end; moby += 0x100u) {
        if (memory.read<std::uint8_t>(moby + 0x20u) != 6
            || memory.read<std::uint16_t>(moby + 0xAAu) != 0x896u) continue;
        // The native controller's own bypass takes this transition once the board is set up.
        // State 8 then moves the platforms, opens the linked objects and restores play.
        using SetState = void (*)(openrac::gaddr, int, int);
        if (const auto set_state = mod.function<SetState>("func_002A8168")) {
            set_state(moby, 8, -1);
        }
        return;
    }
}

} // namespace minigames
