# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(rac2_ng_plus_minigames LANGUAGES CXX)

if(NOT OpenRACMod_DIR)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" sdk_lock)
  string(JSON sdk_version GET "${sdk_lock}" version)
  set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
endif()
find_package(OpenRACMod 1.36 CONFIG REQUIRED)
openrac_add_mod(rac2_ng_plus_minigames MANIFEST openrac-mod.json SOURCES minigames.cpp)
