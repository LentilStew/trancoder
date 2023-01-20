#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include "filter.h"
#include "audio_resample.h"

void audio_resample_init(filters_path *filter_step)
{
    audio_resample_params *params = (audio_resample_params *)filter_step->filter_params;

    // Initialize the resample context with the desired output sample rate, channel layout, and sample format

    printf("params->out_channel_layout,  %li\n\
    params->out_sample_fmt, %i\n\
    params->out_sample_rate, %i\n\
    params->in_channel_layout, %li\n\
    params->in_sample_fmt,%i\n\
    params->in_sample_rate%i \n",
           params->out_channel_layout, params->out_sample_fmt, params->out_sample_rate,
           params->in_channel_layout, params->in_sample_fmt, params->in_sample_rate);
    params->resample_context = swr_alloc_set_opts(NULL,                       // we're allocating a new context
                                                  params->out_channel_layout, // out_ch_layout
                                                  params->out_sample_fmt,     // out_sample_fmt
                                                  params->out_sample_rate,    // out_sample_rate
                                                  params->in_channel_layout,  // in_ch_layout
                                                  params->in_sample_fmt,      // in_sample_fmt
                                                  params->in_sample_rate,     // in_sample_rate
                                                  0,                          // log_offset
                                                  NULL);                      // log_ctx


    // Initialize the resampling context

    int ret = swr_init(params->resample_context);
    if (ret < 0)
    {
        printf("Error initializing resample context %s\n", av_err2str(ret));
        exit(1);
        return;
    }
}

AVFrame *add_samples(AVFrame *in, int samples_to_add)
{
    // Allocate memory for larger output frame
    AVFrame *out = av_frame_alloc();
    if (!out)
    {
        fprintf(stderr, "Error allocating output frame\n");
        exit(1);
    }
    out->format = in->format;
    out->channel_layout = in->channel_layout;
    out->sample_rate = in->sample_rate;
    out->nb_samples = in->nb_samples + samples_to_add;
    if (av_frame_get_buffer(out, 0) < 0)
    {
        fprintf(stderr, "Error allocating output frame buffer\n");
        exit(1);
    }

    // Copy data from input frame to output frame
    av_samples_copy(out->data, in->data, 0, 0, in->nb_samples, in->channels, in->format);

    // Pad output frame with silence
    av_samples_set_silence(out->data, in->nb_samples, samples_to_add, in->channels, in->format);

    // Update the number of samples in the output frame
    out->nb_samples = in->nb_samples + samples_to_add;
    return out;
}

AVFrame *audio_resample(filters_path *filter_step, AVFrame *frame)
{
    audio_resample_params *params = filter_step->filter_params;

    AVFrame *resampled_frame = av_frame_alloc();
    if (!resampled_frame)
    {
        printf("Could not allocate resampled frame\n");
        exit(1);
    }

    // Set the resampled frame parameters
    resampled_frame->format = params->out_sample_fmt;
    resampled_frame->channel_layout = params->out_channel_layout;
    resampled_frame->sample_rate = params->out_sample_rate;

    int64_t delay = swr_get_delay(params->resample_context, params->in_sample_rate);
    resampled_frame->nb_samples = av_rescale_rnd(delay + frame->nb_samples,
                                                 params->out_sample_rate, params->in_sample_rate, AV_ROUND_UP);
    //printf("%i %i\n", frame->nb_samples, resampled_frame->nb_samples);

    // Allocate the resampled frame's data buffer
    int ret = av_frame_get_buffer(resampled_frame, 0);
    if (ret < 0)
    {
        printf("Could not allocate resampled frame data buffer\n");
        exit(1);
    }
    //printf("%i\n", resampled_frame->nb_samples);

    //printf("RESAMPLING\n");
    // Resample the input frame to the resampled frame
    ret = swr_convert_frame(params->resample_context, resampled_frame, frame);
    if (ret < 0)
    {
        printf("Error resampling audio frame %s\n", av_err2str(ret));
        exit(1);
    }

    return resampled_frame;
}

void audio_resample_destroy(filters_path *filter_step)
{
    audio_resample_params *params = (audio_resample_params *)filter_step->filter_params;

    // Free the resampling context and any associated resources
    swr_free(&params->resample_context);
    free(params);
}

filters_path *audio_resample_create(int in_sample_rate, int out_sample_rate,
                                    int64_t in_channel_layout, int64_t out_channel_layout,
                                    enum AVSampleFormat in_sample_fmt, enum AVSampleFormat out_sample_fmt)
{
    filters_path *filter_step = filter_path_create();

    filter_step->filter_frame = audio_resample;
    filter_step->uninit = audio_resample_destroy;
    filter_step->init = audio_resample_init;
    // Allocate audio resample params structure
    audio_resample_params *params = (audio_resample_params *)malloc(sizeof(audio_resample_params));
    if (!params)
    {
        printf("Error allocating audio resample params\n");
        return NULL;
    }

    // Set the input and output sample rates, channel layouts, and sample formats
    params->in_sample_rate = in_sample_rate;
    params->out_sample_rate = out_sample_rate;
    params->in_channel_layout = in_channel_layout;
    params->out_channel_layout = out_channel_layout;
    params->in_sample_fmt = in_sample_fmt;
    params->out_sample_fmt = out_sample_fmt;

    params->out_frame_nb_sample = 1024;

    filter_step->filter_name = "Audio resample";

    filter_step->filter_params = params;
    return filter_step;
}
