cmake_minimum_required(VERSION 3.13)
project(oxo C)

set(CMAKE_C_STANDARD 99)


set(SOURCES
  main.c
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

add_executable(oxo ${SOURCES})

target_include_directories(oxo PUBLIC include "/usr/local/include" "/usr/include")
target_link_libraries(oxo PUBLIC)
