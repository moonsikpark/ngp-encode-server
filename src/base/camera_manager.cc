// Copyright (c) 2022 Moonsik Park.

#include "base/camera_manager.h"

#include <cstdint>
#include <memory>

#include "base/logging.h"
#include "base/video/type_managers.h"
#include "nes.pb.h"

CameraManager::CameraManager(
    std::shared_ptr<types::AVCodecContextManager> codec_scene_left,
    std::shared_ptr<types::AVCodecContextManager> codec_depth_left,
    std::shared_ptr<types::AVCodecContextManager> codec_scene_right,
    std::shared_ptr<types::AVCodecContextManager> codec_depth_right,
    uint32_t default_width, uint32_t default_height)
    : m_codec_scene_left(codec_scene_left),
      m_codec_depth_left(codec_depth_left),
      m_codec_scene_right(codec_scene_right),
      m_codec_depth_right(codec_depth_right) {
  *m_camera_left.mutable_matrix() = {kInitialCameraMatrix,
                                     kInitialCameraMatrix + 12};
  m_camera_left.set_width(default_width);
  m_camera_left.set_height(default_height);

  *m_camera_right.mutable_matrix() = {kInitialCameraMatrix,
                                      kInitialCameraMatrix + 12};
  m_camera_right.set_width(default_width);
  m_camera_right.set_height(default_height);
}

void CameraManager::set_camera_left(nesproto::Camera camera) {
  if (m_camera_left.width() != camera.width() ||
      m_camera_left.height() != camera.height()) {
    // Resolution changed. Reinitialize the encoder.

    // Resolution must be divisible by 2.
    if (camera.width() % 2 != 0) {
      camera.set_width(camera.width() - 1);
    }
    if (camera.height() % 2 != 0) {
      camera.set_height(camera.height() - 1);
    }

    m_codec_scene_left->change_resolution(camera.width(), camera.height());
    m_codec_depth_left->change_resolution(camera.width(), camera.height());
  }
  m_camera_left = camera;
}

void CameraManager::set_camera_right(nesproto::Camera camera) {
  if (m_camera_right.width() != camera.width() ||
      m_camera_right.height() != camera.height()) {
    // Resolution changed. Reinitialize the encoder.

    // Resolution must be divisible by 2.
    if (camera.width() % 2 != 0) {
      camera.set_width(camera.width() - 1);
    }
    if (camera.height() % 2 != 0) {
      camera.set_height(camera.height() - 1);
    }

    m_codec_scene_right->change_resolution(camera.width(), camera.height());
    m_codec_depth_right->change_resolution(camera.width(), camera.height());
  }
  m_camera_right = camera;
}
