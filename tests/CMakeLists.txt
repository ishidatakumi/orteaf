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

# Collect test sources by backend
# Common tests (always compiled) - all .cpp files
file(GLOB_RECURSE ORTEAF_TEST_SOURCES_COMMON CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# CUDA backend tests - all .cu files in tests/
set(ORTEAF_TEST_SOURCES_CUDA "")
if(ORTEAF_ENABLE_CUDA_INT)
    file(GLOB_RECURSE ORTEAF_TEST_SOURCES_CUDA CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cu
    )
endif()

# MPS backend tests - all .mm files in tests/
set(ORTEAF_TEST_SOURCES_MPS "")
if(ORTEAF_ENABLE_MPS_INT)
    file(GLOB_RECURSE ORTEAF_TEST_SOURCES_MPS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.mm
    )
endif()

# Combine all test sources
set(ORTEAF_TEST_SOURCES
    ${ORTEAF_TEST_SOURCES_COMMON}
    ${ORTEAF_TEST_SOURCES_CUDA}
    ${ORTEAF_TEST_SOURCES_MPS}
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
    
    # Set CUDA architectures for test target if CUDA is enabled
    if(ORTEAF_ENABLE_CUDA_INT AND CMAKE_CUDA_ARCHITECTURES)
        set_property(TARGET orteaf_tests PROPERTY CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}")
    endif()
    
    # Propagate compile definitions from main CMakeLists.txt
    target_compile_definitions(orteaf_tests
        PRIVATE
            ORTEAF_ENABLE_CPU=${ORTEAF_ENABLE_CPU_INT}
            ORTEAF_ENABLE_CUDA=${ORTEAF_ENABLE_CUDA_INT}
            ORTEAF_ENABLE_MPS=${ORTEAF_ENABLE_MPS_INT}
    )
    
    # Link Metal/MPS frameworks on macOS when MPS tests are present
    if(APPLE AND ORTEAF_ENABLE_MPS_INT)
        target_link_libraries(orteaf_tests
            PRIVATE
                "-framework Metal"
                "-framework MetalPerformanceShaders"
                "-framework Foundation"
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(orteaf_tests)
else()
    message(STATUS "No test sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
