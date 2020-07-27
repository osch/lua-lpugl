/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

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
   @file mac_cairo.m
   @brief Cairo graphics backend for MacOS.
*/

#include "pugl/detail/implementation.h"
#include "pugl/detail/rect.h"
#include "pugl/detail/mac.h"
#include "pugl/detail/stub.h"
#include "pugl/pugl_cairo.h"

#include <cairo-quartz.h>

#import <Cocoa/Cocoa.h>

#include <assert.h>


#ifndef   PUGL_MAC_CAIRO_OPTIMIZED
  // Speed up for most cases by using cairo software rendering 
  // instead letting cairo use quartz rendering.
  #define PUGL_MAC_CAIRO_OPTIMIZED 1
#endif
#if PUGL_MAC_CAIRO_OPTIMIZED && !defined(PUGL_MAC_CAIRO_OPTIMIZED_CACHE)
  // Speed up by using surface cache for partial rendering if
  // getRectsBeingDrawn is not working (MacOS >= 10.14)
  // see also https://forum.juce.com/t/juce-coregraphics-render-with-multiple-paint-calls-not-working-on-new-mac-mojave/30905
  #define PUGL_MAC_CAIRO_OPTIMIZED_CACHE 1
#endif

@interface PuglCairoView : NSView
@end

@implementation PuglCairoView
{
@public
	PuglView*        puglview;
	cairo_surface_t* surface;
	cairo_t*         cr;
#if PUGL_MAC_CAIRO_OPTIMIZED
	PuglRect         surfaceRect;
    #if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
	NSTimer*         surfaceTimer;
    #endif
#endif
}

- (id) initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];

	return self;
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
	// getRectsBeingDrawn does only give one merged rectangle for MacOS >= 10.14 :-(
	// see also https://forum.juce.com/t/juce-coregraphics-render-with-multiple-paint-calls-not-working-on-new-mac-mojave/30905
	[self getRectsBeingDrawn:&rects count:&rectCount];
	[wrapper dispatchExpose:rect rects:rects count:rectCount];
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

#if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
- (void) processSurfaceTimer
{
    if (surface) {
        cairo_surface_destroy(surface);
        surface = NULL;
    }
}
#endif

@end

#if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
static void releaseSurfaceTimer(PuglView* view) 
{
    PuglCairoView* drawView = (PuglCairoView*)view->impl->drawView;
    if (drawView->surfaceTimer) {
         [drawView->surfaceTimer invalidate];
         [drawView->surfaceTimer release];
         drawView->surfaceTimer = NULL;
    }
}
static void rescheduleSurfaceTimer(PuglView* view)
{
    PuglCairoView* drawView = (PuglCairoView*)view->impl->drawView;
    releaseSurfaceTimer(view);
    drawView->surfaceTimer =
            [[NSTimer scheduledTimerWithTimeInterval:5
                                              target:drawView
                                            selector:@selector(processSurfaceTimer)
                                            userInfo:nil
                                             repeats:NO] retain];
}
#endif

static PuglStatus
puglMacCairoCreate(PuglView* view)
{
	PuglInternals* impl     = view->impl;
	PuglCairoView* drawView = [PuglCairoView alloc];

	drawView->puglview = view;
	[drawView initWithFrame:[impl->wrapperView bounds]];
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

#if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
	releaseSurfaceTimer(view);
#endif
	if (drawView->cr) {
	    cairo_destroy(drawView->cr);
	    drawView->cr = NULL;
	}
        if (drawView->surface) {
            cairo_surface_destroy(drawView->surface);
            drawView->surface = NULL;
        }
	[drawView removeFromSuperview];
	[drawView release];

	view->impl->drawView = nil;
	return PUGL_SUCCESS;
}

