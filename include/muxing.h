/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   muxing.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _MUXING_H_
#define _MUXING_H_

#include <common.h>

extern "C"
{
#include <libavformat/avformat.h>
}

typedef struct
{
    AVFormatContext *oc;
    AVStream *st;
} MuxingContext;

MuxingContext *muxing_context_init(EncodeContext *ectx, std::string rtsp_mrl);
int muxing_context_free(MuxingContext *mctx);

#endif // _MUXING_H_
