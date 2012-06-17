/*
 * FreeTypeGX is a wrapper class for libFreeType which renders a compiled
 * FreeType parsable font into a GX texture for Wii homebrew development.
 * Copyright (C) 2008 Armin Tamzarian
 * Modified by Tantric, 2009
 *
 * This file is part of FreeTypeGX.
 *
 * FreeTypeGX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FreeTypeGX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FreeTypeGX.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xenos/xe.h>
#include <debug.h>
#include "FreeTypeGX.h"


extern "C" struct XenosDevice * g_pVideoDevice;

static FT_Library ftLibrary; /**< FreeType FT_Library instance. */
static FT_Face ftFace; /**< FreeType reusable FT_Face typographic object. */
static FT_GlyphSlot ftSlot; /**< FreeType reusable FT_GlyphSlot glyph container object. */

FreeTypeGX *fontSystem[MAX_FONT_SIZE + 1];

void InitFreeType(uint8_t* fontBuffer, FT_Long bufferSize) {
	FT_Init_FreeType(&ftLibrary);
	FT_New_Memory_Face(ftLibrary, (FT_Byte *) fontBuffer, bufferSize, 0, &ftFace);
	ftSlot = ftFace->glyph;

	for (int i = 0; i < 50; i++)
		fontSystem[i] = NULL;
}

void DeinitFreeType() {
	ClearFontData();
	FT_Done_FreeType(ftLibrary);
	ftLibrary = NULL;
}

void ChangeFontSize(FT_UInt pixelSize) {
	FT_Set_Pixel_Sizes(ftFace, 0, pixelSize);
}

void ClearFontData() {
	for (int i = 0; i < 50; i++) {
		if (fontSystem[i])
			delete fontSystem[i];
		fontSystem[i] = NULL;
	}
}

static wchar_t *UTF8_to_UNICODE(wchar_t *unicode, const char *utf8, int len) {
        int i, j;
        wchar_t ch;

        for (i = 0, j = 0; i < len; ++i, ++j) {
                ch = ((const unsigned char *) utf8)[i];
                if (ch >= 0xF0) {
                        ch = (wchar_t) (utf8[i]&0x07) << 18;
                        ch |= (wchar_t) (utf8[++i]&0x3F) << 12;
                        ch |= (wchar_t) (utf8[++i]&0x3F) << 6;
                        ch |= (wchar_t) (utf8[++i]&0x3F);
                } else
                        if (ch >= 0xE0) {
                        ch = (wchar_t) (utf8[i]&0x0F) << 12;
                        ch |= (wchar_t) (utf8[++i]&0x3F) << 6;
                        ch |= (wchar_t) (utf8[++i]&0x3F);
                } else
                        if (ch >= 0xC0) {
                        ch = (wchar_t) (utf8[i]&0x1F) << 6;
                        ch |= (wchar_t) (utf8[++i]&0x3F);
                }
                unicode[j] = ch;
        }
        unicode[j] = 0;

        return unicode;
}

wchar_t* charToWideChar(const char* strChar) {
        wchar_t *strWChar = new wchar_t[strlen(strChar) + 1];
        if (!strWChar)
                return NULL;
        
        UTF8_to_UNICODE(strWChar,strChar,strlen(strChar));

        return strWChar;
}

uint8_t * glyphData = NULL; //tmp buffer

/**
 * Default constructor for the FreeTypeGX class.
 *
 * @param vertexIndex	Optional vertex format index (GX_VTXFMT*) of the glyph textures as defined by the libogc gx.h header file. If not specified default value is GX_VTXFMT1.
 */
FreeTypeGX::FreeTypeGX(FT_UInt pixelSize, uint8_t vertexIndex) {
	this->ftPointSize = pixelSize;
	this->ftKerningEnabled = FT_HAS_KERNING(ftFace);

	if (glyphData == NULL)
		glyphData = (uint8_t *) malloc(256 * 256 * 4);

	this->vertexIndex = vertexIndex;
}

/**
 * Default destructor for the FreeTypeGX class.
 */
FreeTypeGX::~FreeTypeGX() {
	this->unloadFont();
	//    free(glyphData);
}

