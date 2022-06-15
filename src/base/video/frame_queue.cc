// Copyright (c) 2022 Moonsik Park.

#include "base/video/frame_queue.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include "base/exceptions/lock_timeout.h"

void FrameQueue::push(element &&el) {
  unique_lock lock(m_mutex);
  // Acquire the lock when the mutex is released and the queue is not full.
  // If lock timeout is reached, throw LockTimeout exception.
  if (m_pusher.wait_for(lock, kFrameQueueLockTimeout,
                        [&] { return m_queue.size() < kFrameQueueMaxSize; })) {
    m_queue.push(std::forward<element>(el));
    // Notify one of the threads waiting to pop from the queue.
    m_popper.notify_one();
  } else {
    throw LockTimeout{};
  }
}

FrameQueue::element FrameQueue::pop() {
  unique_lock lock(m_mutex);
  // Acquire a lock when the mutex is released and the queue is not empty.
  // If lock timeout is reached, throw LockTimeout exception.
  if (m_popper.wait_for(lock, kFrameQueueLockTimeout,
                        [&] { return m_queue.size() > 0; })) {
    element item = std::move(m_queue.front());
    m_queue.pop();
    // Notify one of the threads waiting to push to the queue.
    m_pusher.notify_one();
    return item;
  } else {
    throw LockTimeout{};
  }
}
