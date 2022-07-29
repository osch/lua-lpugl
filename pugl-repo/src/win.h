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

#ifndef PUGL_DETAIL_WIN_H
#define PUGL_DETAIL_WIN_H

#include "implementation.h"

#include <windows.h>

#include <stdbool.h>

typedef PIXELFORMATDESCRIPTOR PuglWinPFD;

typedef HRESULT(WINAPI* GetDpi_F)(HMONITOR, int, UINT*, UINT*);

struct PuglWorldInternalsImpl {
  bool     initialized;
  wchar_t* worldClassName;
  wchar_t* windowClassName;
  wchar_t* popupClassName;
  GetDpi_F getDpiForMonitor;
  bool     triedDpiForMonitor;
  HWND     pseudoWin;
  double   timerFrequency;
  double   nextProcessTime;
  HANDLE   processTimer;
};

struct PuglInternalsImpl {
  PuglWinPFD   pfd;
  int          pfId;
  HWND         hwnd;
  HCURSOR      cursor;
  HDC          hdc;
  HRGN         updateRegion;
  RGNDATA*     updateRegionData;
  DWORD        updateRegionLength;
  PuglSurface* surface;
  bool         flashing;
  bool         mouseTracked;
  bool         hasBeginPaint;
  bool         posRequested;
};

static wchar_t*
puglUtf8ToWideChar(const char* const utf8)
{
  const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (len > 0) {
    wchar_t* result = (wchar_t*)calloc((size_t)len, sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result, len);
    return result;
  }

  return NULL;
}

static inline PuglWinPFD
puglWinGetPixelFormatDescriptor(const PuglHints hints)
{
  const int rgbBits = (hints[PUGL_RED_BITS] +   //
                       hints[PUGL_GREEN_BITS] + //
                       hints[PUGL_BLUE_BITS]);

  const DWORD dwFlags = hints[PUGL_DOUBLE_BUFFER] ? PFD_DOUBLEBUFFER : 0u;

  PuglWinPFD pfd;
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize        = sizeof(pfd);
  pfd.nVersion     = 1;
  pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | dwFlags;
  pfd.iPixelType   = PFD_TYPE_RGBA;
  pfd.cColorBits   = (BYTE)rgbBits;
  pfd.cRedBits     = (BYTE)hints[PUGL_RED_BITS];
  pfd.cGreenBits   = (BYTE)hints[PUGL_GREEN_BITS];
  pfd.cBlueBits    = (BYTE)hints[PUGL_BLUE_BITS];
  pfd.cAlphaBits   = (BYTE)hints[PUGL_ALPHA_BITS];
  pfd.cDepthBits   = (BYTE)hints[PUGL_DEPTH_BITS];
  pfd.cStencilBits = (BYTE)hints[PUGL_STENCIL_BITS];
  pfd.iLayerType   = PFD_MAIN_PLANE;
  return pfd;
}

static inline unsigned
puglWinGetWindowFlags(const PuglView* const view)
{
  const bool resizable = view->hints[PUGL_RESIZABLE];
  const bool isPopup   = view->hints[PUGL_IS_POPUP];

  return (WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
          (view->parent
             ? WS_CHILD
             //	         : (WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX |
             : ((isPopup ? (WS_POPUP)
                         : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                            WS_MINIMIZEBOX)) |
                (resizable ? (WS_SIZEBOX | WS_MAXIMIZEBOX) : 0u))));
}

static inline unsigned
puglWinGetWindowExFlags(const PuglView* const view)
{
  const bool isPopup = view->hints[PUGL_IS_POPUP];
  return WS_EX_NOINHERITLAYOUT | WS_EX_WINDOWEDGE |
         ((!view->parent && !view->transientParent && !isPopup)
            ? WS_EX_APPWINDOW
            : 0u) |
         ((!view->parent && view->transientParent && !isPopup)
            ? WS_EX_TOOLWINDOW
            : 0u) |
         ((!view->parent && view->transientParent && isPopup) ? WS_EX_NOACTIVATE
                                                              : 0u);
}

static inline PuglStatus
puglWinCreateWindow(PuglView* const   view,
                    const char* const title,
                    HWND* const       hwnd,
                    HDC* const        hdc)
{
  const wchar_t* className  = view->world->impl->windowClassName;
  const unsigned winFlags   = puglWinGetWindowFlags(view);
  const unsigned winExFlags = puglWinGetWindowExFlags(view);

  if (view->hints[PUGL_IS_POPUP]) {
    className = view->world->impl->popupClassName;
  }

  // The meaning of "parent" depends on the window type (WS_CHILD)
  HWND parent;
  if (view->parent) {
    parent = (HWND)view->parent;
  } else if (view->transientParent) {
    parent = (HWND)view->transientParent;
  } else {
    parent = HWND_DESKTOP;
  }

  // Calculate total window size to accommodate requested view size
  RECT wr = {(long)view->reqX,
             (long)view->reqY,
             (long)view->reqX + (long)view->reqWidth,
             (long)view->reqY + (long)view->reqHeight};
  AdjustWindowRectEx(&wr, winFlags, FALSE, winExFlags);

  wchar_t* titleW = puglUtf8ToWideChar(title);

  // Create window and get drawing context
  if (!titleW) {
    free(titleW);
    return PUGL_FAILURE;
  }
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  if (view->impl->posRequested) {
    x = view->reqX;
    y = view->reqY;
    view->impl->posRequested = false;
  }
  if (!(*hwnd = CreateWindowExW(winExFlags,
                                className,
                                titleW,
                                winFlags,
                                x,
                                y,
                                wr.right - wr.left,
                                wr.bottom - wr.top,
                                parent,
                                NULL,
                                NULL,
                                NULL))) {
    free(titleW);
    return PUGL_REALIZE_FAILED;
  } else if (!(*hdc = GetDC(*hwnd))) {
    DestroyWindow(*hwnd);
    *hwnd = NULL;
    free(titleW);
    return PUGL_REALIZE_FAILED;
  }

  free(titleW);
  return PUGL_SUCCESS;
}

PUGL_API_PRIVATE PuglStatus
puglWinStubConfigure(PuglView* view);

PUGL_API_PRIVATE PuglStatus
puglWinStubEnter(PuglView* view, const PuglEventExpose* expose, PuglRects* rects);

PUGL_API_PRIVATE PuglStatus
puglWinStubLeave(PuglView* view, const PuglEventExpose* expose, PuglRects* rects);

#endif // PUGL_DETAIL_WIN_H
