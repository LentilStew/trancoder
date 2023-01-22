#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include <pthread.h>
#include "filter.h"
#include <string.h>
#include "debug_tools.h"
#include "file.h"

pthread_mutex_t mutex;
// only used for encoding files
file *file_create(int nb_streams, const char *filename, const char *format_name)
{
    int res;
    file *output = malloc(sizeof(file));
    if (!output)
    {
        return NULL;
    }

    output->nb_streams = nb_streams;

    res = avformat_alloc_output_context2(&output->container, NULL, format_name, filename);
    if (res < 0)
    {
        printf("Failed opening output\n");
        return NULL;
    }

    output->codec = calloc(nb_streams, sizeof(AVCodecContext *));
    output->streams = calloc(nb_streams, sizeof(AVStream *));

    output->paths = calloc(nb_streams, sizeof(filters_path *));
    output->frames = malloc(nb_streams * sizeof(int *));

    if (!output->codec || !output->paths || !output->frames)
    {
        printf("Failed allocating ram for file\n");
        return NULL;
    }

    for (int stream = 0; stream < nb_streams; stream++)
    {
        output->paths[stream] = NULL;
        output->codec[stream] = NULL;
        output->streams[stream] = NULL;
        output->frames[stream] = 0;
    }

    output->filename = strdup(filename);
    return output;
}

void file_free(file *f)
{
    if (!f)
        return;

    for (int i = 0; i < f->container->nb_streams; i++)
    {
        avcodec_free_context(&f->codec[i]);

        printf("FREEING f->paths[%i] %p\n", i, f->paths[i]);
        filter_path_uninit(f->paths[i]);
        printf("FREEING f->paths[%i] %p\n", i, f->paths[i]);
    }

    free(f->streams);
    free(f->codec);
    free(f->paths);
    free(f->frames);
    avformat_close_input(&f->container);
    free(f->filename);
    free(f);
}

// open existing file, ready for demuxing (decoding) (1 on error, 0 on success)
file *file_open(const char input_path[])
{
    file *video = malloc(sizeof(file));

    video->container = avformat_alloc_context();

    if (!video->container)
    {
        printf("Failed to alloc memory to the container of the input file\n");
        return NULL;
    }
    if (avformat_open_input(&video->container, input_path, NULL, NULL) != 0)
    {
        printf("Failed to open input file %s\n", input_path);
        return NULL;
    }
    if (avformat_find_stream_info(video->container, NULL) < 0)
    {
        printf("Failed to open read stream info\n");
        return NULL;
    }
    video->filename = strdup(input_path);
    return video;
}

// returns codec index in the container of the media type
int file_find_media_type(int media_type, file *f)
{
    for (int i = 0; i < f->container->nb_streams; i++)
    {
        if (!f->codec[i])
        {
            continue;
        }
        else if (f->codec[i]->codec_type == media_type)
        {
            return i;
        }
    }
    return -1;
}
// open codecs for decoding (1 on error, 0 on success) if video_codec or audio_codec are NULL,, default decoder will be searched, if they are equal to CODEC_SKIP, will be skiped
int file_open_codecs(file *video, const char *video_codec, const char *audio_codec)
{

    video->codec = calloc(video->container->nb_streams, sizeof(*video->codec));
    video->streams = calloc(video->container->nb_streams, sizeof(AVStream *));
    video->frames = calloc(video->container->nb_streams, sizeof(int *));
    video->paths = calloc(video->container->nb_streams, sizeof(*video->paths));
    video->nb_streams = video->container->nb_streams;
    for (unsigned int i = 0; i < video->container->nb_streams; i++)
    {
        video->paths[i] = NULL;

        const char *curr_codec;

        AVStream *stream = video->container->streams[i];
        video->streams[i] = NULL;
        const AVCodec *dec;
        AVCodecContext *codec_ctx;

        switch (stream->codecpar->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            curr_codec = video_codec;
            break;
        case AVMEDIA_TYPE_AUDIO:
            curr_codec = audio_codec;
            break;
        default:
            continue;
            break;
        }

        video->streams[i] = stream;
        if (curr_codec != NULL && (strcmp(curr_codec, CODEC_SKIP) == 0))
        {
            video->codec[i] = NULL;
            video->streams[i] = NULL;
            continue;
        }
        else if (curr_codec == NULL)
            dec = avcodec_find_decoder(stream->codecpar->codec_id);
        else
            dec = avcodec_find_decoder_by_name(video_codec);

        if (!dec)
        {
            printf("failed to find the codec %s\n", curr_codec);
            return 1;
        }

        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx)
        {
            printf("failed to alloc memory for codec context\n");
            return 1;
        }

        if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0)
        {
            printf("failed to fill codec context\n");
            return 1;
        }

        if (avcodec_open2(codec_ctx, dec, NULL) < 0)
        {
            printf("failed to open codec\n");
            return 1;
        }

        video->codec[i] = codec_ctx;
    }
    return 0;
}

