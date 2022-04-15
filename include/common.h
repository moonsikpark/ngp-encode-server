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
#include <server.h>

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

// from https://stackoverflow.com/questions/49640527/two-waiting-threads-producer-consumer-with-a-shared-buffer/49669316#49669316
template <class T>
class ThreadSafeQueue
{
    std::condition_variable consumer_;
    std::mutex mutex_;
    using unique_lock = std::unique_lock<std::mutex>;

    std::queue<T> queue_;

public:
    template <class U>
    void push_back(U &&item)
    {
        unique_lock lock(mutex_);
        queue_.push(std::forward<U>(item));
        consumer_.notify_all();
    }

    T pop_front()
    {
        unique_lock lock(mutex_);
        while (queue_.empty())
            consumer_.wait(lock);
        auto item = queue_.front();
        queue_.pop();
        return item;
    }
};

#endif // _COMMON_H_
