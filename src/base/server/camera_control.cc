// Copyright (c) 2022 Moonsik Park.

#include "base/server/camera_control.h"

#include "base/logging.h"
#include "nes.pb.h"

CameraControlServer::CameraControlServer(
    std::shared_ptr<CameraManager> cameramgr, uint16_t bind_port)
    : WebSocketServer(std::string("CameraControlServer"), bind_port),
      m_camera_manager(cameramgr) {}

void CameraControlServer::message_handler(websocketpp::connection_hdl hdl,
                                          message_ptr msg) {
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
