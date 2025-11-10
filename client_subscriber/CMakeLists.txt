cmake_minimum_required(VERSION 3.16)
project(HFTExchangeSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

find_package(Boost REQUIRED COMPONENTS system)

add_executable(ClientSubscriber src/main.cpp)
target_link_libraries(ClientSubscriber PRIVATE Boost::system)