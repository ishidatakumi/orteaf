option(ORTEAF_FETCH_GTEST "Allow FetchContent to download GoogleTest if not found." ON)

set(ORTEAF_GTEST_AVAILABLE OFF)
find_package(GTest CONFIG QUIET)
if(GTest_FOUND)
    set(ORTEAF_GTEST_AVAILABLE ON)
endif()

if(NOT ORTEAF_GTEST_AVAILABLE AND ORTEAF_FETCH_GTEST)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(ORTEAF_GTEST_AVAILABLE ON)
endif()

if(NOT ORTEAF_GTEST_AVAILABLE)
    message(WARNING "GoogleTest was not found and ORTEAF_FETCH_GTEST is OFF. Skipping test targets.")
    return()
endif()

enable_testing()

file(GLOB_RECURSE ORTEAF_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

if(ORTEAF_TEST_SOURCES)
    add_executable(orteaf_tests ${ORTEAF_TEST_SOURCES})
    target_link_libraries(orteaf_tests
        PRIVATE
            orteaf
            GTest::gtest_main
    )
    target_include_directories(orteaf_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/orteaf/include
            ${ORTEAF_GENERATED_INCLUDE_DIR}
    )
    target_compile_features(orteaf_tests PRIVATE cxx_std_20)

    include(GoogleTest)
    gtest_discover_tests(orteaf_tests)
else()
    message(STATUS "No test sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
