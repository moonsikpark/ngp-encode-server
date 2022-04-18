/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   muxing.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <common.h>
#include <muxing.h>

MuxingContext *muxing_context_init(const AVCodecContext *ctx, std::string rtsp_mrl)
{
    MuxingContext *mctx = (MuxingContext *)malloc(sizeof(MuxingContext));

    avformat_alloc_output_context2(&(mctx->oc), NULL, "rtsp", rtsp_mrl.c_str());
    mctx->st = avformat_new_stream(mctx->oc, NULL);

    mctx->st->codecpar->codec_id = ctx->codec_id;
    mctx->st->codecpar->codec_type = ctx->codec_type;
    mctx->st->codecpar->bit_rate = ctx->bit_rate;
    mctx->st->codecpar->width = ctx->width;
    mctx->st->codecpar->height = ctx->height;
    mctx->st->time_base = ctx->time_base;
    mctx->st->codecpar->format = ctx->pix_fmt;

    if (avformat_write_header(mctx->oc, NULL) < 0)
    {
        tlog::error() << "Failed to write header.";
    }

    return mctx;
}

int muxing_context_free(MuxingContext *mctx)
{

    if (av_write_trailer(mctx->oc) < 0)
    {
        tlog::error() << "Failed to write trailer.";
    }
    avformat_free_context(mctx->oc);
    free(mctx);
    return 0;
}