/*
fills the frame with the next frame of the stream
returns 0 if the frame is filled
returns -1 if file ended
frame must be freed and allocated by the caller
packet must be freed and allocated by the caller
*/
int file_decode_frame(file *decoder, AVFrame *frame, AVPacket *packet)
{
    AVCodecContext *dec;

    while (1)
    {
        av_packet_unref(packet);
        if (av_read_frame(decoder->container, packet) < 0)
            break;

        int index = packet->stream_index;

        dec = decoder->codec[index];

        if (dec == NULL)
        {
            continue;
        }

        int response = avcodec_send_packet(dec, packet);

        if (response < 0)
        {
            printf("Error while sending packet to decoder\n");
            return 1;
        }
        while (response >= 0)
        {
            response = avcodec_receive_frame(dec, frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            {

                break;
            }
            else if (response < 0)
            {
                printf("Error while receiving frame from decoder\n");
                return -1;
            }
            if (response >= 0)
            {
                return 0;
            }
            av_frame_unref(frame);
        }
    }
    return -1;
}
//  1 if stream_nb doesn't match , 0 if success
int file_match_streams(file *f)
{
    // if the file has more streams than the container it can be matched
    if (f->nb_streams > f->container->nb_streams)
    {
        printf("number of streams doesn't match between file and container\n");
        return 1;
    }

    AVStream *stream_curr;

    int stream_index_file;
    for (int stream = 0; stream < f->container->nb_streams; stream++)
    {
        stream_curr = f->container->streams[stream];

        for (stream_index_file = 0;; stream_index_file++)
        {
            if (stream_index_file >= f->nb_streams)
            {
                stream_index_file = -1;
                break;
            }
            else if (f->streams[stream_index_file] == stream_curr)
            {
                break;
            }
        }

        // the stream wasn't found
        if (stream_index_file == -1)
        {
            continue;
        }

        // swap stream_match with stream_curr
        void *tmp = NULL;
        tmp = (void *)f->streams[stream];

        f->streams[stream] = f->streams[stream_index_file];
        f->streams[stream_index_file] = (AVStream *)tmp;

        tmp = (void *)f->codec[stream];

        f->codec[stream] = f->codec[stream_index_file];
        f->codec[stream_index_file] = (AVCodecContext *)tmp;

        tmp = (void *)f->paths[stream];

        f->paths[stream] = f->paths[stream_index_file];
        f->paths[stream_index_file] = (filters_path *)tmp;

        printf("THIS CODE HASN'T BEEN TESTED MAY BE ERROR\n");
    }
    return 0;
}

// returns a copy of the dst paths in the order of the src path
filters_path **file_match_paths(file *src, file *dst)
{
    filters_path **out = calloc(sizeof(filters_path), src->nb_streams);

    for (int stream = 0; stream < dst->nb_streams; stream++)
    {
        int src_index = file_find_media_type(dst->streams[stream]->codecpar->codec_type, src);

        if (src_index == -1)
            continue;

        out[src_index] = fp_copy(dst->paths[stream]);
    }

    return out;
}

/*
returns 1 on error, 0 on success
the extra_paths are meant to be used for the encoder paths that shouldn't be freed, for example pts delay or encode filters
*/
int file_stream(file *dec, filters_path **extra_paths)
{

    filters_path **paths = calloc(sizeof(filters_path *), dec->nb_streams);
    file_match_streams(dec);

    for (int s = 0; s < dec->nb_streams; s++)
    {
        if (extra_paths)
            paths[s] = fp_append(fp_copy(dec->paths[s]), fp_copy(extra_paths[s]));
        else
            paths[s] = fp_copy(dec->paths[s]);

        filters_path *frame_free = filter_path_create();
        frame_free->filter_frame = filter_frame_free;
        frame_free->is_init = 1;
        paths[s] = fp_append(paths[s], frame_free);
    }
    AVFrame *frame;

    AVPacket *packet = av_packet_alloc();

    frame_list *curr = dec->fl;
    while (1)
    {

        int frame_index;
        int res;
        // instead of checking the last curr farme to be NULL, and try to decode the frames left in the decoder, should check the end of the linked list TODO
        if (!dec->fl)
        {
            frame = av_frame_alloc();
            res = file_decode_frame(dec, frame, packet);
            frame_index = packet->stream_index;
        }
        else if (curr != NULL)
        {

            res = 0;
            frame_index = file_find_media_type(curr->media_type, dec);
            
            if (frame_index == -1)
            {
                curr = curr->next;
                continue;
            }
            
            frame = frame_copy(curr->frame, curr->media_type);
            curr = curr->next;

        }
        else
        {
            res = -1;
        }

        if (res == 0)
        {
            filter_path_apply(paths[frame_index], frame);
        }
        else if (res == 1)
        {
            printf("Error decoding a frame\n");
            return 1;
        }
        else if (res == -1)
        {
            break;
        }
    }

    // av_frame_free(&frame);
    av_packet_free(&packet);
    filters_path *head = NULL;
    filters_path *tmp = NULL;

    // I want to free the copy of the path, not the actual path
    for (int s = 0; s < dec->nb_streams; s++)
    {
        head = paths[s];
        while (head != NULL)
        {
            tmp = head->next;

            free(head);

            head = tmp;
        }
    }

    free(paths);

    return 0;
}

typedef struct params_save_frame_fl
{
    frame_list *root;
    frame_list *curr;
    enum AVMediaType type;
    int free;
} params_save_frame_fl;

AVFrame *save_frame_fl(filters_path *filter_props, AVFrame *frame)
{
    params_save_frame_fl *params = filter_props->filter_params;
    if (frame == NULL)
        return frame;

    if (params->root == NULL)
    {
        params->root = frame_list_create();
        params->curr = params->root;
    }
    else
    {
        params->curr->next = frame_list_create();
        params->curr = params->curr->next;
    }

    params->curr->frame = frame_copy(frame, params->type);

    params->curr->media_type = params->type;

    return frame;
}
void frame_list_print(frame_list *fl)
{
    if (fl == NULL)
        return;

    printf("%s %p\n", fl->media_type == 0 ? "VIDEO" : "AUDIO", fl->frame);
    frame_list_print(fl->next);
}

frame_list *file_to_frame_list(file *f)
{
    params_save_frame_fl *save_frame_params;
    for (int s = 0; s < f->nb_streams; s++)
    {
        if (!f->codec[s])
            continue;

        filters_path *save_frame_fl_step = filter_path_create();

        save_frame_params = calloc(sizeof(params_save_frame_fl), 1);
        save_frame_params->root = NULL;
        save_frame_params->curr = NULL;
        save_frame_params->type = f->streams[s]->codecpar->codec_type;

        save_frame_fl_step->filter_frame = save_frame_fl;
        save_frame_fl_step->is_init = 1;
        save_frame_fl_step->filter_params = save_frame_params;
        f->paths[s] = fp_append(f->paths[s], save_frame_fl_step);
    }

    file_stream(f, NULL);
    for (int i = 0; i < f->nb_streams; i++)
    {
        filter_path_uninit(f->paths[i]);
        f->paths[i] = 0;
    }
    return save_frame_params->root;
}
