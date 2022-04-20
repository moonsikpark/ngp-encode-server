/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _ENCODE_H_
#define _ENCODE_H_

#include <common.h>

extern "C"
{
#include <libavutil/pixdesc.h>
}

std::string averror_explain(int err);

class RenderedFrame
{
    uint64_t _index;
    uint32_t _width;
    uint32_t _height;
    std::unique_ptr<uint8_t> _buf;
    uint8_t *_processed_data[AV_NUM_DATA_POINTERS];
    int _processed_linesize[AV_NUM_DATA_POINTERS];

    AVPixelFormat _pix_fmt;
    struct SwsContext *_sws_ctx;
    bool _processed;

public:
    RenderedFrame(uint64_t index,
                  uint32_t width,
                  uint32_t height,
                  AVPixelFormat pix_fmt) : _index(index),
                                           _width(width),
                                           _height(height),
                                           _buf(
                                               std::unique_ptr<uint8_t>(
                                                   new uint8_t[width * height * 4])),
                                           _pix_fmt(pix_fmt),
                                           _processed(false) {}
    // Forbid copying or moving of RenderedFrame.
    // The frame should be wrapped in unique_ptr to be moved.
    RenderedFrame(RenderedFrame &&r) = delete;
    RenderedFrame &operator=(RenderedFrame &&) = delete;
    RenderedFrame(const RenderedFrame &) = delete;
    RenderedFrame &operator=(const RenderedFrame &) = delete;

    friend bool operator<(const RenderedFrame &lhs, const RenderedFrame &rhs)
    {
        // Compare frames by their index.
        return lhs._index < rhs._index;
    }

    friend bool operator==(const RenderedFrame &, const RenderedFrame &)
    {
        // No frames are the same.
        return false;
    }

    void convert_frame(const AVCodecContext *ctx)
    {
        if (this->_processed)
        {
            throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
        }

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

        if (!this->_sws_ctx)
        {
            throw std::runtime_error{"Failed to allocate sws_context."};
        }

        if (av_image_alloc(this->_processed_data, this->_processed_linesize, ctx->width, ctx->height, ctx->pix_fmt, 32) < 0)
        {
            tlog::error("Failed to allocate frame data.");
        }

        const AVPixFmtDescriptor *in_pixfmt = av_pix_fmt_desc_get(this->_pix_fmt);
        int in_ls[AV_NUM_DATA_POINTERS] = {0};
        for (int plane = 0; plane < in_pixfmt->nb_components; plane++)
        {
            in_ls[plane] = av_image_get_linesize(this->_pix_fmt, this->_width, plane);
        }
        uint8_t *in_data[1] = {(uint8_t *)this->_buf.get()};

        sws_scale(this->_sws_ctx,
                  in_data,
                  in_ls,
                  0,
                  this->_height,
                  this->_processed_data,
                  this->_processed_linesize);

        this->_processed = true;
    }

    const uint64_t &index() const
    {
        return this->_index;
    }

    uint8_t *buffer() const
    {
        return this->_buf.get();
    }

    const uint32_t width() const
    {
        return this->_width;
    }

    const uint32_t height() const
    {
        return this->_height;
    }

    uint8_t *processed_data()
    {
        if (!this->_processed)
        {
            throw std::runtime_error{"Tried to access processed_data from not processed RenderedFrame."};
        }

        return this->_processed_data[0];
    }

    const int *processed_linesize() const
    {
        if (!this->_processed)
        {
            throw std::runtime_error{"Tried to access processed_linesize from not processed RenderedFrame."};
        }
        return this->_processed_linesize;
    }

    ~RenderedFrame()
    {
        if (this->_processed)
        {
            av_freep(&this->_processed_data[0]);
            sws_freeContext(this->_sws_ctx);
        }
    }
};

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

    const AVCodecContext *get_const_context() const
    {
        return this->_ctx;
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

#include <muxing.h>
#include <encode_text.h>
void process_frame_thread(AVCodecContextManager &ctxmgr, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, ThreadSafeMap<RenderedFrame> &encode_queue, EncodeTextContext &etctx, std::atomic<bool> &shutdown_requested);
void send_frame_thread(AVCodecContextManager &ctxmgr, ThreadSafeMap<RenderedFrame> &encode_queue, std::atomic<bool> &shutdown_requested);
void receive_packet_thread(AVCodecContextManager &ctxmgr, MuxingContext &mctx, std::atomic<bool> &shutdown_requested);

#endif // _ENCODE_H_
