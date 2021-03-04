cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 14)

add_library(atlas800 SHARED IMPORTED)
set_property(TARGET atlas800 PROPERTY IMPORTED_LOCATION
             ${CMAKE_CURRENT_SOURCE_DIR}/lib/dist/libatlas800.so)
set_property(TARGET atlas800 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
             ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)

add_executable(main main.cpp)
TARGET_LINK_LIBRARIES(main -lpthread -ldl atlas800 -s -fopenmp)
target_compile_options(main  PRIVATE -w -Os -s -fdata-sections -ffunction-sections -fpermissive)

