# Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.

# CMake lowest version requirement
cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 14)
# Add definitions ENABLE_DVPP_INTERFACE to use dvpp api
add_definitions(-DENABLE_DVPP_INTERFACE)
# project information
project(OcrAtlas800 LANGUAGES C CXX ASM)
# Check environment variable
if(NOT DEFINED ENV{ASCEND_HOME})
    message(FATAL_ERROR "please define environment variable:ASCEND_HOME")
endif()

# Compatible with the scenario where the Hisilicon version is installed
set(ASCEND_VERSION "")
if(NOT DEFINED ENV{ASCEND_VERSION})
    message(WARNING "please define environment variable:ASCEND_VERSION")
else()
    set(ASCEND_VERSION $ENV{ASCEND_VERSION})
endif()

set(ARCH_PATTERN "")
if(NOT DEFINED ENV{ARCH_PATTERN})
    message(WARNING "please define environment variable:ARCH_PATTERN")
else()
    set(ARCH_PATTERN $ENV{ARCH_PATTERN})
endif()

# Find ascendbase
set(ASCEND_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ascendbase/src/Base)
get_filename_component(ASCEND_BASE_ABS_DIR ${ASCEND_BASE_DIR} ABSOLUTE)
file(GLOB_RECURSE ASCEND_BASE_SRC_FILES ${ASCEND_BASE_ABS_DIR}/*.cpp)
# exclude the DvppCommonDevice.cpp which need to compile in the open-form
list(REMOVE_ITEM ASCEND_BASE_SRC_FILES ${ASCEND_BASE_ABS_DIR}/DvppCommonDevice/DvppCommonDevice.cpp)
list(REMOVE_ITEM ASCEND_BASE_SRC_FILES ${ASCEND_BASE_ABS_DIR}/Hdc/Hdc.cpp)

# Compile options
add_compile_options(-std=c++11  -fopenmp  -fstack-protector-all -Werror)

# Skip build rpath
set(CMAKE_SKIP_BUILD_RPATH True)

# Set output directory
set(PROJECT_SRC_ROOT ${CMAKE_CURRENT_LIST_DIR}/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SRC_ROOT}/dist)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SRC_ROOT}/dist)

# Set include directory and library directory
set(ACL_INC_DIR $ENV{ASCEND_HOME}/${ASCEND_VERSION}/${ARCH_PATTERN}/acllib/include)
set(ACL_LIB_DIR $ENV{ASCEND_HOME}/${ASCEND_VERSION}/${ARCH_PATTERN}/acllib/lib64/stub)

# Header path
# Src path
aux_source_directory(${PROJECT_SRC_ROOT}/atlas800 Atlas800_SRC_FILES)

file(GLOB_RECURSE src_asm "${PROJECT_SRC_ROOT}/cpu_ops/*.S")
#foreach(X IN ITEMS ${src_asm})
#    set_source_files_properties(${X} PROPERTIES LANGUAGE CXX)
#    set_source_files_properties(${X} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
#endforeach()

aux_source_directory(${PROJECT_SRC_ROOT}/cpu_ops CpuOps_SRC_FILES)
# add host lib path
link_directories(${ACL_LIB_DIR})
add_library(atlas800 SHARED
                     ${src_asm}
                     ${CpuOps_SRC_FILES}
                     ${Atlas800_SRC_FILES}
                     ${ASCEND_BASE_SRC_FILES})
target_include_directories(atlas800 PRIVATE
	                   ${ACL_INC_DIR}
	                   ${ASCEND_BASE_DIR}
			   ${ASCEND_BASE_DIR}/Framework
			   ${PROJECT_SRC_ROOT}/include)
target_link_libraries(atlas800 ascendcl acl_dvpp pthread -Wl,-z,relro,-z,now)


#add_executable(main ${PROJECT_SRC_ROOT}/main.cpp)
#target_link_libraries(main -L${PROJECT_SRC_ROOT}/dist atlas800  -pie -s)
#target_link_libraries(main -L${PROJECT_SRC_ROOT}/dist atlas800 -s -fopenmp)

