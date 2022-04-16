/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   common.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <queue>
#include <condition_variable>

#include <encode.h>
#include <encode_text.h>
#include <muxing.h>

#include <tinylogger/tinylogger.h>

typedef struct
{
    uint32_t width;
    uint32_t height;
    float rotx;
    float roty;
    float dx;
    float dy;
    float dz;
} __attribute__((packed)) Request;

typedef struct
{
    uint32_t filesize;
} __attribute__((packed)) RequestResponse;

class RenderedFrame
{
    uint64_t _order;
    uint32_t _width;
    uint32_t _height;
    std::unique_ptr<uint8_t> _buf;
    AVFrame *_frame;
    AVPixelFormat _pix_fmt;
    struct SwsContext *_sws_ctx;
    bool _processed;

public:
    RenderedFrame(uint64_t order, uint32_t width, uint32_t height, AVPixelFormat pix_fmt)
    {
        this->_order = order;
        this->_width = width;
        this->_height = height;
        this->_buf = std::unique_ptr<uint8_t>(new uint8_t[width * height * 4]);
        this->_frame = av_frame_alloc();
        this->_pix_fmt = pix_fmt;
        this->_processed = false;
    }

    void convert_frame(const AVCodecContext *ctx)
    {
        if (this->_processed)
        {
            throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
        }

        // Set context of AVFrame
        this->_frame->format = ctx->pix_fmt;
        this->_frame->width = ctx->width;
        this->_frame->height = ctx->height;

        // TODO: specify flags
        this->_sws_ctx = sws_getContext(
            this->_width,
            this->_height,
            this->_pix_fmt,
            ctx->width,
            ctx->height,
            ctx->pix_fmt,
            0,
            0,
            0,
            0);

        if (av_image_alloc(this->_frame->data, this->_frame->linesize, ctx->width, ctx->height, ctx->pix_fmt, 32) < 0)
        {
            tlog::error("Failed to allocate frame data.");
        }

        // uint8_t *in_data[1] = {(uint8_t *)this->_buf->data()};
        uint8_t *in_data[1] = {(uint8_t *)this->_buf.get()};
        int in_linesize[1] = {4 * (int)this->_width};

        sws_scale(this->_sws_ctx,
                  in_data,
                  in_linesize,
                  0,
                  this->_height,
                  this->_frame->data,
                  this->_frame->linesize);

        this->_processed = true;
    }

    const uint64_t &order() const
    {
        return this->_order;
    }

    uint8_t *buffer() const
    {
        return this->_buf.get();
    }

    const AVFrame *frame() const
    {
        if (!_processed)
        {
            throw std::runtime_error{"Tried to access a non-processed AVFrame in RenderedFrame."};
        }
        return this->_frame;
    }

    ~RenderedFrame()
    {
        if (_processed)
        {
            sws_freeContext(this->_sws_ctx);
        }
        av_frame_free(&this->_frame);
    }
};

#include <server.h>

#endif // _COMMON_H_
