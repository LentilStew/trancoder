#ifndef FILE22_H_
#define FILE22_H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filter.h"
#include "debug_tools.h"

typedef struct file
{
    AVFormatContext *container;
    AVCodecContext **codec;
    filters_path **paths;
    AVStream **streams;
    frame_list *fl;
    int *frames;
    int nb_streams;
    char *filename;
} file;

extern pthread_mutex_t mutex;

file *file_create(int nb_streams, const char *filename, const char *format_name);

void file_free(file *f);

// open existing file, ready for demuxing (decoding) (1 on error, 0 on success)
file *file_open(const char input_path[]);

// returns codec index in the container of the media type
int file_find_media_type(int media_type, file *f);

#define CODEC_SKIP "SKIP"

// open codecs for decoding (1 on error, 0 on success) if video_codec or audio_codec are NULL,, default decoder will be searched, if they are equal to CODEC_SKIP, will be skiped
int file_open_codecs(file *video, const char *video_codec, const char *audio_codec);
/*
fills the frame with the next frame of the stream
returns 0 if the frame is filled
returns -1 if file ended
frame must be freed and allocated by the caller
packet must be freed and allocated by the caller
*/
int file_decode_frame(file *decoder, AVFrame *frame, AVPacket *packet);

// if 1 stream_nb doesn't match , 0 if success
int file_match_streams(file *f);

filters_path **file_match_paths(file *src, file *dst);

/*
returns 1 on error, 0 on success
the extra_paths are meant to be used for the encoder paths that shouldn't be freed, for example pts delay or encode filters
*/
int file_stream(file *dec, filters_path **extra_paths);

void filter_path_flush(filters_path *path);

// video linked list

// file must me ready for demuxing and decoding
frame_list *file_to_frame_list(file *f);
int frame_list_stream(frame_list *fl, file *enc);

#endif /*FILE_H_*/