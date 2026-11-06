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
    platform/darwin/etch_gpu.m
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

add_library(etch-motion STATIC etch/motion.c etch/svg.c etch/gcode_writer.c etch/silhouette.c etch/timeline.c)
target_include_directories(etch-motion PUBLIC etch)
target_link_libraries(doom-engine PUBLIC etch-motion)
add_executable(etch-export tools/export.c tools/export_files.c tools/headless.c platform/darwin/i_net.c)
target_link_libraries(etch-export PRIVATE doom-engine)
if(BUILD_TESTING)
    add_executable(motion-ir-test tests/motion_ir.c tests/motion_writers.c tests/motion_timeline.c)
    target_link_libraries(motion-ir-test PRIVATE etch-motion)
    target_compile_options(motion-ir-test PRIVATE -UNDEBUG)
    add_test(NAME motion-ir COMMAND motion-ir-test)
endif()

if(BUILD_TESTING)
    add_executable(projected-walls-test tests/projected_walls.c)
    target_link_libraries(projected-walls-test PRIVATE doom-engine)
    target_compile_options(projected-walls-test PRIVATE -UNDEBUG)
    add_test(NAME projected-walls COMMAND projected-walls-test)
    add_executable(svg-bounds-test tests/svg_bounds.c)
    target_compile_options(svg-bounds-test PRIVATE -UNDEBUG)
    if(DOOM_TEST_WAD_DIRECTORY AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
        add_test(NAME etch-walls COMMAND "${CMAKE_COMMAND}"
            "-DEXPORTER=$<TARGET_FILE:etch-export>"
            "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
            "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/etch-walls"
            "-DVALIDATOR=$<TARGET_FILE:svg-bounds-test>"
            -P "${PROJECT_SOURCE_DIR}/tests/etch_walls.cmake")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(sprite-silhouettes-test tests/sprite_silhouettes.c)
    target_link_libraries(sprite-silhouettes-test PRIVATE etch-motion)
    target_compile_options(sprite-silhouettes-test PRIVATE -UNDEBUG)
    add_test(NAME sprite-silhouettes COMMAND sprite-silhouettes-test)
    if(DOOM_TEST_WAD_DIRECTORY AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
        add_test(NAME etch-sprites COMMAND "${CMAKE_COMMAND}"
            "-DEXPORTER=$<TARGET_FILE:etch-export>"
            "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
            "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/etch-sprites"
            "-DVALIDATOR=$<TARGET_FILE:svg-bounds-test>"
            -P "${PROJECT_SOURCE_DIR}/tests/sprite_export.cmake")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(artwork-features-test tests/artwork_features.c)
    target_link_libraries(artwork-features-test PRIVATE etch-motion)
    target_compile_options(artwork-features-test PRIVATE -UNDEBUG)
    add_test(NAME artwork-features COMMAND artwork-features-test)
    if(DOOM_TEST_WAD_DIRECTORY AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
        add_test(NAME etch-overlay COMMAND "${CMAKE_COMMAND}"
            "-DEXPORTER=$<TARGET_FILE:etch-export>"
            "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
            "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/etch-overlay"
            "-DVALIDATOR=$<TARGET_FILE:svg-bounds-test>"
            -P "${PROJECT_SOURCE_DIR}/tests/etch_overlay.cmake")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(hatch-geometry-test tests/hatch_geometry.c)
    target_link_libraries(hatch-geometry-test PRIVATE etch-motion)
    target_compile_options(hatch-geometry-test PRIVATE -UNDEBUG)
    add_test(NAME hatch-geometry COMMAND hatch-geometry-test)
    if(DOOM_TEST_WAD_DIRECTORY AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
        add_test(NAME etch-hatching COMMAND "${CMAKE_COMMAND}"
            "-DEXPORTER=$<TARGET_FILE:etch-export>"
            "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
            "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/etch-hatching"
            "-DVALIDATOR=$<TARGET_FILE:svg-bounds-test>"
            -P "${PROJECT_SOURCE_DIR}/tests/etch_hatching.cmake")
    endif()
endif()

option(DOOM_ENABLE_GPU_TESTS "Run native Metal window tests" OFF)
if(BUILD_TESTING)
    add_executable(capture-snapshot-test tests/capture_snapshot.c)
    target_link_libraries(capture-snapshot-test PRIVATE doom-engine)
    target_compile_options(capture-snapshot-test PRIVATE -UNDEBUG)
    add_test(NAME capture-snapshot COMMAND capture-snapshot-test)
    if(DOOM_ENABLE_GPU_TESTS AND DOOM_TEST_WAD_DIRECTORY
       AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
        add_test(NAME etch-gpu COMMAND "${CMAKE_COMMAND}"
            "-DGAME=$<TARGET_FILE:doom-metal>"
            "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
            "-DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/etch-gpu.cfg"
            -P "${PROJECT_SOURCE_DIR}/tests/etch_gpu.cmake")
        set_tests_properties(etch-gpu PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    endif()
endif()

if(BUILD_TESTING AND DOOM_ENABLE_GPU_TESTS)
    add_executable(etch-gpu-playback-test tests/etch_gpu_playback.m)
    target_compile_options(etch-gpu-playback-test PRIVATE -fobjc-arc -UNDEBUG)
    target_include_directories(etch-gpu-playback-test PRIVATE engine/render engine/core)
    target_link_libraries(etch-gpu-playback-test PRIVATE etch-motion
        "-framework Foundation" "-framework Metal" "-framework QuartzCore")
    add_test(NAME etch-gpu-playback COMMAND etch-gpu-playback-test)
    set_tests_properties(etch-gpu-playback PROPERTIES TIMEOUT 30)
endif()

if(BUILD_TESTING AND DOOM_ENABLE_GPU_TESTS AND DOOM_TEST_WAD_DIRECTORY
   AND EXISTS "${DOOM_TEST_WAD_DIRECTORY}/doom1.wad")
    add_test(NAME etch-gpu-controls COMMAND "${CMAKE_COMMAND}"
        "-DGAME=$<TARGET_FILE:doom-metal>"
        "-DWAD=${DOOM_TEST_WAD_DIRECTORY}/doom1.wad"
        "-DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/etch-gpu-controls.cfg"
        -P "${PROJECT_SOURCE_DIR}/tests/etch_gpu_controls.cmake")
    set_tests_properties(etch-gpu-controls PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()
