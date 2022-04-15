/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <common.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

SocketContext *socket_context_init(std::string socket_location)
{
    int ret;
    SocketContext *sctx = (SocketContext *)malloc(sizeof(SocketContext));
    const char *socket_loc = socket_location.c_str();

    unlink(socket_loc);

    ret = (sctx->sockfd = socket(AF_UNIX, SOCK_STREAM, 0));
    if (ret < 0)
    {
        throw std::runtime_error{"Failed to create socket: " + std::string(std::strerror(errno))};
    }

    memset(&(sctx->addr), 0, sizeof(sctx->addr));
    sctx->addr.sun_family = AF_UNIX;
    strncpy(sctx->addr.sun_path, socket_loc, sizeof(sctx->addr.sun_path) - 1);

    ret = bind(sctx->sockfd, (struct sockaddr *)&(sctx->addr), sizeof(sctx->addr));

    if (ret < 0)
    {
        throw std::runtime_error{"Failed to bind to socket: " + std::string(std::strerror(errno))};
    }

    ret = listen(sctx->sockfd, sctx->concurrent_connection);
    if (ret < 0)
    {
        throw std::runtime_error{"Failed to listen to socket: " + std::string(std::strerror(errno))};
    }

    return sctx;
}

void socket_context_wait_for_client_blocking(SocketContext *sctx)
{
    if ((sctx->client = accept(sctx->sockfd, NULL, NULL)) < 0)
    {
        throw std::runtime_error{"Failed to accept the connection: " + std::string(std::strerror(errno))};
    }
}

int socket_send_blocking(SocketContext *sctx, uint8_t *buf, ssize_t size)
{
    ssize_t ret;
    ssize_t sent = 0;

    auto progress = tlog::progress(size);
    while (sent < size)
    {
        ret = write(sctx->client, buf + sent, size - sent);
        if (ret < 0)
        {
            tlog::error() << "Failed while sending data to socket: " << std::string(std::strerror(errno));
            return (int)ret;
        }
        sent += ret;
        progress.update(sent);
    }
    // todo: duration miliseconds
    tlog::success() << "Successfully sent data to socket after " << tlog::durationToString(progress.duration());

    return 0;
}

int socket_receive_blocking(SocketContext *sctx, uint8_t *buf, uint32_t size)
{
    ssize_t ret;
    uint32_t recv = 0;

    auto progress = tlog::progress(size);
    while (recv < size)
    {
        ret = read(sctx->client, buf + recv, size - recv);
        if (ret < 0)
        {
            tlog::error() << "Failed while receiving from socket: " << std::string(std::strerror(errno));
            return (int)ret;
        }
        if (ret == 0 && recv < size)
        {
            tlog::error() << "Received EOF when transfer is not done.";
            return -1;
        }
        recv += ret;
        progress.update(recv);
    }
    tlog::success() << "Successfully received data from socket after " << tlog::durationToString(progress.duration());

    return 0;
}

int socket_context_free(SocketContext *sctx)
{
    close(sctx->client);
    close(sctx->sockfd);
    free(sctx);

    return 0;
}
