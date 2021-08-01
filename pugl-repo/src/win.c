/*
  Copyright 2012-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "win.h"

#include "implementation.h"
#include "stub.h"

#include "pugl/pugl.h"
#include "pugl/stub.h"

#include <windows.h>
#include <windowsx.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#ifndef WM_MOUSEWHEEL
#  define WM_MOUSEWHEEL 0x020A
#endif
#ifndef WM_MOUSEHWHEEL
#  define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef WHEEL_DELTA
#  define WHEEL_DELTA 120
#endif
#ifndef GWLP_USERDATA
#  define GWLP_USERDATA (-21)
#endif

#define PUGL_LOCAL_CLOSE_MSG (WM_USER + 50)
#define PUGL_LOCAL_MARK_MSG (WM_USER + 51)
#define PUGL_LOCAL_AWAKE_MSG (WM_USER + 52)
#define PUGL_LOCAL_TIMER_MSG (WM_USER + 53)
#define PUGL_LOCAL_CLIPBD_MSG (WM_USER + 54)
#define PUGL_LOCAL_CLIENT_MSG (WM_USER + 52)

typedef BOOL(WINAPI* PFN_SetProcessDPIAware)(void);

LRESULT CALLBACK
worldWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK
wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static char*
puglWideCharToUtf8(const wchar_t* const wstr, size_t* len)
{
  int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (n > 0) {
    char* result = (char*)calloc((size_t)n, sizeof(char));
    if (result) {
      WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result, n, NULL, NULL);
      *len = (size_t)n;
      return result;
    }
  }

  return NULL;
}

PuglStatus
puglInitApplication(PuglApplicationFlags PUGL_UNUSED(flags))
{
  return PUGL_SUCCESS;
}

PuglWorldInternals*
puglInitWorldInternals(PuglWorldType  PUGL_UNUSED(type),
                       PuglWorldFlags PUGL_UNUSED(flags))
{
  PuglWorldInternals* impl =
    (PuglWorldInternals*)calloc(1, sizeof(PuglWorldInternals));
  if (!impl) {
    return NULL;
  }

  HMODULE user32 = LoadLibrary("user32.dll");
  if (user32) {
    PFN_SetProcessDPIAware SetProcessDPIAware =
      (PFN_SetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
    if (SetProcessDPIAware) {
      SetProcessDPIAware();
    }

    FreeLibrary(user32);
  }
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  impl->timerFrequency  = (double)frequency.QuadPart;
  impl->nextProcessTime = -1;
  return impl;
}

static wchar_t*
puglWindowClassName(const char* className, const char* suffix)
{
  size_t   len1    = strlen(className);
  size_t   len2    = suffix ? strlen(suffix) : 0;
  size_t   len     = len1 + len2;
  char*    buffer  = malloc(len + 10);
  wchar_t* wbuffer = NULL;
  if (buffer) {
    strcpy(buffer, className);
    if (suffix)
      strcpy(buffer + len1, suffix);
    int         i  = 0;
    WNDCLASSEXW wc = {0};
    while (i < 32000) {
      if (wbuffer)
        free(wbuffer);
      wbuffer = puglUtf8ToWideChar(buffer);
      if (!wbuffer) {
        free(buffer);
        return NULL;
      }
      if (GetClassInfoExW(GetModuleHandle(NULL), wbuffer, &wc)) {
        // Already registered
        i += 1;
        snprintf(buffer + len, 10, "-%d", i);
      } else {
        break;
      }
    }
    if (i < 32000) {
      free(buffer);
      return wbuffer;
    } else {
      free(buffer);
      free(wbuffer);
      return NULL;
    }
  } else {
    return NULL;
  }
}

static void
puglFreeString(char** ptr)
{
  if (*ptr) {
    free(*ptr);
    *ptr = NULL;
  }
}

static bool
puglInitWorldInternals2(PuglWorld* world)
{
  PuglWorldInternals* impl = world->impl;
  if (impl->initialized) {
    return true;
  }
  {
    wchar_t* buffer = puglWindowClassName(world->className, "-msg");
    if (!buffer) {
      goto failed;
    }
    if (impl->worldClassName)
      free(impl->worldClassName);
    impl->worldClassName = buffer;
  }
  {
    wchar_t* buffer = puglWindowClassName(world->className, NULL);
    if (!buffer) {
      goto failed;
    }
    if (impl->windowClassName)
      free(impl->windowClassName);
    impl->windowClassName = buffer;
  }
  {
    wchar_t* buffer = puglWindowClassName(world->className, "-popup");
    if (!buffer) {
      goto failed;
    }
    if (impl->popupClassName)
      free(impl->popupClassName);
    impl->popupClassName = buffer;
  }

  WNDCLASSEXW wc   = {0};
  wc.cbSize        = sizeof(wc);
  wc.style         = CS_OWNDC;
  wc.lpfnWndProc   = worldWndProc;
  wc.hInstance     = GetModuleHandle(NULL);
  wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = impl->worldClassName;
  if (RegisterClassExW(&wc) == 0) {
    goto failed;
  }
  wc.lpfnWndProc   = wndProc;
  wc.lpszClassName = impl->windowClassName;
  if (RegisterClassExW(&wc) == 0) {
    UnregisterClassW(impl->worldClassName, NULL);
    goto failed;
  }
  wc.style |= 0x00020000 /* CS_DROPSHADOW */;
  wc.lpszClassName = impl->popupClassName;
  if (RegisterClassExW(&wc) == 0) {
    UnregisterClassW(impl->worldClassName, NULL);
    UnregisterClassW(impl->windowClassName, NULL);
    goto failed;
  }
  impl->pseudoWin = CreateWindowExW(0,
                                    impl->worldClassName,
                                    impl->worldClassName,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    HWND_MESSAGE,
                                    NULL,
                                    NULL,
                                    NULL);
  if (!impl->pseudoWin) {
    UnregisterClassW(impl->worldClassName, NULL);
    UnregisterClassW(impl->windowClassName, NULL);
    UnregisterClassW(impl->popupClassName, NULL);
    goto failed;
  }
  SetWindowLongPtr(impl->pseudoWin, GWLP_USERDATA, (LONG_PTR)world);
  impl->initialized = true;

  return true;

failed:
  if (impl->worldClassName) {
    free(impl->worldClassName);
    impl->worldClassName = NULL;
  }
  if (impl->windowClassName) {
    free(impl->windowClassName);
    impl->windowClassName = NULL;
  }
  if (impl->popupClassName) {
    free(impl->popupClassName);
    impl->popupClassName = NULL;
  }
  return false;
}

