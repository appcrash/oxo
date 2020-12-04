cmake_minimum_required(VERSION 3.13)
project(oxo C)

set(CMAKE_C_STANDARD 99)


find_library(LIBEV ev)
if(NOT LIBEV)
  message(FATAL_ERROR "libev library not found")
endif()

set(SOURCES
  main.c
  diagnose.c
  proxy.c
  proxy_timer.c
  util.c
  watcher_handler.c
  circular_buffer.c
)
list(TRANSFORM SOURCES PREPEND "src/")

add_executable(oxo ${SOURCES})

target_include_directories(oxo PUBLIC include "/usr/local/include" "/usr/include")
target_link_libraries(oxo PUBLIC ${LIBEV})
