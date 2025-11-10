Download Command (USING VCPKG AND CMAKE):
* .\vcpkg install boost-asio boost-system
(Within Project Directory): 
* cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
* cmake --build build
* Run executable located in ./build/Debug/HFTExchangeSimulator


* SBT - 