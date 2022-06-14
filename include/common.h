/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   common.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <nes.pb.h>
#include <sys/prctl.h>
#include <tinylogger/tinylogger.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

void set_thread_name(std::string name);

// clang-format off
#include "base/video/type_managers.h"
#include "base/video/frame_map.h"
#include "base/video/frame_queue.h"
#include "base/video/rendered_frame.h"
#include "base/scoped_timer.h"
#include "base/exceptions/lock_timeout.h"
#include <wsserver.h>
#include <encode.h>

#include "base/camera.h"
#include <encode_text.h>
#include <muxing.h>
#include <server.h>

#endif  // _COMMON_H_
