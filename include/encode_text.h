/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode_text.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _ENCODE_TEXT_H_
#define _ENCODE_TEXT_H_

#include <common.h>

extern "C" {
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
}

class EncodeTextContext {
 private:
  FT_Library _library;
  FT_Face _face;

 public:
  enum class RenderPositionOption {
    LEFT_TOP,
    LEFT_BOTTOM,
    RIGHT_TOP,
    RIGHT_BOTTOM,
    CENTER
  };
  EncodeTextContext(std::string font_location) {
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
  void render_string_to_frame(std::unique_ptr<RenderedFrame> &frame,
                              EncodeTextContext::RenderPositionOption opt,
                              std::string content) {
    // TODO: render to frame *after* it's done with libswscale.

    uint8_t *surface = frame->buffer();
    uint32_t width = frame->width();
    uint32_t height = frame->height();
    FT_Error ret;
    FT_GlyphSlot slot = _face->glyph;
    FT_UInt glyph_index;

    int pen_x, pen_y, n;
    int x_box = 300;
    int y_box = 100;
    int margin = 50;

    switch (opt) {
      case EncodeTextContext::RenderPositionOption::LEFT_TOP:
        pen_x = margin;
        pen_y = margin;
        break;
      case EncodeTextContext::RenderPositionOption::LEFT_BOTTOM:
        pen_x = margin;
        pen_y = height - y_box + margin;
        break;
      case EncodeTextContext::RenderPositionOption::RIGHT_TOP:
        pen_x = width - x_box + margin;
        pen_y = margin;
        break;
      case EncodeTextContext::RenderPositionOption::RIGHT_BOTTOM:
        pen_x = width - x_box + margin;
        pen_y = height - y_box + margin;
        break;
      case EncodeTextContext::RenderPositionOption::CENTER:
        pen_x = width / 2 - x_box;
        pen_y = height / 2 - y_box;
        break;
      default:
        pen_x = margin;
        pen_y = margin;
        break;
    }

    int orig_pen_x = pen_x, orig_pen_y = pen_y;

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

  ~EncodeTextContext() { FT_Done_FreeType(_library); }
};

#endif  // _ENCODE_TEXT_H_
