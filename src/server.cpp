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

int socket_send_blocking(int clientfd, uint8_t *buf, ssize_t size)
{
    ssize_t ret;
    ssize_t sent = 0;

    while (sent < size)
    {
        ret = send(clientfd, buf + sent, size - sent, MSG_NOSIGNAL);
        if (ret < 0)
        {
            // Buffer is full. Try again.
            if (errno == EAGAIN)
            {
                continue;
            }
            // Misc error. Terminate the socket.
            tlog::error() << "socket_send_blocking: " << std::string(std::strerror(errno));
            return -errno;
        }
        sent += ret;
    }

    return 0;
}

int socket_receive_blocking(int clientfd, uint8_t *buf, ssize_t size)
{
    ssize_t ret;
    ssize_t recv = 0;

    while (recv < size)
    {
        ret = read(clientfd, buf + recv, size - recv);
        if (ret < 0)
        {
            // Buffer is full. Try again.
            if (errno == EAGAIN)
            {
                continue;
            }
            // Misc error. Terminate the socket.
            tlog::error() << "socket_receive_blocking: " << std::string(std::strerror(errno));
            return -errno;
        }
        if (ret == 0 && recv < size)
        {
            // Client disconnected while sending data. Terminate the socket.
            tlog::error() << "socket_receive_blocking: Received EOF when transfer is not done.";
            return -1;
        }
        recv += ret;
    }

    return 0;
}

void socket_main_thread(std::string socket_location, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, std::atomic<bool> &shutdown_requested)
{

    tlog::info() << "socket_main_thread: Initalizing socket server...";
    struct sockaddr_un addr;
    int sockfd, clientfd, ret;

    const char *socket_loc = socket_location.c_str();

    // Create nonblocking streaming socket.
    if ((ret = unlink(socket_loc)) < 0)
    {
        throw std::runtime_error{"socket_main_thread: Failed to unlink previous socket: " + std::string(std::strerror(errno))};
    }

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
    {
        throw std::runtime_error{"socket_main_thread: Failed to create socket: " + std::string(std::strerror(errno))};
    }

    // Bind the socket to the given address.
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_loc, sizeof(addr.sun_path) - 1);
    if ((bind(sockfd, (struct sockaddr *)&(addr), sizeof(addr))) < 0)
    {
        throw std::runtime_error{"socket_main_thread: Failed to bind to socket: " + std::string(std::strerror(errno))};
    }

    // Listen to the socket.
    if ((listen(sockfd, /* backlog= */ 2)) < 0)
    {
        throw std::runtime_error{"Failed to listen to socket: " + std::string(std::strerror(errno))};
    }

    tlog::success() << "socket_main_thread: Socket server created and listening.";

    while (!shutdown_requested)
    {
        // Wait for connections, but don't block if there aren't any connections.
        if ((clientfd = accept4(sockfd, NULL, NULL, SOCK_NONBLOCK)) < 0)
        {
            // There are no clients to accept.
            if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
            {
                // Sleep and wait again for connections.
                std::this_thread::sleep_for(std::chrono::milliseconds{500});
                continue;
            }
            else
            {
                throw std::runtime_error{"socket_main_thread: Failed to accept client: " + std::string(std::strerror(errno))};
            }
        }
        else
        {
            // A client wants to connect. Spawn a thread with the client's fd and process the client there.
            // TODO: maybe not thread per client but thread pool?
            std::thread _socket_client_thread(socket_client_thread, clientfd, std::ref(req_queue), std::ref(frame_queue), std::ref(shutdown_requested));
            _socket_client_thread.detach();
            tlog::success() << "socket_main_thread: Received client connection (clientfd=" << clientfd << ").";
        }
    }
    // Cleanup: close the socket.
    close(sockfd);
    tlog::success() << "socket_main_thread: Exiting thread.";
}

void socket_client_thread(int clientfd, ThreadSafeQueue<Request> &req_queue, ThreadSafeQueue<RenderedFrame> &frame_queue, std::atomic<bool> &shutdown_requested)
{
    int ret = 0;
    tlog::info() << "socket_client_thread (fd=" << clientfd << "): Spawned.";
    while (!shutdown_requested)
    {
        if (ret < 0)
        {
            // If there were errors, exit the loop.
            tlog::info() << "socket_client_thread (fd=" << clientfd << "): Error occured. Exiting.";
            break;
        }
        // TODO: measure how much this loop takes.
        // TODO: Get request from request queue.
        // Request req = req_queue.pop();
        {
            ScopedTimer timer;
            Request req = {
                .width = 1280,
                .height = 720,
                .rotx = 1,
                .roty = 0,
                .dx = -1,
                .dy = 0,
                .dz = 0};

            RequestResponse resp;

            // Send request from request queue.
            if ((ret = socket_send_blocking(clientfd, (uint8_t *)&req, sizeof(Request))) < 0)
            {
                continue;
            }

            // Receive requestresponse from client.
            if ((ret = socket_receive_blocking(clientfd, (uint8_t *)&resp, sizeof(RequestResponse))) < 0)
            {
                continue;
            }

            // Create a new renderedframe.
            // TODO: Honor index.
            // TODO: change w/h to unsigned int.
            std::unique_ptr<RenderedFrame> frame = std::make_unique<RenderedFrame>(0, (unsigned int)req.width, (unsigned int)req.height, AV_PIX_FMT_BGR32);

            // Receive rendered frame to the buffer of renderedframe.
            if ((ret = socket_receive_blocking(clientfd, frame->buffer(), resp.filesize)) < 0)
            {
                continue;
            }

            try
            {
                // Push the frame to the frame queue.
                frame_queue.push(std::move(frame));
            }
            catch (const lock_timeout &)
            {
                // It takes too much time to acquire a lock of frame_queue. Drop the frame.
                // TODO: don't drop?
                continue;
            }

            tlog::info() << "socket_client_thread (fd=" << clientfd << "): Frame has been received and placed into a queue in " << timer.elapsed().count() << " msec.";
        }
    }
    // Cleanup: close the client socket.
    close(clientfd);
    tlog::info() << "socket_client_thread (fd=" << clientfd << "): Exiting thread.";
}
