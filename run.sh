rm -rf build
mkdir build
cd build
cmake .. 
cmake --build . -j2
./tests