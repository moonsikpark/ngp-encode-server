/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   server.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <thread>

#include "base/camera_manager.h"
#include "base/exceptions/lock_timeout.h"
#include "base/scoped_timer.h"
#include "base/video/frame_queue.h"

int socket_send_blocking(int targetfd, uint8_t *buf, size_t size) {
  ssize_t ret;
  ssize_t sent = 0;

  while (sent < size) {
    ret = send(targetfd, buf + sent, size - sent, MSG_NOSIGNAL);
    if (ret < 0) {
      // Buffer is full. Try again.
      if (errno == EAGAIN) {
        continue;
      }
      // Misc error. Terminate the socket.
      tlog::error() << "socket_send_blocking: "
                    << std::string(std::strerror(errno));
      return -errno;
    }
    sent += ret;
  }

  return 0;
}

// Send message with length prefix framing.
int socket_send_blocking_lpf(int targetfd, uint8_t *buf, size_t size) {
  int ret;
  // hack: not very platform portable
  // but then, the program isn't.
  if ((ret = socket_send_blocking(targetfd, (uint8_t *)&size, sizeof(size))) <
      0) {
    return ret;
  }

  if ((ret = socket_send_blocking(targetfd, buf, size)) < 0) {
    return ret;
  }

  return ret;
}

int socket_receive_blocking(int targetfd, uint8_t *buf, size_t size) {
  ssize_t ret;
  ssize_t recv = 0;

  while (recv < size) {
    ret = read(targetfd, buf + recv, size - recv);
    if (ret < 0) {
      // Buffer is full. Try again.
      if (errno == EAGAIN) {
        continue;
      }
      // Misc error. Terminate the socket.
      tlog::error() << "socket_receive_blocking: "
                    << std::string(std::strerror(errno));
      return -errno;
    }
    if (ret == 0 && recv < size) {
      // Client disconnected while sending data. Terminate the socket.
      tlog::error()
          << "socket_receive_blocking: Received EOF when transfer is not done.";
      return -1;
    }
    recv += ret;
  }

  return 0;
}

// Receive message with length prefix framing.
std::string socket_receive_blocking_lpf(int targetfd) {
  int ret;
  size_t size;
  // hack: not very platform portable
  // todo: silently fail, do not wail error.
  if ((ret = socket_receive_blocking(targetfd, (uint8_t *)&size,
                                     sizeof(size))) < 0) {
    throw std::runtime_error{
        "socket_receive_blocking_lpf: Error while "
        "receiving data size from socket."};
  }

  auto buffer = std::make_unique<char[]>(size);

  if ((ret = socket_receive_blocking(targetfd, (uint8_t *)buffer.get(), size)) <
      0) {
    throw std::runtime_error{
        "socket_receive_blocking_lpf: Error while receiving data from socket."};
  }

  return std::string(buffer.get(), buffer.get() + size);
}

static constexpr unsigned kLogStatsIntervalFrame = 100;

void socket_client_thread(
    int targetfd, std::shared_ptr<FrameQueue> frame_queue_left,
    std::shared_ptr<FrameQueue> frame_queue_right,
    std::atomic<std::uint64_t> &frame_index_left,
    std::atomic<std::uint64_t> &frame_index_right, std::atomic<int> &is_left,
    std::shared_ptr<CameraManager> cameramgr,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth,
    std::atomic<bool> &shutdown_requested) {
  // set_thread_name(std::string("socket_client=") + std::to_string(targetfd));
  int ret = 0;
  tlog::info() << "socket_client_thread (fd=" << targetfd << "): Spawned.";

  uint64_t count = 0;
  uint64_t elapsed = 0;

  while (!shutdown_requested) {
    if (ret < 0) {
      // If there were errors, exit the loop.
      tlog::error() << "socket_client_thread (fd=" << targetfd
                    << "): Error occured. Exiting.";
      break;
    }

    nesproto::FrameRequest req;
    //  is_left xor true op has same effect as not op
    //    t xor t = f (not t)
    //    f xor t = t (not f)
    bool is_left_val = is_left.fetch_xor(true);
    req.set_is_left(is_left_val);

    uint64_t frame_index;
    if (is_left_val) {
      frame_index = frame_index_left.fetch_add(1);
    } else {
      frame_index = frame_index_right.fetch_add(1);
    }
    req.set_index(frame_index);
    // set_allocated_* destroys the object. Use mutable_*()->CopyFrom().
    if (is_left_val) {
      req.mutable_camera()->CopyFrom(cameramgr->get_camera_left());
    } else {
      req.mutable_camera()->CopyFrom(cameramgr->get_camera_right());
    }

    std::string req_serialized = req.SerializeAsString();

    // Send request from request queue.
    if ((ret = socket_send_blocking_lpf(targetfd,
                                        (uint8_t *)req_serialized.data(),
                                        req_serialized.size())) < 0) {
      continue;
    }

    nesproto::RenderedFrame frame;
    {
      ScopedTimer timer;

      try {
        if (!frame.ParseFromString(socket_receive_blocking_lpf(targetfd))) {
          continue;
        }
      } catch (const std::runtime_error &) {
        continue;
      }
      count++;
      elapsed += timer.elapsed().count();
      if (count == kLogStatsIntervalFrame) {
        tlog::debug() << "socket_client_thread (fd=" << targetfd
                      << "): Frame receiving average time of "
                      << kLogStatsIntervalFrame
                      << " frames: " << elapsed / count << " msec.";
        count = 0;
        elapsed = 0;
      }
    }

    std::unique_ptr<RenderedFrame> frame_o = std::make_unique<RenderedFrame>(
        frame, AV_PIX_FMT_RGB24, AV_PIX_FMT_GRAY8, ctxmgr_scene, ctxmgr_depth);

    try {
      // Push the frame to the frame queue.
      if (frame_o->is_left()) {
        frame_queue_left->push(std::move(frame_o));
      } else {
        frame_queue_right->push(std::move(frame_o));
      }
    } catch (const LockTimeout &) {
      // It takes too much time to acquire a lock of frame_queue. Drop the
      // frame. BUG: If we drop the frame, the program will hang and look for
      // the frame.
      continue;
    }
  }
  // Cleanup: close the client socket.
  close(targetfd);
  tlog::info() << "socket_client_thread (fd=" << targetfd
               << "): Exiting thread.";
}

