/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   webrtc.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _WEBRTC_H_
#define _WEBRTC_H_

#include <common.h>

void webrtc_main_thread(std::atomic<bool> &shutdown_requested);

#endif // _WEBRTC_H_
