cmake_minimum_required(VERSION 3.13)
project(oxo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # generate compile_commands.json for rtags

set(OXOLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(OXOLIB_SOURCES
  net.c
  diagnose.c
  proxy.c
  util.c
  watcher_handler.c
  circular_buffer.c
)
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    list(APPEND OXOLIB_SOURCES io_epoll.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
    list(APPEND OXOLIB_SOURCES io_kqueue.c)
else()
    message(FATAL_ERROR "this OS is not supported")
endif()
list(TRANSFORM OXOLIB_SOURCES PREPEND "src/")
add_library(oxolib OBJECT ${OXOLIB_SOURCES})


set(CLIENT
  main.c
  socks.c
)
list(TRANSFORM CLIENT PREPEND "src/client/")

set(SERVER
  main.c
)
list(TRANSFORM SERVER PREPEND "src/server/")


add_executable(xc $<TARGET_OBJECTS:oxolib> ${CLIENT})
target_include_directories(xc PUBLIC include ${OXOLIB_DIR} "/usr/local/include" "/usr/include")
target_link_libraries(xc PUBLIC)

add_executable(xs $<TARGET_OBJECTS:oxolib> ${SERVER})
target_include_directories(xs PUBLIC include ${OXOLIB_DIR} "/usr/local/include" "/usr/include")
target_link_libraries(xs PUBLIC)

execute_process(
  COMMAND {CMAKE_CURRENT_SOURCE_DIR}/scripts/refresh-compile-command.sh
)