/**
 * Clears all loaded font glyph data.
 *
 * This routine clears all members of the font map structure and frees all allocated memory back to the system.
 */
void FreeTypeGX::unloadFont() {
	if (this->fontData.size() == 0)
		return;
	for (std::map<wchar_t, ftgxCharData>::iterator i = this->fontData.begin(), iEnd = this->fontData.end(); i != iEnd; ++i)
		//free(i->second.glyphDataTexture);
		if (i->second.glyphDataTexture) {
			Xe_DestroyTexture(g_pVideoDevice, i->second.glyphDataTexture);
		}
	this->fontData.clear();
}

/* Finds next power of two for n. If n itself
   is a power of two then returns n*/

unsigned int nextPowerOf2(unsigned int n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}

uint16_t FreeTypeGX::adjustTextureWidth(uint16_t textureWidth) {
	//    uint16_t alignment = 4;
	//    return textureWidth % alignment == 0 ? textureWidth : alignment + textureWidth - (textureWidth % alignment);
	//        return nextPowerOf2(textureWidth);
	return textureWidth;
	//    return (textureWidth + 31) &~31;
}

uint16_t FreeTypeGX::adjustTextureHeight(uint16_t textureHeight) {
	//    uint16_t alignment = 4;
	//    return textureHeight % alignment == 0 ? textureHeight : alignment + textureHeight - (textureHeight % alignment);
	//        return nextPowerOf2(textureHeight);
	return textureHeight;
	//    return (textureHeight + 31) &~31;
}

/**
 * Caches the given font glyph in the instance font texture buffer.
 *
 * This routine renders and stores the requested glyph's bitmap and relevant information into its own quickly addressible
 * structure within an instance-specific map.
 *
 * @param charCode	The requested glyph's character code.
 * @return A pointer to the allocated font structure.
 */
ftgxCharData *FreeTypeGX::cacheGlyphData(wchar_t charCode) {
	FT_UInt gIndex;
	uint16_t textureWidth = 0, textureHeight = 0;

	gIndex = FT_Get_Char_Index(ftFace, charCode);
	if (!FT_Load_Glyph(ftFace, gIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER)) {
		if (ftSlot->format == FT_GLYPH_FORMAT_BITMAP) {
			FT_Bitmap *glyphBitmap = &ftSlot->bitmap;

			textureWidth = adjustTextureWidth(glyphBitmap->width);
			textureHeight = adjustTextureHeight(glyphBitmap->rows);

			//            textureWidth = (glyphBitmap->width);
			//            textureHeight = (glyphBitmap->rows);

			this->fontData[charCode] = (ftgxCharData){
				ftSlot->bitmap_left,
				ftSlot->advance.x >> 6,
				gIndex,
				textureWidth,
				textureHeight,
				ftSlot->bitmap_top,
				ftSlot->bitmap_top,
				glyphBitmap->rows - ftSlot->bitmap_top,
				NULL,
				ftSlot->metrics,
				ftSlot->bitmap_top,
			};
			this->loadGlyphData(glyphBitmap, &this->fontData[charCode]);
			return &this->fontData[charCode];
		}
	}
	return NULL;
}

/**
 * Locates each character in this wrapper's configured font face and proccess them.
 *
 * This routine locates each character in the configured font face and renders the glyph's bitmap.
 * Each bitmap and relevant information is loaded into its own quickly addressible structure within an instance-specific map.
 */
uint16_t FreeTypeGX::cacheGlyphDataComplete() {
	uint32_t i = 0;
	FT_UInt gIndex;
	FT_ULong charCode = FT_Get_First_Char(ftFace, &gIndex);
	while (gIndex != 0) {
		if (this->cacheGlyphData(charCode) != NULL)
			++i;
		charCode = FT_Get_Next_Char(ftFace, charCode, &gIndex);
	}
	return (uint16_t) (i);
}

