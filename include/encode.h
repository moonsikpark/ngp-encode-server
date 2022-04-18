/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _ENCODE_H_
#define _ENCODE_H_

#include <common.h>

std::string averror_explain(int err);

class AVCodecContextManager
{
private:
    AVCodecContext *_ctx;
    std::mutex _mutex;

public:
    AVCodecContextManager(AVCodecID codec_id, AVPixelFormat pix_fmt, std::string x264_encode_preset, std::string x264_encode_tune, int width, int height, int bit_rate, int fps)
    {
        int ret;
        AVCodec *codec;
        AVDictionary *options = nullptr;

        if (!(codec = avcodec_find_encoder(codec_id)))
        {
            throw std::runtime_error{"Failed to find encoder."};
        }

        if (!(this->_ctx = avcodec_alloc_context3(codec)))
        {
            throw std::runtime_error{"Failed to allocate codec context."};
        }

        this->_ctx->bit_rate = bit_rate;
        this->_ctx->width = width;
        this->_ctx->height = height;
        this->_ctx->time_base = (AVRational){1, fps};
        this->_ctx->pix_fmt = pix_fmt;

        av_dict_set(&options, "preset", x264_encode_preset.c_str(), 0);
        av_dict_set(&options, "tune", x264_encode_tune.c_str(), 0);

        ret = avcodec_open2(this->_ctx, (const AVCodec *)codec, &options);
        av_dict_free(&options);

        if (ret < 0)
        {
            throw std::runtime_error{std::string("Failed to open codec: ") + averror_explain(ret)};
        }
    }

    std::mutex &get_mutex()
    {
        return this->_mutex;
    }

    AVCodecContext *get_context()
    {
        return this->_ctx;
    }

    ~AVCodecContextManager()
    {
        if (this->_ctx)
        {
            avcodec_close(this->_ctx);
            av_free(this->_ctx);
        }
    }
};

typedef struct
{
    AVCodec *codec;
    AVCodecContext *ctx;
    AVDictionary *options;
} EncodeContext;

#include <muxing.h>

void receive_packet_thread(AVCodecContext ctx, MuxingContext mctx, bool threads_stop_running);
void encode_raw_image_to_frame(EncodeContext *ectx, int width, int height, uint8_t *image_buffer);

#endif // _ENCODE_H_
