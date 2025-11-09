find_package(yaml-cpp 0.8 REQUIRED CONFIG)

if(TARGET yaml-cpp::yaml-cpp)
    set(YAML_CPP_TARGET yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
    set(YAML_CPP_TARGET yaml-cpp)
else()
    message(FATAL_ERROR "yaml-cpp target not found after find_package")
endif()

add_executable(gen_dtypes gen_dtypes.cpp)

target_link_libraries(gen_dtypes
    PRIVATE
        "${YAML_CPP_TARGET}"
)

target_compile_features(gen_dtypes PRIVATE cxx_std_20)

add_executable(gen_ops gen_ops.cpp)

target_link_libraries(gen_ops
    PRIVATE
        "${YAML_CPP_TARGET}"
)

target_compile_features(gen_ops PRIVATE cxx_std_20)

add_executable(gen_backends gen_backends.cpp)

target_link_libraries(gen_backends
    PRIVATE
        "${YAML_CPP_TARGET}"
)

target_compile_features(gen_backends PRIVATE cxx_std_20)

add_executable(gen_architectures gen_architectures.cpp)

target_link_libraries(gen_architectures
    PRIVATE
        "${YAML_CPP_TARGET}"
)

target_compile_features(gen_architectures PRIVATE cxx_std_20)
