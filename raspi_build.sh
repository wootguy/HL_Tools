mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DRASPI:BOOL=true

cd ..

cmake --build build --target HLStdLib
cmake --build build --target HLCore
cmake --build build --target hlms
