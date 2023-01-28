cmake_minimum_required(VERSION 3.10)
project(myproject)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu11 -I//home/guiso/code2/tn -mavx512dq -mavx -mavx2 -mpclmul -msse -msse4.1 -mssse3 -march=native -g -ggdb -pthread -lcrypt -lm")

find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED libavformat libavfilter libavutil libavcodec libswscale libswresample libavdevice libavutil)

add_executable(myproject
    filters/filter.c
    file.c
    video_queue.c
    concat.c
    filters/audio_encoder.c
    filters/video_hash.c
    filters/audio_hash.c
    filters/audio_resample.c
    filters/samples_per_frame.c
    filters/video_encoder.c
    filters/ffmpeg_filter_video.c
    filters/ffmpeg_filter_audio.c
    filters/edges.c
    filters/swscale.c
    filters/pts.c
    filters/delay2.c
    debug_tools.c
)
find_package(Threads REQUIRED)
target_include_directories(myproject PRIVATE ${AV_INCLUDE_DIRS})
target_link_libraries(myproject pthread)
target_link_libraries(myproject m)
target_link_libraries(myproject ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(myproject ${AV_LIBRARIES})