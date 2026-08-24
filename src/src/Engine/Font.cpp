/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Font.h"
#include "DosFont.h"
#include "Surface.h"
#include "FileMap.h"
#include "Unicode.h"
#include "Logger.h"
#ifdef __HDFONTS__
#include <SDL_ttf.h>
#include <algorithm>
#endif

namespace OpenXcom
{

const SDL_Color Font::TerminalColors[2] = {{0, 0, 0, 0}, {185, 185, 185, 255}};

/**
 * Initializes the font with a blank surface.
 */
namespace
{
	/// Characters that are full-width CJK ideographs / fullwidth forms /
	/// CJK punctuation. Bitmap font sheets only carry a subset of these, so
	/// missing ones must still advance at the uniform CJK width instead of
	/// falling back to the narrow '?' cell (which caused uneven letter
	/// spacing when HD glyphs rendered the real characters).
	bool isCjkIdeograph(UCode c)
	{
		return (c >= 0x2E80 && c <= 0x9FFF)   // radicals, kana, CJK unified
			|| (c >= 0x3400 && c <= 0x4DBF)   // ext A
			|| (c >= 0xF900 && c <= 0xFAFF)   // compat ideographs
			|| (c >= 0xFF01 && c <= 0xFF60)   // fullwidth forms
			|| (c >= 0x3000 && c <= 0x303F)   // CJK punctuation
			|| (c >= 0x201C && c <= 0x201D);  // CJK curly quotes
	}
}

Font::Font() : _monospace(false), _cjkAdvance(-1)
{
#ifdef __HDFONTS__
	_hdFont = 0;
	_hdFontCjk = 0;
	_hdScale = 0.0;
#endif
}

/**
 * Deletes the font's surface.
 */
Font::~Font()
{
	for (auto& fontImage : _images)
	{
		delete fontImage.surface;
	}
#ifdef __HDFONTS__
	invalidateHdCache();
	if (_hdFont)
	{
		TTF_CloseFont(_hdFont);
		_hdFont = 0;
	}
	if (_hdFontCjk)
	{
		TTF_CloseFont(_hdFontCjk);
		_hdFontCjk = 0;
	}
#endif
}

/**
 * Loads the font from a YAML file.
 * @param node YAML node.
 */
void Font::load(const YAML::YamlNodeReader& reader)
{
	int width = reader["width"].readVal(0);
	int height = reader["height"].readVal(0);
	int spacing = reader["spacing"].readVal(0);
	_monospace = reader["monospace"].readVal(_monospace);
	for (const auto& imageReader : reader["images"].children())
	{
		FontImage image;
		image.width = imageReader["width"].readVal(width);
		image.height = imageReader["height"].readVal(height);
		image.spacing = imageReader["spacing"].readVal(spacing);
		std::string file = "Language/" + imageReader["file"].readVal<std::string>();
		UString chars = Unicode::convUtf8ToUtf32(imageReader["chars"].readVal<std::string>());
		image.surface = new Surface(image.width, image.height);
		image.surface->loadImage(file);
		_images.push_back(image);
		init(_images.size() - 1, chars);
	}
}

/**
 * Generates a pre-defined Codepage 437 (MS-DOS terminal) font.
 */
void Font::loadTerminal()
{
	FontImage image;
	image.width = 9;
	image.height = 16;
	image.spacing = 0;
	_monospace = true;

	SDL_RWops *rw = SDL_RWFromConstMem(dosFont, DOSFONT_SIZE);
	SDL_Surface *s = SDL_LoadBMP_RW(rw, SDL_TRUE);
	image.surface = new Surface(s->w, s->h);
	image.surface->setPalette(TerminalColors, 0, std::size(TerminalColors));
	SDL_BlitSurface(s, 0, image.surface->getSurface(), 0);
	SDL_FreeSurface(s);
	_images.push_back(image);

	UString chars = Unicode::convUtf8ToUtf32(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
	init(_images.size() - 1, chars);
}

/**
 * Loads an optional HD (TTF) backend for this font. The TTF is opened
 * lazily (and sized) when the first glyph is requested at a known
 * display scale, so this only records the path and resets the cache.
 * @param ttfPath Primary TTF path (used for most glyphs).
 * @param ttfCjkPath Optional CJK fallback TTF path (used when the primary
 *        lacks a given glyph). A trailing "#<index>" selects a face inside
 *        a TTC collection.
 */
void Font::loadHdFont(const std::string &ttfPath, const std::string &ttfCjkPath)
{
#ifdef __HDFONTS__
	static bool ttfReady = false;
	if (!ttfReady)
	{
		if (TTF_Init() != 0)
		{
			Log(LOG_WARNING) << "HD fonts: TTF_Init failed: " << TTF_GetError();
			return;
		}
		ttfReady = true;
	}
	if (_hdFont)
	{
		TTF_CloseFont(_hdFont);
		_hdFont = 0;
	}
	if (_hdFontCjk)
	{
		TTF_CloseFont(_hdFontCjk);
		_hdFontCjk = 0;
	}
	invalidateHdCache();
	_hdTtfPath = ttfPath;
	_hdTtfCjkPath = ttfCjkPath;
	Log(LOG_INFO) << "HD fonts: TTF backend assigned: primary=\"" << ttfPath
		<< "\" cjk=\"" << (ttfCjkPath.empty() ? "(none)" : ttfCjkPath) << "\"";
#else
	(void)ttfPath;
	(void)ttfCjkPath;
#endif
}

/**
 * Returns whether the HD (TTF) backend is available for this font.
 * @return True if a TTF path was assigned and SDL_ttf support is compiled in.
 */
bool Font::isHdFont() const
{
#ifdef __HDFONTS__
	return !_hdTtfPath.empty();
#else
	return false;
#endif
}

/**
 * Drops all cached HD glyphs and closes the scaled TTF handle.
 * Called when the display scale changes (e.g. resolution change),
 * so glyphs get re-rasterized at the new scale.
 */
void Font::invalidateHdCache() const
{
#ifdef __HDFONTS__
	for (auto &kv : _hdGlyphs)
	{
		delete kv.second;
	}
	_hdGlyphs.clear();
	_hdScale = 0.0;
	// Drop both TTF handles so they get reopened (and re-sized) at the
	// new scale on next use. Sharing _hdScale between the primary and the
	// CJK fallback means we must reset both here, otherwise the fallback
	// handle could be left at the previous scale after a resolution change.
	if (_hdFont)
	{
		TTF_CloseFont(_hdFont);
		_hdFont = 0;
	}
	if (_hdFontCjk)
	{
		TTF_CloseFont(_hdFontCjk);
		_hdFontCjk = 0;
	}
#endif
}

namespace
{
/**
 * Splits a "path[#index]" spec into a file path and a TTC face index.
 * @param[in] spec "file.ttf" or "file.ttc#2".
 * @param[out] file Stripped file path.
 * @param[out] index Face index (0 unless "#n" is present).
 */
void splitTtcSpec(const std::string &spec, std::string &file, int &index)
{
	file = spec;
	index = 0;
	std::string::size_type hash = spec.find('#');
	if (hash != std::string::npos)
	{
		file = spec.substr(0, hash);
		try { index = std::stoi(spec.substr(hash + 1)); }
		catch (...) { index = 0; }
	}
}
}

#ifdef __HDFONTS__

/**
 * Opens (or reopens) the TTF handle at the pixel size that makes the
 * font's line height match the original bitmap font cell scaled up to
 * the given display scale. FreeType metrics scale linearly with the
 * requested point size, so one correction pass is enough.
 * @param scale Display scale factor (display px per base px; may be fractional).
 * @return True if the TTF handle is ready for use.
 */
bool Font::ensureHdFont(double scale) const
{
	if (_hdFont != 0 && _hdScale == scale)
		return true;
	if (_hdTtfPath.empty())
		return false;
	if (_hdFont)
	{
		TTF_CloseFont(_hdFont);
		_hdFont = 0;
	}
	// Cached glyph cells are sized for the previous scale; drop them.
	invalidateHdCache();
	int target = (int)lround(getHeight() * scale); // desired line height in display pixels
	if (target < 4)
		target = 4;
	std::string file; int index;
	splitTtcSpec(_hdTtfPath, file, index);
	_hdFont = TTF_OpenFontIndex(file.c_str(), target, index);
	if (!_hdFont)
	{
		Log(LOG_WARNING) << "HD fonts: can't open \"" << _hdTtfPath << "\": " << TTF_GetError();
		return false;
	}
	// Correct the point size so that the real font height (ascent+descent)
	// matches the target line height: height scales linearly with ptsize.
	int height = TTF_FontHeight(_hdFont);
	if (height > 0 && height != target)
	{
		int corrected = std::max(4, target * target / height);
		TTF_CloseFont(_hdFont);
		_hdFont = TTF_OpenFontIndex(file.c_str(), corrected, index);
		if (!_hdFont)
		{
			Log(LOG_WARNING) << "HD fonts: can't reopen \"" << _hdTtfPath << "\": " << TTF_GetError();
			return false;
		}
	}
	TTF_SetFontHinting(_hdFont, TTF_HINTING_LIGHT);
	_hdScale = scale;
	return true;
}

/**
 * Opens (or reopens) the optional CJK fallback TTF at the pixel size that
 * matches the original bitmap cell scaled to the given display scale.
 * @param scale Display scale factor (may be fractional).
 * @return True if the CJK handle is ready (or no CJK path is configured).
 */
bool Font::ensureHdCjkFont(double scale) const
{
	if (_hdTtfCjkPath.empty())
		return false;
	if (_hdFontCjk != 0 && _hdScale == scale)
		return true;
	if (_hdFontCjk)
	{
		TTF_CloseFont(_hdFontCjk);
		_hdFontCjk = 0;
	}
	int target = (int)lround(getHeight() * scale);
	if (target < 4)
		target = 4;
	std::string file; int index;
	splitTtcSpec(_hdTtfCjkPath, file, index);
	_hdFontCjk = TTF_OpenFontIndex(file.c_str(), target, index);
	if (!_hdFontCjk)
	{
		Log(LOG_WARNING) << "HD fonts: can't open CJK \"" << _hdTtfCjkPath << "\": " << TTF_GetError();
		return false;
	}
	int height = TTF_FontHeight(_hdFontCjk);
	if (height > 0 && height != target)
	{
		int corrected = std::max(4, target * target / height);
		TTF_CloseFont(_hdFontCjk);
		_hdFontCjk = TTF_OpenFontIndex(file.c_str(), corrected, index);
		if (!_hdFontCjk)
		{
			Log(LOG_WARNING) << "HD fonts: can't reopen CJK \"" << _hdTtfCjkPath << "\": " << TTF_GetError();
			return false;
		}
	}
	TTF_SetFontHinting(_hdFontCjk, TTF_HINTING_LIGHT);
	return true;
}

/**
 * Rasterizes a single character into an 8-bit glyph cell using the given
 * TTF handle:
 * - width  = character advance (low-res) * scale
 * - height = font cell height (low-res) * scale
 * - ink = palette index 1, transparent = index 0 (same convention as the
 *   bitmap fonts, so PaletteShift-style shading can be reused untouched).
 * The glyph is baseline-aligned inside the cell; coverage is binarized
 * with a 50% threshold.
 * @param c Character to rasterize.
 * @param scale Display scale factor (may be fractional; cells are rounded).
 * @param font TTF handle to render from (primary or CJK fallback).
 * @return Newly created Surface, or nullptr if the glyph can't be rendered
 *         (caller should fall back to the bitmap glyph).
 */
Surface *Font::rasterizeHdChar(UCode c, double scale, TTF_Font *font) const
{
	if (!font)
		return 0;
	SDL_Rect size = getCharSize(c);
	int cellW = (int)lround(size.w * scale);
	int cellH = (int)lround(getHeight() * scale);
	if (cellW <= 0 || cellH <= 0 || !Unicode::isPrintable(c))
	{
		return 0;
	}

	UString tmp(1, c);
	std::string utf8 = Unicode::convUtf32ToUtf8(tmp);
	if (utf8.empty())
	{
		return 0;
	}
	SDL_Color fg = { 255, 255, 255, 255 };
	SDL_Color bg = { 0, 0, 0, 255 };
	SDL_Surface *shaded = TTF_RenderUTF8_Shaded(font, utf8.c_str(), fg, bg);
	if (!shaded)
	{
		return 0;
	}

	// Find the ink bounding box (pixel value = coverage, 0..255).
	SDL_LockSurface(shaded);
	int minX = shaded->w, minY = shaded->h, maxX = -1, maxY = -1;
	for (int y = 0; y < shaded->h; ++y)
	{
		const Uint8 *row = (const Uint8*)shaded->pixels + y * shaded->pitch;
		for (int x = 0; x < shaded->w; ++x)
		{
			if (row[x] >= 128)
			{
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}
	}
	int inkW = (maxX < 0) ? 0 : (maxX - minX + 1);
	int inkH = (maxY < 0) ? 0 : (maxY - minY + 1);
	int ascent = TTF_FontAscent(font);

	Surface *cell = 0;
	if (inkW > 0 && inkH > 0)
	{
		cell = new Surface(cellW, cellH);
		cell->clear();
		// Baseline placement: assume the original bitmap font keeps a
		// descender of roughly a quarter of the cell height (min 1px).
		int descentLowres = std::max(1, getHeight() / 4);
		int cellBaseline = cellH - (int)lround(descentLowres * scale);
		// The shaded surface's top row corresponds to glyph-space y=ascent,
		// so an ink pixel at row (minY+r) sits (ascent-(minY+r)) above the baseline.
		int dstY = cellBaseline - ascent + minY;
		// CJK ideographs: keep the glyph's natural horizontal position from
		// the TTF (the shaded surface is advance-wide and the em equals the
		// uniform cell), so inter-character spacing comes from the font's
		// own metrics instead of ink-width jitter. Latin glyphs keep the
		// ink-cropped placement to stay tight in their narrow cells.
		int srcX0 = isCjkIdeograph(c) ? 0 : minX;
		int copyW = isCjkIdeograph(c) ? std::min(shaded->w, cellW) : inkW;
		{
			static bool inkDiagLogged = false;
			if (!inkDiagLogged)
			{
				inkDiagLogged = true;
				Log(LOG_INFO) << "HD-DIAG-INK: cell=" << cellW << "x" << cellH
					<< " ascent=" << ascent << " minY=" << minY << " ink=" << inkW << "x" << inkH
					<< " cellBaseline=" << cellBaseline << " dstY=" << dstY;
			}
		}
		for (int r = 0; r < inkH; ++r)
		{
			const Uint8 *row = (const Uint8*)shaded->pixels + (minY + r) * shaded->pitch;
			for (int x = 0; x < copyW; ++x)
			{
				if (row[srcX0 + x] >= 128)
				{
					int px = x, py = dstY + r;
					if (px >= 0 && px < cellW && py >= 0 && py < cellH)
						cell->setPixel(px, py, 1);
				}
			}
		}
	}
	SDL_UnlockSurface(shaded);
	SDL_FreeSurface(shaded);
	return cell;
}

/**
 * Gets an HD glyph cell for a character at the given display scale,
 * rasterizing and caching it on first use. Results (including failure,
 * cached as nullptr) are cached per character for the current scale.
 *
 * Glyph source selection:
 * - If the primary TTF contains the character, render from it.
 * - Otherwise, if a CJK fallback TTF is configured and contains the
 *   character, render from it (so Chinese/Kana text isn't missing).
 * - Otherwise return nullptr, and the caller falls back to the bitmap glyph.
 * @param c Character to get.
 * @param scale Display scale factor (may be fractional).
 * @return 8-bit Surface (ink=1, transparent=0) sized advance*scale x height*scale,
 *         or nullptr if the HD backend can't render it (fall back to bitmap).
 */
Surface *Font::getHdChar(UCode c, double scale) const
{
	if (_hdTtfPath.empty())
		return 0;
	if (!ensureHdFont(scale))
		return 0;
	auto it = _hdGlyphs.find(c);
	if (it != _hdGlyphs.end())
		return it->second;

	TTF_Font *src = _hdFont;
	// For UI fonts with a CJK fallback, render EVERYTHING with the CJK face
	// when it provides the glyph: the fallback face (e.g. Microsoft YaHei)
	// designs its Latin/digits to match the CJK ideographs, so mixed
	// CJK+digit text keeps a uniform visual size. Mixing the Latin-only
	// primary (Arial: small x-height) with the CJK face made digits ~60% of
	// the hanzi height. Geo fonts keep the monospaced primary (digit columns).
	bool geoFont = _id.rfind("FONT_GEO", 0) == 0;
	bool preferCjk = !geoFont && !_hdTtfCjkPath.empty();
	// TTF_GlyphIsProvided takes a 16-bit char, so only probe for BMP codepoints.
	bool primaryHas = (c <= 0xFFFF) ? (TTF_GlyphIsProvided(_hdFont, (Uint16)c) != 0) : true;
	if (preferCjk && ensureHdCjkFont(scale) && (c > 0xFFFF || TTF_GlyphIsProvided(_hdFontCjk, (Uint16)c)))
	{
		src = _hdFontCjk;
	}
	else if (!primaryHas && !_hdTtfCjkPath.empty())
	{
		if (ensureHdCjkFont(scale) && (c > 0xFFFF || TTF_GlyphIsProvided(_hdFontCjk, (Uint16)c)))
		{
			src = _hdFontCjk;
		}
	}

	Surface *glyph = rasterizeHdChar(c, scale, src);
	{
		static int n = 0;
		if (n < 60)
		{
			++n;
			Log(LOG_INFO) << "HD-GLYPH: id=" << _id << " U+ " << (int)c
				<< " scale=" << scale << " src=" << (src == _hdFontCjk ? "CJK" : "PRIMARY")
				<< " cell=" << (glyph ? glyph->getWidth() : -1) << "x" << (glyph ? glyph->getHeight() : -1);
		}
	}
	_hdGlyphs[c] = glyph; // cache failures too, so we don't re-render every frame
	return glyph;
}

#endif /* __HDFONTS__ */


/**
 * Calculates the real size and position of each character in
 * the surface and stores them in SDL_Rect's for future use
 * by other classes.
 * @param index The index of the surface to use.
 * @param str A string of characters to map to the surface.
 */
void Font::init(size_t index, const UString &str)
{
	FontImage *image = &_images[index];
	Surface *surface = image->surface;
	surface->lock();
	int length = (surface->getWidth() / image->width);

	_chars.reserve(_chars.size() + str.size());

	if (_monospace)
	{
		for (size_t i = 0; i < str.length(); ++i)
		{
			SDL_Rect rect;
			int startX = i % length * image->width;
			int startY = i / length * image->height;
			rect.x = startX;
			rect.y = startY;
			rect.w = image->width;
			rect.h = image->height;
			_chars[str[i]] = std::make_pair(index, rect);
		}
	}
	else
	{
		for (size_t i = 0; i < str.length(); ++i)
		{
			SDL_Rect rect;
			int left = -1, right = -1;
			int startX = i % length * image->width;
			int startY = i / length * image->height;
			for (int x = startX; x < startX + image->width; ++x)
			{
				for (int y = startY; y < startY + image->height && left == -1; ++y)
				{
					Uint8 pixel = surface->getPixel(x, y);
					if (pixel != 0)
					{
						left = x;
					}
				}
			}
			for (int x = startX + image->width - 1; x >= startX; --x)
			{
				for (int y = startY + image->height; y-- != startY && right == -1;)
				{
					Uint8 pixel = surface->getPixel(x, y);
					if (pixel != 0)
					{
						right = x;
					}
				}
			}
			rect.x = left;
			rect.y = startY;
			rect.w = right - left + 1;
			rect.h = image->height;

			_chars[str[i]] = std::make_pair(index, rect);
		}
	}
	surface->unlock();
}

/**
 * Returns a particular character from the set stored in the font.
 * @param c Character to use for size/position.
 * @return Pointer to the font's surface with the respective
 * cropping rectangle set up.
 */
SurfaceCrop Font::getChar(UCode c) const
{
	auto f = _chars.find(c);
	if (f == _chars.end())
		f = _chars.find('?');
	auto surfaceCrop = _images[f->second.first].surface->getCrop();
	*surfaceCrop.getCrop() = f->second.second;
	return surfaceCrop;
}

/**
 * Returns the maximum width for any character in the font.
 * @return Width in pixels.
 */
int Font::getWidth() const
{
	return _images[0].width;
}

/**
 * Returns the maximum height for any character in the font.
 * @return Height in pixels.
 */
int Font::getHeight() const
{
	return _images[0].height;
}

/**
 * Returns the spacing between any character in the font.
 * @return Spacing in pixels.
 * @note This does not refer to character spacing within the surface,
 * but to the spacing used between successive characters in a line.
 */
int Font::getSpacing() const
{
	return _images[0].spacing;
}

/**
 * Returns the dimensions of a particular character in the font.
 * @param c Font character.
 * @return Width and Height dimensions (X and Y are ignored).
 */
SDL_Rect Font::getCharSize(UCode c) const
{
	SDL_Rect size = { 0, 0, 0, 0 };
	if (Unicode::isPrintable(c))
	{
		auto f = _chars.find(c);
		if (isCjkIdeograph(c))
		{
			// All CJK ideographs use one uniform advance (the font's cell
			// height). Ink-measured widths differ per glyph (and missing
			// glyphs fell back to the narrow '?'), which made letter spacing
			// uneven; the HD backend renders these with the TTF's natural
			// advance, which equals a square em = the font height.
			size.w = getHeight() + getSpacing();
			size.h = getHeight() + getSpacing();
			size.x = size.w;
			size.y = size.h;
			return size;
		}
		if (f == _chars.end())
			f = _chars.find('?');

		const FontImage *image = &_images[f->second.first];
		size.w = f->second.second.w + image->spacing;
		size.h = f->second.second.h + image->spacing;
	}
	else
	{
		if (_monospace)
			size.w = getWidth() + getSpacing();
		else if (c == Unicode::TOK_NBSP)
			size.w = getWidth() / 4;
		else if (c == '\t')
			size.w = getWidth() * 3 / 4;
		else if (c < 32 && c != 10) // control tokens (color flip etc.) draw nothing
			size.w = 0;
		else
			size.w = getWidth() / 2;
		size.h = getHeight() + getSpacing();
	}
	// In case anyone mixes them up
	size.x = size.w;
	size.y = size.h;
	return size;
}

/**
 * Returns the uniform advance width of CJK ideographs in this font,
 * measured from the font's own CJK glyphs (e.g. U+4E00). Falls back to the
 * font height for fonts without any CJK coverage.
 * @return Width in pixels.
 */
int Font::getCjkAdvance() const
{
	if (_cjkAdvance < 0)
	{
		auto f = _chars.find(0x4E00); // '一'
		if (f != _chars.end())
			_cjkAdvance = f->second.second.w + _images[f->second.first].spacing;
		else
			_cjkAdvance = getHeight();
	}
	return _cjkAdvance;
}

}
