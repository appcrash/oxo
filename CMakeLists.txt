cmake_minimum_required(VERSION 3.13)
project(oxo C)

set(CMAKE_C_STANDARD 99)


set(SOURCES
  net.c
  diagnose.c
  proxy.c
  util.c
  watcher_handler.c
  circular_buffer.c
)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    list(APPEND SOURCES io_epoll.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
    list(APPEND SOURCES io_kqueue.c)
else()
    message(FATAL_ERROR "this OS is not supported")
endif()

list(TRANSFORM SOURCES PREPEND "src/")

add_library(oxolib OBJECT ${SOURCES})
add_executable(xc $<TARGET_OBJECTS:oxolib> src/client.c)
target_include_directories(xc PUBLIC include "/usr/local/include" "/usr/include")
target_link_libraries(xc PUBLIC)

add_executable(xs $<TARGET_OBJECTS:oxolib> src/server.c)
target_include_directories(xs PUBLIC include "/usr/local/include" "/usr/include")
target_link_libraries(xs PUBLIC)
