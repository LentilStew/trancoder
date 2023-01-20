#!/bin/bash
gcc -Wall -I/home/guiso/code/transcoder_new  ../filter.c ../file.c ../filters/swscale.c ../debug_tools.c ../test/test_frame_list.c \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libavdevice libavutil`  \
-pthread  -lltdl -lcrypt -lm -lltdl -g -o test_frame_list.o