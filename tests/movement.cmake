if(BUILD_TESTING)
    add_executable(thinker-test tests/thinker.c engine/game/p_tick.c)
    add_executable(zone-test tests/zone.c engine/core/z_zone.c)
    add_executable(collision-test tests/collision.c engine/game/p_map.c
        engine/game/p_maputl.c engine/core/m_fixed.c engine/core/m_random.c)
    foreach(target thinker-test zone-test collision-test)
        target_include_directories(${target} PRIVATE
            engine/core engine/game engine/render engine/ui engine/sound engine/wad)
        target_compile_definitions(${target} PRIVATE NORMALUNIX)
        target_compile_options(${target} PRIVATE -UNDEBUG -fwrapv -fno-strict-aliasing)
        target_link_options(${target} PRIVATE -Wl,-dead_strip)
    endforeach()
    add_test(NAME thinker-lifecycle COMMAND thinker-test)
    add_test(NAME zone-memory COMMAND zone-test)
    add_test(NAME collision-movement COMMAND collision-test)
    set_tests_properties(thinker-lifecycle zone-memory collision-movement
        PROPERTIES TIMEOUT 30)
endif()