PuglStatus
puglSetClassName(PuglWorld* const world, const char* const name)
{
  if (world->impl->initialized) {
    return PUGL_FAILURE;
  } else {
    puglSetString(&world->className, name);
    return puglInitWorldInternals2(world) ? PUGL_SUCCESS : PUGL_FAILURE;
  }
}

void*
puglGetNativeWorld(PuglWorld* PUGL_UNUSED(world))
{
  return GetModuleHandle(NULL);
}

PuglInternals*
puglInitViewInternals(void)
{
  return (PuglInternals*)calloc(1, sizeof(PuglInternals));
}

static PuglStatus
puglPollWinEvents(PuglWorld* world, const double timeout, bool isFirstCall)
{
  if (!world->impl->initialized && !puglInitWorldInternals2(world)) {
    return PUGL_BAD_CONFIGURATION;
  }
  if (isFirstCall) {
    DWORD hasMsg = GetQueueStatus(QS_ALLEVENTS);
    // The high-order word of the return value indicates the types of messages
    // currently in the queue. The low-order word indicates the types of
    // messages that have been added to the queue and that are still in the
    // queue since the last call to the GetQueueStatus, GetMessage, or
    // PeekMessage function.
    if (hasMsg) {
      return PUGL_SUCCESS; // hasEvents
    }
  }

  if (timeout < 0) {
    WaitMessage();
  } else {
    DWORD rc = MsgWaitForMultipleObjects(0, NULL, FALSE, (DWORD)(timeout * 1e3), QS_ALLEVENTS);
    // Functions such as PeekMessage, GetMessage, and WaitMessage mark messages
    // in the queue as old messages. Therefore, after you call one of these
    // functions, a subsequent call to MsgWaitForMultipleObjects will not return
    // until new input of the specified type arrives.
    if (rc != WAIT_OBJECT_0) {
      return PUGL_FAILURE; // timeout
    }
  }
  return PUGL_SUCCESS;  // hasEvents
}

void
puglAwake(PuglWorld* world)
{
  if (world->impl->pseudoWin) {
    PostMessage(world->impl->pseudoWin, PUGL_LOCAL_AWAKE_MSG, 0, 0);
  }
}

PuglStatus
puglRealize(PuglView* view)
{
  PuglInternals* impl = view->impl;
  if (impl->hwnd) {
    return PUGL_FAILURE;
  }

  // Getting depth from the display mode seems tedious, just set usual values
  if (view->hints[PUGL_RED_BITS] == PUGL_DONT_CARE) {
    view->hints[PUGL_RED_BITS] = 8;
  }
  if (view->hints[PUGL_BLUE_BITS] == PUGL_DONT_CARE) {
    view->hints[PUGL_BLUE_BITS] = 8;
  }
  if (view->hints[PUGL_GREEN_BITS] == PUGL_DONT_CARE) {
    view->hints[PUGL_GREEN_BITS] = 8;
  }
  if (view->hints[PUGL_ALPHA_BITS] == PUGL_DONT_CARE) {
    view->hints[PUGL_ALPHA_BITS] = 8;
  }

  // Get refresh rate for resize draw timer
  DEVMODEA devMode = {0};
  EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devMode);
  view->hints[PUGL_REFRESH_RATE] = (int)devMode.dmDisplayFrequency;

  // Register window class if necessary
  if (!view->world->impl->initialized &&
      !puglInitWorldInternals2(view->world)) {
    return PUGL_REGISTRATION_FAILED;
  }

  if (!view->backend || !view->backend->configure) {
    return PUGL_BAD_BACKEND;
  }

  PuglStatus st = PUGL_SUCCESS;
  if ((st = view->backend->configure(view)) ||
      (st = view->backend->create(view))) {
    return st;
  }

  if (view->title) {
    puglSetWindowTitle(view, view->title);
  }

  view->impl->cursor = LoadCursor(NULL, IDC_ARROW);

  SetWindowLongPtr(impl->hwnd, GWLP_USERDATA, (LONG_PTR)view);

  puglDispatchSimpleEvent(view, PUGL_CREATE);

  return PUGL_SUCCESS;
}

