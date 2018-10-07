#include "Drawer2D.h"
#include "GraphicsAPI.h"
#include "Funcs.h"
#include "Platform.h"
#include "ExtMath.h"
#include "ErrorHandler.h"
#include "GraphicsCommon.h"
#include "Bitmap.h"
#include "Game.h"

void DrawTextArgs_Make(struct DrawTextArgs* args, STRING_REF const String* text, FontDesc* font, bool useShadow) {
	args->Text = *text;
	args->Font = *font;
	args->UseShadow = useShadow;
}

void DrawTextArgs_MakeEmpty(struct DrawTextArgs* args, FontDesc* font, bool useShadow) {
	args->Text = String_MakeNull();
	args->Font = *font;
	args->UseShadow = useShadow;
}

void Drawer2D_MakeFont(FontDesc* desc, int size, int style) {
	if (Drawer2D_BitmappedText) {
		desc->Handle = NULL;
		desc->Size   = size;
		desc->Style  = style;
	} else {
		Font_Make(desc, &Game_FontName, size, style);
	}
}

Bitmap Drawer2D_FontBitmap;
int Drawer2D_BoxSize = 8; /* avoid divide by 0 if default.png missing */
/* So really 16 characters per row */
#define DRAWER2D_LOG2_CHARS_PER_ROW 4
Int32 Drawer2D_Widths[256];

static void Drawer2D_CalculateTextWidths(void) {
	int width = Drawer2D_FontBitmap.Width, height = Drawer2D_FontBitmap.Height;
	int i;
	for (i = 0; i < Array_Elems(Drawer2D_Widths); i++) {
		Drawer2D_Widths[i] = 0;
	}

	int x, y, xx;
	for (y = 0; y < height; y++) {
		int charY = (y / Drawer2D_BoxSize);
		UInt32* row = Bitmap_GetRow(&Drawer2D_FontBitmap, y);

		for (x = 0; x < width; x += Drawer2D_BoxSize) {
			int charX = (x / Drawer2D_BoxSize);
			/* Iterate through each pixel of the given character, on the current scanline */
			for (xx = Drawer2D_BoxSize - 1; xx >= 0; xx--) {
				UInt32 pixel = row[x + xx];
				uint8_t a = PackedCol_ARGB_A(pixel);
				if (a < 127) continue;

				/* Check if this is the pixel furthest to the right, for the current character */
				int index = charX | (charY << DRAWER2D_LOG2_CHARS_PER_ROW);
				Drawer2D_Widths[index] = max(Drawer2D_Widths[index], xx + 1);
				break;
			}
		}
	}
	Drawer2D_Widths[' '] = Drawer2D_BoxSize / 4;
}

static void Drawer2D_FreeFontBitmap(void) {
	Mem_Free(Drawer2D_FontBitmap.Scan0);
	Drawer2D_FontBitmap.Scan0 = NULL;
}

void Drawer2D_SetFontBitmap(Bitmap* bmp) {
	Drawer2D_FreeFontBitmap();
	Drawer2D_FontBitmap = *bmp;
	Drawer2D_BoxSize = bmp->Width >> DRAWER2D_LOG2_CHARS_PER_ROW;
	Drawer2D_CalculateTextWidths();
}


void Drawer2D_HexEncodedCol(int i, int hex, uint8_t lo, uint8_t hi) {
	PackedCol* col = &Drawer2D_Cols[i];
	col->R = (uint8_t)(lo * ((hex >> 2) & 1) + hi * (hex >> 3));
	col->G = (uint8_t)(lo * ((hex >> 1) & 1) + hi * (hex >> 3));
	col->B = (uint8_t)(lo * ((hex >> 0) & 1) + hi * (hex >> 3));
	col->A = UInt8_MaxValue;
}

void Drawer2D_Init(void) {
	int i;
	PackedCol col = PACKEDCOL_CONST(0, 0, 0, 0);
	for (i = 0; i < DRAWER2D_MAX_COLS; i++) {
		Drawer2D_Cols[i] = col;
	}

	for (i = 0; i <= 9; i++) {
		Drawer2D_HexEncodedCol('0' + i, i, 191, 64);
	}
	for (i = 10; i <= 15; i++) {
		Drawer2D_HexEncodedCol('a' + (i - 10), i, 191, 64);
		Drawer2D_HexEncodedCol('a' + (i - 10), i, 191, 64);
	}
}

