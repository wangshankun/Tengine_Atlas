#!/bin/bash
path_cur=$(cd `dirname $0`; pwd)
build_type="Release"

function preparePath() {
    rm -rf $1
    mkdir -p $1
    cd $1
}

function buildA300() {
    path_build=$path_cur/build
    preparePath $path_build
    cmake -DCMAKE_BUILD_TYPE=$build_type ..
    make -j16 VERBOSE=1
    ret=$?
    cd ..
    return ${ret}
}

buildA300
if [ $? -ne 0 ]; then
    exit 1
fi


#python test.py --file npu_lstm_out.bin --th 200000
