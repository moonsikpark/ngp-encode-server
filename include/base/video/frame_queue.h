// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_VIDEO_FRAME_QUEUE_
#define NES_BASE_VIDEO_FRAME_QUEUE_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include "base/video/rendered_frame.h"

// Thread-safe queue implementation based on std::queue used to store unique_ptr
// of RenderedFrame.
class FrameQueue {
 public:
  // Max size of FrameQueue.
  static constexpr std::size_t kFrameQueueMaxSize = 100;

  // Timeout of push/pop operation.
  static constexpr std::chrono::milliseconds kFrameQueueLockTimeout{1000};

  using element = std::unique_ptr<RenderedFrame>;

  void push(element &&el);
  element pop();

 private:
  std::queue<element> m_queue;
  std::condition_variable m_pusher, m_popper;
  std::mutex m_mutex;
  using unique_lock = std::unique_lock<std::mutex>;
};

#endif  // NES_BASE_FRAME_QUEUE_