/**
 * Loads the rendered bitmap into the relevant structure's data buffer.
 *
 * This routine does a simple byte-wise copy of the glyph's rendered 8-bit grayscale bitmap into the structure's buffer.
 * Each byte is converted from the bitmap's intensity value into the a uint32_t RGBA value.
 *
 * @param bmp	A pointer to the most recently rendered glyph's bitmap.
 * @param charData	A pointer to an allocated ftgxCharData structure whose data represent that of the last rendered glyph.
 *
 * Optimized for RGBA8 use by Dimok.
 */
void FreeTypeGX::loadGlyphData(FT_Bitmap *bmp, ftgxCharData *charData) {
	int w, h, length;

	if ((charData->textureWidth == 0) || (charData->textureHeight == 0))
		return;

	if (!glyphData)
		return;

	//    w = (charData->textureWidth < 32) ? 32 : charData->textureWidth;
	//    h = (charData->textureHeight < 32) ? 32 : charData->textureHeight;

	w = charData->textureWidth;
	h = charData->textureHeight;

	length = w * h * 4;

	if (charData->glyphDataTexture) {
		Xe_DestroyTexture(g_pVideoDevice, charData->glyphDataTexture);
	}

	charData->glyphDataTexture = Xe_CreateTexture(g_pVideoDevice, (w + 31) &~31, (h + 31) &~31, 0, XE_FMT_8, 0);
	charData->glyphDataTexture->use_filtering = 0;
	charData->glyphDataTexture->u_addressing = XE_TEXADDR_CLAMP;
	charData->glyphDataTexture->v_addressing = XE_TEXADDR_CLAMP;

	memset(glyphData, 0x00, length);

	uint8_t *src = (uint8_t *) bmp->buffer;

	uint8_t * surfbuf = (uint8_t*) Xe_Surface_LockRect(g_pVideoDevice, charData->glyphDataTexture, 0, 0, 0, 0, XE_LOCK_WRITE);
	memset(surfbuf, 0, charData->glyphDataTexture->hpitch * charData->glyphDataTexture->wpitch);

	//    uint32_t * surfbuf = (uint32_t *)glyphData;
	//    uint32_t * dst = (uint32_t *) surfbuf;

	uint8_t * dst = (uint8_t *) surfbuf;

	uint8_t * dst_limit = (uint8_t *) surfbuf + ((charData->glyphDataTexture->hpitch) * (charData->glyphDataTexture->wpitch));
	int hpitch = 0;
	int wpitch = 0;
	//int y_offset = (h-(charData->textureHeight-charData->renderOffsetY));
	//    int y_offset = 0;


	//    for (hpitch = 0; hpitch < charData->glyphDataTexture->hpitch; hpitch += charData->glyphDataTexture->height) {
	//        //        for (int y = 0; y < bmp->rows; y++)
	//        int y = 0;
	//        int y_offset = 0;
	//        int dsty = 0;
	//        for (y = 0, dsty = y_offset; y < (bmp->rows); y++, dsty++) {
	//            for (wpitch = 0; wpitch < charData->glyphDataTexture->wpitch; wpitch += charData->glyphDataTexture->width) {
	//                src = (uint8_t *) bmp->buffer + ((y) * bmp->pitch);
	//                dst = (uint8_t *) surfbuf + ((dsty + hpitch) * (charData->glyphDataTexture->wpitch)) + wpitch;
	//                for (int x = 0; x < bmp->width; x++) {
	//                    if (dst < dst_limit)
	//                        *dst++ = *src++;
	//                }
	//            }
	//        }
	//    }

	for (hpitch = 0; hpitch < charData->glyphDataTexture->hpitch; hpitch += charData->glyphDataTexture->height) {
		//        for (int y = 0; y < bmp->rows; y++)
		int y, dsty = 0;
		//        int y_offset = charData->glyphDataTexture->height;
		int y_offset = 0;

		for (y = 0, dsty = y_offset; y < (bmp->rows); y++, dsty++) {
			//        for (y = 0, dsty = y_offset; y < (bmp->rows); y++, dsty--) {
			for (wpitch = 0; wpitch < charData->glyphDataTexture->wpitch; wpitch += charData->glyphDataTexture->width) {
				src = (uint8_t *) bmp->buffer + ((y) * bmp->pitch);
				dst = (uint8_t *) surfbuf + ((dsty + hpitch) * (charData->glyphDataTexture->wpitch)) + wpitch;
				for (int x = 0; x < bmp->width; x++) {
					if (dst < dst_limit)
						*dst++ = *src++;
				}
			}
		}
	}

	Xe_Surface_Unlock(g_pVideoDevice, charData->glyphDataTexture);
}

