mkdir build
cd build

cmake -G "Visual Studio 17 2022" -A x64 ../
cmake --build ../build --config Release
