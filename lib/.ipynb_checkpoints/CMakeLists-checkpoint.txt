aux_source_directory(. SRC_LIST)

include_directories(../include)

add_library(apue STATIC ${SRC_LIST})