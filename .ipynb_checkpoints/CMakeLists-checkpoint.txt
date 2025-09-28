cmake_minimum_required(VERSION 3.10)
project(apue.3e)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -ansi -D_GNU_SOURCE -DLINUX")

add_subdirectory(lib)
add_subdirectory(intro)
add_subdirectory(advio)
add_subdirectory(environ)
