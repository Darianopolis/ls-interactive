cmake_minimum_required(VERSION 4.0)
project(ls-interactive)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(ls-interactive src/ls-interactive.cpp)
target_compile_options(ls-interactive PUBLIC --std=c++20)

install(TARGETS ls-interactive DESTINATION bin)
install(PROGRAMS src/ls-interactive.sh DESTINATION bin)
