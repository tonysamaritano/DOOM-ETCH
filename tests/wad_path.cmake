# Link startup discovery directly; dead stripping excludes the game loop.
if(BUILD_TESTING)
    add_executable(wad-path-test
        "${CMAKE_SOURCE_DIR}/tests/wad_path.c"
        "${CMAKE_SOURCE_DIR}/engine/core/d_main.c")
    target_include_directories(wad-path-test PRIVATE
        engine/core engine/game engine/render engine/ui engine/sound engine/wad)
    target_compile_definitions(wad-path-test PRIVATE NORMALUNIX)
    target_compile_options(wad-path-test PRIVATE -UNDEBUG)
    target_link_options(wad-path-test PRIVATE -Wl,-dead_strip)
    add_test(NAME startup-wad-path COMMAND wad-path-test)
endif()
