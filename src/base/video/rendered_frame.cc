// Copyright (c) 2022 Moonsik Park.

#include "base/video/rendered_frame.h"

RenderedFrame::RenderedFrame(
    nesproto::RenderedFrame frame, AVPixelFormat pix_fmt,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr)
    : m_frame_response(frame),
      m_pix_fmt(pix_fmt),
      m_converted(false),
      m_source_avframe(types::FrameManager::FrameContext(
                           m_frame_response.camera().width(),
                           m_frame_response.camera().height(), pix_fmt),
                       (uint8_t *)m_frame_response.frame().data()),
      m_converted_avframe(
          types::FrameManager::FrameContext(ctxmgr->get_codec_info())) {}
