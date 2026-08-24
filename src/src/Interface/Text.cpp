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
#include "Text.h"
#include "../fmath.h"
#include "../Engine/Font.h"
#include "../Engine/Options.h"
#include "../Engine/Language.h"
#include "../Engine/Unicode.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/ShaderMove.h"
#include "../Engine/Screen.h"
#include "../Engine/Action.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{

/**
 * Sets up a blank text with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Text::Text(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y),
	_big(0), _small(0), _font(0), _fontOrig(0), _lang(0),
	_wrap(false), _invert(false), _contrast(false), _indent(false), _scroll(false), _ignoreSeparators(false),
	_align(ALIGN_LEFT), _valign(ALIGN_TOP), _color(0), _color2(0), _scrollY(0), _hdEnabled(true), _hdOriginX(0), _hdOriginY(0)
{
}

/**
 *
 */
Text::~Text()
{

}

/**
 * Changes the text to use the big-size font.
 */
void Text::setBig()
{
	_font = _big;
	_fontOrig = _big;
	processText();
}

/**
 * Changes the text to use the small-size font.
 */
void Text::setSmall()
{
	_font = _small;
	_fontOrig = _small;
	processText();
}

/**
 * Returns the font currently used by the text.
 * @return Pointer to font.
 */
Font *Text::getFont() const
{
	return _font;
}

/**
 * Changes the various resources needed for text rendering.
 * The different fonts need to be passed in advance since the
 * text size can change mid-text, and the language affects
 * how the text is rendered.
 * @param big Pointer to large-size font.
 * @param small Pointer to small-size font.
 * @param lang Pointer to current language.
 */
void Text::initText(Font *big, Font *small, Language *lang)
{
	_big = big;
	_small = small;
	_lang = lang;
	setSmall();
}

/**
 * Changes the string displayed on screen.
 * @param text Text string.
 */
void Text::setText(const std::string &text)
{
	_text = text;
	_font = _fontOrig;
	processText();
	// If big text won't fit the space, try small text
	if (!_text.empty())
	{
		if (_font == _big && (getTextWidth() > getWidth() + 4 || getTextHeight() > getHeight()) && _text[_text.size() - 1] != '.')
		{
			_font = _small;
			processText();
		}
	}
}

/**
 * Returns the string displayed on screen.
 * @return Text string.
 */
std::string Text::getText() const
{
	return _text;
}

/**
 * Enables/disables text wordwrapping. When enabled, lines of
 * text are automatically split to ensure they stay within the
 * drawing area, otherwise they simply go off the edge.
 * @param wrap Wordwrapping setting.
 * @param indent Indent wrapped text.
 * @param ignoreSeparators Handle separators as spaces (false) or as normal text (true)?
 */
void Text::setWordWrap(bool wrap, bool indent, bool ignoreSeparators)
{
	if (wrap != _wrap || indent != _indent || ignoreSeparators != _ignoreSeparators)
	{
		_wrap = wrap;
		_indent = indent;
		_ignoreSeparators = ignoreSeparators;
		processText();
	}
}

/**
 * Enables/disables color inverting. Mostly used to make
 * button text look pressed along with the button.
 * @param invert Invert setting.
 */
