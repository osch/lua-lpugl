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

#include <assert.h>
#include <mach/mach_time.h>
#include <cairo-quartz.h>
#include <OpenGL/gl.h>
#import  <Cocoa/Cocoa.h>

#include "implementation.h"
#include "mac.h"
#include "rect.h"
#include "stub.h"

#include "pugl/gl.h"
#include "pugl/cairo.h"

#ifndef __MAC_10_10
#  define NSOpenGLProfileVersion4_1Core NSOpenGLProfileVersion3_2Core
#endif

@interface PuglCairoGLView : NSOpenGLView
@end

@implementation PuglCairoGLView {
@public
    PuglView*        puglview;
    cairo_surface_t* surface;
    cairo_t*         cr;
    NSTimer*         surfaceTimer;
    double           surfaceWidth;
    double           surfaceHeight;
    double           lastDisplayTime;
    double           displayStartTime;
    bool             rendering;
    bool             slowRendering;
}

- (id)initWithFrame:(NSRect)frame
{
    const bool     compat  = puglview->hints[PUGL_USE_COMPAT_PROFILE];
    const unsigned samples = (unsigned)puglview->hints[PUGL_SAMPLES];
    const int      major   = puglview->hints[PUGL_CONTEXT_VERSION_MAJOR];
    const unsigned profile =
      ((compat || major < 3) ? NSOpenGLProfileVersionLegacy
                             : (major >= 4 ? NSOpenGLProfileVersion4_1Core
                                           : NSOpenGLProfileVersion3_2Core));
  
    // Set attributes to default if they are unset
    // (There is no GLX_DONT_CARE equivalent on MacOS)
    if (puglview->hints[PUGL_DEPTH_BITS] == PUGL_DONT_CARE) {
      puglview->hints[PUGL_DEPTH_BITS] = 0;
    }
    if (puglview->hints[PUGL_STENCIL_BITS] == PUGL_DONT_CARE) {
      puglview->hints[PUGL_STENCIL_BITS] = 0;
    }
    if (puglview->hints[PUGL_SAMPLES] == PUGL_DONT_CARE) {
      puglview->hints[PUGL_SAMPLES] = 0;
    }
    if (puglview->hints[PUGL_DOUBLE_BUFFER] == PUGL_DONT_CARE) {
      puglview->hints[PUGL_DOUBLE_BUFFER] = 0;
    }
    if (puglview->hints[PUGL_SWAP_INTERVAL] == PUGL_DONT_CARE) {
      puglview->hints[PUGL_SWAP_INTERVAL] = 0;
    }
  
    const unsigned colorSize = (unsigned)(puglview->hints[PUGL_RED_BITS] +
                                          puglview->hints[PUGL_BLUE_BITS] +
                                          puglview->hints[PUGL_GREEN_BITS] +
                                          puglview->hints[PUGL_ALPHA_BITS]);
  
    NSOpenGLPixelFormatAttribute pixelAttribs[17] = {
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAAccelerated,
      NSOpenGLPFAOpenGLProfile,
      profile,
      NSOpenGLPFAColorSize,
      colorSize,
      NSOpenGLPFADepthSize,
      (unsigned)puglview->hints[PUGL_DEPTH_BITS],
      NSOpenGLPFAStencilSize,
      (unsigned)puglview->hints[PUGL_STENCIL_BITS],
      NSOpenGLPFAMultisample,
      samples ? 1 : 0,
      NSOpenGLPFASampleBuffers,
      samples ? 1 : 0,
      NSOpenGLPFASamples,
      samples,
      0};
    NSOpenGLPixelFormatAttribute* a =
      puglview->hints[PUGL_DOUBLE_BUFFER] ? &pixelAttribs[0] : &pixelAttribs[1];
  
    NSOpenGLPixelFormat* pixelFormat =
      [[NSOpenGLPixelFormat alloc] initWithAttributes:a];
  
    if (pixelFormat) {
      self = [super initWithFrame:frame pixelFormat:pixelFormat];
      [pixelFormat release];
    } else {
      self = [super initWithFrame:frame];
    }
  
    [self setWantsBestResolutionOpenGLSurface:YES];
  
    if (self) {
      [[self openGLContext] makeCurrentContext];
      [self reshape];
    }
    return self;
}

