# SPDX-License-Identifier: GPL-2.0-or-later
file(MAKE_DIRECTORY "${PREFIX}")
function(reject label flag)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        "DOOMWADDIR=${PREFIX}/absent" "DOOMWADPATH="
        "${GAME}" ${ARGN}
        WORKING_DIRECTORY "${PREFIX}" TIMEOUT 5
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(text "${out}${err}")
    if(NOT result MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${label}: expected prompt rejection, got ${result}: ${text}")
    endif()
    string(FIND "${text}" "${flag}" found)
    if(found EQUAL -1 OR text MATCHES "D_DoomMain|W_Init|couldn't open.*wad")
        message(FATAL_ERROR "${label}: reject ${flag} before WAD startup: ${text}")
    endif()
endfunction()

reject(orphan-physical --etch-via-gcode --etch-via-gcode)
reject(orphan-fullscreen --fullscreen --fullscreen)
reject(file-conflict --play-on-etch --play-on-etch --etch-gcode absent.gcode)
reject(live-scale --etch-time-scale --play-on-etch --etch-time-scale 2)
reject(physical-rate --etch-gpu-rate
    --play-on-etch --etch-via-gcode --etch-gpu-rate 2)
reject(physical-instant --etch-gpu-instant
    --play-on-etch --etch-via-gcode --etch-gpu-instant)
message(STATUS "Live mode rejects invalid flags before WAD or window startup")
