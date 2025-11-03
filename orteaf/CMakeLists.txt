set(ORTEAF_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ORTEAF_INCLUDE_ROOT ${ORTEAF_SOURCE_ROOT}/include/orteaf)

set(ORTEAF_USER_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/user)
set(ORTEAF_EXTENSION_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/extension)
set(ORTEAF_INTERNAL_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/internal)

add_library(orteaf_user INTERFACE)
target_include_directories(orteaf_user INTERFACE ${ORTEAF_USER_INCLUDE_DIR})

add_library(orteaf_extension INTERFACE)
target_include_directories(orteaf_extension INTERFACE
    ${ORTEAF_EXTENSION_INCLUDE_DIR}
    ${ORTEAF_USER_INCLUDE_DIR}
)
target_link_libraries(orteaf_extension INTERFACE orteaf_user)

file(GLOB_RECURSE ORTEAF_SOURCES CONFIGURE_DEPENDS
    ${ORTEAF_SOURCE_ROOT}/src/*.cpp
)

if(ORTEAF_SOURCES)
    add_library(orteaf STATIC ${ORTEAF_SOURCES})
else()
    add_library(orteaf STATIC)
endif()

target_link_libraries(orteaf
    PUBLIC
        orteaf_extension
)

target_include_directories(orteaf
    PRIVATE
        ${ORTEAF_INTERNAL_INCLUDE_DIR}
)

target_compile_features(orteaf PUBLIC cxx_std_20)
