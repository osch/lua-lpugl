/*
  Copyright 2019 David Robillard <http://drobilla.net>

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
   @file mac_cairo.m Cairo graphics backend for MacOS.
*/

#include "pugl/detail/implementation.h"
#include "pugl/detail/mac.h"
#include "pugl/pugl_cairo.h"
#include "pugl/pugl_stub.h"

#include <cairo-quartz.h>

#import <Cocoa/Cocoa.h>

#include <assert.h>

@interface PuglCairoView : NSView
{
@public
	PuglView*        puglview;
	cairo_surface_t* surface;
	cairo_t*         cr;
}

@end

@implementation PuglCairoView

- (id) initWithFrame:(NSRect)frame
{
	return (self = [super initWithFrame:frame]);
}

- (void) resizeWithOldSuperviewSize:(NSSize)oldSize
{
	PuglWrapperView* wrapper = (PuglWrapperView*)[self superview];

	[super resizeWithOldSuperviewSize:oldSize];
	[wrapper setReshaped];
}

- (void) drawRect:(NSRect)rect
{
	PuglWrapperView* wrapper = (PuglWrapperView*)[self superview];
	const NSRect* rects = NULL;
	NSInteger rectCount = 0;
	if (puglview->hints[PUGL_DONT_MERGE_RECTS]) {
	    [self getRectsBeingDrawn:&rects count:&rectCount];
	}
	if (rects != NULL && rectCount > 0) {
	    for (int i = 0; i < rectCount; ++i) {
	        [wrapper dispatchExpose:rects[i] count:rectCount - i - 1];
	    }
	} else {
	    [wrapper dispatchExpose:rect count:0];
	}
}

- (BOOL) isFlipped
{
	return YES;
}

- (BOOL) acceptsFirstMouse:(NSEvent*)event
{
	return YES;
}

- (BOOL) shouldDelayWindowOrderingForEvent:(NSEvent*)theEvent
{
	return puglview->transientParent && puglview->hints[PUGL_IS_POPUP];
}

- (BOOL) acceptsFirstResponder
{
	return !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (BOOL) becomeFirstResponder
{
	return !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

@end

static PuglStatus
puglMacCairoCreate(PuglView* view)
{
	PuglInternals* impl     = view->impl;
	PuglCairoView* drawView = [PuglCairoView alloc];

	drawView->puglview = view;
	[drawView initWithFrame:NSMakeRect(0, 0, view->frame.width, view->frame.height)];
	if (view->hints[PUGL_RESIZABLE]) {
		[drawView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	} else {
		[drawView setAutoresizingMask:NSViewNotSizable];
	}

	impl->drawView = drawView;
	return PUGL_SUCCESS;
}

static PuglStatus
puglMacCairoDestroy(PuglView* view)
{
	PuglCairoView* const drawView = (PuglCairoView*)view->impl->drawView;

	[drawView removeFromSuperview];
	[drawView release];

	view->impl->drawView = nil;
	return PUGL_SUCCESS;
}

static PuglStatus
puglMacCairoEnter(PuglView* view, const PuglEventExpose* expose)
{
	PuglCairoView* const drawView = (PuglCairoView*)view->impl->drawView;

	assert(!drawView->surface);
	assert(!drawView->cr);

	if (!expose || (view->hints[PUGL_DONT_MERGE_RECTS] && expose->count > 0)) {
		return PUGL_SUCCESS;
	}

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	drawView->surface = cairo_quartz_surface_create_for_cg_context(
		context, view->frame.width, view->frame.height);

	drawView->cr = cairo_create(drawView->surface);
        
        cairo_push_group_with_content(drawView->cr, CAIRO_CONTENT_COLOR_ALPHA);
	
	return PUGL_SUCCESS;
}

static PuglStatus
puglMacCairoLeave(PuglView* view, const PuglEventExpose* expose)
{
	PuglCairoView* const drawView = (PuglCairoView*)view->impl->drawView;
	if (!expose) {
		return PUGL_SUCCESS;
	}
        if (drawView->surface) {
            assert(drawView->cr);
            
            cairo_pop_group_to_source(drawView->cr);
            cairo_paint(drawView->cr);

            cairo_destroy(drawView->cr);
            cairo_surface_destroy(drawView->surface);
            drawView->cr      = NULL;
            drawView->surface = NULL;
        }
	return PUGL_SUCCESS;
}

static void*
puglMacCairoGetContext(PuglView* view)
{
	return ((PuglCairoView*)view->impl->drawView)->cr;
}

void*
puglCairoBackendGetNativeWorld(PuglWorld* PUGL_UNUSED(world))
{
	CGContextRef contextRef = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
	return contextRef;
}

const PuglBackend* puglCairoBackend(void)
{
	static const PuglBackend backend = {
		puglStubConfigure,
		puglMacCairoCreate,
		puglMacCairoDestroy,
		puglMacCairoEnter,
		puglMacCairoLeave,
		puglStubResize,
		puglMacCairoGetContext
	};

	return &backend;
}
