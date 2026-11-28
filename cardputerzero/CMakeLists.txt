# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
# Linux UI and the AWUS helper. ESP32 firmware stays in the parent tree and is
# compiled here unchanged, behind host-shim/.
cmake_minimum_required(VERSION 3.16)
project(cyd32-cardputerzero LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CYD32_REPO "${CMAKE_CURRENT_SOURCE_DIR}/..")
execute_process(COMMAND git -C "${CYD32_REPO}" describe --tags --always --dirty
                OUTPUT_VARIABLE CYD32_VER OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${CYD32_REPO}" rev-parse --short=12 HEAD
                OUTPUT_VARIABLE CYD32_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT CYD32_VER)
  set(CYD32_VER "dev")
endif()
if(NOT CYD32_COMMIT)
  set(CYD32_COMMIT "unknown")
endif()

set(CYD32_BUILD_UI ON CACHE BOOL "Build the LVGL UI")
set(CYD32_DISPLAY "sdl" CACHE STRING "sdl or fbdev")
set(CYD32_SDL_SOURCE "" CACHE PATH "SDL 2.30 source tree (optional)")
set(CYD32_LVGL_SOURCE "" CACHE PATH "LVGL 9.2.2 source tree (optional)")

add_library(cyd32-core STATIC
  src/clock.cpp
  src/esp_random.cpp
  src/host_alloc.cpp
  src/sha256.cpp
  ${CYD32_REPO}/src/detections.cpp
  ${CYD32_REPO}/src/privacy.cpp
  ${CYD32_REPO}/src/bearing.cpp
  ${CYD32_REPO}/src/signatures.cpp)
target_include_directories(cyd32-core PUBLIC "${CYD32_REPO}/include" "${CMAKE_CURRENT_SOURCE_DIR}/host-shim")
target_link_libraries(cyd32-core PUBLIC pthread)

add_library(cyd32-radio STATIC
  src/radio/detect.cpp
  src/radio/session.cpp
  src/radio/channels.cpp
  src/radio/mgmt.cpp
  src/radio/ble.cpp
  src/radio/hci_find.cpp)
