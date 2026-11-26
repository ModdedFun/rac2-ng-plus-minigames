// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "openrac/mod.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace minigames {
namespace giant_clank_detail {

using openrac::gaddr;

inline bool address(gaddr value, std::uint32_t bytes) {
    return value >= 0x00100000u && value <= 0x02000000u - bytes && value % 4u == 0;
}

inline bool moby(openrac::Memory memory, gaddr value) {
    return address(value, 0x100) && value % 16u == 0
           && memory.read<std::uint8_t>(value + 0x20) < 0x80;
}

// PAL L22_003A0A08-main-g8.c consumes +0x344 before its state switch. It
// performs the complete defeat transition, including the player's animation
// cleanup, then clears the request. State 0x26/category 10 owns the ending.
inline void dobbo(openrac::Mod& mod, openrac::Memory memory, gaddr boss, gaddr vars) {
    const auto started = mod.relocate_level(22, 0x001AF314);
    const auto prompt = mod.relocate_level(22, 0x001AF334);
    if (!address(started, 4) || !address(prompt, 4)
        || memory.read<std::int32_t>(started) == 0
        || memory.read<std::int32_t>(prompt) != 0) return;
    const auto state = memory.read<std::uint8_t>(boss + 0x20);
    const auto category = memory.read<std::uint8_t>(boss + 0x95);
    if (!address(vars, 0x4F0)) return;
    const auto health = memory.read<float>(vars + 0x20);
    if (state == 0x26 && category == 10 && health <= 0.0f) {
        // After the ending's opening tick starts the fade, let its native cleanup run.
        const auto timer = memory.read<std::uint16_t>(boss + 0x96);
        if (timer > 1 && timer < 0x12D) memory.write<std::uint16_t>(boss + 0x96, 0x12D);
        return;
    }
    if (state == 0 || state >= 0x26 || category == 1 || category == 10) return;

    // The native ending dereferences all four parts. In particular +0x388 is
    // created after setup, so completing during the introductory frame is unsafe.
    for (const auto offset : {0x388u, 0x440u, 0x444u, 0x448u}) {
        if (!moby(memory, memory.read<gaddr>(vars + offset))) return;
    }
    if (!std::isfinite(health) || health <= 0.0f
        || memory.read<std::int32_t>(vars + 0x344) != 0) return;

    // The stock lethal hit leaves zero health before the ending consumes its request.
    // Otherwise a hit during the ending can re-enter the combat state.
    memory.write<float>(vars + 0x20, 0.0f);
    // The stock ending awards a combat skill point when +0x4EC is still set.
    memory.write<std::int32_t>(vars + 0x4EC, 0);
    memory.write<std::int32_t>(vars + 0x344, 1);
}

// PAL L23_00420AD0-g8.c: a lethal exposed-core hit enters state 4 and
// releases the player's animation. State 4 runs the normal debris sequence;
// L23_00422C78 then sets story flags A1/88, which the Mapper return giver uses.
inline void damosel(openrac::Mod& mod, openrac::Memory memory,
                    gaddr hero, gaddr boss, gaddr vars) {
    const auto owner = mod.relocate_level(23, 0x001B14A4);
    const auto prompt = mod.relocate_level(23, 0x001B1444);
    if (!address(owner, 4) || !address(prompt, 4)
        || memory.read<gaddr>(owner) != boss
        || memory.read<std::int32_t>(prompt) != 0 || !address(vars, 0x628)) return;
    const auto state = memory.read<std::uint8_t>(boss + 0x20);
    if (state == 0 || state == 4 || state > 20) return;
    const auto health = memory.read<float>(vars + 0x20);
    if (!std::isfinite(health) || health <= 0.0f) return;

    using NoArgs = void (*)();
    using OneMoby = void (*)(gaddr);
    using SetState = void (*)(gaddr, std::int32_t, std::int32_t);
    using ReleaseAnimation = void (*)(gaddr, gaddr);
    using HeroAction = std::int32_t (*)(std::int32_t, std::int32_t);
    const auto select_generator = mod.function_at<NoArgs>(0x00420A48);
    const auto update_shield = mod.function_at<OneMoby>(0x00420AD0);
    const auto set_state = mod.function<SetState>("func_002A8168");
    const auto cancel_animation = mod.function_at<NoArgs>(0x002B83F8);
    const auto release_animation = mod.function_at<ReleaseAnimation>(0x0031C788);
    const auto hero_action = mod.function_at<HeroAction>(0x002CE0A8);
    if (!select_generator || !update_shield || !set_state || !cancel_animation
        || !release_animation || !hero_action) return;

    const auto player = memory.read<gaddr>(hero + 0x2290);
    if (!moby(memory, player)) return;
    std::array<gaddr, 5> generators{};
    for (unsigned i = 0; i != generators.size(); ++i) {
        const auto part = memory.read<gaddr>(vars + 0x358 + i * 4);
        if (!moby(memory, part)) return;
        generators[i] = memory.read<gaddr>(part + 0x68);
        if (!address(generators[i], 0x28)
            || !std::isfinite(memory.read<float>(generators[i] + 0x20))) return;
    }
    const auto held = memory.read<gaddr>(vars + 0x610);
    gaddr held_vars = 0;
    if (held) {
        if (!moby(memory, held)) return;
        held_vars = memory.read<gaddr>(held + 0x68);
        if (!address(held_vars, 0x28)
            || !std::isfinite(memory.read<float>(held_vars + 0x20))) return;
    }

    // Keep depleted generators alive: the native ending uses their transforms
    // for debris. The native selector establishes the exposed-core sentinel -1.
    for (const auto generator : generators) {
        if (memory.read<float>(generator + 0x20) > 0.0f)
            memory.write<float>(generator + 0x20, 0.0f);
    }
    select_generator();
    if (held) {
        // The shield updater owns destruction and clearing +0x610 when this
        // part's health is negative. Finish in this callback, before another
        // active-state update could follow its now-cleared attachment.
        memory.write<float>(held_vars + 0x20, -1.0f);
        update_shield(boss);
    }
    if (memory.read<std::int32_t>(vars + 0x60C) != -1
        || memory.read<gaddr>(vars + 0x610) != 0) return;

    memory.write<float>(vars + 0x20, 0.0f);
    set_state(boss, 4, 0);
    memory.write<std::int32_t>(hero + 0x2294, 0);
    cancel_animation();
    release_animation(player, hero + 0x1870);
    memory.write<std::int32_t>(hero + 0x187C, 0);
    hero_action(0, 1);
}

}  // namespace giant_clank_detail

