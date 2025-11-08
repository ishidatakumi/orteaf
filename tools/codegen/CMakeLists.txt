add_executable(gen_dtypes gen_dtypes.cpp)

find_package(yaml-cpp REQUIRED)

target_link_libraries(gen_dtypes
    PRIVATE
        yaml-cpp::yaml-cpp
)

target_compile_features(gen_dtypes PRIVATE cxx_std_20)
