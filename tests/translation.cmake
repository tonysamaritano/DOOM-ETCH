# Isolate the production table builder from the rest of the renderer.
if(BUILD_TESTING)
    add_executable(translation-test
        "${CMAKE_SOURCE_DIR}/tests/translation.c"
        "${CMAKE_SOURCE_DIR}/engine/render/r_draw.c")
    target_include_directories(translation-test PRIVATE
        engine/core engine/game engine/render engine/ui engine/sound engine/wad)
    target_compile_definitions(translation-test PRIVATE NORMALUNIX)
    target_compile_options(translation-test PRIVATE -UNDEBUG)
    target_link_options(translation-test PRIVATE -Wl,-dead_strip)
    add_test(NAME renderer-translation COMMAND translation-test)
endif()
