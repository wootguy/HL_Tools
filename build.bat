echo off
cls

mkdir build
cd build

cmake .. ^
-DEMSCRIPTEN_ROOT_PATH="../emsdk/upstream/emscripten" ^
-DCMAKE_TOOLCHAIN_FILE="../toolchains/Emscripten.cmake" ^
-Wno-dev ^
-G "NMake Makefiles" 

cd ..

cmake --build build --target HLStdLib
cmake --build build --target HLCore
cmake --build build --target hlms