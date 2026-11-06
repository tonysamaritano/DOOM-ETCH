if(BUILD_TESTING)
    add_executable(level-lines-test ${CMAKE_CURRENT_LIST_DIR}/level_lines.c
        ${PROJECT_SOURCE_DIR}/engine/game/p_setup.c ${PROJECT_SOURCE_DIR}/engine/core/m_bbox.c)
    target_include_directories(level-lines-test PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/core ${PROJECT_SOURCE_DIR}/engine/game
        ${PROJECT_SOURCE_DIR}/engine/render ${PROJECT_SOURCE_DIR}/engine/ui
        ${PROJECT_SOURCE_DIR}/engine/sound ${PROJECT_SOURCE_DIR}/engine/wad)
    target_compile_definitions(level-lines-test PRIVATE NORMALUNIX)
    target_compile_options(level-lines-test PRIVATE -UNDEBUG -fwrapv -fno-strict-aliasing)
    target_link_options(level-lines-test PRIVATE -Wl,-dead_strip)
    add_test(NAME level-lines COMMAND level-lines-test)
    set_tests_properties(level-lines PROPERTIES TIMEOUT 10)
endif()
