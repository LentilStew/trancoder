#include "audio_encoder.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include <libavutil/time.h>
#include <pthread.h>
#include <libavutil/channel_layout.h>

void filter_encode_audio_init(filters_path *filter_step)
{
    filter_audio_encode_params *params = filter_step->filter_params;
    AVCodec *codec = avcodec_find_encoder_by_name(params->encoder);
    if (!codec)
    {
        fprintf(stderr, "Codec not found %s", params->encoder);
        exit(1);
    }
    AVCodecContext *cod_ctx = avcodec_alloc_context3(codec);

    if (!params->cod_ctx)
    {
        fprintf(stderr, "Could not allocate audio codec context");
        exit(1);
    }
    params->cod_ctx[0] = cod_ctx;

    cod_ctx->bit_rate = params->bit_rate;
    cod_ctx->sample_fmt = params->sample_fmt;
    cod_ctx->sample_rate = params->sample_rate;
    cod_ctx->channels = params->channels;
    cod_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    cod_ctx->channel_layout = av_get_default_channel_layout(params->channels);
    cod_ctx->time_base = (AVRational){1, params->sample_rate};

    AVStream *stream = avformat_new_stream(params->container, codec);
    if (!params->stream)
    {
        fprintf(stderr, "Could not allocate stream");
        exit(1);
    }
    params->stream[0] = stream;
    stream->time_base = cod_ctx->time_base;
    stream->codecpar->frame_size = cod_ctx->frame_size;
    if (avcodec_open2(cod_ctx, codec, &params->codec_options) < 0)
    {
        fprintf(stderr, "Could not open codec");
        exit(1);
    }

    if (avcodec_parameters_from_context(stream->codecpar, cod_ctx) < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters");
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
}

void filter_encode_audio_uninit(filters_path *filter_props)
{
    printf("filter_encode_audio_uninit %p\n", filter_props);
    filter_audio_encode_params *params = filter_props->filter_params;
    avcodec_free_context(params->cod_ctx);
    av_packet_free(&params->packet);

    free(params);
}

AVFrame *filter_encode_audio(filters_path *filter_props, AVFrame *frame)
{
    filter_audio_encode_params *params = filter_props->filter_params;
    AVCodecContext *cod_ctx = *params->cod_ctx;
    AVStream *stream = *params->stream;
    printf("ENCODING AUDIO\n");
    int ret = avcodec_send_frame(cod_ctx, frame);
    if (ret > 0)
    {
        printf("ERROR SENFING FRAME TO ENCODER \"%s\"\n", av_err2str(ret));
        exit(1);
    }
    while (ret >= 0)
    {
        av_packet_unref(params->packet);
        ret = avcodec_receive_packet(cod_ctx, params->packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding audio frame: %s", av_err2str(ret));
            exit(1);
        }
        if (frame)
        {
            params->packet->dts = frame->pts;
            params->packet->pts = frame->pts;
            params->packet->duration = frame->pkt_duration;
        }
        params->packet->stream_index = stream->index;
        /*
        params->packet->stream_index = stream->index;
        params->packet->pts = (int64_t)(params->frame_duration * params->packets);
        params->packet->dts = params->packet->pts;
        params->packet->duration = (int64_t)params->frame_duration;
        params->packets++;
        */

        pthread_mutex_lock(&mutex);
        int res = av_interleaved_write_frame(params->container, params->packet);
        if (res != 0)
        {
            printf("ERROR ENCODING AUDIO FRAME \"%s\"\n", av_err2str(res));
            exit(0);
        }
        pthread_mutex_unlock(&mutex);
    }

    return frame;
}

filters_path *filter_encode_audio_create(AVCodecContext **cod_ctx,
                                         AVFormatContext *container,
                                         AVStream **stream,
                                         const char *encoder,
                                         int sample_fmt,
                                         int channels,
                                         int sample_rate,
                                         int bit_rate,
                                         AVDictionary *codec_options)
{
    filters_path *filter_step = filter_path_create();
    if (filter_step == NULL)
    {
        return NULL;
    }
    filter_audio_encode_params *params = malloc(sizeof(filter_audio_encode_params));
    if (params == NULL)
    {
        return NULL;
    }
    params->stream = stream;
    params->start_time = av_gettime();
    params->cod_ctx = cod_ctx;
    params->container = container;
    params->sample_fmt = sample_fmt;
    params->encoder = encoder;
    params->channels = channels;
    params->sample_rate = sample_rate;
    params->bit_rate = bit_rate;
    params->frames = 0;
    params->packets = 0;
    params->stream = stream;
    params->packet = NULL;
    params->frame_duration = 0;
    params->codec_options = codec_options;
    filter_step->filter_params = params;
    filter_step->init = filter_encode_audio_init;
    filter_step->uninit = filter_encode_audio_uninit;
    filter_step->filter_frame = filter_encode_audio;
    filter_step->multiple_output = 0;
    filter_step->filter_name = "audio_encoder";

    return filter_step;
}
