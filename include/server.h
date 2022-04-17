/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <common.h>

#include <sys/un.h>

void socket_main_thread(std::string socket_location, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, bool threads_stop_running);
void socket_client_thread(int clientfd, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, bool threads_stop_running);
int socket_send_blocking(int clientfd, uint8_t *buf, ssize_t size);
int socket_receive_blocking(int clientfd, uint8_t *buf, uint32_t size);

#endif // _SERVER_H_
