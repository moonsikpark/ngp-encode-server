/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <common.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

int socket_send_blocking(int targetfd, uint8_t *buf, size_t size)
{
    ssize_t ret;
    ssize_t sent = 0;

    while (sent < size)
    {
        ret = send(targetfd, buf + sent, size - sent, MSG_NOSIGNAL);
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

// Send message with length prefix framing.
int socket_send_blocking_lpf(int targetfd, uint8_t *buf, size_t size)
{
    int ret;
    // hack: not very platform portable
    // but then, the program isn't.
    if ((ret = socket_send_blocking(targetfd, (uint8_t *)&size, sizeof(size))) < 0)
    {
        goto end;
    }

    if ((ret = socket_send_blocking(targetfd, buf, size)) < 0)
    {
        goto end;
    }
end:
    return ret;
}

int socket_receive_blocking(int targetfd, uint8_t *buf, size_t size)
{
    ssize_t ret;
    ssize_t recv = 0;

    while (recv < size)
    {
        ret = read(targetfd, buf + recv, size - recv);
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

// Receive message with length prefix framing.
std::string socket_receive_blocking_lpf(int targetfd)
{
    int ret;
    size_t size;
    // hack: not very platform portable
    // todo: silently fail, do not wail error.
    if ((ret = socket_receive_blocking(targetfd, (uint8_t *)&size, sizeof(size))) < 0)
    {
        throw std::runtime_error{"socket_receive_blocking_lpf: Error while receiving data size from socket."};
    }

    auto buffer = std::make_unique<char[]>(size);

    if ((ret = socket_receive_blocking(targetfd, (uint8_t *)buffer.get(), size)) < 0)
    {
        throw std::runtime_error{"socket_receive_blocking_lpf: Error while receiving data from socket."};
    }

    return std::string(buffer.get(), buffer.get() + size);
}

void socket_client_thread(int targetfd, ThreadSafeQueue<nesproto::FrameRequest> &req_queue, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, std::atomic<bool> &shutdown_requested)
{
    int ret = 0;
    tlog::info() << "socket_client_thread (fd=" << targetfd << "): Spawned.";
    uint64_t frame_count = 0; // XXX HACK
    while (!shutdown_requested)
    {
        if (ret < 0)
        {
            // If there were errors, exit the loop.
            tlog::info() << "socket_client_thread (fd=" << targetfd << "): Error occured. Exiting.";
            break;
        }
        // TODO: measure how much this loop takes.
        // TODO: Get request from request queue.
        nesproto::FrameRequest req = req_queue.pop();
        {
            ScopedTimer timer;

            std::string req_serialized = req.SerializeAsString();

            // Send request from request queue.
            if ((ret = socket_send_blocking_lpf(targetfd, (uint8_t *)req_serialized.data(), req_serialized.size())) < 0)
            {
                continue;
            }

            nesproto::RenderedFrame frame;

            if (!frame.ParseFromString(socket_receive_blocking_lpf(targetfd)))
            {
                continue;
            }

            // Create a new renderedframe.
            // TODO: Honor index.
            // TODO: change w/h to unsigned int.
            // TODO: set this to correct resolution.
            std::unique_ptr<RenderedFrame> frame_o = std::make_unique<RenderedFrame>(frame, AV_PIX_FMT_BGR32);

            try
            {
                // Push the frame to the frame queue.
                frame_queue.push(std::move(frame_o));
            }
            catch (const lock_timeout &)
            {
                // It takes too much time to acquire a lock of frame_queue. Drop the frame.
                // TODO: don't drop?
                continue;
            }
            frame_count++;
            tlog::info() << "socket_client_thread (fd=" << targetfd << "): Frame has been received and placed into a queue in " << timer.elapsed().count() << " msec.";
        }
    }
    // Cleanup: close the client socket.
    close(targetfd);
    tlog::info() << "socket_client_thread (fd=" << targetfd << "): Exiting thread.";
}

void socket_main_thread(std::vector<std::string> renderers, ThreadSafeQueue<nesproto::FrameRequest> &req_queue, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, std::atomic<bool> &shutdown_requested)
{
    std::vector<std::thread> thr;

    tlog::info() << "socket_main_thread: Connecting to renderers.";

    for (const auto renderer : renderers)
    {
        std::stringstream renderer_parsed(renderer);

        std::string ip;
        std::string port_str;

        std::getline(renderer_parsed, ip, ':');
        std::getline(renderer_parsed, port_str, ':');

        uint16_t port((uint16_t)std::stoi(port_str));

        int fd;

        struct sockaddr_in addr;

        tlog::info() << "socket_main_thread: Connecting to renderer at ip=" << ip << " port=" << port;

        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            throw std::runtime_error{"Failed to create socket: " + std::string(std::strerror(errno))};
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            throw std::runtime_error{"Failed to connect: " + std::string(std::strerror(errno))};
        }
        tlog::success() << "socket_main_thread: Connected to " << renderer;

        thr.push_back(std::thread(socket_client_thread, fd, std::ref(req_queue), std::ref(frame_queue), std::ref(shutdown_requested)));
    }

    tlog::info() << "socket_main_thread: Connectd to all renderers.";

    for (auto &th : thr)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    tlog::info() << "socket_main_thread: Closed all connections. Exiting thread.";
}
