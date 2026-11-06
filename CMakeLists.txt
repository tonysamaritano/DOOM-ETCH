cmake_minimum_required(VERSION 3.21)
if(NOT DEFINED CMAKE_C_COMPILER)
    execute_process(COMMAND xcrun --find clang OUTPUT_VARIABLE DOOM_CLANG
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(CMAKE_C_COMPILER "${DOOM_CLANG}" CACHE FILEPATH "C compiler")
endif()
if(NOT DEFINED CMAKE_OSX_SYSROOT)
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE DOOM_SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(CMAKE_OSX_SYSROOT "${DOOM_SDK}" CACHE PATH "Apple SDK")
endif()
project(doom-etch LANGUAGES C)
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "doom-etch requires Clang")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(DOOM_TEST_WAD_DIRECTORY "" CACHE PATH "Optional local shareware WAD directory")
include(cmake/Instrumentation.cmake)
include(CTest)
file(GLOB_RECURSE DOOM_ENGINE_SOURCES CONFIGURE_DEPENDS engine/*.c)
add_library(doom-engine STATIC ${DOOM_ENGINE_SOURCES})
target_include_directories(doom-engine PUBLIC
    engine/core engine/game engine/render engine/ui engine/sound engine/wad)
target_compile_definitions(doom-engine PUBLIC NORMALUNIX)
# Preserve the original engine's wrapping arithmetic and type punning.
target_compile_options(doom-engine PRIVATE -fwrapv -fno-strict-aliasing)

set(CMAKE_OBJC_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "Objective-C compiler")
enable_language(OBJC)
add_executable(doom-metal
    platform/darwin/main.m
    platform/darwin/i_system.c
    platform/darwin/i_net.c
    platform/darwin/keyboard.m
    platform/darwin/i_video.m
    platform/darwin/i_sound.c)
target_compile_options(doom-metal PRIVATE "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>")
target_link_libraries(doom-metal PRIVATE doom-engine
    "-framework AppKit" "-framework Metal" "-framework QuartzCore"
    "-framework AudioToolbox")

if(BUILD_TESTING)
    include(tests/critical.cmake)
    include(tests/wad_path.cmake)
    include(tests/texture_startup.cmake)
    include(tests/translation.cmake)
    include(tests/sprite_names.cmake)
    add_executable(zone-startup-test tests/zone_startup.c)
    target_link_libraries(zone-startup-test PRIVATE doom-engine)
    add_test(NAME zone-startup-alignment COMMAND zone-startup-test)
endif()

add_library(etch-motion STATIC etch/motion.c etch/svg.c etch/gcode_writer.c)
target_include_directories(etch-motion PUBLIC etch)
target_link_libraries(doom-engine PUBLIC etch-motion)
add_executable(etch-export tools/export.c tools/export_files.c tools/headless.c platform/darwin/i_net.c)
target_link_libraries(etch-export PRIVATE doom-engine)
if(BUILD_TESTING)
    add_executable(motion-ir-test tests/motion_ir.c tests/motion_writers.c)
    target_link_libraries(motion-ir-test PRIVATE etch-motion)
    target_compile_options(motion-ir-test PRIVATE -UNDEBUG)
    add_test(NAME motion-ir COMMAND motion-ir-test)
endif()

if(BUILD_TESTING)
    add_executable(wall-bands-test tests/wall_bands.c)
    target_link_libraries(wall-bands-test PRIVATE doom-engine)
    target_compile_options(wall-bands-test PRIVATE -UNDEBUG)
    add_test(NAME wall-bands COMMAND wall-bands-test)
endif()
