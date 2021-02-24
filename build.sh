#!/bin/bash 
  
CPU_NUMS=64

if [ $# -gt 0 ]
then
	CPU_NUMS=$1
fi
rm -rf build-linux
mkdir -p build-linux
pushd build-linux
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm64.native.gcc.toolchain.cmake ..

cd /root/Tengine_Atlas/atlas_npu/lib/ && ./build.sh && cd -
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm64.native.gcc.toolchain.cmake ..
make -j$CPU_NUMS VERBOSE=1 && make install
popd

export LD_LIBRARY_PATH=./build-linux/install/lib/:/usr/local/lib/:$LD_LIBRARY_PATH

./build-linux/examples/atlas_run  -p test.prototxt  -m test.caffemodel


