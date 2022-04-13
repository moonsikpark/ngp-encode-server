/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode_text.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <common.h>
#include <encode_text.h>

EncodeTextContext *encode_textctx_init(std::string font_location)
{
    EncodeTextContext *etctx = (EncodeTextContext *)malloc(sizeof(EncodeTextContext));

    int ret;
    if ((ret = FT_Init_FreeType(&(etctx->library))) < 0)
    {
        tlog::error() << "Cannot init freetype library.";
        return NULL;
    }

    if ((ret = FT_New_Face(etctx->library, font_location.c_str(), 0, &(etctx->face))) < 0)
    {
        if (ret == FT_Err_Unknown_File_Format)
        {
            tlog::error() << "Unknown font format";
        }
        else
        {
            tlog::error() << "Can't open font file at: " << font_location;
        }
        return NULL;
    }

    if ((ret = FT_Set_Char_Size(
             etctx->face, /* handle to face object           */
             0,           /* char_width in 1/64th of points  */
             20 * 64,     /* char_height in 1/64th of points */
             0,           /* horizontal device resolution    */
             0)           /* vertical device resolution      */
         ) < 0)
    {
        tlog::error() << "Failed to set character size";
    }
    /*
    if ((ret = FT_Set_Pixel_Sizes(etctx->face, 0, 16)) < 0)
    {
        tlog::error() << "Failed to set pixel size";
    }
    */
    return etctx;
}

void render_string(EncodeTextContext *etctx, uint8_t *surface, int width, int height, RenderPositionOption opt, std::string content)
{
    int ret;
    FT_GlyphSlot slot = etctx->face->glyph; /* a small shortcut */
    FT_UInt glyph_index;
    int pen_x, pen_y, n;
    int x_box = 300;
    int y_box = 100;
    int margin = 50;

    switch (opt)
    {
    case RenderPositionOption_LEFT_TOP:
        pen_x = margin;
        pen_y = margin;
        break;
    case RenderPositionOption_LEFT_BOTTOM:
        pen_x = margin;
        pen_y = height - y_box + margin;
        break;
    case RenderPositionOption_RIGHT_TOP:
        pen_x = width - x_box + margin;
        pen_y = margin;
        break;
    case RenderPositionOption_RIGHT_BOTTOM:
        pen_x = width - x_box + margin;
        pen_y = height - y_box + margin;
        break;
    case RenderPositionOption_CENTER:
        pen_x = width / 2;
        pen_y = height / 2;
        break;
    default:
        pen_x = margin;
        pen_y = margin;
        break;
    }

    for (auto &ch : content)
    {
        /* load glyph image into the slot (erase previous one) */
        ret = FT_Load_Char(etctx->face, ch, FT_LOAD_RENDER);
        if (ret)
        {
            tlog::info() << "Error while rendering character=" << ch << " error=" << ret;
        }

        FT_Int i, j, p, q;
        FT_Int x_max = pen_x + slot->bitmap_left + slot->bitmap.width;
        FT_Int y_max = pen_y - slot->bitmap_top + slot->bitmap.rows;
        for (j = pen_y - slot->bitmap_top, q = 0; j < y_max; j++, q++)
        {
            for (i = pen_x + slot->bitmap_left, p = 0; i < x_max; i++, p++)
            {
                if (i < 0 || j < 0 || i >= width || j >= height)
                    continue;
                if (slot->bitmap.buffer[q * slot->bitmap.width + p])
                {
                    surface[(j * width + i) * 4] = 255;
                    surface[(j * width + i) * 4 + 1] = 255;
                    surface[(j * width + i) * 4 + 2] = 255;
                }
            }
        }

        /* increment pen position */
        pen_x += slot->advance.x >> 6;
    }
}

int encode_textctx_free(EncodeTextContext *etctx)
{
    FT_Done_FreeType(etctx->library);
    free(etctx);

    return 0;
}
