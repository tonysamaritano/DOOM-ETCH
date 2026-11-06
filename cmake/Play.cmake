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
