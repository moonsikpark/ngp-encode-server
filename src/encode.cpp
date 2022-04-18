/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <common.h>
#include <encode.h>

std::string averror_explain(int err)
{
    char errbuf[500];
    if (av_strerror(err, errbuf, 500) < 0)
    {
        return std::string("<Failed to get error message>");
    }

    return std::string(errbuf);
}

void process_frame_thread(AVCodecContextManager &ctxmgr, ThreadSafeQueue<RenderedFrame> &queue, EncodeTextContext etctx, std::atomic<bool> &shutdown_requested)
{
    AVFrame *frm = av_frame_alloc();
    int ret;

    while (!shutdown_requested)
    {
        try
        {
            RenderedFrame r = queue.pop();
            encode_textctx_render_string_to_image(&etctx, r.buffer(), ctxmgr.get_context()->width, ctxmgr.get_context()->height, RenderPositionOption_LEFT_BOTTOM, std::string("framecount=") + std::to_string(0));

            r.convert_frame(ctxmgr.get_context(), frm);

            {
                ResourceLock<std::mutex, AVCodecContext> lock{ctxmgr.get_mutex(), ctxmgr.get_context()};
                AVCodecContext *ctx = lock.get();

                // TODO: handle error codes!
                // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
                // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga9395cb802a5febf1f00df31497779169
                ret = avcodec_send_frame(ctx, frm);
            }
        }
        catch (const lock_timeout &)
        {
            continue;
        }
    }

    tlog::info() << "process_frame_thread: Shutdown requested.";
    av_frame_free(&frm);
    tlog::info() << "process_frame_thread: Exiting thread.";
}

void receive_packet_thread(AVCodecContextManager &ctxmgr, MuxingContext mctx, std::atomic<bool> &shutdown_requested)
{
    uint64_t frame_count;
    AVPacket *pkt = av_packet_alloc();
    AVRational h264_timebase = {1, 90000};
    AVRational ctx_timebase;
    int ret;

    while (!shutdown_requested)
    {
        try
        {
            ResourceLock<std::mutex, AVCodecContext> lock{ctxmgr.get_mutex(), ctxmgr.get_context()};
            AVCodecContext *ctx = lock.get();
            ret = avcodec_receive_packet(ctx, pkt);
            if (ret == 0)
            {
                ctx_timebase = ctx->time_base;
            }
        }
        catch (const lock_timeout &)
        {
            continue;
        }

        switch (ret)
        {
        case AVERROR(EAGAIN): // output is not available in the current state - user must try to send input
            continue;
        case 0:
            // TODO: can we set packet's dts and/or pts when we draw an avframe?
            pkt->pts = pkt->dts = av_rescale_q(frame_count, ctx_timebase, h264_timebase);
            // TODO: add more info to print
            tlog::info() << "receive_packet_thread: Received packet; pts=" << pkt->pts << " dts=" << pkt->dts << " size=" << pkt->size;
            if ((ret = av_interleaved_write_frame(mctx.oc, pkt)) < 0)
            {
                tlog::error() << "receive_packet_thread: Failed to write frame to muxing context: " << averror_explain(ret);
            }
            frame_count++;
            break;
        case AVERROR(EINVAL): // codec not opened, or it is a decoder other errors: legitimate encoding errors
        default:
            tlog::error() << "receive_packet_thread: Failed to receive packet: " << averror_explain(ret);
        case AVERROR_EOF: // the encoder has been fully flushed, and there will be no more output packets
            shutdown_requested = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    tlog::info() << "receive_packet_thread: Shutdown requested.";
    av_packet_free(&pkt);
    tlog::info() << "receive_packet_thread: Exiting thread.";
}
