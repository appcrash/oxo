cmake_minimum_required(VERSION 3.15)
project(oxo C)

set(CMAKE_C_STANDARD 99)


find_library(LIBEV ev)
if(NOT LIBEV)
  message(FATAL_ERROR "libev library not found")
endif()

file(GLOB SOURCES "src/*.c")
add_executable(oxo ${SOURCES})

target_include_directories(oxo PUBLIC include "/usr/local/include" "/usr/include")
target_link_libraries(oxo PUBLIC ${LIBEV})
