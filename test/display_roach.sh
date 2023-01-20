#!/bin/bash
gcc -Wall -I/home/guiso/code/transcoder_new  ../filter.c ../file.c ../filters/swscale.c ../filters/video_display.c ../filters/delay.c ../debug_tools.c ../test/display_roach.c \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libavdevice libavutil`  \
`sdl2-config --libs --cflags` -ggdb3 -O0 --std=c99 -Wall -lSDL2_image \
-pthread  -lltdl -lcrypt -lm -lltdl -g -o display_roach.o