// Called at the game-tick boundary after the caller's PAL, NG+ and setting
// checks. No retained level pointers or save flags: native death state is the
// completion latch, and the game's ordinary return flow owns each reward.
inline void complete_giant_clank(openrac::Mod& mod) {
    using namespace giant_clank_detail;
    const int level = mod.loaded_level();
    if (level != 22 && level != 23) return;
    const auto memory = mod.memory();
    const auto hero = mod.relocate(0x00189EA0);
    if (!address(hero, 0x24D0)
        || !moby(memory, memory.read<gaddr>(hero + 0x2290))
        || memory.read<std::int32_t>(hero + 0x24CC) <= 0) return;

    const auto table = mod.relocate(0x001B1B5C);
    if (!address(table, 12)) return;
    const auto first = memory.read<gaddr>(table);
    const auto end = memory.read<gaddr>(table + 8);
    if (!address(first, 0x100) || first % 16u || end <= first
        || end > 0x02000000u || (end - first) % 0x100u) return;
    // These are the actual guest update pointers in PAL's two loaded overlays,
    // verified by config/pal/levels/functions.tsv and release_names.json.
    const gaddr update = level == 22 ? 0x003A0A08 : 0x0041B350;
    for (auto boss = first; boss < end; boss += 0x100) {
        if (memory.read<gaddr>(boss + 0x64) != update || !moby(memory, boss)) continue;
        const auto vars = memory.read<gaddr>(boss + 0x68);
        if (level == 22) dobbo(mod, memory, boss, vars);
        else damosel(mod, memory, hero, boss, vars);
        return;
    }
}

}  // namespace minigames
