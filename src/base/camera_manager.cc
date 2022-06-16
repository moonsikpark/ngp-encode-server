// Copyright (c) 2022 Moonsik Park.

#include "base/camera_manager.h"

#include <cstdint>
#include <memory>

#include "base/video/type_managers.h"
#include "nes.pb.h"

CameraManager::CameraManager(
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth,
    uint32_t default_width, uint32_t default_height)
    : m_ctxmgr_scene(ctxmgr_scene), m_ctxmgr_depth(ctxmgr_depth) {
  *m_camera.mutable_matrix() = {kInitialCameraMatrix,
                                kInitialCameraMatrix + 12};
  m_camera.set_width(default_width);
  m_camera.set_height(default_height);
}

void CameraManager::set_camera(nesproto::Camera camera) {
  if (m_camera.width() != camera.width() ||
      m_camera.height() != camera.height()) {
    // Resolution changed. Reinitialize the encoder.
    m_ctxmgr_scene->change_resolution(camera.width(), camera.height());
    m_ctxmgr_depth->change_resolution(camera.width(), camera.height());
  }
  m_camera = camera;
}
