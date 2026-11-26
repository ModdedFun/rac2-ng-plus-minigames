// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "openrac/mod.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace minigame_race_detail {

inline bool address(std::uint32_t value, std::uint32_t bytes) {
    return value >= 0x100000u && value <= 0x2000000u - bytes && value % 4u == 0;
}

}  // namespace minigame_race_detail

// The caller owns the PAL/NG+/setting gate. The chained controller always runs once.
inline void race_update(openrac::Mod& mod,
                        openrac::Original<void(std::uint32_t)> original,
                        std::uint32_t controller) {
    original(controller);

    using namespace minigame_race_detail;
    const int level = mod.loaded_level();
    if ((level != 4 && level != 11) || !address(controller, 0x100)) return;
    const auto memory = mod.memory();
    if (memory.read<std::uint16_t>(controller + 0xAA) != 0xBBC
        || memory.read<std::uint8_t>(controller + 0x20) != 2
        || memory.read<std::uint8_t>(controller + 0xBC) != 5) return;

    const auto vars = memory.read<std::uint32_t>(controller + 0x68);
    if (!address(vars, 0x76C)) return;

    // release_names.json maps both levels' PAL lap/time words to these addresses.
    const auto lap = mod.relocate_level(level, 0x001A9064);
    const auto time = mod.relocate_level(level, 0x001A9068);
    if (!address(lap, 4) || !address(time, 4)) return;
    const auto elapsed = memory.read<std::int32_t>(time);
    // Only the first active tick, after the native course/countdown initialization.
    // Enabling midway through a race leaves that attempt to the player.
    if (memory.read<std::int32_t>(lap) != 0 || elapsed < 0 || elapsed > 1) return;

    using Contains = std::int32_t (*)(std::uint32_t, std::int32_t);
    using Progress = std::int32_t (*)(std::uint32_t, std::uint32_t);
    const auto contains = mod.function<Contains>("func_002AD728");
    const auto progress = mod.function_at<Progress>(level == 4 ? 0x004169C0 : 0x004066A8);
    if (contains == nullptr || progress == nullptr) return;

    const auto records_slot = mod.relocate_level(level, level == 4 ? 0x001B264C : 0x001B378C);
    const auto pool_slot = mod.relocate_level(level, level == 4 ? 0x001B255C : 0x001B369C);
    const auto course_slot = mod.relocate_level(level, level == 4 ? 0x001B093C : 0x001B08CC);
    if (!address(records_slot, 4) || !address(pool_slot, 4) || !address(course_slot, 4)) return;
    const auto records = memory.read<std::uint32_t>(records_slot);
    const auto pool = memory.read<std::uint32_t>(pool_slot);
    const auto finish = memory.read<std::int32_t>(vars + 0x190);
    const auto checkpoint = memory.read<std::int32_t>(vars + 0x194);
    const auto course = memory.read<std::int32_t>(course_slot);
    if (!address(records, 0x80) || !address(pool, 0x100) || finish < 0
        || checkpoint < -1 || course < 0 || course >= 5) return;
    const auto finish_record = static_cast<std::uint64_t>(records)
                               + static_cast<std::uint32_t>(finish) * 0x80ull;
    if (finish_record > 0x2000000u - 0x80u) return;
    if (checkpoint >= 0 && static_cast<std::uint64_t>(records)
        + static_cast<std::uint32_t>(checkpoint) * 0x80ull > 0x2000000u - 0x80u) return;
    const auto center = static_cast<std::uint32_t>(finish_record) + 0x30;
    const auto position = memory.read<std::array<float, 4>>(center);
    for (float component : position) if (!std::isfinite(component)) return;

    // Check the table interpretation with the game's own predicate. The checkpoint
    // must not overlap this point: progress can disarm it before testing the finish.
    if (contains(center, finish) == 0 || (checkpoint >= 0 && contains(center, checkpoint) != 0)) return;

    // L04_004155D8-g8.c gives every unfinished rival a finish time later than
    // the player's. A rival already finished would invalidate the first-place claim.
    for (unsigned i = 0; i != 21; ++i) {
        const auto slot = memory.read<std::int32_t>(vars + 0x1E4 + i * 4);
        if (slot == -1) continue;
        if (slot < 0) return;
        const auto rival_address = static_cast<std::uint64_t>(pool)
                                   + static_cast<std::uint32_t>(slot) * 0x100ull;
        if (rival_address > 0x2000000u - 0x100u) return;
        const auto rival = static_cast<std::uint32_t>(rival_address);
        if (memory.read<std::uint16_t>(rival + 0xAA) != 0xB2D) return;
        const auto rival_vars = memory.read<std::uint32_t>(rival + 0x68);
        if (!address(rival_vars, 0x1A4)
            || memory.read<std::int32_t>(rival_vars + 0x1A0) != 0) return;
    }

    // PAL compares ticks/5 against 6500/5 or 7350/5 for the speed skill point.
    // Do not award it or improve an existing best time through automatic completion.
    const auto best_slot = mod.relocate(0x001A73E8)
                           + static_cast<std::uint32_t>(course + (level == 4 ? 0 : 5)) * 4;
    if (!address(best_slot, 4)) return;
    const auto best = memory.read<std::int32_t>(best_slot);
    if (best < 0 || best > 1000000000) return;
    const auto finish_time = std::max(best, level == 4 ? 6505 : 7355);

    memory.write(controller + 0x10, position);
    memory.write<std::int32_t>(vars + 0x31C, 1);
    memory.write<std::int32_t>(lap, 2);
    memory.write<std::int32_t>(time, finish_time);

    // L04_004169C0-cc-g8.c owns the complete success branch: finish path,
    // rank, win count, live (NG++-scaled) prize, records, sounds and save.
    // State 3 prevents another completion; later controller ticks own the exit scene.
    progress(controller, vars);
}
