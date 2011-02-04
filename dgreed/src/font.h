#ifndef FONT_H
#define FONT_H

#include "utils.h"
#include "system.h"

typedef uint FontHandle;

// Loads a new font file
FontHandle font_load(const char* filename); 
// Loads a new font file, allows to provide glyph scaling factor
FontHandle font_load_ex(const char* filename, float scale);
// Frees font which was loaded with font_load
void font_free(FontHandle font);
// Returns width of text in specified font
float font_width(FontHandle font, const char* string);
// Returns height of font characters in pixels
float font_height(FontHandle font);

// Draws string with specified font
void font_draw(FontHandle font, const char* string, uint layer,
	const Vector2* topleft, Color tint);

// Returns rectangle bounding drawn text.
RectF font_rect_ex(FontHandle font, const char* string,
	const Vector2* center, float scale);

// Draws centered, scaled, string with specified font.
void font_draw_ex(FontHandle font, const char* string, uint layer,
	const Vector2* center, float scale, Color tint);

#endif