/**
 * Determines the x offset of the rendered string.
 *
 * This routine calculates the x offset of the rendered string based off of a supplied positional format parameter.
 *
 * @param width	Current pixel width of the string.
 * @param format	Positional format of the string.
 */
int16_t FreeTypeGX::getStyleOffsetWidth(uint16_t width, uint16_t format) {
	if (format & FTGX_JUSTIFY_LEFT)
		return 0;
	else if (format & FTGX_JUSTIFY_CENTER)
		return -(width >> 1);
	else if (format & FTGX_JUSTIFY_RIGHT)
		return -width;
	return 0;
}

/**
 * Determines the y offset of the rendered string.
 *
 * This routine calculates the y offset of the rendered string based off of a supplied positional format parameter.
 *
 * @param offset	Current pixel offset data of the string.
 * @param format	Positional format of the string.
 */
int16_t FreeTypeGX::getStyleOffsetHeight(ftgxDataOffset *offset, uint16_t format) {
	switch (format & FTGX_ALIGN_MASK) {
		case FTGX_ALIGN_TOP:
			return offset->ascender;

		default:
		case FTGX_ALIGN_MIDDLE:
			return (offset->ascender + offset->descender + 1) >> 1;

		case FTGX_ALIGN_BOTTOM:
			return offset->descender;

		case FTGX_ALIGN_BASELINE:
			return 0;

		case FTGX_ALIGN_GLYPH_TOP:
			return offset->max;

		case FTGX_ALIGN_GLYPH_MIDDLE:
			return (offset->max + offset->min + 1) >> 1;

		case FTGX_ALIGN_GLYPH_BOTTOM:
			return offset->min;
	}
	return 0;
}

/**
 * Processes the supplied text string and prints the results at the specified coordinates.
 *
 * This routine processes each character of the supplied text string, loads the relevant preprocessed bitmap buffer,
 * a texture from said buffer, and loads the resultant texture into the EFB.
 *
 * @param x	Screen X coordinate at which to output the text.
 * @param y Screen Y coordinate at which to output the text. Note that this value corresponds to the text string origin and not the top or bottom of the glyphs.
 * @param text	NULL terminated string to output.
 * @param color	Optional color to apply to the text characters. If not specified default value is ftgxWhite: (GXColor){0xff, 0xff, 0xff, 0xff}
 * @param textStyle	Flags which specify any styling which should be applied to the rendered string.
 * @return The number of characters printed.
 */
