cmake_minimum_required(VERSION 3.26)
project(li)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(
        /Zc:preprocessor
        /Zc:__cplusplus
        /utf-8)
add_executable(li
        ${CMAKE_SOURCE_DIR}/li.cpp)
target_compile_definitions(li PRIVATE)
target_include_directories(li PRIVATE)
target_link_libraries(li)
set_target_properties(li PROPERTIES LINKER_LANGUAGE CXX)
add_custom_command(TARGET li POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:li>
        ${CMAKE_SOURCE_DIR}/bin/li-win.exe)
