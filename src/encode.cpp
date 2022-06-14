/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

#include "base/camera_manager.h"
#include "base/exceptions/lock_timeout.h"
#include "base/scoped_timer.h"
#include "base/server/packet_stream.h"
#include "base/video/frame_map.h"
#include "base/video/frame_queue.h"
#include "base/video/render_text.h"
#include "base/video/type_managers.h"

std::string timestamp() {
  using namespace std::chrono;
  using clock = system_clock;

  const auto current_time_point{clock::now()};
  const auto current_time{clock::to_time_t(current_time_point)};
  const auto current_localtime{*std::localtime(&current_time)};
  const auto current_time_since_epoch{current_time_point.time_since_epoch()};
  const auto current_milliseconds{
      duration_cast<milliseconds>(current_time_since_epoch).count() % 1000};

  std::ostringstream stream;
  stream << std::put_time(&current_localtime, "%T") << "." << std::setw(3)
         << std::setfill('0') << current_milliseconds;
  return stream.str();
}

void process_frame_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                          std::shared_ptr<FrameQueue> frame_queue,
                          std::shared_ptr<FrameMap> encode_queue,
                          std::shared_ptr<RenderTextContext> etctx,
                          std::atomic<bool> &shutdown_requested) {
  // set_thread_name("process_frame");
  int ret;

  while (!shutdown_requested) {
    try {
      std::unique_ptr<RenderedFrame> frame = frame_queue->pop();
      {
        ScopedTimer timer;
        uint64_t frame_index = frame->index();

        std::stringstream cam_matrix;
        int idx = 0;
        for (auto it : frame->get_cam().matrix()) {
          idx++;
          cam_matrix << std::fixed << std::showpos << std::setw(7)
                     << std::setprecision(5) << std::setfill('0') << it << ' ';
          if (idx % 4 == 0) {
            cam_matrix << '\n';
          }
        }
        cam_matrix << std::fixed << std::showpos << std::setw(7)
                   << std::setprecision(5) << std::setfill('0') << 0.f << ' '
                   << std::fixed << std::showpos << std::setw(7)
                   << std::setprecision(5) << std::setfill('0') << 0.f << ' '
                   << std::fixed << std::showpos << std::setw(7)
                   << std::setprecision(5) << std::setfill('0') << 0.f << ' '
                   << std::fixed << std::showpos << std::setw(7)
                   << std::setprecision(5) << std::setfill('0') << 1.f << ' ';

        etctx->render_string_to_frame(
            frame->source_frame(),
            RenderTextContext::RenderPosition::RENDER_POSITION_LEFT_BOTTOM,
            std::string("index=") + std::to_string(frame->index()));

        etctx->render_string_to_frame(
            frame->source_frame(),
            RenderTextContext::RenderPosition::RENDER_POSITION_LEFT_TOP,
            timestamp());

        etctx->render_string_to_frame(
            frame->source_frame(),
            RenderTextContext::RenderPosition::RENDER_POSITION_CENTER,
            cam_matrix.str());

        frame->convert_frame();

        encode_queue->insert(frame_index, std::move(frame));
        tlog::info() << "process_frame_thread (index=" << frame_index
                     << "): processed frame in " << timer.elapsed().count()
                     << " msec.";
      }
    } catch (const LockTimeout &) {
      continue;
    }
  }

  tlog::info() << "process_frame_thread: Exiting thread.";
}

void send_frame_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                       std::shared_ptr<FrameMap> encode_queue,
                       std::atomic<bool> &shutdown_requested) {
  // set_thread_name("send_frame");
  uint64_t frame_index = 0;
  while (!shutdown_requested) {
    try {
      ScopedTimer timer;
      std::unique_ptr<RenderedFrame> processed_frame =
          encode_queue->get_delete(frame_index);

      switch (ctxmgr->send_frame(
          processed_frame->converted_frame().to_avframe().get())) {
        case AVERROR(EINVAL):
          // Codec not opened, it is a decoder, or requires flush.
          std::runtime_error{
              "Codec not opened, it is a decoder, or requires flush."};
          break;
        case AVERROR(ENOMEM):
          // Failed to add packet to internal queue, or similar other errors:
          // legitimate encoding errors.
          std::runtime_error{
              "Failed to add packet to internal queue, or other."};
          break;
        case AVERROR_EOF:
          // The encoder has been flushed, and no new frames can be sent to it.
          std::runtime_error{
              "The encoder has been flushed, and no new frames can be sent to "
              "it."};
          break;
        case AVERROR(EAGAIN):
          // Input is not accepted in the current state - user must read output
          // with avcodec_receive_packet() (once all output is read, the packet
          // should be resent, and the call will not fail with EAGAIN).
        default:
          // Success.
          break;
      }

      tlog::info() << "send_frame_thread (index=" << frame_index
                   << "): sent frame to encoder in " << timer.elapsed().count()
                   << " msec.";
    } catch (const LockTimeout &) {
      // If the frame is not located until timeout, go to next frame.
      tlog::error() << "send_frame_thread (index=" << frame_index
                    << "): Timeout reached while waiting for frame. Skipping.";
    }
    frame_index++;
  }

  tlog::info() << "send_frame_thread: Shutdown requested.";
  // av_frame_unref(frm);
  // av_freep(&frm->data[0]);
  // av_frame_free(&frm);
  tlog::info() << "send_frame_thread: Exiting thread.";
}

int receive_packet_handler(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                           AVPacket *pkt,
                           std::shared_ptr<PacketStreamServer> mctx,
                           std::atomic<bool> &shutdown_requested) {
  int ret;

  while (!shutdown_requested) {
    ret = ctxmgr->receive_packet(pkt);

    switch (ret) {
      case AVERROR(EAGAIN):  // output is not available in the current state -
                             // user must try to send input
        // We must sleep here so that other threads can acquire
        // AVCodecContext.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        break;
      case 0:
        mctx->consume_packet(pkt);
        return 0;
      case AVERROR(EINVAL):  // codec not opened, or it is a decoder other
                             // errors: legitimate encoding errors
      default:
        tlog::error() << "receive_packet_handler: Failed to receive "
                         "packet: "
            /* << averror_explain(ret)*/;
      case AVERROR_EOF:  // the encoder has been fully flushed, and there will
                         // be no more output packets
        return -1;
    }
  }

  return -1;
}

void receive_packet_thread(std::shared_ptr<types::AVCodecContextManager> ctxmgr,
                           std::shared_ptr<PacketStreamServer> mctx,
                           std::atomic<bool> &shutdown_requested) {
  // set_thread_name("receive_packet");
  while (!shutdown_requested) {
    types::AVPacketManager pkt;
    try {
      if (receive_packet_handler(ctxmgr, pkt(), mctx,
                                 std::ref(shutdown_requested)) < 0) {
        shutdown_requested = true;
      }
    } catch (const LockTimeout &) {
      tlog::info() << "receive_packet_thread: lock_timeout while acquiring "
                      "resource lock for AVCodecContext.";
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  tlog::info() << "receive_packet_thread: Shutdown requested.";
  tlog::info() << "receive_packet_thread: Exiting thread.";
}

void encode_stats_thread(std::atomic<std::uint64_t> &frame_index,
                         std::atomic<bool> &shutdown_requested) {
  // set_thread_name("encode_stats");
  uint64_t previous_index = 0;
  while (!shutdown_requested) {
    uint64_t current_index = frame_index.load();
    tlog::info()
        << "encode_stats_thread: Average frame rate of the last 3 seconds: "
        << (current_index - previous_index) / 3 << " fps.";
    previous_index = current_index;
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}
