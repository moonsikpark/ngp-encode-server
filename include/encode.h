/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _ENCODE_H_
#define _ENCODE_H_

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <encode_text.h>
#include <muxing.h>

#include "base/video/frame_queue.h"
void process_frame_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                          std::shared_ptr<FrameQueue> frame_queue,
                          std::shared_ptr<FrameMap> encode_queue,
                          std::shared_ptr<EncodeTextContext> etctx,
                          std::atomic<bool> &shutdown_requested);
void send_frame_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                       std::shared_ptr<FrameMap> encode_queue,
                       std::atomic<bool> &shutdown_requested);
void receive_packet_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                           std::shared_ptr<MuxingContext> mctx,
                           std::atomic<bool> &shutdown_requested);
void encode_stats_thread(std::atomic<std::uint64_t> &frame_index,
                         std::atomic<bool> &shutdown_requested);

#endif  // _ENCODE_H_
