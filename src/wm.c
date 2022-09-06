#include "wm.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
<<<<<<< HEAD
#include <stdint.h>
=======
#include <stdio.h>
#include <stdlib.h>
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

<<<<<<< HEAD
enum
{
	k_mouse_button_left = 1 << 0, 	// 0001
	k_mouse_button_right = 1 << 1,	// 0010
	k_mouse_button_middle = 1 << 2 	// 0100
};

enum
{
	k_key_arrow_down = 1 << 0, 	// 0001
	k_key_arrow_up = 1 << 1, 	// 0001
	k_key_arrow_left = 1 << 2,	// 0010
	k_key_arrow_right = 1 << 3 	// 0100
};

=======
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
typedef struct wm_window_t
{
	HWND hwnd;
	bool quit;
	bool has_focus;
	uint32_t mouse_mask;
<<<<<<< HEAD
	uint32_t key_mask; //there are 254 virtual keys. in a commercial engine, there would need to be a bit more effort put into masking (ex. stringing 4 64 bit masks together)
};

const struct vk_key_map
=======
	uint32_t key_mask;
	int mouse_x;
	int mouse_y;
} wm_window_t;

wm_window_t A_WINDOW;

const struct
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
{
	int virtual_key;
	int ga_key;
}
<<<<<<< HEAD
k_key_map[] = //anonymous (?) struct - we declare the struct and use it for this one structure (array in this case)
{
	{.virtual_key = VK_LEFT, .ga_key = k_key_arrow_left},
	{.virtual_key = VK_RIGHT, .ga_key = k_key_arrow_right},
	{.virtual_key = VK_UP, .ga_key = k_key_arrow_up},
	{.virtual_key = VK_DOWN, .ga_key = k_key_arrow_down},
=======
k_key_map[] =
{
	{ .virtual_key = VK_LEFT, .ga_key = k_key_left, },
	{ .virtual_key = VK_RIGHT, .ga_key = k_key_right, },
	{ .virtual_key = VK_UP, .ga_key = k_key_up, },
	{ .virtual_key = VK_DOWN, .ga_key = k_key_down, },
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
};

static LRESULT CALLBACK _window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
<<<<<<< HEAD
	wm_window_t* win = (wm_window_t*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_KEYDOWN: //this method of key detection won't register two opposing arrow keys being pressed down at the same time. however, there is another call to get that data: GetKeyState() (which would be in wm_pump)
			for (int i = 0; i < _countof(k_key_map); ++i)
			{
				if(k_key_map[i].virtual_key == wParam)
				{ 
					win->key_mask |= k_key_map[i].ga_key;
					break;
				}
				break;
=======
	wm_window_t* win = (wm_window_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (win)
	{
		switch (uMsg)
		{
		case WM_KEYDOWN:
			for (int i = 0; i < _countof(k_key_map); ++i)
			{
				if (k_key_map[i].virtual_key == wParam)
				{
					win->key_mask |= k_key_map[i].ga_key;
					break;
				}
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
			}
			break;
		case WM_KEYUP:
			for (int i = 0; i < _countof(k_key_map); ++i)
			{
<<<<<<< HEAD
				if(k_key_map[i].virtual_key == wParam)
				{ 
					win->key_mask |= k_key_map[i].ga_key;
					break;
				}
				break;
=======
				if (k_key_map[i].virtual_key == wParam)
				{
					win->key_mask &= ~k_key_map[i].ga_key;
					break;
				}
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
			}
			break;
		case WM_LBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_left;
<<<<<<< HEAD
			printf("press\n");
			break;
		case WM_LBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_left;
			// ^= bitwise XOR
			printf("release\n");
=======
			break;
		case WM_LBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_left;
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
			break;
		case WM_RBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_right;
			break;
		case WM_RBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_right;
			break;
		case WM_MBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_middle;
			break;
		case WM_MBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_middle;
			break;
<<<<<<< HEAD
		case WM_MOUSEMOVE:
			printf("%dx%d\n", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (win->has_focus)
			{
				POINT cursor;
				GetCursorPos(&cursor);	

				RECT window_rect;
				GetWindowRect(hwnd, &window_rect);
				SetCursorPos((window_rect.left + window_rect.right) / 2, (window_rect.top + window_rect.bottom) / 2);

				POINT new_cursor; //use to get delta
				GetCursorPos(&new_cursor);
				printf("d %dx%d\n", cursor.x - new_cursor.x, cursor.y - new_cursor.y);
			}
			break;
		case WM_ACTIVATEAPP:
			ShowCursor(wParam);
			win->has_focus = wParam;
			break;
		case WM_CLOSE:
			win->quit = true;
			break;
=======

		case WM_MOUSEMOVE:
			if (win->has_focus)
			{
				// Relative mouse movement in four steps:
				// 1. Get current mouse position (old_cursor).
				// 2. Move mouse back to center of window.
				// 3. Get current mouse position (new_cursor).
				// 4. Compute relative movement (old_cursor - new_cursor).

				POINT old_cursor;
				GetCursorPos(&old_cursor);

				RECT window_rect;
				GetWindowRect(hwnd, &window_rect);
				SetCursorPos(
					(window_rect.left + window_rect.right) / 2,
					(window_rect.top + window_rect.bottom) / 2);

				POINT new_cursor;
				GetCursorPos(&new_cursor);

				win->mouse_x = old_cursor.x - new_cursor.x;
				win->mouse_y = old_cursor.y - new_cursor.y;
			}
			break;

		case WM_ACTIVATEAPP:
			ShowCursor(!wParam);
			win->has_focus = wParam;
			break;

		case WM_CLOSE:
			win->quit = true;
			break;
		}
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
	}
	

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

wm_window_t* wm_create()
{
	WNDCLASS wc =
	{
		.lpfnWndProc = _window_proc,
		.hInstance = GetModuleHandle(NULL),
		.lpszClassName = L"ga2022 window class",
	};
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"GA 2022",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		wc.hInstance,
		NULL);

	if (!hwnd)
	{
<<<<<<< HEAD
		fprintf(stderr, "window initialization failed!\n");
		return NULL;
	}

	
	wm_window_t* win = malloc(1, sizeof(wm_window_t));
	win->has_focus = false;
	win->quit = false;
	win->key_mask = 0;
	win->mouse_mask = 0;
	win->hwnd = hwnd;

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) win);
	ShowWindow(hwnd, TRUE); //windows are created hidden by default
=======
		return NULL;
	}

	wm_window_t* win = malloc(sizeof(wm_window_t));
	win->has_focus = false;
	win->hwnd = hwnd;
	win->key_mask = 0;
	win->mouse_mask = 0;
	win->quit = false;

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);

	// Windows are created hidden by default, so we
	// need to show it here.
	ShowWindow(hwnd, TRUE);

>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
	return win;
}

bool wm_pump(wm_window_t* window)
{
	MSG msg = { 0 };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return window->quit;
}

uint32_t wm_get_mouse_mask(wm_window_t* window)
{
<<<<<<< HEAD
	return window->mouse_mask;	 //%x to print binary
=======
	return window->mouse_mask;
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
}

uint32_t wm_get_key_mask(wm_window_t* window)
{
<<<<<<< HEAD
	return window->key_mask;	
}

void wm_get_mouse_move(int *x, int *y)
{
=======
	return window->key_mask;
}

void wm_get_mouse_move(wm_window_t* window, int* x, int* y)
{
	*x = window->mouse_x;
	*y = window->mouse_y;
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
}

void wm_destroy(wm_window_t* window)
{
<<<<<<< HEAD
	DestroyWindow(window);
=======
	DestroyWindow(window->hwnd);
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
	free(window);
}