#if PUGL_MAC_CAIRO_OPTIMIZED
static void copyRect(unsigned char* dstData, size_t dstBytesPerRow, unsigned char* srcData, size_t srcBytesPerRow,
                     size_t dstX, size_t dstY, size_t srcX, size_t srcY, size_t w, size_t h)
{
    unsigned char* src = srcData + (size_t)(srcY * srcBytesPerRow + 4*srcX);
    unsigned char* dst = dstData + (size_t)(dstY * dstBytesPerRow + 4*dstX);
    size_t widthBytes = 4 * w;
    for (size_t i = 0; i < h; ++i) {
        memcpy(dst, src, widthBytes);
        dst += dstBytesPerRow;
        src += srcBytesPerRow;
    }
}
#endif

static PuglStatus
puglMacCairoEnter(PuglView* view, const PuglEventExpose* expose)
{
        PuglCairoView* const drawView = (PuglCairoView*)view->impl->drawView;

        assert(!drawView->cr);

        if (!expose) {
                return PUGL_SUCCESS;
        }

        const NSSize sizePt  = [drawView bounds].size;
        const NSSize sizePx  = [drawView convertSizeToBacking: sizePt];

#if PUGL_MAC_CAIRO_OPTIMIZED
        PuglRect viewRect   = { 0, 0, sizePx.width, sizePx.height };
        PuglRect exposeRect = { expose->x, expose->y, expose->width, expose->height };
        
    #if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
        if (   !drawView->surface
            || !doesRectContain(&drawView->surfaceRect, &exposeRect)
            || !doesRectContain(&viewRect, &drawView->surfaceRect))
        {
            if (drawView->surface) {
                cairo_surface_destroy(drawView->surface);
                drawView->surface = NULL;
            }
            if (view->impl->trySurfaceCache) {
                view->rectsCount = 0;
            }
    #else
            assert(!drawView->surface);
    #endif
            drawView->surfaceRect = exposeRect;
    
            drawView->surface = cairo_image_surface_create (
                    CAIRO_FORMAT_RGB24, expose->width, expose->height);
                    // CAIRO_FORMAT_RGB24 = each pixel is a 32-bit quantity, with the
                    // upper 8 bits unused. Red, Green, and Blue are stored in the remaining
                    // 24 bits in that order. The 32-bit quantities are stored native-endian.
    #if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
        }
    #endif
        drawView->cr = cairo_create(drawView->surface);
        cairo_translate(drawView->cr, -drawView->surfaceRect.x, -drawView->surfaceRect.y);
#else
        CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
         
        CGContextScaleCTM(context, sizePt.width/sizePx.width, sizePt.height/sizePx.height);

        drawView->surface = cairo_quartz_surface_create_for_cg_context(
                context, (unsigned)sizePx.width, (unsigned)sizePx.height);

        drawView->cr = cairo_create(drawView->surface);
#endif        
        if (view->rectsCount > 0) {
            for (int i = 0; i < view->rectsCount; ++i) {
                const PuglRect* r = view->rects + i;
                cairo_rectangle(drawView->cr, r->x, r->y, r->width, r->height);
            }
        } else {
            cairo_rectangle(drawView->cr, expose->x, expose->y, 
                                          expose->width, expose->height);
        }
        cairo_clip(drawView->cr);
        cairo_push_group_with_content(drawView->cr, CAIRO_CONTENT_COLOR);
        
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

            int32_t tst = 0xf0000000;
            bool bigEndian = ((unsigned char*)&tst)[0] == 0xf0;
            
#if PUGL_MAC_CAIRO_OPTIMIZED
	    PuglRect* surfaceRect  = &drawView->surfaceRect;
            
            cairo_surface_flush(drawView->surface);
            
            unsigned char* surfaceData = cairo_image_surface_get_data(drawView->surface);

            CGContextRef    context     = [[NSGraphicsContext currentContext] graphicsPort];
            unsigned char*  contextData = CGBitmapContextGetData(context);
            bool            optimized   = false;
            if (contextData)
            {
                CGColorSpaceRef  colorSpace = [[[view->impl->drawView window] colorSpace] CGColorSpace];
                CFStringRef      cname      = CGColorSpaceCopyName(colorSpace);
                CGImageAlphaInfo alphaInfo  = CGBitmapContextGetAlphaInfo(context);
                
                if (cname) {
                    if (   CFStringCompare(cname, kCGColorSpaceSRGB, 0) == kCFCompareEqualTo
                        && CGBitmapContextGetBitsPerPixel(context) == 32
                        && CGBitmapContextGetBitsPerComponent(context) == 8
                        && (CGBitmapContextGetBitmapInfo(context) & kCGBitmapByteOrderMask)
                            == (bigEndian ?  kCGBitmapByteOrder32Big : kCGBitmapByteOrder32Little)
                        && (   alphaInfo ==  kCGImageAlphaNoneSkipFirst 
                            || alphaInfo ==  kCGImageAlphaPremultipliedFirst) 
                        && view->frame.width  == CGBitmapContextGetWidth(context)
                        && view->frame.height == CGBitmapContextGetHeight(context))
                    {
                        optimized = true;
                    }
                    CFRelease(cname);
                }
            }
            if (optimized) {
                size_t srcBytesPerRow  = (size_t)(4 * surfaceRect->width);
                size_t destBytesPerRow = CGBitmapContextGetBytesPerRow(context);
                copyRect(contextData, destBytesPerRow, surfaceData, srcBytesPerRow,
                         expose->x, expose->y, 
                         expose->x - surfaceRect->x, expose->y - surfaceRect->y, 
                         expose->width, expose->height);
            } else {
                CGColorSpaceRef colorSpace  = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                CGContextRef bitmapContext = CGBitmapContextCreate(surfaceData 
                                                                   + (size_t)((expose->y - surfaceRect->y) * 4 * surfaceRect->width) 
                                                                   + (size_t)((expose->x - surfaceRect->x) * 4), 
                                                                   expose->width, 
                                                                   expose->height,
                                                                   8, // bitsPerComponent
                                                                   4 * surfaceRect->width,// inputBytesPerRow, 
                                                                   colorSpace,
                                                                     (bigEndian ? kCGBitmapByteOrder32Big 
                                                                                : kCGBitmapByteOrder32Little)
                                                                   | kCGImageAlphaNoneSkipFirst);
                CGImageRef cgImage = CGBitmapContextCreateImage(bitmapContext);
                {
                    const NSSize viewSizePt = [drawView bounds].size;
                    const NSSize viewSizePx = [drawView convertSizeToBacking: viewSizePt];
                    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
                    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
                    
                    double scaleFactor = [[view->impl->window screen] backingScaleFactor];
                    CGContextTranslateCTM(context, 0.0, viewSizePt.height);
                    CGContextScaleCTM(context, 1/scaleFactor, -1/scaleFactor);
                    
                    CGContextDrawImage(context, 
                                       CGRectMake(expose->x, 
                                                  viewSizePx.height - expose->y - expose->height, 
                                                  expose->width, 
                                                  expose->height), 
                                       cgImage);
                }
                CGImageRelease(cgImage);
                CGContextRelease(bitmapContext);
                CGColorSpaceRelease(colorSpace);
            }
#endif
            cairo_destroy(drawView->cr);
            drawView->cr      = NULL;
#if PUGL_MAC_CAIRO_OPTIMIZED_CACHE
            if (view->impl->trySurfaceCache) {
                rescheduleSurfaceTimer(view);
            } else {
                cairo_surface_destroy(drawView->surface);
                drawView->surface = NULL;
            }
#else
            cairo_surface_destroy(drawView->surface);
            drawView->surface = NULL;
#endif
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
	static const PuglBackend backend = {puglStubConfigure,
	                                    puglMacCairoCreate,
	                                    puglMacCairoDestroy,
	                                    puglMacCairoEnter,
	                                    puglMacCairoLeave,
	                                    puglMacCairoGetContext};

	return &backend;
}
