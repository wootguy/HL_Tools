mkdir build
cd build

cmake .. -DRASPI:BOOL=true

cd ..

cmake --build build --target HLStdLib
cmake --build build --target HLCore
cmake --build build --target hlms