uint16_t FreeTypeGX::drawText(int16_t x, int16_t y, wchar_t *text, XeColor color, uint16_t textStyle) {
	//printf("Disabled !!\r\n");
	//return 0;
	uint16_t x_pos = x, printed = 0;
	uint16_t x_offset = 0, y_offset = 0;

	//GXTexObj glyphTexture;
	FT_Vector pairDelta;
	ftgxDataOffset offset;

	if (textStyle & FTGX_JUSTIFY_MASK) {
		x_offset = this->getStyleOffsetWidth(this->getWidth(text), textStyle);
	}
	if (textStyle & FTGX_ALIGN_MASK) {
		this->getOffset(text, &offset);
		y_offset = this->getStyleOffsetHeight(&offset, textStyle);
	}

	int i = 0;
	while (text[i]) {
		ftgxCharData* glyphData = NULL;
		if (this->fontData.find(text[i]) != this->fontData.end()) {
			glyphData = &this->fontData[text[i]];
		} else {
			glyphData = this->cacheGlyphData(text[i]);
		}

		if (glyphData != NULL) {
			if (this->ftKerningEnabled && i) {
				FT_Get_Kerning(ftFace, this->fontData[text[i - 1]].glyphIndex, glyphData->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
				x_pos += pairDelta.x >> 6;
			}
			if (glyphData->glyphDataTexture) {
				int RenderOffsetY = (glyphData->be.horiBearingY >> 6);
				int RenderOffsetX = glyphData->renderOffsetX; // - dx;

				Menu_T(glyphData->glyphDataTexture, glyphData->glyphDataTexture->width, glyphData->glyphDataTexture->height, x_pos + RenderOffsetX + x_offset, y - RenderOffsetY + y_offset, color);

			}
			x_pos += (glyphData->glyphAdvanceX);
			++printed;
		}
		++i;
	}

	if (textStyle & FTGX_STYLE_MASK) {
		this->getOffset(text, &offset);
	}

	return printed;
}

/**
 * \overload
 */
uint16_t FreeTypeGX::drawText(int16_t x, int16_t y, wchar_t const *text, XeColor color, uint16_t textStyle) {
	return this->drawText(x, y, (wchar_t *)text, color, textStyle);
}

/**
 * Processes the supplied string and return the width of the string in pixels.
 *
 * This routine processes each character of the supplied text string and calculates the width of the entire string.
 * Note that if precaching of the entire font set is not enabled any uncached glyph will be cached after the call to this function.
 *
 * @param text	NULL terminated string to calculate.
 * @return The width of the text string in pixels.
 */
uint16_t FreeTypeGX::getWidth(wchar_t *text) {
	uint16_t strWidth = 0;
	FT_Vector pairDelta;

	int i = 0;
	while (text[i]) {
		ftgxCharData* glyphData = NULL;
		if (this->fontData.find(text[i]) != this->fontData.end()) {
			glyphData = &this->fontData[text[i]];
		} else {
			glyphData = this->cacheGlyphData(text[i]);
		}

		if (glyphData != NULL) {
			if (this->ftKerningEnabled && (i > 0)) {
				FT_Get_Kerning(ftFace, this->fontData[text[i - 1]].glyphIndex, glyphData->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
				strWidth += pairDelta.x >> 6;
			}

			strWidth += glyphData->glyphAdvanceX;
		}
		++i;
	}
	return strWidth;
}

/**
 *
 * \overload
 */
uint16_t FreeTypeGX::getWidth(wchar_t const *text) {
	return this->getWidth((wchar_t *)text);
}

/**
 * Processes the supplied string and return the height of the string in pixels.
 *
 * This routine processes each character of the supplied text string and calculates the height of the entire string.
 * Note that if precaching of the entire font set is not enabled any uncached glyph will be cached after the call to this function.
 *
 * @param text	NULL terminated string to calculate.
 * @return The height of the text string in pixels.
 */
uint16_t FreeTypeGX::getHeight(wchar_t *text) {
	ftgxDataOffset offset;
	this->getOffset(text, &offset);
	return offset.max - offset.min;
}

/**
 *
 * \overload
 */
uint16_t FreeTypeGX::getHeight(wchar_t const *text) {
	return this->getHeight((wchar_t *)text);
}

/**
 * Get the maximum offset above and minimum offset below the font origin line.
 *
 * This function calculates the maximum pixel height above the font origin line and the minimum
 * pixel height below the font origin line and returns the values in an addressible structure.
 *
 * @param text	NULL terminated string to calculate.
 * @param offset returns the max and min values above and below the font origin line
 *
 */
void FreeTypeGX::getOffset(wchar_t *text, ftgxDataOffset* offset) {
	int16_t strMax = 0, strMin = 9999;

	int i = 0;
	while (text[i]) {
		ftgxCharData* glyphData = NULL;
		if (this->fontData.find(text[i]) != this->fontData.end()) {
			glyphData = &this->fontData[text[i]];
		} else {
			glyphData = this->cacheGlyphData(text[i]);
		}

		if (glyphData != NULL) {
			strMax = glyphData->renderOffsetMax > strMax ? glyphData->renderOffsetMax : strMax;
			strMin = glyphData->renderOffsetMin < strMin ? glyphData->renderOffsetMin : strMin;
		}
		++i;
	}
	offset->ascender = ftFace->size->metrics.ascender >> 6;
	offset->descender = ftFace->size->metrics.descender >> 6;
	offset->max = strMax;
	offset->min = strMin;
}

/**
 *
 * \overload
 */
void FreeTypeGX::getOffset(wchar_t const *text, ftgxDataOffset* offset) {
	this->getOffset(text, offset);
}

XenosSurface * FreeTypeGX::createText(wchar_t const *text, XeColor color, uint16_t textStyle) {
	uint16_t x_pos = 0, printed = 0;
	uint16_t x_offset = 0, y_offset = 0;

	uint16_t w = getWidth(text);
	uint16_t h = getHeight(text);

	//GXTexObj glyphTexture;
	FT_Vector pairDelta;
	ftgxDataOffset offset;

	XenosSurface * textTexture = NULL;

	textTexture = Xe_CreateTexture(g_pVideoDevice, (w + 31) &~31, (h + 31) &~31, 0, XE_FMT_8, 0);
	textTexture->use_filtering = 1;
	textTexture->u_addressing = XE_TEXADDR_CLAMP;
	textTexture->v_addressing = XE_TEXADDR_CLAMP;
	
	memset(textTexture->base, 0, textTexture->hpitch * textTexture->wpitch);

	if (textStyle & FTGX_JUSTIFY_MASK) {
		x_offset = this->getStyleOffsetWidth(this->getWidth(text), textStyle);
	}
	if (textStyle & FTGX_ALIGN_MASK) {
		this->getOffset(text, &offset);
		y_offset = this->getStyleOffsetHeight(&offset, textStyle);
	}
	
	int RenderOffsetY = 0;
	int RenderOffsetX = 0;

	int i = 0;
	while (text[i]) {

		FT_UInt gIndex = FT_Get_Char_Index(ftFace, text[i]);
		if (!FT_Load_Glyph(ftFace, gIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER)) {
			if (ftSlot->format == FT_GLYPH_FORMAT_BITMAP) {
				FT_Bitmap *bmp = &ftSlot->bitmap;
				
				RenderOffsetY += (ftSlot->metrics.horiBearingY >> 6);
				RenderOffsetX += ftSlot->bitmap_left; // - dx;
				
//				if (this->ftKerningEnabled && i) {
//					FT_Get_Kerning(ftFace, this->fontData[text[i - 1]].glyphIndex, glyphData->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
//					x_pos += pairDelta.x >> 6;
//				}

				uint8_t *src = (uint8_t *) bmp->buffer;

				uint8_t * surfbuf = (uint8_t*) Xe_Surface_LockRect(g_pVideoDevice, textTexture, RenderOffsetX, RenderOffsetY, bmp->rows,  bmp->width, XE_LOCK_WRITE);
				

				//    uint32_t * surfbuf = (uint32_t *)glyphData;
				//    uint32_t * dst = (uint32_t *) surfbuf;

				uint8_t * dst = (uint8_t *) surfbuf;

				uint8_t * dst_limit = (uint8_t *) surfbuf + ((textTexture->hpitch) * (textTexture->wpitch));
				int hpitch = 0;
				int wpitch = 0;

				for (hpitch = 0; hpitch < textTexture->hpitch; hpitch += textTexture->height) {
					//        for (int y = 0; y < bmp->rows; y++)
					int y, dsty = 0;
					//        int y_offset = charData->glyphDataTexture->height;
					int y_offset = 0;

					for (y = 0, dsty = y_offset; y < (bmp->rows); y++, dsty++) {
						//        for (y = 0, dsty = y_offset; y < (bmp->rows); y++, dsty--) {
						for (wpitch = 0; wpitch < textTexture->wpitch; wpitch += textTexture->width) {
							src = (uint8_t *) bmp->buffer + ((y) * bmp->pitch);
							dst = (uint8_t *) surfbuf + ((dsty + hpitch) * (textTexture->wpitch)) + wpitch;
							for (int x = 0; x < bmp->width; x++) {
								if (dst < dst_limit)
									*dst++ = *src++;
							}
						}
					}
				}

				Xe_Surface_Unlock(g_pVideoDevice, textTexture);

			}
		}
		++i;
	}

	return textTexture;
}