- (void)reshape
{
    PuglWrapperView* wrapper = (PuglWrapperView*)[self superview];
  
    [super reshape];
    [wrapper setReshaped];
}

- (void)drawRect:(NSRect)rect
{
    PuglWrapperView* wrapper   = (PuglWrapperView*)[self superview];
    const NSRect*    rects     = NULL;
    NSInteger        rectCount = 0;
    // getRectsBeingDrawn does only give one merged rectangle for MacOS >= 10.14
    // :-( see also
    // https://forum.juce.com/t/juce-coregraphics-render-with-multiple-paint-calls-not-working-on-new-mac-mojave/30905
    [self getRectsBeingDrawn:&rects count:&rectCount];
    [wrapper dispatchExpose:rect rects:rects count:rectCount];
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
    return YES;
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)theEvent
{
    return puglview->transientParent && puglview->hints[PUGL_IS_POPUP];
}

static void
releaseSurfaceTimer(PuglView* view)
{
    PuglCairoGLView* drawView = (PuglCairoGLView*)view->impl->drawView;
    if (drawView->surfaceTimer) {
      [drawView->surfaceTimer invalidate];
      [drawView->surfaceTimer release];
      drawView->surfaceTimer = NULL;
    }
}
static void
rescheduleSurfaceTimer(PuglView* view)
{
    PuglCairoGLView* drawView = (PuglCairoGLView*)view->impl->drawView;
    releaseSurfaceTimer(view);
    drawView->surfaceTimer =
      [[NSTimer scheduledTimerWithTimeInterval:5
                                        target:drawView
                                      selector:@selector(processSurfaceTimer)
                                      userInfo:nil
                                       repeats:NO] retain];
}

- (void)processSurfaceTimer
{
    if (surface && !slowRendering) {
        if (!rendering && mach_absolute_time() / 1e9 >= lastDisplayTime + 5) {
            cairo_surface_destroy(surface);
            surface = NULL;
            releaseSurfaceTimer(puglview);
        } else {
            rescheduleSurfaceTimer(puglview);
        }
    } else {
        releaseSurfaceTimer(puglview);
    }
}
@end

