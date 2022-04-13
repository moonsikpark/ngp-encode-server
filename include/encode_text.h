/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode_text.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _ENCODE_TEXT_H_
#define _ENCODE_TEXT_H_

#include <common.h>

extern "C"
{
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
}

typedef struct
{
    FT_Library library;
    FT_Face face;
} EncodeTextContext;

enum RenderPositionOption
{
    RenderPositionOption_LEFT_TOP,
    RenderPositionOption_LEFT_BOTTOM,
    RenderPositionOption_RIGHT_TOP,
    RenderPositionOption_RIGHT_BOTTOM,
    RenderPositionOption_CENTER
};

EncodeTextContext *encode_textctx_init(std::string font_location);
void render_string(EncodeTextContext *etctx, uint8_t *surface, int width, int height, RenderPositionOption opt, std::string content);
int encode_textctx_free(EncodeTextContext *etctx);

#endif // _ENCODE_TEXT_H_