void Drawer2D_Free(void) { Drawer2D_FreeFontBitmap(); }

/* Draws a 2D flat rectangle. */
void Drawer2D_Rect(Bitmap* bmp, PackedCol col, int x, int y, int width, int height);

void Drawer2D_Clear(Bitmap* bmp, PackedCol col, int x, int y, int width, int height) {
	if (x < 0 || y < 0 || (x + width) > bmp->Width || (y + height) > bmp->Height) {
		ErrorHandler_Fail("Drawer2D_Clear - tried to clear at invalid coords");
	}

	int xx, yy;
	UInt32 argb = PackedCol_ToARGB(col);
	for (yy = 0; yy < height; yy++) {
		UInt32* row = Bitmap_GetRow(bmp, y + yy) + x;
		for (xx = 0; xx < width; xx++) { row[xx] = argb; }
	}
}

int Drawer2D_FontHeight(FontDesc* font, bool useShadow) {
	struct DrawTextArgs args;
	String text = String_FromConst("I");
	DrawTextArgs_Make(&args, &text, font, useShadow);
	return Drawer2D_MeasureText(&args).Height;
}

void Drawer2D_MakeTextTexture(struct Texture* tex, struct DrawTextArgs* args, int X, int Y) {
	Size2D size = Drawer2D_MeasureText(args);
	if (size.Width == 0 && size.Height == 0) {
		struct Texture empty = { NULL, TEX_RECT(X,Y, 0,0), TEX_UV(0,0, 1,1) };
		*tex = empty; return;
	}

	Bitmap bmp; Bitmap_AllocateClearedPow2(&bmp, size.Width, size.Height);
	{
		Drawer2D_DrawText(&bmp, args, 0, 0);
		Drawer2D_Make2DTexture(tex, &bmp, size, X, Y);
	}
	Mem_Free(bmp.Scan0);
}

void Drawer2D_Make2DTexture(struct Texture* tex, Bitmap* bmp, Size2D used, int X, int Y) {
	GfxResourceID texId = Gfx_CreateTexture(bmp, false, false);
	float u2 = (float)used.Width  / (float)bmp->Width;
	float v2 = (float)used.Height / (float)bmp->Height;

	struct Texture tmp = { texId, TEX_RECT(X,Y, used.Width,used.Height), TEX_UV(0,0, u2,v2) };
	*tex = tmp;
}

bool Drawer2D_ValidColCodeAt(const String* text, int i) {
	if (i >= text->length) return false;
	return Drawer2D_GetCol(text->buffer[i]).A > 0;
}
bool Drawer2D_ValidColCode(char c) { return Drawer2D_GetCol(c).A > 0; }

bool Drawer2D_IsEmptyText(const String* text) {
	if (!text->length) return true;

	int i;
	for (i = 0; i < text->length; i++) {
		if (text->buffer[i] != '&') return false;
		if (!Drawer2D_ValidColCodeAt(text, i + 1)) return false;
		i++; /* skip colour code */
	}
	return true;
}

char Drawer2D_LastCol(const String* text, int start) {
	if (start >= text->length) start = text->length - 1;
	int i;
	for (i = start; i >= 0; i--) {
		if (text->buffer[i] != '&') continue;
		if (Drawer2D_ValidColCodeAt(text, i + 1)) {
			return text->buffer[i + 1];
		}
	}
	return '\0';
}
bool Drawer2D_IsWhiteCol(char c) { return c == '\0' || c == 'f' || c == 'F'; }

#define Drawer2D_ShadowOffset(point) (point / 8)
#define Drawer2D_XPadding(point) (Math_CeilDiv(point, 8))
static int Drawer2D_Width(int point, char c) {
	return Math_CeilDiv(Drawer2D_Widths[(uint8_t)c] * point, Drawer2D_BoxSize);
}
static int Drawer2D_AdjHeight(int point) { return Math_CeilDiv(point * 3, 2); }

