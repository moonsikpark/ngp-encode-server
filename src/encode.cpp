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

void receive_packet_thread(AVCodecContext ctx, MuxingContext mctx, bool threads_stop_running)
{
    uint64_t frame_count;
    AVPacket *pkt = av_packet_alloc();
    AVRational h264_timebase = {1, 90000};
    int ret;
    bool keep_running = true;

    while (keep_running)
    {
        if (threads_stop_running)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // XXX: Each and every access to AVCodecContext should be protected.
        // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
        ret = avcodec_receive_packet(&ctx, pkt);

        switch (ret)
        {
        case AVERROR(EAGAIN): // output is not available in the current state - user must try to send input
            continue;
        case 0:
            // TODO: can we set packet's dts and/or pts when we draw an avframe?
            pkt->pts = pkt->dts = av_rescale_q(frame_count, ctx.time_base, h264_timebase);
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
            keep_running = false;
            break;
        }
    }

    av_packet_free(&pkt);
    tlog::info() << "receive_packet_thread: exiting thread.";
}