PuglStatus
puglShow(PuglView* view)
{
  PuglInternals* impl = view->impl;

  if (!impl->hwnd) {
    const PuglStatus st = puglRealize(view);
    if (st) {
      return st;
    }
  }

  if (!view->parent && view->transientParent && view->hints[PUGL_IS_POPUP]) {
    ShowWindow(impl->hwnd, SW_SHOWNOACTIVATE);
  } else {
    ShowWindow(impl->hwnd, SW_SHOWNORMAL);
    SetFocus(impl->hwnd);
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglHide(PuglView* view)
{
  PuglInternals* impl = view->impl;

  ShowWindow(impl->hwnd, SW_HIDE);
  return PUGL_SUCCESS;
}

void
puglFreeViewInternals(PuglView* view)
{
  if (view) {
    if (view->backend) {
      view->backend->destroy(view);
    }

    ReleaseDC(view->impl->hwnd, view->impl->hdc);
    SetWindowLongPtr(view->impl->hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
    DestroyWindow(view->impl->hwnd);
    if (view->impl->updateRegion) {
      DeleteObject(view->impl->updateRegion);
    }
    if (view->impl->updateRegionData) {
      free(view->impl->updateRegionData);
    }
    free(view->impl);
  }
}

void
puglFreeWorldInternals(PuglWorld* world)
{
  if (world->impl->processTimer) {
    DeleteTimerQueueTimer(NULL, world->impl->processTimer, NULL);
  }
  if (world->impl->worldClassName) {
    UnregisterClassW(world->impl->worldClassName, NULL);
    free(world->impl->worldClassName);
    world->impl->worldClassName = NULL;
  }
  if (world->impl->windowClassName) {
    UnregisterClassW(world->impl->windowClassName, NULL);
    free(world->impl->windowClassName);
    world->impl->windowClassName = NULL;
  }
  if (world->impl->popupClassName) {
    UnregisterClassW(world->impl->popupClassName, NULL);
    free(world->impl->popupClassName);
    world->impl->popupClassName = NULL;
  }
  if (world->impl->pseudoWin) {
    SetWindowLongPtr(world->impl->pseudoWin, GWLP_USERDATA, (LONG_PTR)NULL);
    DestroyWindow(world->impl->pseudoWin);
  }

  free(world->impl);
}

static PuglKey
keySymToSpecial(WPARAM sym)
{
  // clang-format off
	switch (sym) {
	case VK_F1:       return PUGL_KEY_F1;
	case VK_F2:       return PUGL_KEY_F2;
	case VK_F3:       return PUGL_KEY_F3;
	case VK_F4:       return PUGL_KEY_F4;
	case VK_F5:       return PUGL_KEY_F5;
	case VK_F6:       return PUGL_KEY_F6;
	case VK_F7:       return PUGL_KEY_F7;
	case VK_F8:       return PUGL_KEY_F8;
	case VK_F9:       return PUGL_KEY_F9;
	case VK_F10:      return PUGL_KEY_F10;
	case VK_F11:      return PUGL_KEY_F11;
	case VK_F12:      return PUGL_KEY_F12;
	case VK_BACK:     return PUGL_KEY_BACKSPACE;
	case VK_DELETE:   return PUGL_KEY_DELETE;
	case VK_LEFT:     return PUGL_KEY_LEFT;
	case VK_UP:       return PUGL_KEY_UP;
	case VK_RIGHT:    return PUGL_KEY_RIGHT;
	case VK_DOWN:     return PUGL_KEY_DOWN;
	case VK_PRIOR:    return PUGL_KEY_PAGE_UP;
	case VK_NEXT:     return PUGL_KEY_PAGE_DOWN;
	case VK_HOME:     return PUGL_KEY_HOME;
	case VK_END:      return PUGL_KEY_END;
	case VK_INSERT:   return PUGL_KEY_INSERT;
	case VK_SHIFT:
	case VK_LSHIFT:   return PUGL_KEY_SHIFT_L;
	case VK_RSHIFT:   return PUGL_KEY_SHIFT_R;
	case VK_CONTROL:
	case VK_LCONTROL: return PUGL_KEY_CTRL_L;
	case VK_RCONTROL: return PUGL_KEY_CTRL_R;
	case VK_MENU:
	case VK_LMENU:    return PUGL_KEY_ALT_L;
	case VK_RMENU:    return PUGL_KEY_ALT_R;
	case VK_LWIN:     return PUGL_KEY_SUPER_L;
	case VK_RWIN:     return PUGL_KEY_SUPER_R;
	case VK_CAPITAL:  return PUGL_KEY_CAPS_LOCK;
	case VK_SCROLL:   return PUGL_KEY_SCROLL_LOCK;
	case VK_NUMLOCK:  return PUGL_KEY_NUM_LOCK;
	case VK_SNAPSHOT: return PUGL_KEY_PRINT_SCREEN;
	case VK_PAUSE:    return PUGL_KEY_PAUSE;
	}
  // clang-format on

  return (PuglKey)0;
}

static uint32_t
getModifiers(void)
{
  // clang-format off
	return (((GetKeyState(VK_SHIFT)   < 0) ? PUGL_MOD_SHIFT  : 0u) |
	        ((GetKeyState(VK_CONTROL) < 0) ? PUGL_MOD_CTRL   : 0u) |
	        ((GetKeyState(VK_MENU)    < 0) ? PUGL_MOD_ALT    : 0u) |
	        ((GetKeyState(VK_LWIN)    < 0) ? PUGL_MOD_SUPER  : 0u) |
	        ((GetKeyState(VK_RWIN)    < 0) ? PUGL_MOD_SUPER  : 0u));
  // clang-format on
}

static void
initMouseEvent(PuglEvent* event,
               PuglView*  view,
               int        button,
               bool       press,
               LPARAM     lParam)
{
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  ClientToScreen(view->impl->hwnd, &pt);

  if (press) {
    SetCapture(view->impl->hwnd);
  } else {
    ReleaseCapture();
  }

  event->button.time   = GetMessageTime() / 1e3;
  event->button.type   = press ? PUGL_BUTTON_PRESS : PUGL_BUTTON_RELEASE;
  event->button.x      = GET_X_LPARAM(lParam);
  event->button.y      = GET_Y_LPARAM(lParam);
  event->button.xRoot  = pt.x;
  event->button.yRoot  = pt.y;
  event->button.state  = getModifiers();
  event->button.button = (uint32_t)button;
}

static void
initScrollEvent(PuglEvent* event, PuglView* view, LPARAM lParam)
{
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  ScreenToClient(view->impl->hwnd, &pt);

  event->scroll.time  = GetMessageTime() / 1e3;
  event->scroll.type  = PUGL_SCROLL;
  event->scroll.x     = pt.x;
  event->scroll.y     = pt.y;
  event->scroll.xRoot = GET_X_LPARAM(lParam);
  event->scroll.yRoot = GET_Y_LPARAM(lParam);
  event->scroll.state = getModifiers();
  event->scroll.dx    = 0;
  event->scroll.dy    = 0;
}

/// Return the code point for buf, or the replacement character on error
static uint32_t
puglDecodeUTF16(const wchar_t* buf, const int len)
{
  const uint32_t c0 = buf[0];
  const uint32_t c1 = buf[0];
  if (c0 >= 0xD800 && c0 < 0xDC00) {
    if (len < 2) {
      return 0xFFFD; // Surrogate, but length is only 1
    } else if (c1 >= 0xDC00 && c1 <= 0xDFFF) {
      return ((c0 & 0x03FF) << 10) + (c1 & 0x03FF) + 0x10000;
    }

    return 0xFFFD; // Unpaired surrogates
  }

  return c0;
}

static bool
initKeyEvent(PuglEventKey* event,
             PuglView*     view,
             bool          press,
             WPARAM        wParam,
             LPARAM        lParam,
             UINT          wmCharMsgType)
{
  POINT rpos = {0, 0};
  GetCursorPos(&rpos);

  POINT cpos = {rpos.x, rpos.y};
  ScreenToClient(view->impl->hwnd, &rpos);

  const unsigned scode = (uint32_t)((lParam & 0xFF0000) >> 16);
  const unsigned vkey =
    ((wParam == VK_SHIFT) ? MapVirtualKey(scode, MAPVK_VSC_TO_VK_EX)
                          : (unsigned)wParam);

  const unsigned vcode = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
  const unsigned kchar = MapVirtualKey(vkey, MAPVK_VK_TO_CHAR);
  const bool     dead  = kchar >> (sizeof(UINT) * 8 - 1) & 1;
  const bool     ext   = lParam & 0x01000000;

  event->type    = press ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
  event->time    = GetMessageTime() / 1e3;
  event->state   = getModifiers();
  event->xRoot   = rpos.x;
  event->yRoot   = rpos.y;
  event->x       = cpos.x;
  event->y       = cpos.y;
  event->keycode = (uint32_t)((lParam & 0xFF0000) >> 16);
  event->key     = 0;

  const PuglKey special = keySymToSpecial(vkey);
  if (special) {
    if (ext && (special == PUGL_KEY_CTRL || special == PUGL_KEY_ALT)) {
      event->key = (uint32_t)special + 1u; // Right hand key
    } else {
      event->key = (uint32_t)special;
    }
  } else if (!dead) {
    // Translate unshifted key
    BYTE    keyboardState[256] = {0};
    wchar_t buf[5]             = {0};

    event->key = puglDecodeUTF16(
      buf, ToUnicode(vkey, vcode, keyboardState, buf, 4, 1 << 2));
  }
  MSG msg;
  while (PeekMessageW(
    &msg, view->impl->hwnd, wmCharMsgType, wmCharMsgType, PM_REMOVE)) {
    const wchar_t utf16[2] = {msg.wParam & 0xFFFF, (msg.wParam >> 16) & 0xFFFF};
    char          bytes[8];
    int           len = WideCharToMultiByte(
      CP_UTF8, 0, utf16, utf16[1] ? 2 : 1, bytes, sizeof(bytes), NULL, NULL);
    if (len > 0) {
      char* newInput;
      if (event->inputLength + len > 8) {
        if (event->inputLength > 8) {
          newInput = realloc(event->input.ptr, event->inputLength + len);
        } else {
          newInput = malloc(event->inputLength + len);
          if (newInput)
            memcpy(newInput, event->input.data, event->inputLength);
        }
        if (newInput) {
          event->input.ptr = newInput;
        }
      } else {
        newInput = event->input.data;
      }
      if (newInput) {
        memcpy(newInput + event->inputLength, bytes, len);
        event->inputLength += len;
      }
    }
  }
  if (event->state == PUGL_MOD_ALT && event->key == PUGL_KEY_F4) {
    return false;
  } else {
    return true;
  }
}

static void
initCharEvent(PuglEventKey* event, PuglView* view, WPARAM wParam, LPARAM lParam)
{
  POINT rpos = {0, 0};
  GetCursorPos(&rpos);

  POINT cpos = {rpos.x, rpos.y};
  ScreenToClient(view->impl->hwnd, &rpos);

  const unsigned scode = (uint32_t)((lParam & 0xFF0000) >> 16);
  const unsigned vkey =
    ((wParam == VK_SHIFT) ? MapVirtualKey(scode, MAPVK_VSC_TO_VK_EX)
                          : (unsigned)wParam);

  const unsigned vcode = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
  const unsigned kchar = MapVirtualKey(vkey, MAPVK_VK_TO_CHAR);
  const bool     dead  = kchar >> (sizeof(UINT) * 8 - 1) & 1;
  const bool     ext   = lParam & 0x01000000;

  event->type    = PUGL_KEY_PRESS;
  event->time    = GetMessageTime() / 1e3;
  event->state   = getModifiers();
  event->xRoot   = rpos.x;
  event->yRoot   = rpos.y;
  event->x       = cpos.x;
  event->y       = cpos.y;
  event->keycode = (uint32_t)((lParam & 0xFF0000) >> 16);
  event->key     = 0;

  const wchar_t utf16[2] = {wParam & 0xFFFF, (wchar_t)(wParam >> 16) & 0xFFFF};
  int           len      = WideCharToMultiByte(
    CP_UTF8, 0, utf16, utf16[1] ? 2 : 1, event->input.data, 8, NULL, NULL);
  if (len > 0) {
    event->inputLength = len;
  } else {
    event->inputLength = 0;
  }
}

static bool
ignoreKeyEvent(PuglView* view, LPARAM lParam)
{
  return view->hints[PUGL_IGNORE_KEY_REPEAT] && (lParam & (1 << 30));
}

static RECT
handleConfigure(PuglView* view, PuglEvent* event)
{
  RECT rect;
  GetClientRect(view->impl->hwnd, &rect);
  MapWindowPoints(view->impl->hwnd,
                  view->parent ? (HWND)view->parent : HWND_DESKTOP,
                  (LPPOINT)&rect,
                  2);

  const LONG width  = rect.right - rect.left;
  const LONG height = rect.bottom - rect.top;

  view->frame.x = rect.left;
  view->frame.y = rect.top;

  event->configure.type   = PUGL_CONFIGURE;
  event->configure.x      = view->frame.x;
  event->configure.y      = view->frame.y;
  event->configure.width  = width;
  event->configure.height = height;

  if (view->frame.width != width || view->frame.height != height) {
    view->frame.width  = width;
    view->frame.height = height;
  }

  return rect;
}

static void
handleCrossing(PuglView* view, const PuglEventType type, POINT pos)
{
  POINT root_pos = pos;
  ClientToScreen(view->impl->hwnd, &root_pos);

  PuglEventCrossing ev = {
    type,
    0,
    GetMessageTime() / 1e3,
    (double)pos.x,
    (double)pos.y,
    (double)root_pos.x,
    (double)root_pos.y,
    getModifiers(),
    PUGL_CROSSING_NORMAL,
  };

  puglDispatchEvent(view, (PuglEvent*)&ev);
}

static void
constrainAspect(const PuglView* const view,
                RECT* const           size,
                const WPARAM          wParam)
{
  const float minA = (float)view->minAspectX / (float)view->minAspectY;
  const float maxA = (float)view->maxAspectX / (float)view->maxAspectY;
  const float w    = (float)(size->right - size->left);
  const float h    = (float)(size->bottom - size->top);
  const float a    = w / h;

  switch (wParam) {
  case WMSZ_TOP:
    size->top = (a < minA   ? (LONG)((float)size->bottom - w * minA)
                 : a > maxA ? (LONG)((float)size->bottom - w * maxA)
                            : size->top);
    break;
  case WMSZ_TOPRIGHT:
  case WMSZ_RIGHT:
  case WMSZ_BOTTOMRIGHT:
    size->right = (a < minA   ? (LONG)((float)size->left + h * minA)
                   : a > maxA ? (LONG)((float)size->left + h * maxA)
                              : size->right);
    break;
  case WMSZ_BOTTOM:
    size->bottom = (a < minA   ? (LONG)((float)size->top + w * minA)
                    : a > maxA ? (LONG)((float)size->top + w * maxA)
                               : size->bottom);
    break;
  case WMSZ_BOTTOMLEFT:
  case WMSZ_LEFT:
  case WMSZ_TOPLEFT:
    size->left = (a < minA   ? (LONG)((float)size->right - h * minA)
                  : a > maxA ? (LONG)((float)size->right - h * maxA)
                             : size->left);
    break;
  }
}

static LRESULT
handleMessage(PuglView* view, UINT message, WPARAM wParam, LPARAM lParam)
{
  PuglEvent   event     = {{PUGL_NOTHING, 0}};
  RECT        rect      = {0, 0, 0, 0};
  POINT       pt        = {0, 0};
  MINMAXINFO* mmi       = NULL;
  void*       dummy_ptr = NULL;

  if (InSendMessageEx(dummy_ptr)) {
    event.any.flags |= PUGL_IS_SEND_EVENT;
  }
  switch (message) {
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT) {
      SetCursor(view->impl->cursor);
      return true;
    } else {
      return DefWindowProcW(view->impl->hwnd, message, wParam, lParam);
    }
  case WM_MOVE:
    handleConfigure(view, &event);
    break;
  case WM_SHOWWINDOW:
    if (wParam) {
      handleConfigure(view, &event);
      puglDispatchEvent(view, &event);
      event.type = PUGL_NOTHING;

      RedrawWindow(view->impl->hwnd,
                   NULL,
                   NULL,
                   RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_INTERNALPAINT);
    }

    if ((bool)wParam != view->visible) {
      view->visible  = wParam;
      event.any.type = wParam ? PUGL_MAP : PUGL_UNMAP;
    }
    break;
  case WM_SIZE:
    handleConfigure(view, &event);
    InvalidateRect(view->impl->hwnd, NULL, false);
    break;
  case WM_SIZING:
    if (view->minAspectX) {
      constrainAspect(view, (RECT*)lParam, wParam);
      return TRUE;
    }
    break;
  case WM_ENTERSIZEMOVE:
  case WM_ENTERMENULOOP:
    puglDispatchSimpleEvent(view, PUGL_LOOP_ENTER);
    break;
  case WM_TIMER:
    break;
  case WM_EXITSIZEMOVE:
  case WM_EXITMENULOOP:
    puglDispatchSimpleEvent(view, PUGL_LOOP_LEAVE);
    break;
  case WM_GETMINMAXINFO: {
    RECT rcClient, rcWind;
    GetClientRect(view->impl->hwnd, &rcClient);
    GetWindowRect(view->impl->hwnd, &rcWind);
    int dx                = (rcWind.right - rcWind.left) - rcClient.right;
    int dy                = (rcWind.bottom - rcWind.top) - rcClient.bottom;
    mmi                   = (MINMAXINFO*)lParam;
    mmi->ptMinTrackSize.x = view->minWidth + dx;
    mmi->ptMinTrackSize.y = view->minHeight + dy;
    if (view->maxWidth > 0) {
      mmi->ptMaxTrackSize.x = view->maxWidth + dx;
    }
    if (view->maxHeight > 0) {
      mmi->ptMaxTrackSize.y = view->maxHeight + dy;
    }
    break;
  }
  case WM_PAINT: {
    GetUpdateRect(view->impl->hwnd, &rect, false);
    event.expose.type   = PUGL_EXPOSE;
    event.expose.x      = rect.left;
    event.expose.y      = rect.top;
    event.expose.width  = rect.right - rect.left;
    event.expose.height = rect.bottom - rect.top;
    event.expose.count  = 0;

    if (!view->impl->updateRegion) {
      view->impl->updateRegion = CreateRectRgn(0, 0, 0, 0);
      if (!view->impl->updateRegion)
        goto fallback;
    }

    HRGN region = view->impl->updateRegion;
    if (GetUpdateRgn(view->impl->hwnd, region, false) == ERROR)
      goto fallback;

    DWORD bytesCount = GetRegionData(region, 0, NULL);
    if (bytesCount > view->impl->updateRegionLength) {
      RGNDATA* newData = realloc(view->impl->updateRegionData, bytesCount);
      if (!newData)
        goto fallback;
      view->impl->updateRegionData   = newData;
      view->impl->updateRegionLength = bytesCount;
    }
    RGNDATA* data = view->impl->updateRegionData;
    if (GetRegionData(region, bytesCount, data) == 0)
      goto fallback;

    DWORD count = data->rdh.nCount;
    if (data->rdh.iType != RDH_RECTANGLES || count <= 0)
      goto fallback;

    PuglInternals* impl  = view->impl;
    RECT*          rects = (RECT*)&data->Buffer;

    if (!puglRectsInit(&view->rects, count)) {
      goto fallback;
    }
    for (int i = 0; i < count; ++i) {
      PuglRect* r = view->rects.rectsList + i;
      r->x        = rects[i].left;
      r->y        = rects[i].top;
      r->width    = rects[i].right - rects[i].left;
      r->height   = rects[i].bottom - rects[i].top;
    }
    view->rects.rectsCount = count;
    view->backend->enter(view, &event.expose, &view->rects);
    if (view->hints[PUGL_DONT_MERGE_RECTS] && count > 0) {
      for (int i = 0; i < count; ++i) {
        PuglRect*       r = view->rects.rectsList + i;
        PuglEventExpose e = {
          PUGL_EXPOSE, 0, r->x, r->y, r->width, r->height, count - 1 - i};
        puglDispatchEventInContext(view, (PuglEvent*)&e);
      }
    } else {
      puglDispatchEventInContext(view, &event);
    }
    view->backend->leave(view, &event.expose, &view->rects);
    view->rects.rectsCount = 0;
    return 0;
  fallback:
    view->backend->enter(view, &event.expose, NULL);
    puglDispatchEventInContext(view, &event);
    view->backend->leave(view, &event.expose, NULL);
    return 0;
  }
  case WM_ERASEBKGND:
    return true;
  case WM_MOUSEACTIVATE:
    if (!view->parent && view->transientParent && view->hints[PUGL_IS_POPUP]) {
      return MA_NOACTIVATE;
    } else {
      return DefWindowProcW(view->impl->hwnd, message, wParam, lParam);
    }
    break;
  case WM_MOUSEMOVE:
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);

    if (!view->impl->mouseTracked) {
      TRACKMOUSEEVENT tme = {0};

      tme.cbSize    = sizeof(tme);
      tme.dwFlags   = TME_LEAVE;
      tme.hwndTrack = view->impl->hwnd;
      TrackMouseEvent(&tme);

      handleCrossing(view, PUGL_POINTER_IN, pt);
      view->impl->mouseTracked = true;
    }

    ClientToScreen(view->impl->hwnd, &pt);
    event.motion.type  = PUGL_MOTION;
    event.motion.time  = GetMessageTime() / 1e3;
    event.motion.x     = GET_X_LPARAM(lParam);
    event.motion.y     = GET_Y_LPARAM(lParam);
    event.motion.xRoot = pt.x;
    event.motion.yRoot = pt.y;
    event.motion.state = getModifiers();
    break;
  case WM_MOUSELEAVE:
    GetCursorPos(&pt);
    ScreenToClient(view->impl->hwnd, &pt);
    handleCrossing(view, PUGL_POINTER_OUT, pt);
    view->impl->mouseTracked = false;
    break;
  case WM_LBUTTONDOWN:
    initMouseEvent(&event, view, 1, true, lParam);
    break;
  case WM_MBUTTONDOWN:
    initMouseEvent(&event, view, 2, true, lParam);
    break;
  case WM_RBUTTONDOWN:
    initMouseEvent(&event, view, 3, true, lParam);
    break;
  case WM_LBUTTONUP:
    initMouseEvent(&event, view, 1, false, lParam);
    break;
  case WM_MBUTTONUP:
    initMouseEvent(&event, view, 2, false, lParam);
    break;
  case WM_RBUTTONUP:
    initMouseEvent(&event, view, 3, false, lParam);
    break;
  case WM_MOUSEWHEEL:
    initScrollEvent(&event, view, lParam);
    event.scroll.dy = GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA;
    event.scroll.direction =
      (event.scroll.dy > 0 ? PUGL_SCROLL_UP : PUGL_SCROLL_DOWN);
    break;
  case WM_MOUSEHWHEEL:
    initScrollEvent(&event, view, lParam);
    event.scroll.dx = -GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA;
    event.scroll.direction =
      (event.scroll.dx > 0 ? PUGL_SCROLL_LEFT : PUGL_SCROLL_RIGHT);
    break;
  case WM_KEYDOWN:
    if (!ignoreKeyEvent(view, lParam)) {
      initKeyEvent(&event.key, view, true, wParam, lParam, WM_CHAR);
    }
    break;
  case WM_KEYUP:
    initKeyEvent(&event.key, view, false, wParam, lParam, WM_CHAR);
    break;
  case WM_CHAR:
  case WM_IME_CHAR:
    initCharEvent(&event.key, view, wParam, lParam);
    break;
  case WM_SETFOCUS:
    event.type = PUGL_FOCUS_IN;
    break;
  case WM_KILLFOCUS:
    event.type = PUGL_FOCUS_OUT;
    break;
  case WM_SYSKEYDOWN: {
    if (!initKeyEvent(&event.key, view, true, wParam, lParam, WM_SYSCHAR)) {
      event.type = PUGL_NOTHING;
      DefWindowProcW(view->impl->hwnd, message, wParam, lParam);
    }
    break;
  }
  case WM_SYSKEYUP:
    if (!initKeyEvent(&event.key, view, false, wParam, lParam, WM_SYSCHAR)) {
      event.type = PUGL_NOTHING;
      DefWindowProcW(view->impl->hwnd, message, wParam, lParam);
    }
    break;
  case WM_SYSCHAR:
    return TRUE;
  case PUGL_LOCAL_CLIENT_MSG:
    event.client.type  = PUGL_CLIENT;
    event.client.data1 = (uintptr_t)wParam;
    event.client.data2 = (uintptr_t)lParam;
    break;
  case WM_QUIT:
  case PUGL_LOCAL_CLOSE_MSG:
    event.type = PUGL_CLOSE;
    break;
  case WM_DESTROY:
    event.type = PUGL_MUST_FREE;
    break;
  case PUGL_LOCAL_CLIPBD_MSG:
    if (view->clipboard.data) {
      event.received.type  = PUGL_DATA_RECEIVED;
      event.received.data  = view->clipboard.data;
      event.received.len   = view->clipboard.len;
      view->clipboard.data = NULL;
      view->clipboard.len  = 0;
    }
    break;
  default:
    return DefWindowProcW(view->impl->hwnd, message, wParam, lParam);
  }

  puglDispatchEvent(view, &event);

  if (event.type == PUGL_MUST_FREE &&
      view == (PuglView*)GetWindowLongPtr(view->impl->hwnd, GWLP_USERDATA)) {
    fprintf(stderr,
            "error: puglFreeView() should have been called in response to "
            "PUGL_MUST_FREE event.\n");
    abort();
  }

  return 0;
}

PuglStatus
puglGrabFocus(PuglView* view)
{
  SetFocus(view->impl->hwnd);
  return PUGL_SUCCESS;
}

bool
puglHasFocus(const PuglView* view)
{
  return GetFocus() == view->impl->hwnd;
}

PuglStatus
puglRequestAttention(PuglView* view)
{
  FLASHWINFO info = {
    sizeof(FLASHWINFO), view->impl->hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 1, 0};

  FlashWindowEx(&info);

  return PUGL_SUCCESS;
}

void
puglSetProcessFunc(PuglWorld*      world,
                   PuglProcessFunc processFunc,
                   void*           userData)
{
  world->processFunc     = processFunc;
  world->processUserData = userData;
}

static void CALLBACK
processTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
  PuglWorld* world = (PuglWorld*)lpParameter;
  if (world->impl->pseudoWin) {
    PostMessage(world->impl->pseudoWin, PUGL_LOCAL_TIMER_MSG, 0, 0);
  }
}

