# SPDX-License-Identifier: GPL-2.0-or-later
target_sources(etch-controller PRIVATE machine/live.c)
target_sources(doom-metal PRIVATE
    platform/darwin/etch_packet.m platform/darwin/etch_play.m
    platform/darwin/etch_resources.m)
target_link_libraries(doom-metal PRIVATE "-framework WebKit")
include(cmake/PlayResources.cmake)

if(BUILD_TESTING)
    add_test(NAME machine-play-flags COMMAND "${CMAKE_COMMAND}"
        "-DGAME=$<TARGET_FILE:doom-metal>"
        "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/play-flags"
        -P "${PROJECT_SOURCE_DIR}/tests/play_flags.cmake")
    set_tests_properties(machine-play-flags PROPERTIES TIMEOUT 40)
endif()

if(BUILD_TESTING)
    add_executable(play-packet-test tests/play_packet.m platform/darwin/etch_packet.m)
    target_include_directories(play-packet-test PRIVATE platform/darwin)
    target_compile_options(play-packet-test PRIVATE -fobjc-arc -UNDEBUG)
    target_link_libraries(play-packet-test PRIVATE "-framework Foundation")
    add_test(NAME machine-play-packet COMMAND play-packet-test)
    if(DOOM_NODE_EXECUTABLE)
        add_test(NAME viewer-play-decoder COMMAND "${DOOM_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/play_decoder.mjs")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(machine-live-test tests/machine_live.c)
    target_link_libraries(machine-live-test PRIVATE etch-controller)
    target_compile_options(machine-live-test PRIVATE -UNDEBUG)
    add_test(NAME machine-live COMMAND machine-live-test)
    add_executable(play-resources-test tests/play_resources.m platform/darwin/etch_resources.m)
    target_include_directories(play-resources-test PRIVATE platform/darwin)
    target_compile_options(play-resources-test PRIVATE -fobjc-arc -UNDEBUG)
    target_link_libraries(play-resources-test PRIVATE "-framework Foundation")
    add_test(NAME machine-play-resources COMMAND play-resources-test)
endif()

if(BUILD_TESTING AND DOOM_ENABLE_GPU_TESTS)
    add_executable(play-readback-test tests/play_readback.m platform/darwin/etch_gpu.m)
    target_include_directories(play-readback-test PRIVATE platform/darwin engine/render engine/core)
    target_compile_options(play-readback-test PRIVATE -fobjc-arc -UNDEBUG)
    target_link_libraries(play-readback-test PRIVATE etch-motion
        "-framework Foundation" "-framework Metal" "-framework QuartzCore")
    add_test(NAME machine-play-readback COMMAND play-readback-test)
    set_tests_properties(machine-play-readback PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endif()
