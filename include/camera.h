/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   camera.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <common.h>

class CameraManager {
 private:
  nesproto::Camera _camera;
  std::condition_variable _wait;
  std::mutex _mutex;

  using unique_lock = std::unique_lock<std::mutex>;

 public:
  CameraManager() {
    float init[] = {1.0f, 0.0f, 0.0f, 0.5f, 0.0f,  -1.0f,
                    0.0f, 0.5f, 0.0f, 0.0f, -1.0f, 0.5f};
    *this->_camera.mutable_matrix() = {init, init + 12};
  }
  void set_camera(nesproto::Camera camera) {
    //unique_lock lock(this->_mutex);
    // XXX: Is it okay to not wait for the conditional_variable??
    this->_camera = camera;
  }

  nesproto::Camera get_camera() {
    //unique_lock lock(this->_mutex);
    return this->_camera;
  }
};

class CameraControlServer : public WebSocketServer {
 private:
  std::shared_ptr<CameraManager> m_cameramgr;

 public:
  CameraControlServer(std::shared_ptr<CameraManager> cameramgr,
                      uint16_t bind_port)
      : WebSocketServer(std::string("CameraControlServer"), bind_port),
        m_cameramgr(cameramgr) {}

  void message_handler(websocketpp::connection_hdl hdl, message_ptr msg) {
    nesproto::Camera cam;

    if (cam.ParseFromString(msg->get_raw_payload())) {
      m_cameramgr->set_camera(cam);
      tlog::success() << "CameraControlServer: Got Camera matrix.";
    } else {
      tlog::error() << "CameraControlServer: Failed to set camera matrix.";
    }
  }
};

#endif  // _CAMERA_H_
