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
  std::shared_ptr<AVCodecContextManager> _ctxmgr;

  using unique_lock = std::unique_lock<std::mutex>;

 public:
  CameraManager(std::shared_ptr<AVCodecContextManager> ctxmgr,
                uint32_t default_width, uint32_t default_height)
      : _ctxmgr(ctxmgr) {
    // Set default transformation matrix, width and height
    float init[] = {1.0f, 0.0f, 0.0f, 0.5f, 0.0f,  -1.0f,
                    0.0f, 0.5f, 0.0f, 0.0f, -1.0f, 0.5f};
    *this->_camera.mutable_matrix() = {init, init + 12};
    this->_camera.set_width(default_width);
    this->_camera.set_height(default_height);
  }
  void set_camera(nesproto::Camera camera) {
    // unique_lock lock(this->_mutex);
    // XXX: Is it okay to not wait for the conditional_variable??

    // Resolution has changed. Reset the encoder.
    if (this->_camera.width() != camera.width() ||
        this->_camera.height() != camera.height()) {
      this->_ctxmgr->codec_setup(camera.width(), camera.height());
    }
    this->_camera = camera;
  }

  nesproto::Camera get_camera() {
    // unique_lock lock(this->_mutex);
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
