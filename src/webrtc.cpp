/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   webrtc.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include "nlohmann/json.hpp"
#include <common.h>
#include <webrtc.h>

#include <streamer/main.hpp>

void webrtc_main_thread(std::shared_ptr<AVCodecContextManager> ctxmgr, std::atomic<bool> &shutdown_requested)
{
    set_userspace_thread_name("webrtc_main_thread");
    stmain(ctxmgr, std::ref(shutdown_requested));
}