static void
deleteProcessTimer(PuglWorld* world)
{
  if (world->impl->processTimer) {
    DeleteTimerQueueTimer(NULL, world->impl->processTimer, NULL);
    world->impl->processTimer = 0;
  }
}

void
puglSetNextProcessTime(PuglWorld* world, double seconds)
{
  if (!world->impl->initialized && !puglInitWorldInternals2(world)) {
    return;
  }
  if (seconds >= 0) {
    deleteProcessTimer(world);
    world->impl->nextProcessTime = puglGetTime(world) + seconds;
    CreateTimerQueueTimer(&world->impl->processTimer,
                          NULL,
                          processTimerCallback,
                          world,
                          (DWORD)(seconds * 1000),
                          0,
                          WT_EXECUTEINTIMERTHREAD);
  } else {
    world->impl->nextProcessTime = -1;
    deleteProcessTimer(world);
  }
}

PuglStatus
puglSendEvent(PuglView* view, const PuglEvent* event)
{
  if (event->type == PUGL_CLIENT) {
    PostMessage(view->impl->hwnd,
                PUGL_LOCAL_CLIENT_MSG,
                (WPARAM)event->client.data1,
                (LPARAM)event->client.data2);

    return PUGL_SUCCESS;
  }

  return PUGL_UNSUPPORTED_TYPE;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglWaitForEvent(PuglView* PUGL_UNUSED(view))
{
  WaitMessage();
  return PUGL_SUCCESS;
}
#endif

static PuglStatus
puglDispatchWinEvents(PuglWorld* PUGL_UNUSED(world))
{
  /* Windows has no facility to process only currently queued messages, which
     causes the event loop to run forever in cases like mouse movement where
     the queue is constantly being filled with new messages.  To work around
     this, we post a message to ourselves before starting, record its time
     when it is received, then break the loop on the first message that was
     created afterwards. */

  PostMessage(NULL, PUGL_LOCAL_MARK_MSG, 0, 0);
  
  bool hadEvents = false;
  long markTime = 0;
  MSG  msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == PUGL_LOCAL_MARK_MSG) {
      markTime = GetMessageTime();
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      hadEvents = true;
      if (markTime != 0 && GetMessageTime() > markTime) {
        break;
      }
    }
  }

  return hadEvents ? PUGL_SUCCESS : PUGL_FAILURE;
}

