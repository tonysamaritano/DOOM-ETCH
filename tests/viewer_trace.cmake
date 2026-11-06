# Pure JavaScript/native comparison; no browser, network or downloaded packages.
find_program(DOOM_NODE_EXECUTABLE node)
set(DOOM_VIEWER_TRACE_MODULE "${PROJECT_SOURCE_DIR}/viewer/trace.js" CACHE FILEPATH
    "Pure viewer trace module for optional Node sampling tests")
if(BUILD_TESTING AND DOOM_NODE_EXECUTABLE AND EXISTS "${DOOM_VIEWER_TRACE_MODULE}")
    add_executable(viewer-trace-fixture tests/viewer_trace_fixture.c)
    target_link_libraries(viewer-trace-fixture PRIVATE etch-controller)
    target_compile_options(viewer-trace-fixture PRIVATE -UNDEBUG)
    add_test(NAME viewer-trace-sampling COMMAND "${DOOM_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/viewer_trace.mjs" "${DOOM_VIEWER_TRACE_MODULE}"
        "$<TARGET_FILE:viewer-trace-fixture>" "${CMAKE_CURRENT_BINARY_DIR}/viewer-trace")
    set_tests_properties(viewer-trace-sampling PROPERTIES TIMEOUT 45)
endif()