target_include_directories(cyd32-radio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/radio")
target_link_libraries(cyd32-radio PUBLIC cyd32-core)

add_executable(cyd32-awus-helper
  src/helper_main.cpp
  src/radio/exec_posix.cpp
  src/radio/capture.cpp
  src/radio/ble_sock.cpp)
target_link_libraries(cyd32-awus-helper PRIVATE cyd32-radio)

add_executable(cyd32-radio-test src/test_radio.cpp)
target_link_libraries(cyd32-radio-test PRIVATE cyd32-radio)

enable_testing()
# Local (and CI on a ready PR):
#   cmake -S cardputerzero -B build-cz -DCYD32_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-cz -j4
#   ctest --test-dir build-cz --output-on-failure --no-tests=error
add_test(NAME radio COMMAND cyd32-radio-test)
add_test(NAME radio-helper COMMAND cyd32-radio-test --helper $<TARGET_FILE:cyd32-awus-helper>)
add_test(NAME helper-sys-refused COMMAND cyd32-radio-test --sys-refused $<TARGET_FILE:cyd32-awus-helper>)
add_test(NAME helper-restore-type COMMAND cyd32-radio-test --restore-type $<TARGET_FILE:cyd32-awus-helper>)
add_test(NAME helper-hop-freq COMMAND cyd32-radio-test --hop-freq $<TARGET_FILE:cyd32-awus-helper>)

if(CYD32_BUILD_UI)
  if(CYD32_DISPLAY STREQUAL "fbdev")
    set(CYD32_LV_SDL 0)
    set(CYD32_LV_FBDEV 1)
  else()
    set(CYD32_LV_SDL 1)
    set(CYD32_LV_FBDEV 0)
  endif()
  configure_file(lv_conf.h.in "${CMAKE_CURRENT_BINARY_DIR}/lv_conf.h" @ONLY)
  set(LV_CONF_PATH "${CMAKE_CURRENT_BINARY_DIR}/lv_conf.h" CACHE STRING "" FORCE)
  set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
  set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
  set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)

  if(CYD32_LV_SDL)
    if(CYD32_SDL_SOURCE)
      foreach(opt SDL_SHARED SDL_TEST SDL_TESTS SDL_ALSA SDL_PULSEAUDIO SDL_JACK SDL_PIPEWIRE
                  SDL_ESD SDL_DBUS SDL_IBUS SDL_X11 SDL_WAYLAND SDL_OPENGL SDL_OPENGLES SDL_VULKAN
                  SDL_KMSDRM SDL_RPI SDL_LIBUDEV SDL_SNDIO SDL_ARTS SDL_NAS SDL_OSS SDL_DISKAUDIO
                  SDL_LIBSAMPLERATE SDL_HIDAPI SDL_CCACHE SDL_ASSEMBLY SDL_RPATH)
        set(${opt} OFF CACHE BOOL "" FORCE)
      endforeach()
      set(SDL_STATIC ON CACHE BOOL "" FORCE)
      set(SDL_DUMMYVIDEO ON CACHE BOOL "" FORCE)
      set(SDL_DUMMYAUDIO ON CACHE BOOL "" FORCE)
      set(SDL_OFFSCREEN ON CACHE BOOL "" FORCE)
      add_subdirectory("${CYD32_SDL_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/sdl" EXCLUDE_FROM_ALL)
      set(CYD32_SDL_LIB SDL2-static)
    else()
      find_package(SDL2 REQUIRED)
      set(CYD32_SDL_LIB SDL2::SDL2)
    endif()
  endif()

  if(CYD32_LVGL_SOURCE)
    add_subdirectory("${CYD32_LVGL_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl" EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(lvgl
      URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.2.2.tar.gz
      URL_HASH SHA256=129b4e00e06639fa79d7e8a6cab3c1ecce2445b1a246652ccd34f22e7b17ad6f
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(lvgl)
  endif()
  if(CYD32_LV_SDL)
    # LVGL's SDL driver includes <SDL2/SDL.h>. The static SDL target publishes that path.
    target_link_libraries(lvgl PUBLIC ${CYD32_SDL_LIB})
  endif()

  set(CYD32_UI_MAIN src/main_sdl.cpp)
  if(CYD32_LV_FBDEV)
    set(CYD32_UI_MAIN src/main_fb.cpp)
  endif()
  add_executable(cyd32-ui
    ${CYD32_UI_MAIN}
    src/app.cpp
    src/live.cpp
    src/creature_view.cpp
    ${CYD32_REPO}/src/ui_radar.cpp
    ${CYD32_REPO}/src/ui_common.cpp
    ${CYD32_REPO}/src/ui_feed.cpp
    ${CYD32_REPO}/src/creature.cpp
    ${CYD32_REPO}/src/creatures/cyd32_fenni.c
    ${CYD32_REPO}/src/creatures/cyd32_lumo.c
    ${CYD32_REPO}/src/creatures/cyd32_krabbi.c
    ${CYD32_REPO}/src/creatures/cyd32_poulpi.c)
  target_include_directories(cyd32-ui PRIVATE "${CYD32_REPO}/include" "${CMAKE_CURRENT_SOURCE_DIR}/host-shim")
  target_compile_definitions(cyd32-ui PRIVATE "CYD32_CZ_VERSION=\"${CYD32_VER}\"" "CYD32_CZ_COMMIT=\"${CYD32_COMMIT}\"")
  target_link_libraries(cyd32-ui PRIVATE lvgl cyd32-radio m)
  if(CYD32_LV_SDL)
    target_link_libraries(cyd32-ui PRIVATE ${CYD32_SDL_LIB})
  endif()
  set_source_files_properties(
    ${CYD32_REPO}/src/creatures/cyd32_fenni.c
    ${CYD32_REPO}/src/creatures/cyd32_lumo.c
    ${CYD32_REPO}/src/creatures/cyd32_krabbi.c
    ${CYD32_REPO}/src/creatures/cyd32_poulpi.c
    PROPERTIES COMPILE_OPTIONS "-O0")
endif()