PuglStatus
puglUpdate(PuglWorld* world, double timeout)
{
  const double startTime = puglGetTime(world);
  PuglStatus   st        = PUGL_SUCCESS;

  if (timeout < 0.0) {
    bool isFirst = true;
  again:
    st = puglPollWinEvents(world, timeout, isFirst);
    if (st == PUGL_SUCCESS) {
      st = puglDispatchWinEvents(world);
      if (st == PUGL_FAILURE) { // noEvents
        isFirst = false; 
        goto again; 
      }
    }
  } else if (timeout == 0.0) {
    st = puglDispatchWinEvents(world);
  } else {
    const double endTime = startTime + timeout - 0.001;
    bool isFirst = true;
    for (double t = startTime; t < endTime; t = puglGetTime(world)) {
      if (st = puglPollWinEvents(world, endTime - t, isFirst)) {
        break;
      }
      st = puglDispatchWinEvents(world);
      if (st == PUGL_SUCCESS || st != PUGL_FAILURE) {
        // PUGL_SUCCESS=hadEvents, PUGL_FAILURE=noEvents
        break;
      }
      isFirst = false;
    }
  }
  // Disabled because we always are returning if events have been processed
  /*for (size_t i = 0; i < world->numViews; ++i) {
    if (world->views[i]->visible) {
      puglDispatchSimpleEvent(world->views[i], PUGL_UPDATE);
    }

    UpdateWindow(world->views[i]->impl->hwnd);
  }*/

  return st;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglProcessEvents(PuglView* view)
{
  return puglUpdate(view->world, 0.0);
}
#endif

LRESULT CALLBACK
worldWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PuglWorld* world = (PuglWorld*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (message) {
  case PUGL_LOCAL_TIMER_MSG:
    if (world && hwnd == world->impl->pseudoWin) {
      PuglWorldInternals* impl    = world->impl;
      double              current = puglGetTime(world);
      if (impl->nextProcessTime >= 0 && impl->nextProcessTime <= current) {
        impl->nextProcessTime = -1;
        deleteProcessTimer(world);
        if (world->processFunc)
          world->processFunc(world, world->processUserData);
      } else if (impl->nextProcessTime >= 0) {
        double seconds = impl->nextProcessTime - current;
        deleteProcessTimer(world);
        CreateTimerQueueTimer(&world->impl->processTimer,
                              NULL,
                              processTimerCallback,
                              world,
                              (DWORD)(seconds * 1000),
                              0,
                              WT_EXECUTEINTIMERTHREAD);
      } else {
        deleteProcessTimer(world);
      }
    }
    break;
  case PUGL_LOCAL_AWAKE_MSG:
    if (world && hwnd == world->impl->pseudoWin) {
      if (world->processFunc)
        world->processFunc(world, world->processUserData);
    }
    break;
  default:
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }
}

LRESULT CALLBACK
wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PuglView* view = (PuglView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (message) {
  case WM_CREATE:
    return 0;
  case WM_CLOSE:
    PostMessage(hwnd, PUGL_LOCAL_CLOSE_MSG, wParam, lParam);
    return 0;
  default:
    if (view && hwnd == view->impl->hwnd) {
      return handleMessage(view, message, wParam, lParam);
    } else {
      return DefWindowProcW(hwnd, message, wParam, lParam);
    }
  }
}

double
puglGetTime(const PuglWorld* world)
{
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return ((double)count.QuadPart / world->impl->timerFrequency -
          world->startTime);
}

PuglStatus
puglPostRedisplay(PuglView* view)
{
  InvalidateRect(view->impl->hwnd, NULL, false);
  return PUGL_SUCCESS;
}

PuglStatus
puglPostRedisplayRect(PuglView* view, const PuglRect rect)
{
  const RECT r = {(long)floor(rect.x),
                  (long)floor(rect.y),
                  (long)ceil(rect.x + rect.width),
                  (long)ceil(rect.y + rect.height)};

  InvalidateRect(view->impl->hwnd, &r, false);

  return PUGL_SUCCESS;
}

PuglNativeView
puglGetNativeWindow(PuglView* view)
{
  return (PuglNativeView)view->impl->hwnd;
}

PuglStatus
puglSetWindowTitle(PuglView* view, const char* title)
{
  puglSetString(&view->title, title);

  if (view->impl->hwnd) {
    wchar_t* wtitle = puglUtf8ToWideChar(title);
    if (wtitle) {
      SetWindowTextW(view->impl->hwnd, wtitle);
      free(wtitle);
    }
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetFrame(PuglView* view, const PuglRect frame)
{
  view->reqX      = (int)frame.x;
  view->reqY      = (int)frame.y;
  view->reqWidth  = (int)frame.width;
  view->reqHeight = (int)frame.height;

  if (view->impl->hwnd) {
    RECT rect = {(long)frame.x,
                 (long)frame.y,
                 (long)frame.x + (long)frame.width,
                 (long)frame.y + (long)frame.height};

    AdjustWindowRectEx(
      &rect, puglWinGetWindowFlags(view), FALSE, puglWinGetWindowExFlags(view));

    if (!SetWindowPos(view->impl->hwnd,
                      HWND_TOP,
                      rect.left,
                      rect.top,
                      rect.right - rect.left,
                      rect.bottom - rect.top,
                      SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER)) {
      return PUGL_UNKNOWN_ERROR;
    }
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetSize(PuglView* view, int width, int height)
{
  view->reqWidth  = width;
  view->reqHeight = height;

  if (view->impl->hwnd) {
    RECT rect = {(long)view->frame.x,
                 (long)view->frame.y,
                 (long)view->frame.x + width,
                 (long)view->frame.y + height};

    AdjustWindowRectEx(
      &rect, puglWinGetWindowFlags(view), FALSE, puglWinGetWindowExFlags(view));

    if (!SetWindowPos(
          view->impl->hwnd,
          HWND_TOP,
          rect.left,
          rect.top,
          rect.right - rect.left,
          rect.bottom - rect.top,
          (SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER))) {
      return PUGL_UNKNOWN_ERROR;
    }
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetMinSize(PuglView* const view, const int width, const int height)
{
  view->minWidth  = width;
  view->minHeight = height;

  PuglRect r = puglGetFrame(view);
  if (r.width < width || r.height < height) {
    int w = (r.width  > width)  ? r.width  : width;
    int h = (r.height > height) ? r.height : height;
    puglSetSize(view, w, h);
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglSetMaxSize(PuglView* const view, const int width, const int height)
{
  view->maxWidth  = width;
  view->maxHeight = height;
  PuglRect r = puglGetFrame(view);
  if ((width > 0 && r.width > width) || (height > 0 && r.height > height)) {
    int w = (width  <= 0 || r.width  < width)  ? r.width  : width;
    int h = (height <= 0 || r.height < height) ? r.height : height;
    puglSetSize(view, w, h);
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglSetAspectRatio(PuglView* const view,
                   const int       minX,
                   const int       minY,
                   const int       maxX,
                   const int       maxY)
{
  view->minAspectX = minX;
  view->minAspectY = minY;
  view->maxAspectX = maxX;
  view->maxAspectY = maxY;
  return PUGL_SUCCESS;
}

PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeView parent)
{
  if (!view->impl->hwnd) {
    view->transientParent = parent;
    return PUGL_SUCCESS;
  } else {
    return PUGL_FAILURE;
  }
}

PuglStatus
puglRequestClipboard(PuglView* view)
{
  PuglInternals* const impl = view->impl;

  if (!IsClipboardFormatAvailable(CF_UNICODETEXT) ||
      !OpenClipboard(impl->hwnd)) {
    return PUGL_FAILURE;
  }

  HGLOBAL  mem  = GetClipboardData(CF_UNICODETEXT);
  wchar_t* wstr = mem ? (wchar_t*)GlobalLock(mem) : NULL;
  if (!wstr) {
    CloseClipboard();
    return PUGL_FAILURE;
  }

  free(view->clipboard.data);
  view->clipboard.data = puglWideCharToUtf8(wstr, &view->clipboard.len);
  GlobalUnlock(mem);
  CloseClipboard();

  PostMessage(impl->hwnd, PUGL_LOCAL_CLIPBD_MSG, 0, 0);

  return PUGL_SUCCESS;
}

PuglStatus
puglSetClipboard(PuglWorld*  world,
                 const char* type,
                 const void* data,
                 size_t      len)
{
  PuglWorldInternals* const impl = world->impl;

  if (!impl->initialized && !puglInitWorldInternals2(world)) {
    return PUGL_FAILURE;
  }

  PuglStatus st = puglSetInternalClipboard(world, type, data, len);
  if (st) {
    return st;
  } else if (!OpenClipboard(impl->pseudoWin)) {
    return PUGL_UNKNOWN_ERROR;
  }

  // Measure string and allocate global memory for clipboard
  const char* str  = (const char*)data;
  const int   wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
  HGLOBAL     mem =
    GlobalAlloc(GMEM_MOVEABLE, (size_t)(wlen + 1) * sizeof(wchar_t));
  if (!mem) {
    CloseClipboard();
    return PUGL_UNKNOWN_ERROR;
  }

  // Lock global memory
  wchar_t* wstr = (wchar_t*)GlobalLock(mem);
  if (!wstr) {
    GlobalFree(mem);
    CloseClipboard();
    return PUGL_UNKNOWN_ERROR;
  }

  // Convert string into global memory and set it as clipboard data
  MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wstr, wlen);
  wstr[wlen] = 0;
  GlobalUnlock(mem);
  SetClipboardData(CF_UNICODETEXT, mem);
  CloseClipboard();
  return PUGL_SUCCESS;
}

bool
puglHasClipboard(PuglWorld* world)
{
  return false;
}

static const char* const cursor_ids[] = {
  IDC_ARROW,  // ARROW
  IDC_IBEAM,  // CARET
  IDC_CROSS,  // CROSSHAIR
  IDC_HAND,   // HAND
  IDC_NO,     // NO
  IDC_SIZEWE, // LEFT_RIGHT
  IDC_SIZENS, // UP_DOWN
  NULL,       // HIDDEN
};

PuglStatus
puglSetCursor(PuglView* view, PuglCursor cursor)
{
  PuglInternals* const impl  = view->impl;
  const unsigned       index = (unsigned)cursor;
  const unsigned       count = sizeof(cursor_ids) / sizeof(cursor_ids[0]);

  if (index >= count) {
    return PUGL_BAD_PARAMETER;
  }
  const char* id  = cursor_ids[index];
  HCURSOR     cur = NULL;
  if (id != NULL) {
    cur = LoadCursor(NULL, id);
    if (!cur) {
      return PUGL_FAILURE;
    }
  }

  impl->cursor = cur;
  if (impl->mouseTracked) {
    SetCursor(cur);
  }

  return PUGL_SUCCESS;
}

double
puglGetScreenScale(PuglView* view)
{
  PuglWorldInternals* worldImpl = view->world->impl;
  if (!worldImpl->triedDpiForMonitor) {
    worldImpl->triedDpiForMonitor = true;
    worldImpl->getDpiForMonitor =
      (GetDpi_F)GetProcAddress(LoadLibrary("Shcore.DLL"), "GetDpiForMonitor");
  }
  if (worldImpl->getDpiForMonitor) {
    HMONITOR mon =
      MonitorFromWindow(view->impl->hwnd, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX, dpiY;
    if (worldImpl->getDpiForMonitor(mon, 0, &dpiX, &dpiY) == S_OK) {
      double dpi  = (dpiX + dpiY) / 2;
      double rslt = ((int)((dpi / 96) * 100 + 0.5)) / (double)100;
      if (rslt > 0) {
        return rslt;
      }
    }
  }
  return 1.0;
}

double
puglGetDefaultScreenScale(PuglWorld* world)
{
  PuglWorldInternals* worldImpl = world->impl;
  if (!worldImpl->triedDpiForMonitor) {
    worldImpl->triedDpiForMonitor = true;
    worldImpl->getDpiForMonitor =
      (GetDpi_F)GetProcAddress(LoadLibrary("Shcore.DLL"), "GetDpiForMonitor");
  }
  if (worldImpl->getDpiForMonitor) {
    HMONITOR mon =
      MonitorFromWindow(worldImpl->pseudoWin, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX, dpiY;
    if (worldImpl->getDpiForMonitor(mon, 0, &dpiX, &dpiY) == S_OK) {
      double dpi  = (dpiX + dpiY) / 2;
      double rslt = ((int)((dpi / 96) * 100 + 0.5)) / (double)100;
      if (rslt > 0) {
        return rslt;
      }
    }
  }
  return 1.0;
}
