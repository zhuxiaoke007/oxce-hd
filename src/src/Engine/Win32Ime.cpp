#ifdef _WIN32

#include "Win32Ime.h"

#include <SDL.h>
#include <windows.h>
#include <imm.h>

#include <cstring>
#include <string>

#include "Logger.h"
#include "Unicode.h"

namespace OpenXcom
{

static WNDPROC sOldWndProc = 0;
static HWND sAttachedHwnd = 0;
static HWND sFoundHwnd = 0;
static bool sHasHighSurrogate = false;
static UCode sHighSurrogate = 0;

static BOOL CALLBACK findWindowCallback(HWND hWnd, LPARAM lParam)
{
	DWORD windowPid = 0;
	GetWindowThreadProcessId(hWnd, &windowPid);
	if (windowPid == (DWORD)lParam && IsWindowVisible(hWnd) && GetWindow(hWnd, GW_OWNER) == 0)
	{
		sFoundHwnd = hWnd;
		return FALSE;
	}
	return TRUE;
}

static HWND findGameWindow()
{
	sFoundHwnd = 0;
	EnumWindows(findWindowCallback, (LPARAM)GetCurrentProcessId());
	return sFoundHwnd;
}

static void pushChar(UCode c)
{
	if (c < 0x20)
		return;
	Log(LOG_INFO) << "Win32Ime: injected U+" << (int)c;
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = SDL_KEYDOWN;
	e.key.which = 0;
	e.key.state = SDL_PRESSED;
	e.key.keysym.scancode = 0;
	e.key.keysym.sym = SDLK_UNKNOWN;
	e.key.keysym.unicode = (Uint16)c;
	SDL_PushEvent(&e);
}

static void pushUtf16(const wchar_t *s, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		wchar_t w = s[i];
		if (w >= 0xD800 && w <= 0xDBFF)
		{
			sHasHighSurrogate = true;
			sHighSurrogate = w;
			continue;
		}
		if (w >= 0xDC00 && w <= 0xDFFF && sHasHighSurrogate)
		{
			pushChar(0x10000 + ((sHighSurrogate - 0xD800) << 10) + (w - 0xDC00));
			sHasHighSurrogate = false;
			continue;
		}
		pushChar((UCode)w);
		sHasHighSurrogate = false;
	}
}

static LRESULT CALLBACK imeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_IME_CHAR:
	{
		UCode c = (UCode)(wParam & 0xFFFF);
		if (c >= 0xD800 && c <= 0xDBFF)
		{
			sHasHighSurrogate = true;
			sHighSurrogate = c;
		}
		else if (c >= 0xDC00 && c <= 0xDFFF && sHasHighSurrogate)
		{
			pushChar(0x10000 + ((sHighSurrogate - 0xD800) << 10) + (c - 0xDC00));
			sHasHighSurrogate = false;
		}
		else
		{
			pushChar(c);
			sHasHighSurrogate = false;
		}
		return 0;
	}
	case WM_IME_COMPOSITION:
		if ((lParam & GCS_RESULTSTR) != 0)
		{
			HIMC imc = ImmGetContext(hWnd);
			if (imc)
			{
				LONG len = ImmGetCompositionStringW(imc, GCS_RESULTSTR, 0, 0);
				if (len > 0)
				{
					std::wstring s(len / sizeof(wchar_t), L'\0');
					ImmGetCompositionStringW(imc, GCS_RESULTSTR, &s[0], len);
					pushUtf16(s.data(), s.size());
				}
				ImmReleaseContext(hWnd, imc);
			}
		}
		return 0;
	default:
		break;
	}
	return CallWindowProc(sOldWndProc, hWnd, msg, wParam, lParam);
}

static BOOL CALLBACK listWindowCallback(HWND hWnd, LPARAM lParam)
{
	DWORD windowPid = 0;
	GetWindowThreadProcessId(hWnd, &windowPid);
	if (windowPid == (DWORD)lParam)
	{
		char cls[64] = {0}, title[64] = {0};
		GetClassNameA(hWnd, cls, 63);
		GetWindowTextA(hWnd, title, 63);
		Log(LOG_INFO) << "Win32Ime: window hwnd=" << hWnd << " class=" << cls << " title='" << title << "' visible=" << (int)IsWindowVisible(hWnd);
	}
	return TRUE;
}

void Win32Ime::attach()
{
	if (sAttachedHwnd && ::GetWindowLongPtrW(sAttachedHwnd, GWLP_WNDPROC) == (LONG_PTR)&imeWndProc)
		return;
	HWND hwnd = findGameWindow();
	if (!hwnd)
		return;
	WNDPROC cur = (WNDPROC)::GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
	if (cur == &imeWndProc)
	{
		sAttachedHwnd = hwnd;
		return;
	}
	sOldWndProc = cur;
	::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&imeWndProc);
	sAttachedHwnd = hwnd;
	Log(LOG_INFO) << "Win32Ime: hooked window " << hwnd;
}

}

#endif /* _WIN32 */
