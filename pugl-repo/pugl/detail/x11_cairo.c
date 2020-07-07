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
   @file x11_cairo.c
   @brief Cairo graphics backend for X11.
*/

#include "pugl/detail/types.h"
#include "pugl/detail/x11.h"
#include "pugl/pugl.h"
#include "pugl/pugl_cairo.h"

#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>

#include <stdlib.h>

typedef struct  {
	cairo_surface_t* crSurface;
	cairo_t*         crContext;
} PuglX11CairoSurface;

static void
puglX11CairoClose(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

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
puglX11CairoOpen(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	puglX11CairoClose(view); // just to be sure
	
	surface->crSurface = cairo_xlib_surface_create(
		impl->display, impl->win, impl->vi->visual, 
		view->frame.width, view->frame.height);
	
	if (surface->crSurface) {
		surface->crContext = cairo_create(surface->crSurface);
	}
	if (surface->crContext) {
		return PUGL_SUCCESS;
	} else {
		puglX11CairoClose(view);
		return PUGL_CREATE_CONTEXT_FAILED;
	}
}

static PuglStatus
puglX11CairoCreate(PuglView* view)
{
	PuglInternals* const impl    = view->impl;

	impl->surface = calloc(1, sizeof(PuglX11CairoSurface));

	return PUGL_SUCCESS;
}

static PuglStatus
puglX11CairoDestroy(PuglView* view)
{
	PuglInternals* const impl = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	puglX11CairoClose(view);
	free(surface);
	impl->surface = NULL;
	return PUGL_SUCCESS;
}

static PuglStatus
puglX11CairoEnter(PuglView* view, const PuglEventExpose* expose)
{
	return PUGL_SUCCESS;
}



static PuglStatus
puglX11CairoLeave(PuglView* view, const PuglEventExpose* expose)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	if (expose && surface->crContext) {
		cairo_pop_group_to_source(surface->crContext);
		cairo_paint(surface->crContext);
	}
	puglX11CairoClose(view);

	return PUGL_SUCCESS;
}

static void*
puglX11CairoGetContext(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	if (!surface->crContext) {
		puglX11CairoOpen(view);
		if (surface->crContext) {
			cairo_push_group_with_content(surface->crContext, CAIRO_CONTENT_COLOR_ALPHA);
		}
	}
	return surface->crContext;
}

void*
puglCairoBackendGetNativeWorld(PuglWorld* world)
{
	return world->impl->display;
}

const PuglBackend*
puglCairoBackend(void)
{
	static const PuglBackend backend = {puglX11StubConfigure,
	                                    puglX11CairoCreate,
	                                    puglX11CairoDestroy,
	                                    puglX11CairoEnter,
	                                    puglX11CairoLeave,
	                                    puglX11CairoGetContext};

	return &backend;
}
