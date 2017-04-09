cmake_minimum_required(VERSION 3.6)
project(v4l_grab)

set(CMAKE_C_STANDARD 99)

#set(LDFLAGS  ${LDFLAGS})
#set(CMAKE_C_FLAGS -lv4l2)

set(SOURCE_FILES main.c jpg.c)
add_executable(v4l2_grab_jpg ${SOURCE_FILES})
target_link_libraries(v4l2_grab_jpg v4l2 jpeg)