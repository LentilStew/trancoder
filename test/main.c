#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filters/audio_encoder.h"
#include "filters/video_encoder.h"
#include "filters/edges.h"
#include "file.h"

int main()
{
    av_log_set_level(AV_LOG_DEBUG);
    // char *input_file = https://clips-media-assets2.twitch.tv/AT-cm%7CDrXvgXJ5clyGcV2zdNm2tA-360.mp4"https://clips-media-assets2.twitch.tv/ygOethlRyc8ggR1OhKaiqg/AT-cm%7CygOethlRyc8ggR1OhKaiqg.mp4";
    char *output_file = "output/test.mp4";
    char *video_encoder = "libx264";
    char *audio_encoder = "aac";
    // CREATE INPUT
    file *input = malloc(sizeof(file));
    int res;
    res = file_open(input, "https://clips-media-assets2.twitch.tv/AT-cm%7CDrXvgXJ5clyGcV2zdNm2tA-360.mp4");
    if (res != 0)
    {
        printf("Failed opening input\n");
        return -1;
    }

    res = file_open_codecs(input, NULL, NULL);
    if (res != 0)
    {
        printf("Failed opening codecs\n");
        return -1;
    }

    AVStream *input_video_stream = input->container->streams[file_find_media_type(AVMEDIA_TYPE_VIDEO, input)];
    AVStream *input_audio_stream = input->container->streams[file_find_media_type(AVMEDIA_TYPE_AUDIO, input)];

    // CREATE OUTPUT

    file *output = file_create(2, output_file, "mp4");
    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    filters_path *fp = filter_encode_audio_create(output->codec[1], output->container, audio_encoder, 2, input_audio_stream->codecpar->sample_rate, 128000);
    output->paths[1] = fp;
    if (!fp)
    {
        printf("failed creating filter\n");
        return 1;
    }

    AVRational framerate = av_guess_frame_rate(input->container, input_video_stream, NULL);
    filters_path *fp2 = filter_edges_create();

    AVDictionary *codec_options = NULL;
    av_dict_set(&codec_options, "preset", "fast", 0);

    fp2->next = filter_encode_video_create(output->codec[0], output->container, video_encoder,
                                           input_video_stream->codecpar->width,
                                           input_video_stream->codecpar->height,
                                           input_video_stream->codecpar->format,
                                           input_video_stream->codecpar->sample_aspect_ratio,
                                           av_inv_q(framerate),
                                           framerate,
                                           1000000,
                                           codec_options);
    output->paths[0] = fp2;
    if (!fp2)
    {
        printf("failed creating filter\n");
        return 1;
    }

    filter_path_init(fp2);
    filter_path_init(fp);

    // INITIALIZE OUTPUT
    /*
    if (output->container->oformat->flags & AVFMT_GLOBALHEADER)
        output->container->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    */
    if (!(output->container->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&output->container->pb, output_file, AVIO_FLAG_WRITE) < 0)
        {
            printf("could not open the output file %s", output_file);
            return 1;
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(output->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        return 1;
    }
    
    av_dump_format(output->container, 0, NULL, 1);

    // TRANSCODE
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();

    while (1)
    {
        res = file_decode_frame(input, frame, packet);
        if (res == 1)
        {
            printf("Error decoding a frame\n");
            return 1;
        }
        else if (res == 0)
        {
            frame = filter_path_apply(output->paths[packet->stream_index], frame);
        }
        else if (res == -1)
        {
            printf("File ended\n");
            break;
        }
    }

    // CLEANUP
    filter_path_apply(output->paths[0], NULL);
    filter_path_apply(output->paths[1], NULL);
    filter_path_uninit(output->paths[0]);
    filter_path_uninit(output->paths[1]);
    free(output->paths[0]);
    free(output->paths[1]);
    av_write_trailer(output->container);
    av_frame_free(&frame);
    av_packet_free(&packet);
    file_free(input);
    file_free(output);
}