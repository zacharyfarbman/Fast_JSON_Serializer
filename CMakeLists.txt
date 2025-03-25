cmake_minimum_required(VERSION 3.30)
project(json_serializer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_BUILD_TYPE Release)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(json_serializer_v4 src/v4.cpp)
add_executable(json_serializer_v3 src/v3.cpp)

find_package(benchmark REQUIRED)
target_link_libraries(json_serializer_v4 benchmark pthread)
target_link_libraries(json_serializer_v3 benchmark pthread)
