cmake_minimum_required(VERSION 3.16)
project(MarketBookBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

find_package(Boost REQUIRED COMPONENTS system)
find_package(benchmark CONFIG REQUIRED)

add_executable(MarketBookBenchmarks src/bench_main.cpp)

target_include_directories(MarketBookBenchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/../server/include
)

target_link_libraries(MarketBookBenchmarks PRIVATE
    Boost::system
    benchmark::benchmark
    benchmark::benchmark_main
)

add_executable(Repro src/repro_main.cpp)
target_include_directories(Repro PRIVATE ${CMAKE_SOURCE_DIR}/../server/include)
target_link_libraries(Repro PRIVATE Boost::system)