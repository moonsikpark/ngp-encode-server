// Copyright (c) 2022 Moonsik Park.

#include "base/video/frame_map.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include "base/exceptions/lock_timeout.h"
#include "base/logging.h"

void FrameMap::insert(FrameMap::keytype index, FrameMap::element&& item) {
  unique_lock lock(m_mutex);
  // Acquire the lock when the mutex is released and the map is not full.
  // If lock timeout is reached, throw LockTimeout exception.
  if (m_inserter.wait_for(lock, kFrameMapLockTimeout,
                          [&] { return m_map.size() < kFrameMapMaxSize; })) {
    m_map.insert({index, std::forward<FrameMap::element>(item)});
    // Notify one of the threads waiting to get from the map.
    m_getter.notify_all();
  } else {
    throw LockTimeout{};
  }
}

FrameMap::element FrameMap::get_delete(FrameMap::keytype index) {
  unique_lock lock(m_mutex);
  // Acquire a lock when the mutex is released and there is a frame of requested
  // index. If lock timeout is reached, throw LockTimeout exception.
  if (m_getter.wait_for(lock, kFrameMapLockTimeout,
                        [&] { return m_map.contains(index); })) {
    element elem = std::move(m_map.at(index));
    m_map.erase(index);
    // When interval is reached, delete frames where frame_index < index to
    // clean up unused frames.
    if (index % kFrameMapDropFramesInterval == 0) {
      const auto count = std::erase_if(m_map, [&index](const auto& item) {
        auto const& [key, value] = item;
        return key < index;
      });

      if (count) {
        tlog::error() << "FrameMap: " << count
                      << "frames dropped; current index=" << index;
      }
    }
    // Notify one of the threads waiting to insert to the map.
    m_inserter.notify_one();
    return elem;
  } else {
    throw LockTimeout{};
  }
}
