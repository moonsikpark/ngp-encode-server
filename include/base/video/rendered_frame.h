// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_RENDERED_FRAME_
#define NES_BASE_RENDERED_FRAME_

#include "base/video/type_managers.h"
#include "nes.pb.h"

// RenderedFrame stores all information related to an uncompressed frame. It is
// created with a raw RGB image stored in m_source_avframe. The RGB image buffer
// should be visible to other programs to make modifications such as overlaying
// texts. It converts the RGB image to YUV image using swscale and stores it in
// m_converted_avframe_scene. After the image is ready, the program provides the
// converted image to the encoder.
class RenderedFrame {
 public:
  RenderedFrame(nesproto::RenderedFrame frame, AVPixelFormat pix_fmt_scene,
                AVPixelFormat pix_fmt_depth,
                std::shared_ptr<types::AVCodecContextManager> ctxmgr_scene,
                std::shared_ptr<types::AVCodecContextManager> ctxmgr_depth);

  // Convert frame stored in m_source_avframe from RGB to YUV and store it in
  // m_converted_avframe_scene.
  inline void convert_frame() {
    if (m_converted) {
      throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
    }
    types::SwsContextManager sws_context_scene(m_source_avframe_scene,
                                               m_converted_avframe_scene);
    types::SwsContextManager sws_context_depth(m_source_avframe_depth,
                                               m_converted_avframe_depth);
    m_converted = true;
  }

  // Index of the frame.
  const inline uint64_t index() const { return this->m_frame_response.index(); }

  const inline bool is_left() const { return this->m_frame_response.is_left(); }

  // Camera FOV and coordinate of the frame.
  const inline nesproto::Camera &get_cam() const {
    return this->m_frame_response.camera();
  }

  // Raw RGB frame.
  inline types::FrameManager &source_frame_scene() {
    return m_source_avframe_scene;
  }

  // Converted YUV frame.
  inline types::FrameManager &converted_frame_scene() {
    return m_converted_avframe_scene;
  }

  // Converted YUV depth frame.
  inline types::FrameManager &converted_frame_depth() {
    return m_converted_avframe_depth;
  }

 private:
  nesproto::RenderedFrame m_frame_response;
  types::FrameManager m_source_avframe_scene;
  types::FrameManager m_converted_avframe_scene;
  AVPixelFormat m_pix_fmt_scene;
  types::FrameManager m_source_avframe_depth;
  types::FrameManager m_converted_avframe_depth;
  AVPixelFormat m_pix_fmt_depth;
  bool m_converted;
};

#endif  // NES_BASE_RENDERED_FRAME_
