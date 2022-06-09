/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <common.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string timestamp()
{
    using namespace std::chrono;
    using clock = system_clock;
    
    const auto current_time_point {clock::now()};
    const auto current_time {clock::to_time_t (current_time_point)};
    const auto current_localtime {*std::localtime (&current_time)};
    const auto current_time_since_epoch {current_time_point.time_since_epoch()};
    const auto current_milliseconds {duration_cast<milliseconds> (current_time_since_epoch).count() % 1000};
    
    std::ostringstream stream;
    stream << std::put_time (&current_localtime, "%T") << "." << std::setw (3) << std::setfill ('0') << current_milliseconds;
    return stream.str();
}

std::string averror_explain(int err) {
  char errbuf[500];
  if (av_strerror(err, errbuf, 500) < 0) {
    return std::string("<Failed to get error message>");
  }

  return std::string(errbuf);
}

void process_frame_thread(
    std::shared_ptr<VideoEncodingParams> veparams,
    std::shared_ptr<AVCodecContextManager> ctxmgr,
    std::shared_ptr<ThreadSafeQueue<std::unique_ptr<RenderedFrame>>>
        frame_queue,
    std::shared_ptr<ThreadSafeMap<RenderedFrame>> encode_queue,
    std::shared_ptr<EncodeTextContext> etctx,
    std::atomic<bool> &shutdown_requested) {
  set_userspace_thread_name("process_frame");
  int ret;

  while (!shutdown_requested) {
    try {
      std::unique_ptr<RenderedFrame> frame = frame_queue->pop();
      {
        ScopedTimer timer;
        uint64_t frame_index = frame->index();

        nesproto::Camera cam = frame->get_cam();

        std::stringstream cam_matrix;
        int idx = 0;
        for (auto it: cam.matrix()) {
            idx++;
            cam_matrix << std::fixed << std::showpos << std::setw(7) << std::setprecision(5) << std::setfill('0') << it << ' ';
            if (idx % 4 == 0) {
              cam_matrix << '\n';
            }
        }
        cam_matrix << std::fixed << std::showpos << std::setw(7) << std::setprecision(5) << std::setfill('0') << 0.f << ' ';
        cam_matrix << std::fixed << std::showpos << std::setw(7) << std::setprecision(5) << std::setfill('0') << 0.f << ' ';
        cam_matrix << std::fixed << std::showpos << std::setw(7) << std::setprecision(5) << std::setfill('0') << 0.f << ' ';
        cam_matrix << std::fixed << std::showpos << std::setw(7) << std::setprecision(5) << std::setfill('0') << 1.f << ' ';

        etctx->render_string_to_frame(
            frame, EncodeTextContext::RenderPositionOption::LEFT_BOTTOM,
            std::string("index=") + std::to_string(frame_index));

        etctx->render_string_to_frame(
            frame, EncodeTextContext::RenderPositionOption::LEFT_TOP,
            timestamp());

        etctx->render_string_to_frame(
            frame, EncodeTextContext::RenderPositionOption::CENTER,
            cam_matrix.str());

        frame->convert_frame(veparams);

        encode_queue->insert(frame_index, std::move(frame));
        tlog::info() << "process_frame_thread (index=" << frame_index
                     << "): processed frame in " << timer.elapsed().count()
                     << " msec.";
      }
    } catch (const lock_timeout &) {
      continue;
    }
  }

  tlog::info() << "process_frame_thread: Exiting thread.";
}

