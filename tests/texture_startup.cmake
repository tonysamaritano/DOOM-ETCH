if(BUILD_TESTING)
    add_executable(texture-startup-test ${CMAKE_CURRENT_LIST_DIR}/texture_startup.c)
    target_include_directories(texture-startup-test PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/core ${PROJECT_SOURCE_DIR}/engine/game
        ${PROJECT_SOURCE_DIR}/engine/render ${PROJECT_SOURCE_DIR}/engine/ui
        ${PROJECT_SOURCE_DIR}/engine/sound ${PROJECT_SOURCE_DIR}/engine/wad)
    target_compile_definitions(texture-startup-test PRIVATE NORMALUNIX)
    target_compile_options(texture-startup-test PRIVATE -fwrapv -fno-strict-aliasing)
    foreach(case record tables colormap)
        add_test(NAME texture-startup-${case} COMMAND texture-startup-test ${case})
        set_tests_properties(texture-startup-${case} PROPERTIES TIMEOUT 10)
    endforeach()
endif()
