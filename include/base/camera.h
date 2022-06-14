// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_CAMERA_
#define NES_BASE_CAMERA_

#include <memory>

#include "base/video/type_managers.h"
#include "nes.pb.h"
#include "wsserver.h"

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
  CameraManager(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                uint32_t default_width, uint32_t default_height)
      : m_ctxmgr(ctxmgr) {
    *m_camera_data.mutable_matrix() = {kInitialCameraMatrix,
                                       kInitialCameraMatrix + 12};
    m_camera_data.set_width(default_width);
    m_camera_data.set_height(default_height);
  }

  // Replace camera with the provided camera data. If the resolution has
  // changed, reinitialize the encoder.
  void set_camera(nesproto::Camera camera) {
    if (m_camera_data.width() != camera.width() ||
        m_camera_data.height() != camera.height()) {
      // Resolution changed. Reinitialize the encoder.
      m_ctxmgr->change_resolution(camera.width(), camera.height());
    }
    m_camera_data = camera;
  }

  inline nesproto::Camera get_camera() const { return m_camera_data; }

 private:
  nesproto::Camera m_camera_data;
  std::shared_ptr<types::AVCodecContextManager> m_ctxmgr;
};

// A Websocket server that receives nesproto::Camera.
class CameraControlServer : public WebSocketServer {
 public:
  // Interval of logging the receive event.
  static constexpr unsigned kReceivedLoggingInterval = 1000;

  CameraControlServer(std::shared_ptr<CameraManager> cameramgr,
                      uint16_t bind_port)
      : WebSocketServer(std::string("CameraControlServer"), bind_port),
        m_camera_manager(cameramgr) {}

  void message_handler(websocketpp::connection_hdl hdl, message_ptr msg) {
    nesproto::Camera cam;

    if (cam.ParseFromString(msg->get_raw_payload())) {
      m_camera_manager->set_camera(cam);
      if (m_message_count % kReceivedLoggingInterval == 0) {
        tlog::success()
            << "CameraControlServer: Receiving camera matrix... count="
            << m_message_count;
      }
    } else {
      tlog::error() << "CameraControlServer: Failed to set camera matrix.";
    }
    m_message_count++;
  }

 private:
  std::shared_ptr<CameraManager> m_camera_manager;
  uint64_t m_message_count = 0;
};

#endif  // NES_BASE_CAMERA_