void socket_manage_thread(
    std::string renderer, std::shared_ptr<FrameQueue> frame_queue_left,
    std::shared_ptr<FrameQueue> frame_queue_right,
    std::atomic<std::uint64_t> &frame_index_left,
    std::atomic<std::uint64_t> &frame_index_right, std::atomic<int> &is_left,
    std::shared_ptr<CameraManager> cameramgr,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth,
    std::atomic<bool> &shutdown_requested) {
  // set_thread_name(std::string("socket_manage=") + renderer);
  int error_times = 0;
  while (!shutdown_requested) {
    std::stringstream renderer_parsed(renderer);

    std::string ip;
    std::string port_str;

    std::getline(renderer_parsed, ip, ':');
    std::getline(renderer_parsed, port_str, ':');

    uint16_t port((uint16_t)std::stoi(port_str));

    int fd;

    struct sockaddr_in addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      tlog::error() << "socket_client_thread_factory(" << renderer
                    << "): Failed to create socket : " << std::strerror(errno)
                    << "; Retrying.";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      error_times++;
      if (error_times > 30) {
        tlog::error() << "socket_client_thread_factory(" << renderer
                      << "): Failed to connect : " << std::strerror(errno)
                      << "; Retrying.";
        error_times = 0;
      }
      continue;
    }

    tlog::success() << "socket_client_thread_factory(" << renderer
                    << "): Connected to " << renderer;

    std::thread _socket_client_thread(
        socket_client_thread, fd, frame_queue_left, frame_queue_right,
        std::ref(frame_index_left), std::ref(frame_index_right),
        std::ref(is_left), cameramgr, ctxmgr_scene, ctxmgr_depth,
        std::ref(shutdown_requested));

    _socket_client_thread.join();

    error_times = 0;

    tlog::error() << "socket_manage_thread (" << renderer
                  << "): Connection is dead. Trying to reconnect.";
  }
}

void socket_main_thread(
    std::vector<std::string> renderers,
    std::shared_ptr<FrameQueue> frame_queue_left,
    std::shared_ptr<FrameQueue> frame_queue_right,
    std::atomic<std::uint64_t> &frame_index_left,
    std::atomic<std::uint64_t> &frame_index_right, std::atomic<int> &is_left,
    std::shared_ptr<CameraManager> cameramgr,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth,
    std::atomic<bool> &shutdown_requested) {
  // set_thread_name("socket_main");
  std::vector<std::thread> threads;

  tlog::info() << "socket_main_thread: Connecting to renderers.";

  for (const auto renderer : renderers) {
    threads.push_back(
        std::thread(socket_manage_thread, renderer, frame_queue_left,
                    frame_queue_right, std::ref(frame_index_left),
                    std::ref(frame_index_right), std::ref(is_left), cameramgr,
                    ctxmgr_scene, ctxmgr_depth, std::ref(shutdown_requested)));
  }

  tlog::info() << "socket_main_thread: Connectd to all renderers.";

  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  tlog::info() << "socket_main_thread: Closed all connections. Exiting thread.";
}
