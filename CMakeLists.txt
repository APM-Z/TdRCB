cmake_minimum_required(VERSION 3.10)
project(TdRCB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

include_directories(include)

file(GLOB SOURCES "src/*.cpp")
add_executable(TdRCB ${SOURCES})

if(UNIX AND NOT MSVC)
    target_link_libraries(TdRCB m)
endif()

if(WIN32)
    target_compile_definitions(TdRCB PRIVATE _WIN32)
endif()