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

typedef struct
{
    uint32_t cache_size;
    int concurrent_connection;
    struct sockaddr_un addr;
    uint32_t received;
    int sockfd;
    int client;
} SocketContext;

SocketContext *socket_context_init(std::string socket_location);
void socket_context_wait_for_client_blocking(SocketContext *sctx);
int socket_send_blocking(SocketContext *sctx, uint8_t *buf, ssize_t size);
int socket_receive_blocking(SocketContext *sctx, uint8_t *buf, uint32_t size);
int socket_context_free(SocketContext *sctx);

#endif // _SERVER_H_
