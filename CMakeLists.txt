cmake_minimum_required(VERSION 3.26)
project(li)

add_executable(li ${CMAKE_SOURCE_DIR}/li.cpp)
target_compile_features(li PUBLIC cxx_std_23)
add_custom_command(TARGET li POST_BUILD COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:li> ${CMAKE_SOURCE_DIR}/bin/li-win.exe
        )
