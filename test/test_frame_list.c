#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "file.h"
#include "filter.h"
#include "debug_tools.h"
#include "filters/swscale.h"
#include <malloc.h>

int main()
{
    char *file_name = "../video_inputs/roach.mp4";
    file *input = malloc(sizeof(file));
    int res;

    res = file_open(input, file_name);
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

    frame_list *fl = file_to_frame_list(input);

    while (fl != NULL)
    {
        if (fl->media_type == AVMEDIA_TYPE_VIDEO)
        {
            save_gray_frame(fl->frame, NULL);
        }
        fl = fl->next;
    }
}
