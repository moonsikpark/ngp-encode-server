// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_VIDEO_RENDER_TEXT_
#define NES_BASE_VIDEO_RENDER_TEXT_

#include <string>

#include "base/video/type_managers.h"

extern "C" {
#include "freetype2/ft2build.h"
#include FT_FREETYPE_H
}

class RenderTextContext {
 public:
  enum RenderPosition {
    RENDER_POSITION_LEFT_TOP,
    RENDER_POSITION_LEFT_BOTTOM,
    RENDER_POSITION_RIGHT_TOP,
    RENDER_POSITION_RIGHT_BOTTOM,
    RENDER_POSITION_CENTER
  };

  RenderTextContext(std::string font_location);

  void render_string_to_frame(types::FrameManager &frame,
                              RenderTextContext::RenderPosition opt,
                              std::string content);

  ~RenderTextContext();

 private:
  FT_Library _library;
  FT_Face _face;
  std::mutex m_mutex;
  std::condition_variable m_wait;
  using unique_lock = std::unique_lock<std::mutex>;
};

#endif  // NES_BASE_VIDEO_RENDER_TEXT_
