#!/bin/bash
gcc -Wall -I/home/guiso/code/transcoder_new  filter.c file.c video_queue.c album.c \
filters/audio_encoder.c filters/video_encoder.c filters/edges.c filters/swscale.c filters/pts.c filters/delay2.c debug_tools.c \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libavdevice libavutil`  \
-pthread  -lltdl -lcrypt -lm -lltdl -g  