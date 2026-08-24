#pragma once
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
#include <unordered_map>
#include <vector>
#include <utility>
#include <string>
#include <SDL.h>
#ifdef __HDFONTS__
#include <SDL_ttf.h>
#endif
#include "../Engine/Yaml.h"
#include "Unicode.h"

#include "Surface.h"

namespace OpenXcom
{

class Surface;
class Palette;

struct FontImage
{
	int width, height, spacing;
	Surface *surface;
};

/**
 * Takes care of loading and storing each character in a sprite font.
 * Sprite fonts consist of a set of characters split in fixed-size regions.
 * @note The characters don't all need to be the same size, they can
 * have blank space and will be automatically lined up properly.
 */
class Font
{
private:
	std::vector<FontImage> _images;
	std::unordered_map< UCode, std::pair<size_t, SDL_Rect> > _chars;
	bool _monospace;
	/// Cached advance width for CJK ideographs (measured from the bitmap
	/// font's own CJK glyphs; -1 = not yet measured). Used so characters
	/// missing from the bitmap sheets still lay out with the uniform CJK
	/// width instead of the '?' fallback's half width.
	mutable int _cjkAdvance;
#ifdef __HDFONTS__
	/// Optional HD (TTF) backend: path to the primary TTF file (empty = disabled).
	std::string _hdTtfPath;
	/// Optional CJK fallback TTF (used when the primary lacks a glyph). Empty = none.
	std::string _hdTtfCjkPath;
	/// TTF handle for the primary font, opened at the pixel size required by the current scale.
	mutable TTF_Font *_hdFont;
	/// TTF handle for the CJK fallback font (lazily opened).
	mutable TTF_Font *_hdFontCjk;
	/// Scale factor (display/base resolution) the TTFs were opened for.
	/// Fractional at window sizes that don't scale by an exact integer.
	mutable double _hdScale;
	/// Cache of rasterized HD glyph cells for the current scale (may hold nullptr = fallback).
	/// Keyed by (codepoint, source-preference) so a glyph rendered from the
	/// CJK face next to hanzi isn't confused with the same glyph rendered
	/// from the primary face in a pure-ASCII context.
	mutable std::unordered_map<uint64_t, Surface*> _hdGlyphs;
	/// Font id (e.g. "FONT_BIG"), used to pick a role-appropriate TTF.
	std::string _id;
	/// (Re)opens the primary TTF at the pixel size required by the given scale.
	bool ensureHdFont(double scale) const;
	/// (Re)opens the CJK fallback TTF at the pixel size required by the given scale.
	bool ensureHdCjkFont(double scale) const;
	/// Rasterizes a single character into an 8-bit cell using the given handle
	/// (ink = index 1, transparent = index 0). Returns nullptr if it can't render.
	Surface *rasterizeHdChar(UCode c, double scale, TTF_Font *font) const;
#endif
	/// Determines the size and position of each character in the font.
	void init(size_t index, const UString &str);
public:

	/// Default palette for terminal text.
	static const SDL_Color TerminalColors[2];

	/// Creates a blank font.
	Font();
	/// Cleans up the font.
	~Font();
	/// Loads the font from YAML.
	void load(const YAML::YamlNodeReader& reader);
	/// Generate the terminal font.
	void loadTerminal();
	/// Loads an optional HD (TTF) backend for this font.
	/// @param ttfPath Primary TTF (used for most glyphs).
	/// @param ttfCjkPath Optional CJK fallback TTF (used when the primary lacks a glyph).
	void loadHdFont(const std::string &ttfPath, const std::string &ttfCjkPath = std::string());
	/// Is the HD (TTF) backend available for this font?
	bool isHdFont() const;
	/// Drops all cached HD glyphs (e.g. after display resolution change).
	void invalidateHdCache() const;
#ifdef __HDFONTS__
	/// Sets the font id (used for role-based TTF selection).
	void setId(const std::string &id) { _id = id; }
	/// Gets the font id.
	const std::string &getId() const { return _id; }
	/// Gets an HD glyph cell for a character at the given display scale:
	/// 8-bit surface, width = advance * scale, height = font height * scale,
	/// ink = index 1, transparent = index 0. Returns nullptr on failure
	/// (caller should fall back to the bitmap glyph).
	/// @param preferCjk Prefer the CJK fallback face for this glyph (used when
	/// the surrounding line contains CJK text, so digits/heights stay uniform).
	Surface *getHdChar(UCode c, double scale, bool preferCjk) const;
#else
	/// Stub when built without SDL_ttf.
	void setId(const std::string &id) { (void)id; }
	Surface *getHdChar(UCode c, double scale, bool preferCjk) const { (void)c; (void)scale; (void)preferCjk; return 0; }
#endif
	/// Gets a particular character from the font, with its real size.
	SurfaceCrop getChar(UCode c) const;
	/// Gets the font's character width.
	int getWidth() const;
	/// Gets the font's character height.
	int getHeight() const;
	/// Gets the spacing between characters.
	int getSpacing() const;
	/// Gets the size of a particular character;
	SDL_Rect getCharSize(UCode c) const;
	/// Uniform advance width of CJK ideographs in this font.
	int getCjkAdvance() const;
};

}
