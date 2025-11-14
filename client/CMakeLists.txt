cmake_minimum_required(VERSION 3.16)
project(HFTExchangeSimulatorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

find_package(Boost REQUIRED COMPONENTS system)

file(COPY ${CMAKE_SOURCE_DIR}/data/orders.json
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

add_executable(HFTExchangeSimulatorClient src/main.cpp)
target_link_libraries(HFTExchangeSimulatorClient PRIVATE Boost::system)