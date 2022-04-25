/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   common.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <exception>
#include <map>

#include <tinylogger/tinylogger.h>

#include <proto/nes.pb.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class lock_timeout : public std::exception
{
    virtual const char *what() const throw()
    {
        return "Waiting for lock timed out.";
    }
};

class VideoEncodingParams
{
private:
    unsigned int _width;
    unsigned int _height;
    unsigned int _bitrate;
    unsigned int _fps;
    AVPixelFormat _pix_fmt;

public:
    VideoEncodingParams(unsigned int width, unsigned int height, unsigned int bitrate, unsigned int fps, AVPixelFormat pix_fmt) : _width(width), _height(height), _bitrate(bitrate), _fps(fps), _pix_fmt(pix_fmt) {}

    const unsigned int width() const
    {
        return this->_width;
    }
    const unsigned int height() const
    {
        return this->_height;
    }
    const unsigned int bitrate() const
    {
        return this->_bitrate;
    }
    const unsigned int fps() const
    {
        return this->_fps;
    }
    const AVPixelFormat pix_fmt() const
    {
        return this->_pix_fmt;
    }
};

template <class T>
class ThreadSafeQueue
{
private:
    unsigned int _max_size;
    std::queue<T> _queue;
    std::condition_variable _pusher, _popper;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    ThreadSafeQueue(unsigned int max_size) : _max_size(max_size) {}

    template <class U>
    void push(U &&item)
    {
        unique_lock lock(this->_mutex);
        if (this->_pusher.wait_for(lock, std::chrono::milliseconds(10000), [&]
                                   { return this->_queue.size() < this->_max_size; }))
        {
            this->_queue.push(std::forward<U>(item));
            this->_popper.notify_one();
        }
        else
        {
            throw lock_timeout{};
        }
    }

    T pop()
    {
        unique_lock lock(this->_mutex);
        if (this->_popper.wait_for(lock, std::chrono::milliseconds(10000), [&]
                                   { return this->_queue.size() > 0; }))
        {
            T item = std::move(this->_queue.front());
            this->_queue.pop();
            this->_pusher.notify_one();
            return item;
        }
        else
        {
            throw lock_timeout{};
        }
    }
};

template <class T>
class ThreadSafeMap
{
private:
    std::map<uint64_t, std::unique_ptr<T>> _map;
    uint64_t _max_size;
    std::condition_variable _inserter, _getter;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    ThreadSafeMap(uint64_t max_size) : _max_size(max_size) {}

    template <class U>
    void insert(uint64_t index, U &&item)
    {
        unique_lock lock(this->_mutex);
        if (this->_inserter.wait_for(lock, std::chrono::milliseconds(10000), [&]
                                     { return this->_map.size() < this->_max_size; }))
        {
            this->_map.insert({index, std::forward<U>(item)});
            this->_getter.notify_all();
        }
        else
        {
            throw lock_timeout{};
        }
    }

    std::unique_ptr<T> pop_el(uint64_t index)
    {
        // TODO: When popping, delete frames where frame_index < index
        // to prevent any leftover frames hogging memory.
        unique_lock lock(this->_mutex);
        // This timeout value is crucial to skip frames that are taking too long.
        if (this->_getter.wait_for(lock, std::chrono::milliseconds(10000), [&]
                                   { return this->_map.contains(index); }))
        {
            std::unique_ptr<T> it = std::move(this->_map.at(index));
            this->_map.erase(index);
            this->_inserter.notify_one();
            return it;
        }
        else
        {
            throw lock_timeout{};
        }
    }
};

// TODO: implement a conditional_variable like feature.
// TODO: need to have timeout for the lock!
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

class ScopedTimer
{
    using clock = std::chrono::steady_clock;
    using time_format = std::chrono::milliseconds;

private:
    std::chrono::time_point<clock> _start;

public:
    ScopedTimer() : _start(clock::now()) {}

    time_format elapsed()
    {
        return std::chrono::duration_cast<time_format>(clock::now() - this->_start);
    }
};

#include <encode.h>
#include <server.h>
#include <encode_text.h>
#include <camera.h>
#include <muxing.h>

#endif // _COMMON_H_
