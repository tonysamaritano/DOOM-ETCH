if(BUILD_TESTING)
    add_executable(save-roundtrip-test ${CMAKE_CURRENT_LIST_DIR}/save_roundtrip.c
        ${PROJECT_SOURCE_DIR}/engine/game/p_saveg.c)
    target_include_directories(save-roundtrip-test PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/core ${PROJECT_SOURCE_DIR}/engine/game
        ${PROJECT_SOURCE_DIR}/engine/render ${PROJECT_SOURCE_DIR}/engine/ui
        ${PROJECT_SOURCE_DIR}/engine/sound ${PROJECT_SOURCE_DIR}/engine/wad)
    target_compile_definitions(save-roundtrip-test PRIVATE NORMALUNIX)
    target_compile_options(save-roundtrip-test PRIVATE -UNDEBUG -fwrapv -fno-strict-aliasing)
    add_test(NAME save-roundtrip COMMAND save-roundtrip-test)
    set_tests_properties(save-roundtrip PROPERTIES TIMEOUT 10)
endif()
