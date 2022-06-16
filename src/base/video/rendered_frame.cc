// Copyright (c) 2022 Moonsik Park.

#include "base/video/rendered_frame.h"

RenderedFrame::RenderedFrame(
    nesproto::RenderedFrame frame, AVPixelFormat pix_fmt_scene,
    AVPixelFormat pix_fmt_depth,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
    std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth)
    : m_frame_response(frame),
      m_pix_fmt_scene(pix_fmt_scene),
      m_pix_fmt_depth(pix_fmt_depth),
      m_converted(false),
      m_source_avframe_scene(
          types::FrameManager::FrameContext(m_frame_response.camera().width(),
                                            m_frame_response.camera().height(),
                                            pix_fmt_scene),
          (uint8_t *)m_frame_response.frame().data()),
      m_converted_avframe_scene(
          types::FrameManager::FrameContext(ctxmgr_scene->get_codec_info())),
      m_source_avframe_depth(
          types::FrameManager::FrameContext(m_frame_response.camera().width(),
                                            m_frame_response.camera().height(),
                                            pix_fmt_depth),
          (uint8_t *)m_frame_response.depth().data()),
      m_converted_avframe_depth(
          types::FrameManager::FrameContext(ctxmgr_depth->get_codec_info())) {}
