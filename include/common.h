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
#include <thread>
#include <chrono>

#include <tinylogger/tinylogger.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

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
    uint64_t _index;
    uint32_t _width;
    uint32_t _height;
    std::unique_ptr<uint8_t> _buf;
    AVPixelFormat _pix_fmt;
    struct SwsContext *_sws_ctx;
    bool _processed;

public:
    RenderedFrame(uint64_t index, uint32_t width, uint32_t height, AVPixelFormat pix_fmt)
    {
        this->_index = index;
        this->_width = width;
        this->_height = height;
        this->_buf = std::unique_ptr<uint8_t>(new uint8_t[width * height * 4]);
        this->_pix_fmt = pix_fmt;
        this->_processed = false;
    }
    RenderedFrame() = default;
    RenderedFrame(RenderedFrame &&r) = default;

    void convert_frame(const AVCodecContext *ctx, AVFrame *frame)
    {
        if (this->_processed)
        {
            throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
        }

        // Set context of AVFrame
        frame->format = ctx->pix_fmt;
        frame->width = ctx->width;
        frame->height = ctx->height;

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

        if (av_image_alloc(frame->data, frame->linesize, ctx->width, ctx->height, ctx->pix_fmt, 32) < 0)
        {
            tlog::error("Failed to allocate frame data.");
        }

        uint8_t *in_data[1] = {(uint8_t *)this->_buf.get()};
        int in_linesize[1] = {4 * (int)this->_width};

        sws_scale(this->_sws_ctx,
                  in_data,
                  in_linesize,
                  0,
                  this->_height,
                  frame->data,
                  frame->linesize);

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

    ~RenderedFrame()
    {
        if (this->_processed)
        {
            sws_freeContext(this->_sws_ctx);
        }
    }
};

template <class T>
class ThreadSafeQueue
{
private:
    unsigned int _max_size;
    std::queue<T> _queue;
    std::condition_variable _consumer, _producer;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    ThreadSafeQueue(unsigned int max_size)
    {
        this->_max_size = max_size;
    }

    void push(T &&t)
    {
        unique_lock lock(this->_mutex);
        this->_producer.wait(lock, [&]
                             { return this->_queue.size() < this->_max_size; });
        this->_queue.push(std::move(t));
        _consumer.notify_one();
    }

    T pop()
    {
        unique_lock lock(this->_mutex);
        this->_consumer.wait(lock, [&]
                             { return this->_queue.size() > 0; });
        T item = std::move(this->_queue.front());
        if (this->_queue.size() == this->_max_size)
        {
            this->_producer.notify_all();
        }
        this->_queue.pop();
        return item;
    }
};

// TODO: implement a conditional_variable like feature.
template <class Lockable, class Resource>
class ResourceLock
{
private:
    Lockable &_lockable;
    Resource *_resource;

public:
    ResourceLock(Lockable &l, Resource *r) : _lockable(l), _resource(r)
    {
        this->_lockable.lock();
    }
    Resource *get()
    {
        return this->_resource;
    }
    ~ResourceLock()
    {
        this->_lockable.unlock();
    }
};

#include <server.h>
#include <encode.h>
#include <encode_text.h>
#include <muxing.h>

#endif // _COMMON_H_
