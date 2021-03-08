cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 14)

add_executable(main main.cpp)
TARGET_LINK_LIBRARIES(main -lpthread -fopenmp -ldl -s )