void send_frame_thread(
    std::shared_ptr<VideoEncodingParams> veparams,
    std::shared_ptr<AVCodecContextManager> ctxmgr,
    std::shared_ptr<ThreadSafeMap<RenderedFrame>> encode_queue,
    std::atomic<bool> &shutdown_requested) {
  set_userspace_thread_name("send_frame");
  AVFrame *frm;
  uint64_t frame_index = 0;
  int ret;

  if (!(frm = av_frame_alloc())) {
    throw std::runtime_error{"send_frame_thread: Failed to allocate AVFrame."};
  }

  while (!shutdown_requested) {
    try {
      std::unique_ptr<RenderedFrame> processed_frame =
          encode_queue->pop_el(frame_index);
      {
        ScopedTimer timer;

        frm->format = veparams->pix_fmt();
        frm->width = processed_frame->width();
        frm->height = processed_frame->height();

        if ((ret = av_image_alloc(frm->data, frm->linesize, processed_frame->width(),
                                  processed_frame->height(), veparams->pix_fmt(),
                                  32)) < 0) {
          throw std::runtime_error{
              std::string(
                  "send_frame_thread: Failed to allocate AVFrame data: ") +
              averror_explain(ret)};
        }

        ret = av_image_fill_pointers(frm->data, veparams->pix_fmt(),
                                     processed_frame->height(),
                                     processed_frame->processed_data(),
                                     processed_frame->processed_linesize());
        {
          std::unique_lock<std::mutex> lock(ctxmgr->get_mutex());
          AVCodecContext *ctx = ctxmgr->get_context();

          // TODO: handle error codes!
          // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
          // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga9395cb802a5febf1f00df31497779169
          ret = avcodec_send_frame(ctx, frm);
        }
        av_frame_unref(frm);
        tlog::info() << "send_frame_thread (index=" << frame_index
                     << "): sent frame to encoder in "
                     << timer.elapsed().count() << " msec.";
        frame_index++;
      }
    } catch (const lock_timeout &) {
      // If the frame is not located until timeout, go to next frame.
      tlog::error() << "send_frame_thread (index=" << frame_index
                    << "): Timeout reached while waiting for frame. Skipping.";
      frame_index++;
      continue;
    }
  }

  tlog::info() << "send_frame_thread: Shutdown requested.";
  av_frame_unref(frm);
  av_freep(&frm->data[0]);
  av_frame_free(&frm);
  tlog::info() << "send_frame_thread: Exiting thread.";
}

int receive_packet_handler(std::shared_ptr<AVCodecContextManager> ctxmgr,
                           AVPacket *pkt, std::shared_ptr<MuxingContext> mctx,
                           std::atomic<bool> &shutdown_requested) {
  int ret;

  while (!shutdown_requested) {
    {
      // The lock must be in this scope so that it would be unlocked right after
      // avcodec_receive_packet() returns.
      std::unique_lock<std::mutex> lock(ctxmgr->get_mutex());
      AVCodecContext *ctx = ctxmgr->get_context();
      ret = avcodec_receive_packet(ctx, pkt);
    }

    switch (ret) {
    case AVERROR(EAGAIN): // output is not available in the current state - user
                          // must try to send input
      // We must sleep here so that other threads can acquire AVCodecContext.
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      break;
    case 0:
      mctx->consume_packet(pkt);
      return 0;
    case AVERROR(EINVAL): // codec not opened, or it is a decoder other errors:
                          // legitimate encoding errors
    default:
      tlog::error() << "receive_packet_handler: Failed to receive packet: "
                    << averror_explain(ret);
    case AVERROR_EOF: // the encoder has been fully flushed, and there will be
                      // no more output packets
      return -1;
    }
  }

  return -1;
}

void receive_packet_thread(std::shared_ptr<AVCodecContextManager> ctxmgr,
                           std::shared_ptr<MuxingContext> mctx,
                           std::atomic<bool> &shutdown_requested) {
  set_userspace_thread_name("receive_packet");
  while (!shutdown_requested) {
    AVPacketManager pktmgr;
    try {
      if (receive_packet_handler(ctxmgr, pktmgr.get(), mctx,
                                 std::ref(shutdown_requested)) < 0) {
        shutdown_requested = true;
      }
    } catch (const lock_timeout &) {
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
  set_userspace_thread_name("encode_stats");
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
