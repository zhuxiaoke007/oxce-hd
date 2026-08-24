/*
 * Win32 IME bridge for the OpenXcom game window (Windows only).
 *
 * SDL 1.2 has no IME support: the result of a CJK input method arrives at the
 * window as WM_IME_CHAR / WM_IME_COMPOSITION messages, which SDL ignores, so
 * Chinese text can never be typed into TextEdit boxes (e.g. base names).
 *
 * This module subclasses the SDL window procedure, intercepts the two IME
 * result messages, converts the UTF-16 characters to UCode and injects them
 * via SDL_PushEvent as synthetic SDL_KEYDOWN events carrying key.unicode.
 * TextEdit consumes key.keysym.unicode, so no game-side changes are needed.
 * Everything else is forwarded to the original (SDL) window procedure;
 * real keyboard input (backspace/enter/ASCII) is untouched.
 */
#pragma once

#ifdef _WIN32

namespace OpenXcom
{

class Win32Ime
{
public:
	/// Installs the IME bridge on the game's SDL window (idempotent; also
	/// re-installs after SDL recreates the window on display resets).
	static void attach();
};

}

#endif /* _WIN32 */
