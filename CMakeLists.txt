cmake_minimum_required(VERSION 3.24)
project(FaceRelay)

set(CMAKE_CXX_STANDARD 17)

add_executable(FaceRelay main.cpp)

install(TARGETS FaceRelay)