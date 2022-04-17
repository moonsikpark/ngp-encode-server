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

// TODO: Gracefully close the socket.
void socket_main_thread(std::string socket_location, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, bool threads_stop_running)
{

    tlog::info() << "socket_main_thread: Initalizing socket server...";
    struct sockaddr_un addr;
    int sockfd, clientfd;
    bool keep_running = true;

    const char *socket_loc = socket_location.c_str();

    unlink(socket_loc);

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        throw std::runtime_error{"socket_main_thread: Failed to create socket: " + std::string(std::strerror(errno))};
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_loc, sizeof(addr.sun_path) - 1);

    if ((bind(sockfd, (struct sockaddr *)&(addr), sizeof(addr))) < 0)
    {
        throw std::runtime_error{"socket_main_thread: Failed to bind to socket: " + std::string(std::strerror(errno))};
    }

    // TODO: How much should backlog be?
    if ((listen(sockfd, 1)) < 0)
    {
        throw std::runtime_error{"Failed to listen to socket: " + std::string(std::strerror(errno))};
    }
    tlog::success() << "socket_main_thread: Socket server created and listening.";

    while (keep_running)
    {
        if ((clientfd = accept(sockfd, NULL, NULL)) < 0)
        {
            throw std::runtime_error{"socket_main_thread: Failed to accept client: " + std::string(std::strerror(errno))};
        }
        std::thread _socket_client_thread(socket_client_thread, clientfd, std::ref(req_queue), std::ref(frame_queue), std::ref(threads_stop_running));
        _socket_client_thread.detach();
        tlog::success() << "socket_main_thread: Received client connection (clientfd=" << clientfd << ").";
    }
}

void socket_client_thread(int clientfd, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, bool threads_stop_running)
{
    bool keep_running = true;

    tlog::info() << "socket_client_thread (fd=" << clientfd << "): Spawned.";
    while (keep_running)
    {
        if (threads_stop_running)
        {
            break;
        }
        // TODO: Get request from request queue.
        // Request req = req_queue.pop();

        Request req = {
            .width = 1280,
            .height = 720,
            .rotx = 1,
            .roty = 0,
            .dx = -1,
            .dy = 0,
            .dz = 0};

        RequestResponse resp;

        socket_send_blocking(clientfd, (uint8_t *)&req, sizeof(Request));

        socket_receive_blocking(clientfd, (uint8_t *)&resp, sizeof(RequestResponse));

        RenderedFrame frame{0, req.width, req.height, AV_PIX_FMT_BGR32};

        socket_receive_blocking(clientfd, frame.buffer(), resp.filesize);

        frame_queue.push(std::move(frame));

        tlog::info() << "socket_client_thread (fd=" << clientfd << "): Frame has been received and placed into a queue.";
    }
}

// TODO: Gracefully close the socket.
//    close(sctx->client);
//    close(sctx->sockfd);
int socket_send_blocking(int clientfd, uint8_t *buf, ssize_t size)
{
    ssize_t ret;
    ssize_t sent = 0;

    auto progress = tlog::progress(size);
    while (sent < size)
    {
        ret = write(clientfd, buf + sent, size - sent);
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

int socket_receive_blocking(int clientfd, uint8_t *buf, uint32_t size)
{
    ssize_t ret;
    uint32_t recv = 0;

    auto progress = tlog::progress(size);
    while (recv < size)
    {
        ret = read(clientfd, buf + recv, size - recv);
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