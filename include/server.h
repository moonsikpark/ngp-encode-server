/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <common.h>

#include <thread>
#include <sys/un.h>

void socket_main_thread(std::string socket_location, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, std::atomic<bool> &shutdown_requested);
void socket_client_thread(int clientfd, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, std::atomic<bool> &shutdown_requested);

#endif // _SERVER_H_
