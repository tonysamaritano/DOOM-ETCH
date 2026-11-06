# SPDX-License-Identifier: GPL-2.0-or-later
target_sources(etch-controller PRIVATE machine/live.c)
target_sources(doom-metal PRIVATE
    platform/darwin/etch_packet.m platform/darwin/etch_play.m)
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
