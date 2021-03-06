// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_CAMERA_MANAGER_
#define NES_BASE_CAMERA_MANAGER_

#include <memory>

#include "base/video/type_managers.h"
#include "nes.pb.h"

// CameraManager handles camera matrix used for rendering a frame. It accepts a
// user position converted to a camera matrix, stores it internally, and
// provides it when a FrameRequest is generated.
class CameraManager {
 public:
  // Initial camera matrix set to the initial coordinate (0, 0, 0) and field of
  // view.
  static constexpr float kInitialCameraMatrix[] = {
      1.0f, 0.0f, 0.0f, 0.5f, 0.0f, -1.0f, 0.0f, 0.5f, 0.0f, 0.0f, -1.0f, 0.5f};

  // Initialize Camera with kInitialCameraMatrix and provided default
  // dimensions.
  CameraManager(std::shared_ptr<types::AVCodecContextManager> codec_scene_left,
                std::shared_ptr<types::AVCodecContextManager> codec_depth_left,
                std::shared_ptr<types::AVCodecContextManager> codec_scene_right,
                std::shared_ptr<types::AVCodecContextManager> codec_depth_right,
                uint32_t default_width, uint32_t default_height);

  // Replace camera with the provided camera data. If the resolution has
  // changed, reinitialize the encoder.
  void set_camera_left(nesproto::Camera camera);
  void set_camera_right(nesproto::Camera camera);

  inline nesproto::Camera get_camera_left() const { return m_camera_left; }
  inline nesproto::Camera get_camera_right() const { return m_camera_right; }

 private:
  nesproto::Camera m_camera_left;
  nesproto::Camera m_camera_right;
  std::shared_ptr<types::AVCodecContextManager> m_codec_scene_left;
  std::shared_ptr<types::AVCodecContextManager> m_codec_depth_left;
  std::shared_ptr<types::AVCodecContextManager> m_codec_scene_right;
  std::shared_ptr<types::AVCodecContextManager> m_codec_depth_right;
};

#endif  // NES_BASE_CAMERA_MANAGER_
