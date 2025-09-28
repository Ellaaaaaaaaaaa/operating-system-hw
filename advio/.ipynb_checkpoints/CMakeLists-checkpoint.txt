include_directories(../include)

add_executable(deadlock deadlock.c)
target_link_libraries(deadlock apue)

