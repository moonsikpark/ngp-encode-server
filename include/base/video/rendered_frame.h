// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_RENDERED_FRAME_
#define NES_BASE_RENDERED_FRAME_

#include "base/video/type_managers.h"
#include "nes.pb.h"

// RenderedFrame stores all information related to an uncompressed frame. It is
// created with a raw RGB image stored in m_source_avframe. The RGB image buffer
// should be visible to other programs to make modifications such as overlaying
// texts. It converts the RGB image to YUV image using swscale and stores it in
// m_converted_avframe. After the image is ready, the program provides the
// converted image to the encoder.
class RenderedFrame {
 public:
  RenderedFrame(nesproto::RenderedFrame frame, AVPixelFormat pix_fmt,
                std::shared_ptr<types::AVCodecContextManager> ctxmgr);

  // Convert frame stored in m_source_avframe from RGB to YUV and store it in
  // m_converted_avframe.
  inline void convert_frame() {
    if (m_converted) {
      throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
    }
    types::SwsContextManager sws_context(m_source_avframe, m_converted_avframe);
    m_converted = true;
  }

  // Index of the frame.
  const inline uint64_t index() const { return this->m_frame_response.index(); }

  // Camera FOV and coordinate of the frame.
  const inline nesproto::Camera get_cam() const {
    return this->m_frame_response.camera();
  }

  // Raw RGB frame.
  inline types::FrameManager &source_frame() { return m_source_avframe; }

  // Converted YUV frame.
  inline types::FrameManager &converted_frame() { return m_converted_avframe; }

 private:
  nesproto::RenderedFrame m_frame_response;
  types::FrameManager m_source_avframe;
  types::FrameManager m_converted_avframe;
  AVPixelFormat m_pix_fmt;
  bool m_converted;
};

#endif  // NES_BASE_RENDERED_FRAME_
