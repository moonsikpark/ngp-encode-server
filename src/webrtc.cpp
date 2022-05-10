/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   webrtc.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include "nlohmann/json.hpp"
#include <webrtc.h>

void webrtc_main_thread(std::atomic<bool> &shutdown_requested)
{
    set_userspace_thread_name("webrtc_main_thread");
}
