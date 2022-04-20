/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   pov.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _POV_H_
#define _POV_H_

#include <common.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class POV
{
public:
    float x;
    float y;
    float z;
    float rotx;
    float roty;

    POV() : x(0.f), y(0.f), z(0.f), rotx(0.f), roty(0.f) {}
    POV(float x, float y, float z, float rotx, float roty) : x(x), y(y), z(z), rotx(rotx), roty(roty) {}
};

class POVManager
{
private:
    POV _pov;
    std::condition_variable _wait;
    std::mutex _mutex;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    void set_pov(POV &&pov)
    {
        unique_lock lock(this->_mutex);
        // XXX: Is it okay to not wait for the conditional_variable??
        this->_pov = std::move(pov);
    }

    POV get_pov()
    {
        // POV data will be copy-assigned to the caller.
        unique_lock lock(this->_mutex);
        return this->_pov;
    }
};

#endif // _POV_H_
