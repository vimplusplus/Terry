cmake_minimum_required(VERSION 3.16)
project(Terry CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── FTXUI (TUI framework) ──────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG        v5.0.0
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# ── Skill content embedding (runs at configure time, only if references/ present)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/references")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            "-DOUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/generated/skill_content.h"
            "-DREFS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/references"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_skills.cmake"
        RESULT_VARIABLE embed_result
    )
    if(NOT embed_result EQUAL 0)
        message(WARNING "embed_skills.cmake failed (exit ${embed_result}) — skill content may be missing")
    endif()
else()
    message(STATUS "references/ not present — using pre-generated skill_content.h")
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/cell_buffer.cpp
    src/vt_parser.cpp
    src/boot_sequence.cpp
    src/particle_system.cpp
    src/animations.cpp
    src/background_art.cpp
    src/tui_app.cpp
    src/vscode_launcher.cpp
    src/skill_registry.cpp
)

if(WIN32)
    list(APPEND SOURCES src/shell_host_windows.cpp)
    # Embed application icon (resource ID 1 = Windows auto-selects as app icon)
    enable_language(RC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/terry.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/terry.rc"
        @ONLY
    )
    list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/terry.rc")
else()
    list(APPEND SOURCES src/shell_host_posix.cpp)
endif()

add_executable(Terry ${SOURCES})

target_include_directories(Terry PRIVATE include)

target_link_libraries(Terry PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# forkpty lives in libutil on Linux
if(UNIX)
    target_link_libraries(Terry PRIVATE util)
endif()

# WinINet (download) and Shell32 (ShellExecute) for Rincewind command
if(WIN32)
    target_link_libraries(Terry PRIVATE wininet shell32)
endif()

# Suppress MSVC noise and prevent Windows macro conflicts
if(MSVC)
    target_compile_definitions(Terry PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(Terry PRIVATE /utf-8 /W3)
endif()