void Text::setInvert(bool invert)
{
	_invert = invert;
	_redraw = true;
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape UI.
 * @param contrast High contrast setting.
 */
void Text::setHighContrast(bool contrast)
{
	_contrast = contrast;
	_redraw = true;
}

/**
 * Changes the way the text is aligned horizontally
 * relative to the drawing area.
 * @param align Horizontal alignment.
 */
void Text::setAlign(TextHAlign align)
{
	_align = align;
	_redraw = true;
}

/**
 * Returns the way the text is aligned horizontally
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextHAlign Text::getAlign() const
{
	return _align;
}

/**
 * Changes the way the text is aligned vertically
 * relative to the drawing area.
 * @param valign Vertical alignment.
 */
void Text::setVerticalAlign(TextVAlign valign)
{
	_valign = valign;
	_redraw = true;
}

/**
 * Returns the way the text is aligned vertically
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextVAlign Text::getVerticalAlign() const
{
	return _valign;
}

/**
 * Changes the color used to render the text. Unlike regular graphics,
 * fonts are greyscale so they need to be assigned a specific position
 * in the palette to be displayed.
 * @param color Color value.
 */
void Text::setColor(Uint8 color)
{
	_color = color;
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the color used to render the text.
 * @return Color value.
 */
Uint8 Text::getColor() const
{
	return _color;
}

/**
 * Changes the secondary color used to render the text. The text
 * switches between the primary and secondary color whenever there's
 * a 0x01 in the string.
 * @param color Color value.
 */
void Text::setSecondaryColor(Uint8 color)
{
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the secondary color used to render the text.
 * @return Color value.
 */
Uint8 Text::getSecondaryColor() const
{
	return _color2;
}

int Text::getNumLines() const
{
	return _wrap ? _lineHeight.size() : 1;
}

/**
 * Enables or disables the HD overlay path for this text.
 * Embedded texts (blitted onto a parent surface, e.g. a TextButton label)
 * must keep this disabled because the overlay assumes absolute positioning.
 * @param enabled Whether to use the HD overlay path.
 */
void Text::setHdEnabled(bool enabled)
{
	_hdEnabled = enabled;
	_redraw = true;
}

/**
 * Sets the base-resolution origin of the owning surface. Embedded texts are
 * blitted onto a parent surface (TextList rows, TextButton labels), so their
 * getX()/getY() are relative; the HD overlay needs absolute base coordinates.
 * @param x X position of the parent surface in base coordinates.
 * @param y Y position of the parent surface in base coordinates.
 */
void Text::setHdOrigin(int x, int y)
{
	if (_hdOriginX != x || _hdOriginY != y)
	{
		_hdOriginX = x;
		_hdOriginY = y;
		_redraw = true;
	}
}

/**
 * Returns the rendered text's height. Useful to check if wordwrap applies.
 * @param line Line to get the height, or -1 to get whole text height.
 * @return Height in pixels.
 */
int Text::getTextHeight(int line) const
{
	if (line == -1)
	{
		int height = 0;
		for (int lh : _lineHeight)
		{
			height += lh;
		}
		return height;
	}
	else
	{
		return _lineHeight[line];
	}
}

/**
 * Returns the rendered text's width.
 * @param line Line to get the width, or -1 to get whole text width.
 * @return Width in pixels.
 */
int Text::getTextWidth(int line) const
{
	if (line == -1)
	{
		int width = 0;
		for (int lw : _lineWidth)
		{
			if (lw > width)
			{
				width = lw;
			}
		}
		return width;
	}
	else
	{
		return _lineWidth[line];
	}
}

/**
 * Takes care of any text post-processing like converting
 * encoded text to individual codepoints and calculating
 * line metrics for alignment and wordwrapping.
 */
void Text::processText()
{
	if (_font == 0 || _lang == 0)
	{
		return;
	}

	_processedText = Unicode::convUtf8ToUtf32(_text);
	_lineWidth.clear();
	_lineHeight.clear();
	_scrollY = 0;

	// Defensive guard against wordwrap infinite loops (e.g. CJK text in a
	// column narrower than a single glyph, or a malformed font metric that
	// returns a non-positive character width). The original loop only exits
	// when `c > str.size()`, but every wordwrap insert grows `str` by 1, so a
	// path that re-processes the same character forever would OOM the process.
	// Cap total iterations at a generous multiple of the input length; if we
	// exceed it, log diagnostics and bail out instead of eating all memory.
	const size_t originalLen = _processedText.size();
	const size_t iterCap = (originalLen + 16) * 64 + 4096;
	size_t iterCount = 0;

	int width = 0, word = 0;
	size_t space = 0, textIndentation = 0;
	bool start = true;
	Font *font = _font;
	UString &str = _processedText;

	// Go through the text character by character
	for (size_t c = 0; c <= str.size(); ++c)
	{
		if (++iterCount > iterCap)
		{
			// Suspected wordwrap infinite loop. Print everything we know and
			// abort the (re)shaping; the caller will fall back to whatever
			// _lineWidth/_lineHeight we have collected so far. This keeps the
			// process alive and lets the player back out of the screen.
			Log(LOG_ERROR) << "processText: aborting after " << iterCount
			               << " iterations (input len=" << originalLen
			               << ", current str len=" << str.size()
			               << ", c=" << c << ", getWidth()=" << getWidth()
			               << ", _wrap=" << (_wrap ? 1 : 0)
			               << ", wrap mode=" << (int)_lang->getTextWrapping()
			               << ", _text=\"" << _text << "\""
			               << ", font=" << (void*)font
			               << ", font height=" << (font ? font->getHeight() : -1)
			               << ")";
			if (!_lineWidth.empty())
			{
				_redraw = true;
			}
			return;
		}
		// End of the line
		if (c == str.size() || Unicode::isLinebreak(str[c]))
		{
			// Add line measurements for alignment later
			_lineWidth.push_back(width);
			_lineHeight.push_back(font->getCharSize('\n').h);
			start = true;
			width = 0;
			word = 0;

			if (c == str.size())
				break;
			else if (str[c] == Unicode::TOK_NL_SMALL)
				font = _small;
		}
		// Keep track of spaces for wordwrapping
		else if (Unicode::isSpace(str[c]) || (!_ignoreSeparators && Unicode::isSeparator(str[c])))
		{
			// Store existing indentation
			if (c == textIndentation)
			{
				textIndentation++;
			}
			space = c;
			start = start && (word == 0); // consider initial spaces still as start of line until first character is met
			width += font->getCharSize(str[c]).w;
			word = 0;
		}
		// Custom format, skip 3 next chars
		else if (str[c] == Unicode::TOK_CUSTOM_FORMAT)
		{
			if (c + 3u > str.size())
			{
				str.resize(c + 3u); // add missing character
				str[c + 1u] = '\0';
				str[c + 2u] = '\0';
			}
			c += 2u;
		}
		// Keep track of the width of the last line and word
		else if (str[c] != Unicode::TOK_COLOR_FLIP)
		{
			int charWidth = font->getCharSize(str[c]).w;

			width += charWidth;
			word += charWidth;

			// Wordwrap if the last word doesn't fit the line.
			// The `word < getWidth()` guard prevents an infinite loop when
			// the current word (a single CJK glyph in WRAP_LETTERS mode, or
			// any unbreakable token in WRAP_WORDS mode) is itself wider than
			// the column: without it, every iteration would insert a break
			// and then re-wrap the same word on the next line, growing `str`
			// without bound. When the word can't fit on any line we just let
			// it overflow rather than spinning forever.
			// CJK text contains no spaces (its punctuation isn't a wrap
			// separator either), so the "word" of a whole sentence outgrows
			// the column even though WRAP_LETTERS could legally break at any
			// glyph. Allow breaks in WRAP_LETTERS mode as long as dropping
			// the current glyph still leaves less than a full line; the
			// single-glyph-wider-than-column case keeps the guard (width >
			// charWidth) so the loop always advances.
			bool lettersBreak = (_lang->getTextWrapping() == WRAP_LETTERS) && (width > charWidth);
			if (_wrap && width >= getWidth() && (!start || _lang->getTextWrapping() == WRAP_LETTERS)
				&& (word < getWidth() || lettersBreak))
			{
				size_t indentLocation = c;
				if (_lang->getTextWrapping() == WRAP_WORDS || Unicode::isSpace(str[c]))
				{
					// Go back to the last space and put a linebreak there
					width -= word;
					indentLocation = space;
					if (Unicode::isSpace(str[space]))
					{
						width -= font->getCharSize(str[space]).w;
						str[space] = '\n';
					}
					else
					{
						str.insert(space+1, 1, '\n');
						indentLocation++;
					}
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					// Go back to the last letter and put a linebreak there
					str.insert(c, 1, '\n');
					width -= charWidth;
				}

				// Keep initial indentation of text
				if (textIndentation > 0)
				{
					str.insert(indentLocation+1, textIndentation, '\t');
					indentLocation += textIndentation;
				}
				// Indent due to word wrap.
				if (_indent)
				{
					str.insert(indentLocation+1, 1, '\t');
					width += font->getCharSize('\t').w;
				}

				_lineWidth.push_back(width);
				_lineHeight.push_back(font->getCharSize('\n').h);
				if (_lang->getTextWrapping() == WRAP_WORDS)
				{
					width = word;
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					width = 0;
				}
				start = true;

				// In WRAP_LETTERS mode the break was inserted AT position c
				// (shifting the original character to c+1, or c+2 when
				// _indent also inserted a '\t' at c+1). The next loop
				// iteration would otherwise process the '\t' (adding
				// tab_width to width) and then the original character
				// (adding char_width); if tab_width + char_width >=
				// getWidth(), wordwrap re-triggers on the same character
				// and the loop never advances. Skip past the inserted
				// break (and indent) so the next iteration processes the
				// original character on the new line with width=0. The
				// '\n' was already accounted for (lineWidth pushed, width
				// reset); the '\t' remains in str for visual indent at
				// render time.
				if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					c += _indent ? 1 : 0;
				}
			}
		}
	}

	_redraw = true;
}

namespace
{

struct PaletteShift
{
	static inline void func(Uint8& dest, const Uint8& src, int off, int mul, int mid)
	{
		if(src)
		{
			int inverseOffset = mid ? 2 * (mid - src) : 0;
			dest = off + src * mul + inverseOffset;
		}
	}
};

} //namespace

/**
 * Calculates the starting X position for a line of text.
 * @param line The line number (0 = first, etc).
 * @return The X position in pixels.
 */
int Text::getLineX(int line) const
{
	int x = 0;
	switch (_lang->getTextDirection())
	{
	case DIRECTION_LTR:
		switch (_align)
		{
		case ALIGN_LEFT:
			break;
		case ALIGN_CENTER:
			x = (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = getWidth() - 1 - _lineWidth[line];
			break;
		}
		break;
	case DIRECTION_RTL:
		switch (_align)
		{
		case ALIGN_LEFT:
			x = getWidth() - 1;
			break;
		case ALIGN_CENTER:
			x = getWidth() - (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = _lineWidth[line];
			break;
		}
		break;
	}
	return x;
}

/**
 * Draws all the characters in the text with a really
 * nasty complex gritty text rendering algorithm logic stuff.
 */
void Text::draw()
{
	Surface::draw();
	if (_text.empty() || _font == 0)
	{
		return;
	}

	// Show text borders for debugging
	if (Options::debugUi)
	{
		SDL_Rect r;
		r.w = getWidth();
		r.h = getHeight();
		r.x = 0;
		r.y = 0;
		this->drawRect(&r, 5);
		r.w-=2;
		r.h-=2;
		r.x++;
		r.y++;
		this->drawRect(&r, 0);
	}

	int x = 0, y = 0, line = 0, height = 0;
	Font *font = _font;
	int color = _color;
	bool isAltColor = false;
	const UString &s = _processedText;

	// HD (TTF) text overlay: when enabled, glyphs are rasterized at display
	// resolution onto the Screen overlay instead of this low-res surface.
	Screen *hdScreen = 0;
	double hdScale = 1.0;
	double hdZoom = 1.0;
	bool hdActive = false;
#ifdef __HDFONTS__
	// Only probe the overlay when a font with an HD backend is actually used:
	// per-glyph this is enforced below via font->isHdFont(). No longer gated on
	// Options::hdFontTtf, because the effective TTF path is resolved per-font in
	// Mod (defaults apply when the option is empty) and isn't written back.
	// Also require software rendering: under OpenGL the overlay is never composed
	// (see Screen::flip), so activating it here would leave every glyph undrawn —
	// the bitmap font must stay the active path in that case.
	if (_hdEnabled && Options::hdFonts && !Options::useOpenGL)
	{
		hdScreen = Screen::getActiveScreen();
		if (hdScreen && hdScreen->getTextOverlaySurface())
		{
			hdScale = hdScreen->getHdScale();
			hdZoom = (double)Options::hdFontScale / 100.0;
			if (hdZoom < 1.0) hdZoom = 1.0;
			hdActive = true;
		}
	}
#endif

	height = getTextHeight();

	if (_scroll && (getHeight() - height < 0))
	{
		y = _scrollY;
	}
	else
	{
		switch (_valign)
		{
		case ALIGN_TOP:
			y = 0;
			break;
		case ALIGN_MIDDLE:
			y = (int)ceil((getHeight() - height) / 2.0);
			break;
		case ALIGN_BOTTOM:
			y = getHeight() - height;
			break;
		}
	}

	x = getLineX(line);

	// Set up text color
	int mul = 1;
	if (_contrast)
	{
		mul = 3;
	}

	// Set up text direction
	int dir = 1;
	if (_lang->getTextDirection() == DIRECTION_RTL)
	{
		dir = -1;
	}

	// Invert text by inverting the font palette on index 3 (font palettes use indices 1-5)
	int mid = _invert ? 3 : 0;

	// Per line: does the line contain CJK (or any non-Latin) glyph? Geo-font
	// lines that mix digits with hanzi (e.g. "5分钟") should render their
	// digits from the CJK face too so glyph heights stay uniform; pure-ASCII
	// lines (globe coordinates, dates) keep the monospaced primary face.
	UString::const_iterator lineStart = s.begin();
	bool lineCjk = false;
	auto computeLineCjk = [&s](const UString::const_iterator &begin) -> bool {
		for (UString::const_iterator it = begin; it != s.end(); ++it)
		{
			if (Unicode::isLinebreak(*it))
				return false;
			if (*it > 0x7F)
				return true;
		}
		return false;
	};
	lineCjk = computeLineCjk(lineStart);

	// Draw each letter one by one
	for (UString::const_iterator c = s.begin(); c != s.end(); ++c)
	{
		if (Unicode::isSpace(*c) || *c == '\t')
		{
			x += dir * font->getCharSize(*c).w;
		}
		else if (Unicode::isLinebreak(*c))
		{
			line++;
			y += font->getCharSize(*c).h;
			x = getLineX(line);
			lineStart = c + 1;
			lineCjk = computeLineCjk(lineStart);
			if (*c == Unicode::TOK_NL_SMALL)
			{
				font = _small;
			}
		}
		else if (*c == Unicode::TOK_COLOR_FLIP)
		{
			if (isAltColor == false)
			{
				color = _color2;
				isAltColor = true;
			}
			else
			{
				color = _color;
				isAltColor = false;
			}
		}
		else if (*c == Unicode::TOK_CUSTOM_FORMAT)
		{
			const auto op = *(c + 1u);
			const auto arg = *(c + 2u);
			switch (op)
			{
				case 'C': // custom color like "\eC\x45"
					color = arg;
					isAltColor = false;
					break;

				case 'c': // specific color like "\ecP" - primary, "\ecS" - secondary
					if (arg == 'P')
					{
						color = _color;
						isAltColor = false;
					}
					else if (arg == 'S')
					{
						color = _color2;
						isAltColor = true;
					}
					break;

				default:
					// nothing
					break;
			}
			c += 2;
		}
		else
		{
			if (dir < 0)
				x += dir * (int)lround(font->getCharSize(*c).w * (hdActive ? hdZoom : 1.0));

#ifdef __HDFONTS__
			if (hdActive && font->isHdFont())
			{
				Surface *glyph = font->getHdChar(*c, hdScale, lineCjk);
				if (glyph)
				{
					static bool hdDiagLogged = false;
					if (!hdDiagLogged)
					{
						hdDiagLogged = true;
						Log(LOG_INFO) << "HD-DIAG: scale=" << hdScale << " zoom=" << hdZoom
							<< " glyph=" << (void*)glyph << " cell=" << glyph->getWidth() << "x" << glyph->getHeight()
							<< " color=" << color << " mul=" << mul << " mid=" << mid;
					}
					int dx, dy;
					hdScreen->baseToDisplay(_hdOriginX + getX() + x, _hdOriginY + getY() + y, dx, dy);
					ShaderDraw<PaletteShift>(
					ShaderSurface(hdScreen->getTextOverlaySurface(), 0, 0),
					ShaderCrop(SurfaceCrop(glyph), dx, dy),
					ShaderScalar(color), ShaderScalar(mul), ShaderScalar(mid));
				// Advance by the zoomed width so HD glyphs tile without overlap.
				if (dir > 0)
					x += dir * (int)lround(font->getCharSize(*c).w * hdZoom);
				continue;
				}
				// Glyph unavailable: fall through to the bitmap rendering below.
			}
#endif
			auto chr = font->getChar(*c);
			chr.setX(x);
			chr.setY(y);
			ShaderDraw<PaletteShift>(ShaderSurface(this, 0, 0), ShaderCrop(chr), ShaderScalar(color), ShaderScalar(mul), ShaderScalar(mid));
			if (dir > 0)
				x += dir * font->getCharSize(*c).w;
		}
	}
}

/**
 * Allows the text to be scrollable via mouse wheel.
 */
void Text::setScrollable(bool scroll)
{
	_scroll = scroll;
}

/**
 * Handles scrolling.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Text::mousePress(Action* action, State* state)
{
	InteractiveSurface::mousePress(action, state);
	if (_scroll &&
		(action->getDetails()->button.button == SDL_BUTTON_WHEELUP ||
		action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN))
	{
		int scrollArea = getHeight() - getTextHeight();
		if (scrollArea < 0)
		{
			int scrollAmount = _font->getHeight() + _font->getSpacing();
			if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
				scrollAmount = -scrollAmount;

			_scrollY = Clamp(_scrollY + scrollAmount, scrollArea, 0);
			_redraw = true;
		}
	}
}

}
