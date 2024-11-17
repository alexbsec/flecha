# src/tests/CMakeLists.txt
file(GLOB TEST_SOURCES *.cpp)

# Only add tests if there are source files in the tests directory
if(TEST_SOURCES)
    add_executable(test_flecha ${TEST_SOURCES})
    target_link_libraries(test_flecha PRIVATE core memory runtime utils std ${GTEST_LIBRARIES})
    add_test(NAME FlechaTests COMMAND test_flecha)
endif()