static PuglStatus
puglMacCairoGlCreate(PuglView* view)
{
    PuglInternals*  impl     = view->impl;
    PuglCairoGLView* drawView = [PuglCairoGLView alloc];
  
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
puglMacCairoGlDestroy(PuglView* view)
{
    PuglCairoGLView* const drawView = (PuglCairoGLView*)view->impl->drawView;
  
    releaseSurfaceTimer(view);
  
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

static PuglStatus
puglMacCairoGlEnter(PuglView*              view,
                    const PuglEventExpose* expose,
                    PuglRects*             rects)
{
    PuglCairoGLView* const drawView = (PuglCairoGLView*)view->impl->drawView;
  
    assert(!drawView->cr);
  
    if (!drawView->slowRendering) {
        drawView->displayStartTime = mach_absolute_time() / 1e9;
    }
    
    if (!expose) {
            return PUGL_SUCCESS;
    }
    drawView->rendering = true;
    
    const NSSize sizePt  = [drawView bounds].size;
    const NSSize sizePx  = [drawView convertSizeToBacking: sizePt];
  
    PuglRect viewRect   = { 0, 0, sizePx.width, sizePx.height };
    PuglRect exposeRect = { expose->x, expose->y, expose->width, expose->height };
    
    if (  !drawView->surface
        || drawView->surfaceWidth  != viewRect.width 
        || drawView->surfaceHeight != viewRect.height)
    {
        if (rects && view->impl->trySurfaceCache) {
            rects->rectsCount = 0;
        }
        if (drawView->surface) {
            cairo_surface_destroy(drawView->surface);
            drawView->surface = NULL;
        }
        drawView->surfaceWidth  = viewRect.width;
        drawView->surfaceHeight = viewRect.height;

        drawView->surface = cairo_image_surface_create (
                CAIRO_FORMAT_ARGB32, drawView->surfaceWidth, drawView->surfaceHeight);
                // CAIRO_FORMAT_RGB24 = each pixel is a 32-bit quantity, with the
                // upper 8 bits unused. Red, Green, and Blue are stored in the remaining
                // 24 bits in that order. The 32-bit quantities are stored native-endian.
                
                // CAIRO_FORMAT_ARGB32 each pixel is a 32-bit quantity, with alpha
                // in the upper 8 bits, then red, then green, then blue. The
                // 32-bit quantities are stored native-endian. Pre-multiplied
                // alpha is used. (That is, 50% transparent red is 0x80800000, not
                // 0x80ff0000.) 
    }
    drawView->cr = cairo_create(drawView->surface);
    cairo_matrix_t m = {1, 0, 0, -1, 0, sizePx.height};
    cairo_transform(drawView->cr, &m);
  
    if (rects && rects->rectsCount > 0) {
        for (int i = 0; i < rects->rectsCount; ++i) {
            const PuglRect* r = rects->rectsList + i;
            cairo_rectangle(drawView->cr, r->x, r->y, r->width, r->height);
        }
      cairo_clip(drawView->cr);
    }

    [[drawView openGLContext] makeCurrentContext];

    return PUGL_SUCCESS;
}

static PuglStatus
puglMacCairoGlLeave(PuglView*              view,
                    const PuglEventExpose* expose,
                    PuglRects*             PUGL_UNUSED(rects))
{
    PuglCairoGLView* const drawView = (PuglCairoGLView*)view->impl->drawView;
  
    if (   !drawView->slowRendering && drawView->displayStartTime 
        && mach_absolute_time() / 1e9 > drawView->displayStartTime + 0.100) 
    {
        drawView->slowRendering = true;
    }
    if (!expose) {
      return PUGL_SUCCESS;
    }
    if (drawView->surface) {
        assert(drawView->cr);
        cairo_surface_flush(drawView->surface);
        int32_t tst = 0xf0000000;
        bool bigEndian = ((unsigned char*)&tst)[0] == 0xf0;
          unsigned char* surfaceData = cairo_image_surface_get_data(drawView->surface);
        glDrawPixels(drawView->surfaceWidth, drawView->surfaceHeight,  
                     GL_BGRA, 
                     bigEndian ? GL_UNSIGNED_INT_8_8_8_8
                               : GL_UNSIGNED_INT_8_8_8_8_REV,
                     surfaceData);
    }                
    if (expose) {
      if (view->hints[PUGL_DOUBLE_BUFFER]) {
        [[drawView openGLContext] flushBuffer];
      } else {
        glFlush();
      }
    }
    [NSOpenGLContext clearCurrentContext];
  
    cairo_destroy(drawView->cr);
    drawView->cr      = NULL;

    drawView->lastDisplayTime = mach_absolute_time() / 1e9;
    if (!drawView->slowRendering && !drawView->surfaceTimer) {
        rescheduleSurfaceTimer(view);
    }
    drawView->rendering = false;
    return PUGL_SUCCESS;
}

static void*
puglMacCairoGetContext(PuglView* view)
{
    return ((PuglCairoGLView*)view->impl->drawView)->cr;
}

void*
puglCairoBackendGetNativeWorld(PuglWorld* PUGL_UNUSED(world))
{
    CGContextRef contextRef =
      (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    return contextRef;
}

const PuglBackend*
puglCairoBackend(void)
{
    static const PuglBackend backend = {puglStubConfigure,
                                        puglMacCairoGlCreate,
                                        puglMacCairoGlDestroy,
                                        puglMacCairoGlEnter,
                                        puglMacCairoGlLeave,
                                        puglMacCairoGetContext};
  
    return &backend;
}
