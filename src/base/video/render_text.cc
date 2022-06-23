// Copyright (c) 2022 Moonsik Park.

#include "base/video/render_text.h"

extern "C" {
#include "freetype2/ft2build.h"
#include FT_FREETYPE_H
}

RenderTextContext::RenderTextContext(std::string font_location) {
  FT_Error ret;
  if ((ret = FT_Init_FreeType(&_library)) < 0) {
    throw std::runtime_error{
        std::string("EncodeTextContext: Failed to init freetype: ") +
        std::string(FT_Error_String(ret))};
  }

  if ((ret = FT_New_Face(_library, font_location.c_str(), 0, &_face)) < 0) {
    throw std::runtime_error{
        std::string("EncodeTextContext: Failed to init font face: ") +
        std::string(FT_Error_String(ret))};
  }
  if ((ret = FT_Set_Char_Size(_face,   /* handle to face object */
                              0,       /* char_width in 1/64th of points  */
                              20 * 64, /* char_height in 1/64th of points */
                              0,       /* horizontal device resolution    */
                              0)       /* vertical device resolution      */
       ) < 0) {
    throw std::runtime_error{
        std::string("EncodeTextContext: Failed to set character size: ") +
        std::string(FT_Error_String(ret))};
  }
}

void RenderTextContext::render_string_to_frame(
    types::FrameManager &frame, RenderTextContext::RenderPosition opt,
    std::string content) {
  unique_lock lock(m_mutex);
  m_wait.wait(lock, [] { return true; });
  uint8_t *surface = frame.data().data[0];
  uint32_t width = frame.context().width;
  uint32_t height = frame.context().height;
  FT_Error ret;
  FT_GlyphSlot slot = _face->glyph;
  FT_UInt glyph_index;

  int pen_x, pen_y, n;
  int x_box = 300;
  int y_box = 100;
  int margin = 50;

  switch (opt) {
    case RenderPosition::RENDER_POSITION_LEFT_TOP:
      pen_x = margin;
      pen_y = margin;
      break;
    case RenderPosition::RENDER_POSITION_LEFT_BOTTOM:
      pen_x = margin;
      pen_y = height - y_box + margin;
      break;
    case RenderPosition::RENDER_POSITION_RIGHT_TOP:
      pen_x = width - x_box + margin;
      pen_y = margin;
      break;
    case RenderPosition::RENDER_POSITION_RIGHT_BOTTOM:
      pen_x = width - x_box + margin;
      pen_y = height - y_box + margin;
      break;
    case RenderPosition::RENDER_POSITION_CENTER:
      pen_x = width / 2 - x_box;
      pen_y = height / 2 - y_box;
      break;
    default:
      pen_x = margin;
      pen_y = margin;
      break;
  }

  int orig_pen_x = pen_x;

  for (auto &ch : content) {
    if (ch == '\n') {
      pen_x = orig_pen_x;
      pen_y = pen_y + 20;
      continue;
    }
    /* load glyph image into the slot (erase previous one) */
    ret = FT_Load_Char(_face, ch, FT_LOAD_RENDER);
    if (ret) {
      tlog::info() << "Error while rendering character=" << ch
                   << " error=" << ret;
    }

    FT_Int i, j, p, q;
    FT_Int x_max = pen_x + slot->bitmap_left + slot->bitmap.width;
    FT_Int y_max = pen_y - slot->bitmap_top + slot->bitmap.rows;
    for (j = pen_y - slot->bitmap_top, q = 0; j < y_max; j++, q++) {
      for (i = pen_x + slot->bitmap_left, p = 0; i < x_max; i++, p++) {
        if (i < 0 || j < 0 || i >= width || j >= height) continue;
        if (slot->bitmap.buffer[q * slot->bitmap.width + p]) {
          surface[(j * width + i) * 3] = 255;
          surface[(j * width + i) * 3 + 1] = 255;
          surface[(j * width + i) * 3 + 2] = 255;
        }
      }
    }

    /* increment pen position */
    pen_x += slot->advance.x >> 6;
  }
}

RenderTextContext::~RenderTextContext() { FT_Done_FreeType(_library); }
