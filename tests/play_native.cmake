# SPDX-License-Identifier: GPL-2.0-or-later
file(MAKE_DIRECTORY "${PREFIX}/unrelated-cwd")
set(extra)
if(MODE STREQUAL "physical")
    list(APPEND extra --etch-via-gcode)
elseif(NOT MODE STREQUAL "visual")
    message(FATAL_ERROR "Unknown live test mode: ${MODE}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DOOMWADDIR=${WAD}"
    "${GAME}" --play-on-etch ${extra} -warp 1 1 -nosound
    --run-frames 90 -config "${PREFIX}/${MODE}.cfg"
    WORKING_DIRECTORY "${PREFIX}/unrelated-cwd"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 75)
set(log "${out}\n${err}")
file(WRITE "${PREFIX}/${MODE}.log" "${log}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Live ${MODE} run failed (${result}): ${log}")
endif()
string(REGEX MATCH "Presented[ \t]+([0-9]+)[ \t]+frames" presented "${log}")
if(NOT presented OR NOT "${CMAKE_MATCH_1}" EQUAL 90)
    message(FATAL_ERROR "Expected exactly 90 acknowledged WK frames: ${log}")
endif()
if(MODE STREQUAL "visual")
    string(REGEX MATCH "Etch GPU: frames=0 uploads=([0-9]+)" uploads "${log}")
    if(NOT uploads OR CMAKE_MATCH_1 LESS 1)
        message(FATAL_ERROR "Visual rig never uploaded a planned scene: ${log}")
    endif()
endif()
message(STATUS "${MODE}: ${presented}; display frames do not count completed physical drawings")
