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

/**
   @file x11.h
   @brief Shared definitions for X11 implementation.
*/
#ifndef PUGL_DETAIL_IMPLEMENTATION_X11_H
#define PUGL_DETAIL_IMPLEMENTATION_X11_H

#include "pugl/detail/types.h"
#include "pugl/pugl.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	Atom CLIPBOARD;
	Atom TARGETS;
	Atom INCR;
	Atom UTF8_STRING;
	Atom WM_PROTOCOLS;
	Atom WM_DELETE_WINDOW;
	Atom PUGL_CLIENT_MSG;
	Atom NET_WM_NAME;
	Atom NET_WM_STATE;
	Atom NET_WM_STATE_DEMANDS_ATTENTION;
} PuglX11Atoms;

typedef struct PuglX11IncrTarget {
	Window       win;
	Atom         prop;
	Atom         type;
	size_t       pos;
} PuglX11IncrTarget;

struct PuglWorldInternalsImpl {
	Display*     display;
	PuglX11Atoms atoms;
	XIM          xim;
	Window       pseudoWin;
	PuglX11IncrTarget incrTarget;
	double       nextProcessTime;
	bool         needsProcessing;
	int          awake_fds[2];
	bool         dispatchingEvents;
	int          shiftKeyStates;
	int          controlKeyStates;
	int          altKeyStates;
	int          altgrKeyStates;
	int          superKeyStates;
};

struct PuglInternalsImpl {
	Display*     display;
	XVisualInfo* vi;
	Window       win;
	XIC          xic;
	PuglSurface* surface;
	PuglEvent    pendingConfigure;
	PuglEvent    pendingExpose;
	int          screen;
	unsigned     cursorShape;
	int          clipboardRequested;
	bool         incrClipboardRequest;
	bool         displayed;
	bool         posRequested;
};

static inline PuglStatus
puglX11StubConfigure(PuglView* view)
{
	PuglInternals* const impl = view->impl;
	XVisualInfo          pat  = {0};
	int                  n    = 0;

	pat.screen = impl->screen;
	impl->vi   = XGetVisualInfo(impl->display, VisualScreenMask, &pat, &n);

	return PUGL_SUCCESS;
}

#endif // PUGL_DETAIL_IMPLEMENTATION_X11_H
