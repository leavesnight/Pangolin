#include "ffmpeg.h"

namespace pangolin
{

FfmpegVideo::FfmpegVideo(const char *filename)
    :pFormatCtx(0)
{
    // Register all formats and codecs
    av_register_all();

    // Open video file
    if(av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)!=0)
        throw VideoException("Couldn't open file");

    // Retrieve stream information
    if(av_find_stream_info(pFormatCtx)<0)
        throw VideoException("Couldn't find stream information");

    // Dump information about file onto standard error
    dump_format(pFormatCtx, 0, filename, false);

    // Find the first video stream
    videoStream=-1;
    for(unsigned i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO)
        {
            videoStream=i;
            break;
        }
    if(videoStream==-1)
        throw VideoException("Couldn't find a video stream");

    // Get a pointer to the codec context for the video stream
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL)
        throw VideoException("Codec not found");

    // Open codec
    if(avcodec_open(pCodecCtx, pCodec)<0)
        throw VideoException("Could not open codec");

    // Hack to correct wrong frame rates that seem to be generated by some codecs
    if(pCodecCtx->time_base.num>1000 && pCodecCtx->time_base.den==1)
        pCodecCtx->time_base.den=1000;

    // Allocate video frame
    pFrame=avcodec_alloc_frame();

    // Allocate an AVFrame structure
    pFrameRGB=avcodec_alloc_frame();
    if(pFrameRGB==NULL)
        throw VideoException("Couldn't allocate frame");

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
        pCodecCtx->height);

    buffer=(uint8_t*)malloc(numBytes);

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
        pCodecCtx->width, pCodecCtx->height);
}

FfmpegVideo::~FfmpegVideo()
{
    // Free the RGB image
    free(buffer);
    av_free(pFrameRGB);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codec
    avcodec_close(pCodecCtx);

    // Close the video file
    av_close_input_file(pFormatCtx);
}


unsigned FfmpegVideo::Width() const
{
    return pCodecCtx->width;
}

unsigned FfmpegVideo::Height() const
{
    return pCodecCtx->height;
}

void FfmpegVideo::Start()
{
}

void FfmpegVideo::Stop()
{
}

bool FfmpegVideo::GrabNext(unsigned char* image, bool /*wait*/)
{
    int gotFrame = 0;

    while(!gotFrame && av_read_frame(pFormatCtx, &packet)>=0)
    {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream)
        {
            // Decode video frame
            avcodec_decode_video(pCodecCtx, pFrame, &gotFrame,
                packet.data, packet.size);
        }

        // Did we get a video frame?
        if(gotFrame)
        {
            static struct SwsContext *img_convert_ctx;

			if(img_convert_ctx == NULL) {
				const int w = pCodecCtx->width;
				const int h = pCodecCtx->height;

				img_convert_ctx = sws_getContext(w, h,
								pCodecCtx->pix_fmt,
								w, h, PIX_FMT_RGB24, SWS_BICUBIC,
								NULL, NULL, NULL);
				if(img_convert_ctx == NULL) {
					fprintf(stderr, "Cannot initialize the conversion context!\n");
					exit(1);
				}
			}
			sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

            memcpy(image,pFrameRGB->data[0],numBytes);
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    return gotFrame;
}

bool FfmpegVideo::GrabNewest(unsigned char *image, bool wait)
{
    return GrabNext(image,wait);
}

}
