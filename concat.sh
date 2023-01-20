#!/bin/bash
gcc -Wall -I/home/guiso/code/code/tn  filter.c file.c video_queue.c concat.c \
filters/audio_encoder.c filters/audio_hash.c filters/audio_resample.c filters/samples_per_frame.c filters/video_encoder.c filters/ffmpeg_filter_video.c filters/ffmpeg_filter_audio.c filters/edges.c filters/swscale.c filters/pts.c filters/delay2.c  debug_tools.c \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libswresample libavdevice libavutil`  \
-pthread   -lcrypt -lm  -g  -ggdb