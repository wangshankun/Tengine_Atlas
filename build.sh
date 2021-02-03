#!/bin/bash 
  
CPU_NUMS=64

if [ $# -gt 0 ]
then
	CPU_NUMS=$1
fi
rm -rf build-linux
mkdir -p build-linux
pushd build-linux
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm64.native.gcc.toolchain.cmake ..
make -j$CPU_NUMS && make install
popd

./build-linux/examples/atlas_run  -p test.prototxt  -m test.caffemodel


