include_directories(../include)

add_executable(hello hello.c)
target_link_libraries(hello apue)

add_executable(shell1 shell1.c)
target_link_libraries(shell1 apue)