void Drawer2D_ReducePadding_Tex(struct Texture* tex, int point, int scale) {
	if (!Drawer2D_BitmappedText) return;

	int padding = (tex->Height - point) / scale;
	float vAdj = (float)padding / Math_NextPowOf2(tex->Height);
	tex->V1 += vAdj; tex->V2 -= vAdj;
	tex->Height -= (uint16_t)(padding * 2);
}

void Drawer2D_ReducePadding_Height(int* height, int point, int scale) {
	if (!Drawer2D_BitmappedText) return;

	int padding = (*height - point) / scale;
	*height -= padding * 2;
}

static int Drawer2D_NextPart(int i, STRING_REF String* value, String* part, char* nextCol) {
	int length = 0, start = i;
	for (; i < value->length; i++) {
		if (value->buffer[i] == '&' && Drawer2D_ValidColCodeAt(value, i + 1)) break;
		length++;
	}

	*part = String_UNSAFE_Substring(value, start, length);
	i += 2; /* skip over colour code */

	if (i <= value->length) *nextCol = value->buffer[i - 1];
	return i;
}

void Drawer2D_Underline(Bitmap* bmp, int x, int y, int width, int height, PackedCol col) {
	int xx, yy;
	UInt32 argb = PackedCol_ToARGB(col);

	for (yy = y; yy < y + height; yy++) {
		if (yy >= bmp->Height) return;
		UInt32* row = Bitmap_GetRow(bmp, yy);

		for (xx = x; xx < x + width; xx++) {
			if (xx >= bmp->Width) break;
			row[xx] = argb;
		}
	}
}

static void Drawer2D_DrawCore(Bitmap* bmp, struct DrawTextArgs* args, int x, int y, bool shadow) {
	PackedCol black = PACKEDCOL_BLACK;
	PackedCol col = Drawer2D_Cols['f'];
	if (shadow) {
		col = Drawer2D_BlackTextShadows ? black : PackedCol_Scale(col, 0.25f);
	}

	String text = args->Text;
	int point   = args->Font.Size, count = 0, i;

	uint8_t coords[256];
	PackedCol cols[256];
	uint16_t dstWidths[256];

	for (i = 0; i < text.length; i++) {
		char c = text.buffer[i];
		if (c == '&' && Drawer2D_ValidColCodeAt(&text, i + 1)) {
			col = Drawer2D_GetCol(text.buffer[i + 1]);
			if (shadow) {
				col = Drawer2D_BlackTextShadows ? black : PackedCol_Scale(col, 0.25f);
			}
			i++; continue; /* skip over the colour code */
		}

		coords[count] = c;
		cols[count] = col;
		dstWidths[count] = Drawer2D_Width(point, c);
		count++;
	}

	int dstHeight = point, startX = x, xx, yy;
	/* adjust coords to make drawn text match GDI fonts */
	int xPadding = Drawer2D_XPadding(point);
	int yPadding = (Drawer2D_AdjHeight(dstHeight) - dstHeight) / 2;

	for (yy = 0; yy < dstHeight; yy++) {
		int dstY = y + (yy + yPadding);
		if (dstY >= bmp->Height) break;

		int fontY = 0 + yy * Drawer2D_BoxSize / dstHeight;
		UInt32* dstRow = Bitmap_GetRow(bmp, dstY);

		for (i = 0; i < count; i++) {
			int srcX = (coords[i] & 0x0F) * Drawer2D_BoxSize;
			int srcY = (coords[i] >> 4)   * Drawer2D_BoxSize;
			UInt32* fontRow = Bitmap_GetRow(&Drawer2D_FontBitmap, fontY + srcY);

			int srcWidth = Drawer2D_Widths[coords[i]], dstWidth = dstWidths[i];
			col = cols[i];

			for (xx = 0; xx < dstWidth; xx++) {
				int fontX = srcX + xx * srcWidth / dstWidth;
				UInt32 src = fontRow[fontX];
				if (PackedCol_ARGB_A(src) == 0) continue;

				int dstX = x + xx;
				if (dstX >= bmp->Width) break;

				UInt32 pixel = src & ~0xFFFFFF;
				pixel |= ((src & 0xFF)         * col.B / 255);
				pixel |= (((src >> 8) & 0xFF)  * col.G / 255) << 8;
				pixel |= (((src >> 16) & 0xFF) * col.R / 255) << 16;
				dstRow[dstX] = pixel;
			}
			x += dstWidth + xPadding;
		}
		x = startX;
	}

	if (args->Font.Style != FONT_STYLE_UNDERLINE) return;
	/* scale up bottom row of a cell to drawn text font */
	int cellY      = (8 - 1) * dstHeight / 8;
	int underlineY = y + cellY + yPadding, underlineHeight = dstHeight - cellY;

	for (i = 0; i < count; ) {
		int width     = 0;
		PackedCol col = cols[i];

		for (; i < count && PackedCol_Equals(col, cols[i]); i++) {
			width += dstWidths[i] + xPadding;
		}
		Drawer2D_Underline(bmp, x, underlineY, width, underlineHeight, col);
		x += width;
	}
}

