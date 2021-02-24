#!/bin/sh

rm -rf build
mkdir build
cd build
cmake .. 
make
./main /root/Tengine_Atlas/resnet_50/setup.config
cd -
