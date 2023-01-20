#include "video_encoder.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include <libavutil/time.h>
#include <pthread.h>
#include "file.h"

void filter_encode_video_init(filters_path *filter_step)
{
    filter_video_encode_params *params = filter_step->filter_params;
    AVCodec *codec = avcodec_find_encoder_by_name(params->encoder);
    if (!codec)
    {
        fprintf(stderr, "Codec not found %s", params->encoder);
        exit(1);
    }

    AVCodecContext *cod_ctx = avcodec_alloc_context3(codec);
    if (!cod_ctx)
    {
        fprintf(stderr, "Could not allocate video codec context");
        exit(1);
    }
    params->cod_ctx[0] = cod_ctx;

    cod_ctx->width = params->width;
    cod_ctx->height = params->height;
    cod_ctx->pix_fmt = params->pix_fmt;
    cod_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    cod_ctx->sample_aspect_ratio = params->sample_aspect_ratio;
    cod_ctx->time_base = params->time_base;
    cod_ctx->framerate = params->framerate;
    cod_ctx->bit_rate = params->bit_rate;
    cod_ctx->gop_size = 12;
    cod_ctx->codec_tag = 0;
    if (params->container->oformat->flags & AVFMT_GLOBALHEADER)
    {
        cod_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    AVStream *stream = avformat_new_stream(params->container, codec);
    if (!params->stream)
    {
        fprintf(stderr, "Could not allocate stream");
        exit(1);
    }
    params->stream[0] = stream;

    stream->time_base = cod_ctx->time_base;

    if (avcodec_parameters_from_context(stream->codecpar, cod_ctx) < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters");
        exit(1);
    }

    if (avcodec_open2(cod_ctx, codec, &params->codec_options) < 0)
    {
        fprintf(stderr, "Could not open codec");
        exit(1);
    }

    stream->codecpar->extradata_size = cod_ctx->extradata_size;
    stream->codecpar->extradata = av_mallocz(cod_ctx->extradata_size);
    memcpy(stream->codecpar->extradata, cod_ctx->extradata, cod_ctx->extradata_size);

    params->packet = av_packet_alloc();
    if (!params->packet)
    {
        fprintf(stderr, "Could not allocate packet");
        exit(1);
    }

    params->frame_duration = (int64_t)(av_q2d(av_div_q((AVRational){stream->time_base.den, 1}, cod_ctx->framerate)));
    params->time_base = stream->time_base;
}

void filter_encode_video_uninit(filters_path *filter_props)
{
    printf("filter_encode_video_uninit %p\n", filter_props);
    filter_video_encode_params *params = filter_props->filter_params;
    avcodec_free_context(params->cod_ctx);
    av_packet_free(&params->packet);
    av_dict_free(&params->codec_options);

    free(params);
}

AVFrame *filter_encode_video(filters_path *filter_props, AVFrame *frame)
{
    printf("ENCODING VIDEO\n");

    filter_video_encode_params *params = filter_props->filter_params;
    AVCodecContext *cod_ctx = *params->cod_ctx;
    AVStream *stream = *params->stream;
    if (frame)
    {
        frame->pict_type = AV_PICTURE_TYPE_NONE;
    }

    int ret = avcodec_send_frame(cod_ctx, frame);

    while (ret >= 0)
    {

        av_packet_unref(params->packet);

        ret = avcodec_receive_packet(cod_ctx, params->packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            if (ret == AVERROR_EOF)
            {
                printf("EOF\n");
            }
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding video frame: %s", av_err2str(ret));
            exit(1);
        }
        else if (ret == AVERROR(EINVAL))
        {
            printf("ERROR HERE\n");
            exit(0);
        }
        if (frame)
        {
            params->packet->dts = frame->pts;
            params->packet->pts = frame->pts;
        }
        params->packet->stream_index = stream->index;
        params->packet->duration = (int64_t)params->frame_duration;
        /*
        params->packet->stream_index = stream->index;
        params->packet->pts = (int64_t)(params->frame_duration * params->packets);
        params->packet->dts = params->packet->pts;
        params->packet->duration = (int64_t)params->frame_duration;
*/
        params->packets++;
            //printf("ENCODING FRAME FR VIDEO\n");

        pthread_mutex_lock(&mutex);
        int res = av_interleaved_write_frame(params->container, params->packet);
        if (res != 0)
        {
            printf("ERROR ENCODING VIDEO FRAME \"%s\"\n", av_err2str(res));
            exit(0);
        }
        pthread_mutex_unlock(&mutex);
    }

    return frame;
}

filters_path *filter_encode_video_create(AVCodecContext **cod_ctx,
                                         AVFormatContext *container,
                                         AVStream **stream,
                                         const char *encoder,
                                         int width,
                                         int height,
                                         enum AVPixelFormat pix_fmt,
                                         AVRational sample_aspect_ratio,
                                         AVRational time_base,
                                         AVRational framerate,
                                         int bit_rate,
                                         AVDictionary *codec_options)
{
    printf("cod_ctx %p\n", cod_ctx);
    printf("container %p\n", container);
    printf("encoder %s\n", encoder);
    printf("width %i\n", width);
    printf("height %i\n", height);
    printf("pix_fmt %i\n", pix_fmt);
    printf("sample_aspect_ratio %i %i\n", sample_aspect_ratio.num, sample_aspect_ratio.den);
    printf("time_base %i %i\n", time_base.num, time_base.den);
    printf("framerate %i %i\n", framerate.num, framerate.den);
    printf("bit_rate %i\n", bit_rate);
    filters_path *filter_step = filter_path_create();
    if (filter_step == NULL)
    {
        return NULL;
    }
    filter_video_encode_params *params = calloc(sizeof(filter_video_encode_params), 1);
    if (params == NULL)
    {
        return NULL;
    }
    params->cod_ctx = cod_ctx;
    params->container = container;
    params->encoder = encoder;
    params->width = width;
    params->height = height;
    params->pix_fmt = pix_fmt;
    params->sample_aspect_ratio = sample_aspect_ratio;
    params->time_base = time_base;
    params->framerate = framerate;
    params->bit_rate = bit_rate;
    params->codec_options = codec_options;
    params->frames = 0;
    params->packets = 0;
    params->frame_duration = 0;
    params->stream = stream;
    params->packet = NULL;

    filter_step->filter_params = params;
    filter_step->init = filter_encode_video_init;
    filter_step->uninit = filter_encode_video_uninit;
    filter_step->filter_frame = filter_encode_video;
    filter_step->multiple_output = 0;
    filter_step->filter_name = "video_encoder";

    return filter_step;
}
