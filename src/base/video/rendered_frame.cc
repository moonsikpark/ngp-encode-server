// Copyright (c) 2022 Moonsik Park.

#include "base/video/rendered_frame.h"

#include "base/video/type_managers.h"

void RenderedFrame::convert_frame() {
  if (m_processed) {
    throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
  }

  types::AVFrameManager::FrameContext source_frame_context(
      m_frame_response.camera().width(), m_frame_response.camera().height(),
      m_pix_fmt);

  types::AVFrameManager source(source_frame_context,
                               (uint8_t *)m_frame_response.frame().data());

  types::SwsContextManager sws_context(source, m_avframe);

  m_processed = true;
}
