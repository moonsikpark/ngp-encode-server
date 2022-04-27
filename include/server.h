/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <common.h>

#include <thread>
#include <sys/un.h>

void socket_main_thread(std::vector<std::string> renderers, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, std::atomic<std::uint64_t> &frame_index, VideoEncodingParams &veparams, CameraManager &cameramgr, std::atomic<bool> &shutdown_requested);
void socket_client_thread(int targetfd, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, std::atomic<std::uint64_t> &frame_index, VideoEncodingParams &veparams, CameraManager &cameramgr, std::atomic<bool> &shutdown_requested);

#endif // _SERVER_H_
