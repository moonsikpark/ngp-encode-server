/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   muxing.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _MUXING_H_
#define _MUXING_H_

#include <common.h>

extern "C"
{
#include <libavformat/avformat.h>
}

class MuxingContext
{
private:
    AVFormatContext *_fctx;
    AVStream *_st;

public:
    MuxingContext(const AVCodecContext *ctx, std::string rtsp_mrl)
    {
        int ret;
        if ((ret = avformat_alloc_output_context2(&this->_fctx, NULL, "rtsp", rtsp_mrl.c_str())) < 0)
        {
            throw std::runtime_error{std::string("MuxingContext: Failed to allocate output context: ") + averror_explain(ret)};
        }

        if (!(this->_st = avformat_new_stream(this->_fctx, NULL)))
        {
            throw std::runtime_error{"MuxingContext: Failed to allocate new stream."};
        }

        this->_st->codecpar->codec_id = ctx->codec_id;
        this->_st->codecpar->codec_type = ctx->codec_type;
        this->_st->codecpar->bit_rate = ctx->bit_rate;
        this->_st->codecpar->width = ctx->width;
        this->_st->codecpar->height = ctx->height;
        this->_st->time_base = ctx->time_base;
        this->_st->codecpar->format = ctx->pix_fmt;

        if ((ret = avformat_write_header(this->_fctx, NULL)) < 0)
        {
            throw std::runtime_error{std::string("MuxingContext: Failed to write header: ") + averror_explain(ret)};
        }
    }

    AVFormatContext *get_ctx()
    {
        return this->_fctx;
    }

    ~MuxingContext()
    {
        int ret;
        if ((ret = av_write_trailer(this->_fctx)) < 0)
        {
            tlog::error() << "MuxingContext: Failed to write trailer: " << averror_explain(ret);
        }
        avformat_free_context(this->_fctx);
    }
};

#endif // _MUXING_H_
