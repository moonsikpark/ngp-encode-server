/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <common.h>

std::string averror_explain(int err)
{
    char errbuf[500];
    if (av_strerror(err, errbuf, 500) < 0)
    {
        return std::string("<Failed to get error message>");
    }

    return std::string(errbuf);
}

void process_frame_thread(VideoEncodingParams &veparams, AVCodecContextManager &ctxmgr, ThreadSafeQueue<std::unique_ptr<RenderedFrame>> &frame_queue, ThreadSafeMap<RenderedFrame> &encode_queue, EncodeTextContext &etctx, std::atomic<bool> &shutdown_requested)
{
    int ret;

    while (!shutdown_requested)
    {
        try
        {
            std::unique_ptr<RenderedFrame> frame = frame_queue.pop();
            {
                ScopedTimer timer;
                uint64_t frame_index = frame->index();

                etctx.render_string_to_frame(frame, EncodeTextContext::RenderPositionOption::LEFT_BOTTOM, std::string("index=") + std::to_string(frame_index));

                frame->convert_frame(veparams);

                encode_queue.insert(frame_index, std::move(frame));
                tlog::info() << "process_frame_thread (index=" << frame_index << "): processed frame in " << timer.elapsed().count() << " msec.";
            }
        }
        catch (const lock_timeout &)
        {
            continue;
        }
    }

    tlog::info() << "process_frame_thread: Exiting thread.";
}

void send_frame_thread(VideoEncodingParams &veparams, AVCodecContextManager &ctxmgr, ThreadSafeMap<RenderedFrame> &encode_queue, std::atomic<bool> &shutdown_requested)
{
    AVFrame *frm;
    uint64_t frame_index = 0;
    int ret;

    if (!(frm = av_frame_alloc()))
    {
        throw std::runtime_error{"send_frame_thread: Failed to allocate AVFrame."};
    }

    while (!shutdown_requested)
    {
        try
        {
            std::unique_ptr<RenderedFrame> processed_frame = encode_queue.pop_el(frame_index);
            {
                ScopedTimer timer;

                frm->format = veparams.pix_fmt();
                frm->width = veparams.width();
                frm->height = veparams.height();

                if ((ret = av_image_alloc(frm->data, frm->linesize, veparams.width(), veparams.height(), veparams.pix_fmt(), 32)) < 0)
                {
                    throw std::runtime_error{std::string("send_frame_thread: Failed to allocate AVFrame data: ") + averror_explain(ret)};
                }

                ret = av_image_fill_pointers(frm->data, veparams.pix_fmt(), veparams.height(), processed_frame->processed_data(), processed_frame->processed_linesize());
                {
                    ResourceLock<std::mutex, AVCodecContext> lock{ctxmgr.get_mutex(), ctxmgr.get_context()};
                    AVCodecContext *ctx = lock.get();

                    // TODO: handle error codes!
                    // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
                    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga9395cb802a5febf1f00df31497779169
                    ret = avcodec_send_frame(ctx, frm);
                }
                av_frame_unref(frm);
                tlog::info() << "send_frame_thread (index=" << frame_index << "): sent frame to encoder in " << timer.elapsed().count() << " msec.";
                frame_index++;
            }
        }
        catch (const lock_timeout &)
        {
            // If the frame is not located until timeout, go to next frame.
            tlog::error() << "send_frame_thread (index=" << frame_index << "): Timeout reached while waiting for frame. Skipping.";
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

void receive_packet_thread(AVCodecContextManager &ctxmgr, MuxingContext &mctx, std::atomic<bool> &shutdown_requested)
{
    int ret;

    while (!shutdown_requested)
    {
        {
            ScopedTimer timer;
            // TODO: Gracefully create and free AVPackets.
            AVPacket *pkt = av_packet_alloc();
            try
            {
                ResourceLock<std::mutex, AVCodecContext> lock{ctxmgr.get_mutex(), ctxmgr.get_context()};
                AVCodecContext *ctx = lock.get();
                ret = avcodec_receive_packet(ctx, pkt);
            }
            catch (const lock_timeout &)
            {
                av_packet_free(&pkt);
                continue;
            }

            switch (ret)
            {
            case AVERROR(EAGAIN): // output is not available in the current state - user must try to send input

                av_packet_free(&pkt);
                continue;
            case 0:
                // Packet pts and dts will be based on wall clock.
                pkt->pts = pkt->dts = av_rescale_q(av_gettime(), AV_TIME_BASE_Q, mctx.get_stream()->time_base);
                // TODO: add more info to print
                tlog::info() << "receive_packet_thread: Received packet in " << timer.elapsed().count() << " msec; pts=" << pkt->pts << " dts=" << pkt->dts << " size=" << pkt->size;
                if ((ret = av_interleaved_write_frame(mctx.get_fctx(), pkt)) < 0)
                {
                    tlog::error() << "receive_packet_thread: Failed to write frame to muxing context: " << averror_explain(ret);
                }
                av_packet_free(&pkt);
                break;
            case AVERROR(EINVAL): // codec not opened, or it is a decoder other errors: legitimate encoding errors
            default:
                tlog::error() << "receive_packet_thread: Failed to receive packet: " << averror_explain(ret);
            case AVERROR_EOF: // the encoder has been fully flushed, and there will be no more output packets
                shutdown_requested = true;
                av_packet_free(&pkt);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    tlog::info() << "receive_packet_thread: Shutdown requested.";
    tlog::info() << "receive_packet_thread: Exiting thread.";
}

void encode_stats_thread(std::atomic<std::uint64_t> &frame_index, std::atomic<bool> &shutdown_requested)
{
    uint64_t previous_index = 0;
    while (!shutdown_requested)
    {
        uint64_t current_index = frame_index.load();
        tlog::info() << "encode_stats_thread: Current frame rate: " << current_index - previous_index << " fps.";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
