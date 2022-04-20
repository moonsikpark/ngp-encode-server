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
#include <map>

#include <tinylogger/tinylogger.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
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
    std::condition_variable _pusher, _popper;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    ThreadSafeQueue(unsigned int max_size) : _max_size(max_size) {}

    template <class U>
    void push(U &&item)
    {
        unique_lock lock(this->_mutex);
        if (this->_pusher.wait_for(lock, std::chrono::milliseconds(300), [&]
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
        if (this->_popper.wait_for(lock, std::chrono::milliseconds(300), [&]
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
        if (this->_inserter.wait_for(lock, std::chrono::milliseconds(300), [&]
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
        unique_lock lock(this->_mutex);
        if (this->_getter.wait_for(lock, std::chrono::milliseconds(300), [&]
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
#include <pov.h>
#include <muxing.h>

#endif // _COMMON_H_
