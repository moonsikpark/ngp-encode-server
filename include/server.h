/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <common.h>
#include <sys/un.h>

#include <thread>

#include "base/video/frame_queue.h"
void socket_main_thread(std::vector<std::string> renderers,
                        std::shared_ptr<FrameQueue> frame_queue,
                        std::atomic<std::uint64_t> &frame_index,
                        std::shared_ptr<CameraManager> cameramgr,
                        std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                        std::atomic<bool> &shutdown_requested);
void socket_client_thread(int targetfd, std::shared_ptr<FrameQueue> frame_queue,
                          std::atomic<std::uint64_t> &frame_index,
                          std::shared_ptr<CameraManager> cameramgr,
                          std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                          std::atomic<bool> &shutdown_requested);

#endif  // _SERVER_H_
