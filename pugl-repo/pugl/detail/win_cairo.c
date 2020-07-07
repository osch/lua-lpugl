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
   @file win_cairo.c
   @brief Cairo graphics backend for Windows.
*/

#include "pugl/detail/stub.h"
#include "pugl/detail/types.h"
#include "pugl/detail/win.h"
#include "pugl/pugl_cairo.h"

#include <cairo-win32.h>
#include <cairo.h>

#include <stdlib.h>

typedef struct  {
	cairo_surface_t* crSurface;
	cairo_t*         crContext;
	bool             hasBeginPaint;
} PuglWinCairoSurface;

static void
puglWinCairoClose(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	if (surface) {
		if (surface->crContext) {
			cairo_destroy(surface->crContext);
			surface->crContext = NULL;
		}
		if (surface->crSurface) {
			cairo_surface_destroy(surface->crSurface);
			surface->crSurface = NULL;
		}
	}
}

static PuglStatus 
puglWinCairoOpen(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	puglWinCairoClose(view); // just to be sure
	
	surface->crSurface = cairo_win32_surface_create(impl->hdc);
	
	if (surface->crSurface) {
		surface->crContext = cairo_create(surface->crSurface);
	}
	if (surface->crContext) {
		return PUGL_SUCCESS;
	} else {
		puglWinCairoClose(view);
		return PUGL_CREATE_CONTEXT_FAILED;
	}
}

static PuglStatus
puglWinCairoCreate(PuglView* view)
{
	PuglInternals* const impl    = view->impl;

	impl->surface = calloc(1, sizeof(PuglWinCairoSurface));

	return PUGL_SUCCESS;
}

static PuglStatus
puglWinCairoDestroy(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	puglWinCairoClose(view);
	free(surface);
	impl->surface = NULL;

	return PUGL_SUCCESS;
}

static PuglStatus
puglWinCairoEnter(PuglView* view, const PuglEventExpose* expose)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	if (!expose) {
	    return PUGL_SUCCESS;
	}
	else if (view->hints[PUGL_DONT_MERGE_RECTS] && expose->count > 0) {
            surface->hasBeginPaint = false;
        } else {
	    PAINTSTRUCT ps;
	    BeginPaint(view->impl->hwnd, &ps);
            surface->hasBeginPaint = true;
	}
	return PUGL_SUCCESS;
}

static PuglStatus
puglWinCairoLeave(PuglView* view, const PuglEventExpose* expose)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	if (expose && surface->crContext) {
		cairo_pop_group_to_source(surface->crContext);
		cairo_paint(surface->crContext);
		if (surface->hasBeginPaint) {
		    PAINTSTRUCT ps;
		    EndPaint(impl->hwnd, &ps);
		    surface->hasBeginPaint = false;
		}
	}
	puglWinCairoClose(view);

	return PUGL_SUCCESS;
}

static void*
puglWinCairoGetContext(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglWinCairoSurface* const surface = (PuglWinCairoSurface*)impl->surface;

	if (!surface->crContext) {
		puglWinCairoOpen(view);
		if (surface->crContext) {
			cairo_push_group_with_content(surface->crContext, CAIRO_CONTENT_COLOR_ALPHA);
		}
	}
	return surface->crContext;
}

const PuglBackend*
puglCairoBackend()
{
	static const PuglBackend backend = {puglWinStubConfigure,
	                                    puglWinCairoCreate,
	                                    puglWinCairoDestroy,
	                                    puglWinCairoEnter,
	                                    puglWinCairoLeave,
	                                    puglWinCairoGetContext};

	return &backend;
}
