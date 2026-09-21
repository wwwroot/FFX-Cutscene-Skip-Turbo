#include "osd.h"
#include "config.h"
#include "speedhack.h"
#include <atomic>
#include <cstdio>

static const wchar_t OSD_CLASS_NAME[] = L"FFX_CutsceneSkip_OSD";
static HWND g_hOsdWnd = NULL;
static HWND g_hGameWnd = NULL;
static std::atomic<bool> g_OsdActive(false);
static float g_OsdSpeed = 8.0f;
static std::atomic<bool> g_OsdThreadRunning(false);
static HANDLE g_hOsdThread = NULL;
static HFONT g_hFont = NULL;

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == GetCurrentProcessId())
	{
		// Make sure it's a visible main window, not child or message-only
		if (IsWindowVisible(hwnd) && (GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD) == 0)
		{
			HWND* pOut = (HWND*)lParam;
			*pOut = hwnd;
			return FALSE; // Stop enum
		}
	}
	return TRUE;
}

static HWND FindGameWindow()
{
	HWND hwnd = NULL;
	EnumWindows(EnumWindowsCallback, (LPARAM)&hwnd);
	return hwnd;
}

static LRESULT CALLBACK OsdWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT rc;
		GetClientRect(hwnd, &rc);

		// Minimalist Clean Black & White Design:
		// Pure black background, no border
		HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
		FillRect(hdc, &rc, bgBrush);
		DeleteObject(bgBrush);

		// Crisp white text
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, RGB(255, 255, 255));
		HFONT oldFont = (HFONT)SelectObject(hdc, g_hFont);

		wchar_t textBuf[64];
		swprintf_s(textBuf, L"TURBO %.1fx", g_OsdSpeed);

		RECT textRc = rc;
		DrawTextW(hdc, textBuf, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		SelectObject(hdc, oldFont);
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_DESTROY:
		return 0;
	default:
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
}

static DWORD WINAPI OsdThreadProc(LPVOID lpParam)
{
	HINSTANCE hInst = GetModuleHandleW(NULL);

	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = OsdWndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszClassName = OSD_CLASS_NAME;

	RegisterClassExW(&wc);

	// Clean, legible Segoe UI font (Height: 16, Bold)
	g_hFont = CreateFontW(
		16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
	);

	// WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
	// Click-through, no taskbar button, doesn't steal focus
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
	DWORD style = WS_POPUP;

	// Compact badge: 120 x 26 px
	g_hOsdWnd = CreateWindowExW(
		exStyle,
		OSD_CLASS_NAME,
		L"FFX Turbo OSD",
		style,
		0, 0, 120, 26,
		NULL, NULL, hInst, NULL
	);

	if (g_hOsdWnd)
	{
		// Clean ~88% opacity (225 / 255) for subtle black background integration
		SetLayeredWindowAttributes(g_hOsdWnd, 0, 225, LWA_ALPHA);
		ShowWindow(g_hOsdWnd, SW_HIDE);
	}

	bool wasCurrentlyVisible = false;

	while (g_OsdThreadRunning.load())
	{
		// Find game window if not already cached or invalidated
		if (!g_hGameWnd || !IsWindow(g_hGameWnd))
		{
			g_hGameWnd = FindGameWindow();
		}

		bool turboActive = g_OsdActive.load();

		// Check game focus and state:
		// Do NOT display if:
		// 1. User Alt-Tabbed to another app (game is not foreground)
		// 2. Game is minimized (IsIconic)
		// 3. Game window is hidden
		HWND foregroundWnd = GetForegroundWindow();
		bool isGameFocused = (g_hGameWnd && foregroundWnd == g_hGameWnd);
		bool isGameMinimized = (g_hGameWnd && IsIconic(g_hGameWnd));
		bool isGameVisible = (g_hGameWnd && IsWindowVisible(g_hGameWnd));
		bool inBattle = IsInBattle();
		bool inMovie = IsMoviePlaying();

		bool shouldBeVisible = turboActive && g_Config.showOSD && isGameFocused && !isGameMinimized && isGameVisible && !inBattle && !inMovie;

		if (shouldBeVisible)
		{
			// Position in the top-right of the game window client area with 20px right padding
			const int OSD_WIDTH = 120;
			const int OSD_HEIGHT = 26;
			const int PADDING_X = 20;
			const int PADDING_Y = 16;

			RECT clientRc;
			GetClientRect(g_hGameWnd, &clientRc);
			POINT pt = { clientRc.right - OSD_WIDTH - PADDING_X, clientRc.top + PADDING_Y };
			ClientToScreen(g_hGameWnd, &pt);

			if (!wasCurrentlyVisible)
			{
				SetWindowPos(g_hOsdWnd, HWND_TOPMOST, pt.x, pt.y, OSD_WIDTH, OSD_HEIGHT,
					SWP_NOACTIVATE | SWP_SHOWWINDOW);
				InvalidateRect(g_hOsdWnd, NULL, TRUE);
				wasCurrentlyVisible = true;
			}
			else
			{
				// Keep anchored in case game window moved or resized
				SetWindowPos(g_hOsdWnd, HWND_TOPMOST, pt.x, pt.y, OSD_WIDTH, OSD_HEIGHT,
					SWP_NOACTIVATE | SWP_NOSIZE);
			}
		}
		else
		{
			if (wasCurrentlyVisible)
			{
				ShowWindow(g_hOsdWnd, SW_HIDE);
				wasCurrentlyVisible = false;
			}
		}

		// Message pump for the overlay window
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		Sleep(25); // ~40 Hz refresh rate (<0.01% CPU)
	}

	if (g_hOsdWnd)
	{
		DestroyWindow(g_hOsdWnd);
		g_hOsdWnd = NULL;
	}

	if (g_hFont)
	{
		DeleteObject(g_hFont);
		g_hFont = NULL;
	}

	UnregisterClassW(OSD_CLASS_NAME, hInst);
	return 0;
}

void InitOSD()
{
	if (g_OsdThreadRunning.load()) return;

	g_OsdThreadRunning.store(true);
	g_hOsdThread = CreateThread(NULL, 0, OsdThreadProc, NULL, 0, NULL);
}

void UpdateOSD(bool active, float speed)
{
	g_OsdSpeed = speed;
	g_OsdActive.store(active);
	if (g_hOsdWnd)
	{
		InvalidateRect(g_hOsdWnd, NULL, TRUE);
	}
}

void ShutdownOSD()
{
	if (!g_OsdThreadRunning.load()) return;

	g_OsdActive.store(false);
	g_OsdThreadRunning.store(false);

	if (g_hOsdThread)
	{
		WaitForSingleObject(g_hOsdThread, 1000);
		CloseHandle(g_hOsdThread);
		g_hOsdThread = NULL;
	}
}
