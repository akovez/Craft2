cmake_minimum_required(VERSION 2.8)

project(craft)

FILE(GLOB SOURCE_FILES src/*.c*)

add_executable(
        craft
        ${SOURCE_FILES}
        deps/glew/src/glew.c
        deps/lodepng/lodepng.c
        deps/noise/noise.c
        deps/sqlite/sqlite3.c
        deps/tinycthread/tinycthread.c)

add_definitions(-std=c99 -O0 -mavx -g -gdwarf-2 -mavx2)

add_subdirectory(deps/glfw)
include_directories(deps/glew/include)
include_directories(deps/glfw/include)
include_directories(deps/lodepng)
include_directories(deps/noise)
include_directories(deps/sqlite)
include_directories(deps/tinycthread)

if(MINGW)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH}
            "C:/Program Files/CURL/lib" "C:/Program Files (x86)/CURL/lib")
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH}
            "C:/Program Files/CURL/include" "C:/Program Files (x86)/CURL/include")
endif()

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})

if(APPLE)
    target_link_libraries(craft glfw
            ${GLFW_LIBRARIES} ${CURL_LIBRARIES})
endif()

if(UNIX)
    message(UNITX!!)
    target_link_libraries(craft dl glfw  GL GLU
            ${GLFW_LIBRARIES} ${CURL_LIBRARIES})
endif()

if(MINGW)
    target_link_libraries(craft ws2_32.lib glfw
            ${GLFW_LIBRARIES} ${CURL_LIBRARIES})
endif()

file(COPY textures DESTINATION ${CMAKE_BINARY_DIR})
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
