// Copyright (c) 2022 Moonsik Park.
#ifndef NES_BASE_RENDERED_FRAME_
#define NES_BASE_RENDERED_FRAME_

#include "base/video/type_managers.h"
#include "nes.pb.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class RenderedFrame {
 public:
  RenderedFrame(nesproto::RenderedFrame frame, AVPixelFormat pix_fmt,
                std::shared_ptr<types::AVCodecContextManager> ctxmgr)
      : m_frame_response(frame),
        m_pix_fmt(pix_fmt),
        m_processed(false),
        m_avframe(
            types::AVFrameManager::FrameContext(ctxmgr->get_codec_info())) {}

  void convert_frame();

  const uint64_t index() const { return this->m_frame_response.index(); }

  uint8_t *buffer() { return (uint8_t *)this->m_frame_response.frame().data(); }

  const uint32_t width() const {
    return this->m_frame_response.camera().width();
  }

  const uint32_t height() const {
    return this->m_frame_response.camera().height();
  }

  const nesproto::Camera get_cam() const {
    return this->m_frame_response.camera();
  }

  uint8_t *processed_data() {
    if (!m_processed) {
      throw std::runtime_error{
          "Tried to access processed_data from not processed RenderedFrame."};
    }

    return m_avframe.data().data[0];
  }

  int *processed_linesize() {
    if (!m_processed) {
      throw std::runtime_error{
          "Tried to access processed_linesize from not "
          "processed RenderedFrame."};
    }
    return m_avframe.data().linesize;
  }

 private:
  nesproto::RenderedFrame m_frame_response;
  types::AVFrameManager m_avframe;
  AVPixelFormat m_pix_fmt;
  bool m_processed;
};

#endif  // NES_BASE_RENDERED_FRAME_
