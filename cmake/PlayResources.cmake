# SPDX-License-Identifier: GPL-2.0-or-later
set(DOOM_PLAY_STAGE "$<TARGET_FILE_DIR:doom-metal>/etch-play")
# This target always runs, so JavaScript edits do not require a native relink.
add_custom_target(etch-play-resources)
add_dependencies(doom-metal etch-play-resources)
function(stage_play source destination)
    get_filename_component(directory "${destination}" DIRECTORY)
    add_custom_command(TARGET etch-play-resources POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DOOM_PLAY_STAGE}/${directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${PROJECT_SOURCE_DIR}/${source}" "${DOOM_PLAY_STAGE}/${destination}"
        VERBATIM)
endfunction()
foreach(file index.html game.js decoder.js screen.js scene.js style.css)
    stage_play("platform/darwin/etch_play/${file}" "${file}")
endforeach()
foreach(file store.js drives.js belts.js belt-loop.js)
    stage_play("viewer/${file}" "shared/${file}")
endforeach()
stage_play(assets/rig/etch-rig.glb rig.glb)
stage_play(assets/rig/bezel-note.png bezel-note.png)
stage_play(assets/rig/cheat-note.jpg cheat-note.jpg)
