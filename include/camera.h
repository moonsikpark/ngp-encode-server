/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   camera.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <common.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class CameraManager
{
private:
    nesproto::Camera _camera;
    std::condition_variable _wait;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    CameraManager()
    {
        float init[] = {1.0f, 0.0f, 0.0f, 0.5f,
                        0.0f, -1.0f, 0.0f, 0.5f,
                        0.0f, 0.0f, -1.0f, 0.5f};
        *this->_camera.mutable_matrix() = {init, init + 12};
    }
    void set_camera(nesproto::Camera camera)
    {
        unique_lock lock(this->_mutex);
        // XXX: Is it okay to not wait for the conditional_variable??
        this->_camera = camera;
    }

    nesproto::Camera get_camera()
    {
        unique_lock lock(this->_mutex);
        return this->_camera;
    }
};

void camera_websocket_main_thread(CameraManager &cameramgr, uint16_t bind_port, std::string server_cert_location, std::string dhparam_location, std::atomic<bool> &shutdown_requested);
void framerequest_provider_thread(VideoEncodingParams &veparams, CameraManager &cameramgr, ThreadSafeQueue<nesproto::FrameRequest> &request_queue, std::atomic<bool> &shutdown_requested);

#endif // _CAMERA_H_
