/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _ENCODE_H_
#define _ENCODE_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

typedef struct
{
    AVCodec *codec;
    AVCodecContext *ctx;
    AVDictionary *options;
    AVFrame *frame;
    AVPacket *pkt;
    struct SwsContext *sws_ctx;
} EncodeContext;

EncodeContext *encode_context_init(uint32_t width, uint32_t height, AVCodecID codec_id, std::string encode_preset, std::string encode_tune, int bit_rate, int fps);

#include <muxing.h>

void receive_packet_thread(EncodeContext ectx, MuxingContext mctx, bool threads_stop_running);
void encode_raw_image_to_frame(EncodeContext *ectx, int width, int height, uint8_t *image_buffer);
int encode_context_free(EncodeContext *ectx);

#endif // _ENCODE_H_