static void Drawer2D_DrawBitmapText(Bitmap* bmp, struct DrawTextArgs* args, int x, int y) {
	int offset = Drawer2D_ShadowOffset(args->Font.Size);

	if (args->UseShadow) {
		Drawer2D_DrawCore(bmp, args, x + offset, y + offset, true);
	}
	Drawer2D_DrawCore(bmp, args, x, y, false);
}

static Size2D Drawer2D_MeasureBitmapText(struct DrawTextArgs* args) {
	int point = args->Font.Size;
	/* adjust coords to make drawn text match GDI fonts */
	int xPadding = Drawer2D_XPadding(point), i;
	Size2D total = { 0, Drawer2D_AdjHeight(point) };

	String text = args->Text;
	for (i = 0; i < text.length; i++) {
		char c = text.buffer[i];
		if (c == '&' && Drawer2D_ValidColCodeAt(&text, i + 1)) {
			i++; continue; /* skip over the colour code */
		}
		total.Width += Drawer2D_Width(point, c) + xPadding;
	}

	/* TODO: this should be uncommented */
	/* Remove padding at end */
	/*if (total.Width > 0) total.Width -= xPadding;*/

	if (args->UseShadow) {
		int offset = Drawer2D_ShadowOffset(point);
		total.Width += offset; total.Height += offset;
	}
	return total;
}

void Drawer2D_DrawText(Bitmap* bmp, struct DrawTextArgs* args, int x, int y) {
	if (Drawer2D_IsEmptyText(&args->Text)) return;
	if (Drawer2D_BitmappedText) { Drawer2D_DrawBitmapText(bmp, args, x, y); return; }
	
	String value = args->Text;
	char nextCol = 'f';
	int i = 0;

	while (i < value.length) {
		char colCode = nextCol;
		i = Drawer2D_NextPart(i, &value, &args->Text, &nextCol);
		PackedCol col = Drawer2D_GetCol(colCode);
		if (!args->Text.length) continue;

		if (args->UseShadow) {
			PackedCol black = PACKEDCOL_BLACK;
			PackedCol backCol = Drawer2D_BlackTextShadows ? black : PackedCol_Scale(col, 0.25f);
			Platform_TextDraw(args, bmp, x + DRAWER2D_OFFSET, y + DRAWER2D_OFFSET, backCol);
		}

		Size2D partSize = Platform_TextDraw(args, bmp, x, y, col);
		x += partSize.Width;
	}
	args->Text = value;
}

Size2D Drawer2D_MeasureText(struct DrawTextArgs* args) {
	Size2D size = { 0, 0 };
	if (Drawer2D_IsEmptyText(&args->Text)) return size;
	if (Drawer2D_BitmappedText) return Drawer2D_MeasureBitmapText(args);

	String value = args->Text;
	char nextCol = 'f';
	int i = 0;

	while (i < value.length) {
		i = Drawer2D_NextPart(i, &value, &args->Text, &nextCol);
		if (!args->Text.length) continue;

		Size2D partSize = Platform_TextMeasure(args);
		size.Width += partSize.Width;
		size.Height = max(size.Height, partSize.Height);
	}

	/* TODO: Is this font shadow offset right? */
	if (args->UseShadow) { size.Width += DRAWER2D_OFFSET; size.Height += DRAWER2D_OFFSET; }
	args->Text = value;
	return size;
}
