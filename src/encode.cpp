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

EncodeContext *encode_context_init(uint32_t width, uint32_t height, AVCodecID codec_id, std::string encode_preset, std::string encode_tune, int bit_rate, int fps)
{
    EncodeContext *ectx = (EncodeContext *)malloc(sizeof(EncodeContext));

    // find encoder
    ectx->codec = avcodec_find_encoder(codec_id);
    if (!ectx->codec)
    {
        tlog::error("Failed to find encoder.");
        goto fail;
    }

    // allocate codec context
    ectx->ctx = avcodec_alloc_context3(ectx->codec);
    if (!ectx->ctx)
    {
        tlog::error("Failed to allocate context.");
        goto fail;
    }

    // setup codec context
    ectx->ctx->bit_rate = bit_rate;
    ectx->ctx->width = width;
    ectx->ctx->height = height;
    // h264's clock rate is 90kHz.
    ectx->ctx->time_base = (AVRational){1, fps};
    ectx->ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    av_dict_set(&(ectx->options), "preset", encode_preset.c_str(), 0);
    av_dict_set(&(ectx->options), "tune", encode_tune.c_str(), 0);

    // open codec
    if (avcodec_open2(ectx->ctx, ectx->codec, &(ectx->options)) < 0)
    {
        tlog::error("Failed to open codec.");
        goto fail;
    }

    // allocate AVFrame
    // AVFrame holds the raw image data
    ectx->frame = av_frame_alloc();
    if (!ectx->frame)
    {
        tlog::error("Failed to allocate frame.");
        goto fail;
    }

    // Set context of AVFrame
    ectx->frame->format = ectx->ctx->pix_fmt;
    ectx->frame->width = ectx->ctx->width;
    ectx->frame->height = ectx->ctx->height;

    if (av_image_alloc(ectx->frame->data, ectx->frame->linesize, ectx->ctx->width, ectx->ctx->height, ectx->ctx->pix_fmt, 32) < 0)
    {
        tlog::error("Failed to allocate frame data.");
        goto fail;
    }
    // prepare convert context
    ectx->sws_ctx = sws_getContext(ectx->ctx->width, ectx->ctx->height, AV_PIX_FMT_BGR32,
                                   ectx->ctx->width, ectx->ctx->height, ectx->ctx->pix_fmt,
                                   0, 0, 0, 0);

    ectx->pkt = av_packet_alloc();
    if (!ectx->pkt)
    {
        tlog::error("Failed to allocate packet");
    }

    return ectx;

// todo: what to do when failed?
fail:;
}

void encode_raw_image_to_frame(EncodeContext *ectx, int width, int height, uint8_t *image_buffer)
{
    uint8_t *in_data[1] = {(uint8_t *)image_buffer};
    int in_linesize[1] = {4 * (int)width};

    sws_scale(ectx->sws_ctx,
              in_data,
              in_linesize,
              0,
              height,
              ectx->frame->data,
              ectx->frame->linesize);
}

void receive_packet_thread(EncodeContext ectx, MuxingContext mctx, bool threads_stop_running)
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

        ret = avcodec_receive_packet(ectx.ctx, pkt);

        switch (ret)
        {
        case AVERROR(EAGAIN): // output is not available in the current state - user must try to send input
            continue;
        case 0:
            pkt->pts = pkt->dts = av_rescale_q(frame_count, ectx.ctx->time_base, h264_timebase);
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

int encode_context_free(EncodeContext *ectx)
{
    av_packet_free(&(ectx->pkt));
    sws_freeContext(ectx->sws_ctx);
    av_free(ectx->frame);
    av_dict_free(&(ectx->options));
    avcodec_close(ectx->ctx);
    av_free(ectx->ctx);
    free(ectx);

    return 0;
}
