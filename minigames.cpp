// SPDX-License-Identifier: GPL-3.0-or-later
#include "openrac/games/rac2.hpp"
#include "openrac/mod.hpp"
#include "electrolyzer.hpp"
#include "giant-clank.hpp"
#include "race.hpp"

#include <memory>

namespace {

using openrac::gaddr;

constexpr gaddr kSave = 0x001A7A80u;
constexpr gaddr kCycle = 0x12u;
constexpr gaddr kMode = 0x001A8F80u;
constexpr gaddr kMobyTable = 0x001B1B5Cu;
constexpr gaddr kMobyBytes = 0x100u;
constexpr std::uint16_t kInfiltratorBall = 0xB28u;

bool challenge_mode(openrac::Mod& mod) {
    return mod.memory().read<std::int16_t>(mod.relocate(kSave) + kCycle) > 0;
}

bool infiltrator_level(int level) {
    // PAL's shared timed Infiltrator controller and Grelbin's variant.
    switch (level) {
    case 3: case 11: case 12: case 14: case 17: case 19: case 20: case 30:
        return true;
    default:
        return false;
    }
}

void complete_infiltrator(openrac::Mod& mod) {
    const int level = mod.loaded_level();
    if (!infiltrator_level(level)) return;
    const auto memory = mod.memory();
    const auto mode = mod.relocate_level(level, kMode);
    if (!mode || memory.read<int>(mode) != 7) return;

    const auto table = mod.relocate(kMobyTable);
    const auto first = memory.read<gaddr>(table);
    const auto end = memory.read<gaddr>(table + 8);
    if (!first || end <= first || (end - first) % kMobyBytes
        || end > 0x02000000u) return;
    for (auto moby = first; moby < end; moby += kMobyBytes) {
        if (memory.read<std::uint16_t>(moby + 0xAA) != kInfiltratorBall) continue;
        const auto state = memory.read<std::uint8_t>(moby + 0x20);
        if (state < 1 || state > 4) continue;
        // The ball has initialized the puzzle. Its own success branch notifies the lock,
        // ends player mode 7 and removes the ball. The lock then opens and saves normally.
        memory.write<std::uint16_t>(moby + 0x96, 0);
        memory.write<std::uint8_t>(moby + 0x20, 5);
        return;
    }
}

struct Settings {
    bool infiltrator;
    bool hoverbike;
    bool electrolyzer;
    bool giant_clank;
};

Settings settings(openrac::Mod& mod) {
    return {mod.setting<bool>("skip_infiltrator"), mod.setting<bool>("skip_hoverbike"),
            mod.setting<bool>("skip_electrolyzer"), mod.setting<bool>("skip_giant_clank")};
}

void load(openrac::Mod& mod) {
    if (mod.game() != "rac2" || mod.version() != "pal") {
        throw openrac::Error("Challenge Minigame Skip requires Going Commando PAL");
    }
    auto current = std::make_shared<Settings>(settings(mod));
    mod.on_settings([&mod, current] { *current = settings(mod); });
    mod.on_frame([&mod, current] {
        if (mod.loaded_level() < 0 || !challenge_mode(mod)) return;
        if (current->giant_clank) minigames::complete_giant_clank(mod);
        if (current->electrolyzer) minigames::complete_electrolyzer(mod);
        if (current->infiltrator) complete_infiltrator(mod);
    });
    mod.hook<openrac::rac2::Moby_RaceController_Update>([&mod, current](auto original, gaddr controller) {
        if (current->hoverbike && challenge_mode(mod)) race_update(mod, original, controller);
        else original(controller);
    });
}

} // namespace

OPENRAC_MOD_CPP("rac2-ng-plus-minigames", "0.2.1", load)
