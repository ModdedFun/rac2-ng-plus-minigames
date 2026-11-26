# Challenge Minigame Skip

For OpenRAC Going Commando PAL on Windows x64. Import the ZIP with the OpenRAC launcher and place the add-on after NG++ when using both. Choose minigame types independently in the add-on's settings.

- **Giant Clank battles** completes the Dobbo and Damosel orbital fights after you start them. The game's endings handle story progression and the return to each planet.
- **Electrolyzer puzzles** completes the board through its native success state after you open it. The game opens the linked objects and records completion.
- **Infiltrator puzzles** completes the active puzzle through the game's own success branch after you open it.
- **Hoverbike races** finishes the race through the native results after the Barlow or Joba countdown. The game handles the prize and unlocks. A skipped race uses a time above the speed skill-point threshold.

The add-on acts in Challenge Mode and later cycles. Space missions need separate mission-specific completion paths.

PAL runtime checks with NG++ 0.6.21 returned from both orbital fights to their planets. The Dobbo skip shortens the death animation after the fade begins so a late hit cannot restart it. An initialized Maktar Electrolyzer board reached the native solved state and recorded its completion. The board fixture starts at the active-puzzle state.

## Build

The release uses OpenRAC mod API 1.36. The build requires CMake 3.24 or newer, a C++17 compiler, and Python 3 for fetching and packaging the SDK.

```sh
python tools/fetch_sdk.py
cmake -S . -B build
cmake --build build --config Release
python .sdk/openrac-mod-sdk-1.36.0/tools/package_mod.py build/package/rac2_ng_plus_minigames -o dist
```
