// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_SERVER_CAMERA_CONTROL_
#define NES_BASE_SERVER_CAMERA_CONTROL_

#include <memory>

#include "base/camera_manager.h"
#include "base/logging.h"
#include "base/server/websocket_server.h"

// A Websocket server that receives nesproto::Camera.
class CameraControlServer : public WebSocketServer {
 public:
  // Interval of logging the receive event.
  static constexpr unsigned kReceivedLoggingInterval = 1000;

  CameraControlServer(std::shared_ptr<CameraManager> cameramgr,
                      uint16_t bind_port);

  void message_handler(websocketpp::connection_hdl hdl, message_ptr msg);

 private:
  std::shared_ptr<CameraManager> m_camera_manager;
  uint64_t m_message_count = 0;
};

#endif  // NES_BASE_SERVER_CAMERA_CONTROL_
