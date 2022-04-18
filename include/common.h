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
#include <chrono>
#include <atomic>
#include <exception>

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

class lock_timeout : public std::exception
{
    virtual const char *what() const throw()
    {
        return "Waiting for lock timed out.";
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
        if (this->_producer.wait_for(lock, std::chrono::milliseconds(300), [&]
                                     { return this->_queue.size() < this->_max_size; }))
        {
            this->_queue.push(std::move(t));
            _consumer.notify_one();
        }
        else
        {
            throw lock_timeout{};
        }
    }

    T pop()
    {
        unique_lock lock(this->_mutex);
        if (this->_consumer.wait_for(lock, std::chrono::milliseconds(300), [&]
                                     { return this->_queue.size() > 0; }))
        {
            T item = std::move(this->_queue.front());
            if (this->_queue.size() == this->_max_size)
            {
                this->_producer.notify_all();
            }
            this->_queue.pop();
            return item;
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

#include <encode.h>
#include <server.h>
#include <encode_text.h>
#include <muxing.h>

#endif // _COMMON_H_
