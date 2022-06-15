// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_VIDEO_FRAME_MAP_
#define NES_BASE_VIDEO_FRAME_MAP_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>

#include "base/video/rendered_frame.h"

// Thread-safe Map implementation based on std::map used to store unique_ptr of
// RenderedFrame.
class FrameMap {
 public:
  // Max size of FrameMap.
  static constexpr std::size_t kFrameMapMaxSize = 100;

  // Timeout waiting for insert/get of FrameMap.
  // This timeout value is important to skip frames that are taking too long
  // to render or occured an error while rendering.
  static constexpr std::chrono::milliseconds kFrameMapLockTimeout{1000};

  // Interval of cleaning up unused Frames (i.e. frames older than the
  // requested frame).
  static constexpr unsigned kFrameMapDropFramesInterval = 1000;

  using element = std::unique_ptr<RenderedFrame>;
  using keytype = std::uint64_t;

  void insert(keytype index, element &&el);
  element get_delete(keytype index);

 private:
  std::map<keytype, element> m_map;
  std::condition_variable m_getter, m_inserter;
  std::mutex m_mutex;
  using unique_lock = std::unique_lock<std::mutex>;
};

#endif  // NES_BASE_FRAME_MAP_
