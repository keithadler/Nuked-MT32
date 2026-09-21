cmake_minimum_required(VERSION 3.16)
project(nuked-mt32 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Emulation core (fully portable, no platform deps) ---------------------
add_library(mt32core STATIC
    mt32.cpp
    la32.cpp
    lcd.cpp
    rom.cpp
    reverb.cpp
    munt/BReverbModel.cpp
    mame/mcs96.cpp
    mame/i8x9x.cpp
)
target_include_directories(mt32core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                          ${CMAKE_CURRENT_SOURCE_DIR}/munt)

if(MSVC)
    target_compile_options(mt32core PRIVATE /W3)
else()
    # -Wno-missing-braces: lcd_font.h initialises a [][8] table from a flat
    #   list, which is legal C++ and not worth a 160-line diff to upstream data.
    # -Wno-unused-function: mame/emu.h keeps logerror() for parity with MAME.
    target_compile_options(mt32core PRIVATE
        -Wall -Wextra
        -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter
        -Wno-missing-braces -Wno-unused-function -Wno-sign-compare)
endif()

# --- Platform MIDI input backend -------------------------------------------
if(APPLE)
    set(MIDI_BACKEND midi_coremidi.cpp)
elseif(WIN32)
    set(MIDI_BACKEND midi_win32.cpp)
else()
    find_package(ALSA)
    if(ALSA_FOUND)
        set(MIDI_BACKEND midi_alsa.cpp)
    else()
        message(WARNING "ALSA not found - building without live MIDI input")
        set(MIDI_BACKEND midi_null.cpp)
    endif()
endif()

# --- GUI application --------------------------------------------------------
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(SDL2_TARGET PkgConfig::SDL2)
else()
    set(SDL2_TARGET SDL2::SDL2)
endif()

find_package(OpenGL REQUIRED)

add_executable(nuked-mt32 main.cpp panel.cpp ${MIDI_BACKEND})

# On Windows SDL2 renames main() to SDL_main(), and the real entry point comes
# from SDL2main. It must be linked before SDL2 itself or the link fails with an
# unresolved `main`.
if(TARGET SDL2::SDL2main)
    target_link_libraries(nuked-mt32 PRIVATE SDL2::SDL2main)
elseif(WIN32 AND SDL2MAIN_LIBRARY)
    target_link_libraries(nuked-mt32 PRIVATE ${SDL2MAIN_LIBRARY})
endif()

target_link_libraries(nuked-mt32 PRIVATE mt32core ${SDL2_TARGET} OpenGL::GL)

if(APPLE)
    target_link_libraries(nuked-mt32 PRIVATE "-framework CoreMIDI" "-framework CoreFoundation")
elseif(WIN32)
    target_link_libraries(nuked-mt32 PRIVATE winmm)
elseif(ALSA_FOUND)
    target_link_libraries(nuked-mt32 PRIVATE ALSA::ALSA)
endif()

# --- Headless offline renderer (test harness) -------------------------------
add_executable(mt32-render tools/render.cpp)
target_link_libraries(mt32-render PRIVATE mt32core)

# --- ROM identification utility ---------------------------------------------
add_executable(mt32-romid tools/romid.cpp)
target_link_libraries(mt32-romid PRIVATE mt32core)

# --- Front panel capture (the picture in the README) -------------------------
add_executable(mt32-panelshot tools/panelshot.cpp panel.cpp)
target_link_libraries(mt32-panelshot PRIVATE mt32core)

# --- End-to-end self test ---------------------------------------------------
add_executable(mt32-selftest tools/selftest.cpp)
target_link_libraries(mt32-selftest PRIVATE mt32core)

enable_testing()
if(DEFINED MT32_TEST_CONTROL_ROM AND DEFINED MT32_TEST_PCM_ROM)
    add_test(NAME selftest
             COMMAND mt32-selftest ${MT32_TEST_CONTROL_ROM} ${MT32_TEST_PCM_ROM})
endif()

if(MSVC)
    # Keep a console so the ROM diagnostics and --help are visible.
    set_target_properties(nuked-mt32 PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

install(TARGETS nuked-mt32 mt32-render mt32-romid mt32-selftest
        RUNTIME DESTINATION bin)
