#!/bin/bash
gcc -Wall -I/home/guiso/code/transcoder_new  filter.c file.c spinning_cockroach.c filters/audio_encoder.c filters/video_encoder.c filters/edges.c filters/swscale.c debug_tools.c filters/video_display.c \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libavdevice libavutil`  \
`sdl2-config --libs --cflags` -ggdb3 -O0 --std=c99 -Wall -lSDL2_image \
-pthread  -lltdl -lcrypt -lm -lltdl -g 