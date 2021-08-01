/*
  Copyright 2012-2020 David Robillard <d@drobilla.net>
  Copyright 2017 Hanspeter Portner <dev@open-music-kontrollers.ch>

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

#define GL_SILENCE_DEPRECATION 1

#include "mac.h"

#include "implementation.h"

#include "rect.h"

#include "pugl/pugl.h"

#import <Cocoa/Cocoa.h>

#include <mach/mach_time.h>

#include <stdlib.h>

#ifndef __MAC_10_10
typedef NSUInteger NSEventModifierFlags;
#endif

#ifndef __MAC_10_12
typedef NSUInteger NSWindowStyleMask;
#endif

static NSRect
rectToScreen(NSScreen* screen, NSRect rect)
{
  const double screenHeight = [screen frame].size.height;

  rect.origin.y = screenHeight - rect.origin.y - rect.size.height;
  return rect;
}

static NSScreen*
viewScreen(PuglView* view)
{
  return view->impl->window ? [view->impl->window screen]
                            : [NSScreen mainScreen];
}

static NSRect
nsRectToPoints(PuglView* view, const NSRect rect)
{
  const double scaleFactor = [viewScreen(view) backingScaleFactor];

  return NSMakeRect(rect.origin.x / scaleFactor,
                    rect.origin.y / scaleFactor,
                    rect.size.width / scaleFactor,
                    rect.size.height / scaleFactor);
}

static NSRect
nsRectFromPoints(PuglView* view, const NSRect rect)
{
  const double scaleFactor = [viewScreen(view) backingScaleFactor];

  return NSMakeRect(rect.origin.x * scaleFactor,
                    rect.origin.y * scaleFactor,
                    rect.size.width * scaleFactor,
                    rect.size.height * scaleFactor);
}

static NSPoint
nsPointFromPoints(PuglView* view, const NSPoint point)
{
  const double scaleFactor = [viewScreen(view) backingScaleFactor];

  return NSMakePoint(point.x * scaleFactor, point.y * scaleFactor);
}

static NSRect
rectToNsRect(const PuglRect rect)
{
  return NSMakeRect(rect.x, rect.y, rect.width, rect.height);
}

static NSSize
sizePoints(PuglView* view, const double width, const double height)
{
  const double scaleFactor = [viewScreen(view) backingScaleFactor];

  return NSMakeSize(width / scaleFactor, height / scaleFactor);
}

static void
updateViewRect(PuglView* view)
{
  NSWindow* const window = view->impl->window;
  if (window) {
    const NSRect screenFramePt = [[NSScreen mainScreen] frame];
    const NSRect screenFramePx = nsRectFromPoints(view, screenFramePt);
    const NSRect framePt       = [window frame];
    const NSRect contentPt     = [window contentRectForFrameRect:framePt];
    const NSRect contentPx     = nsRectFromPoints(view, contentPt);
    const double screenHeight  = screenFramePx.size.height;

    view->frame.x = contentPx.origin.x;
    view->frame.y = screenHeight - contentPx.origin.y - contentPx.size.height;
    view->frame.width  = contentPx.size.width;
    view->frame.height = contentPx.size.height;
  }
  else {
    const NSRect contentPt     = [view->impl->wrapperView frame];
    const NSRect contentPx     = nsRectFromPoints(view, contentPt);
    NSView* const superView    = [view->impl->wrapperView superview];
    view->frame.x = contentPx.origin.x;
    view->frame.y = contentPx.origin.y;
    if (superView && ![superView isFlipped]) {
        const NSRect superFramePt = [superView frame];
        const NSRect superFramePx = nsRectFromPoints(view, superFramePt);
        const double superHeight  = superFramePx.size.height;
        view->frame.y = superHeight - contentPx.origin.y - contentPx.size.height;
    }
    view->frame.width  = contentPx.size.width;
    view->frame.height = contentPx.size.height;
  }
}

@interface PuglWorldProxy : NSObject {
  PuglWorld* world;
}
@end

static void
releaseProcessTimer(PuglWorldInternals* impl)
{
  if (impl->processTimer) {
    [impl->processTimer invalidate];
    [impl->processTimer release];
    impl->processTimer = NULL;
  }
}

static void
rescheduleProcessTimer(PuglWorld* world)
{
  PuglWorldInternals* impl = world->impl;
  releaseProcessTimer(impl);
  if (impl->nextProcessTime >= 0) {
    impl->processTimer =
      [[NSTimer timerWithTimeInterval:impl->nextProcessTime - puglGetTime(world)
                               target:world->impl->worldProxy
                             selector:@selector(processTimerCallback)
                             userInfo:nil
                              repeats:NO] retain];
    [[NSRunLoop currentRunLoop] addTimer:impl->processTimer
                                 forMode:NSRunLoopCommonModes];
  }
}

@implementation PuglWorldProxy
- (void)processTimerCallback
{
  if (world) {
    PuglWorldInternals* impl = world->impl;
    if (impl->nextProcessTime >= 0 &&
        impl->nextProcessTime <= puglGetTime(world)) {
      releaseProcessTimer(impl);
      impl->nextProcessTime = -1;
      if (world->processFunc) {
        world->processFunc(world, world->processUserData);
        if (world->impl->polling) {
          NSEvent* event =
            [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                               location:NSMakePoint(0, 0)
                          modifierFlags:0
                              timestamp:0.0
                           windowNumber:0
                                context:nil
                                subtype:0
                                  data1:0
                                  data2:0];
          [world->impl->app postEvent:event atStart:false];
        }
      }
    } else {
      rescheduleProcessTimer(world);
    }
  }
}
- (void)awakeCallback
{
  if (world) {
    if (world->processFunc) {
      world->processFunc(world, world->processUserData);
      if (world->impl->polling) {
        NSEvent* event =
          [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                             location:NSMakePoint(0, 0)
                        modifierFlags:0
                            timestamp:0.0
                         windowNumber:0
                              context:nil
                              subtype:0
                                data1:0
                                data2:0];
        [world->impl->app postEvent:event atStart:false];
      }
    }
  }
}
- (void)setPuglWorld:(PuglWorld*)w
{
  world = w;
}
@end

@implementation PuglWindow {
@public
  PuglView* puglview;
}

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag
{
  (void)flag;

  NSWindow* result = [super initWithContentRect:contentRect
                                      styleMask:aStyle
                                        backing:bufferingType
                                          defer:NO];

  [result setAcceptsMouseMovedEvents:YES];
  return (PuglWindow*)result;
}

- (void)setPuglview:(PuglView*)view
{
  puglview = view;

  [self setContentSize:sizePoints(view, view->frame.width, view->frame.height)];
}

- (BOOL)canBecomeKeyWindow
{
  return puglview &&
         !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (BOOL)canBecomeMainWindow
{
  return puglview &&
         !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (void)setIsVisible:(BOOL)flag
{
  if (flag && !puglview->visible) {
    PuglEventConfigure ev = {
      PUGL_CONFIGURE,
      0,
      puglview->frame.x,
      puglview->frame.y,
      puglview->frame.width,
      puglview->frame.height,
    };

    puglDispatchEvent(puglview, (PuglEvent*)&ev);
    puglDispatchSimpleEvent(puglview, PUGL_MAP);
  } else if (!flag && puglview->visible) {
    puglDispatchSimpleEvent(puglview, PUGL_UNMAP);
  }

  puglview->visible = flag;

  [super setIsVisible:flag];
}

@end

@implementation PuglWrapperView {
@public
  PuglView*                  puglview;
  NSTrackingArea*            trackingArea;
  NSMutableAttributedString* markedText;
  bool                       reshaped;
  PuglEventKey*              processingKeyEvent;

  NSLayoutConstraint*        minWidthConstraint;
  NSLayoutConstraint*        minHeightConstraint;
  NSLayoutConstraint*        maxWidthConstraint;
  NSLayoutConstraint*        maxHeightConstraint;
}

- (void)dispatchExpose:(NSRect)rect
                 rects:(const NSRect*)nsRects
                 count:(int)nsRectsCount
{
  if (!puglview)
    return;

  const double scaleFactor = [[NSScreen mainScreen] backingScaleFactor];

  if (reshaped) {
    updateViewRect(puglview);

    PuglEventConfigure ev = {
      PUGL_CONFIGURE,
      0,
      puglview->frame.x,
      puglview->frame.y,
      puglview->frame.width,
      puglview->frame.height,
    };

    puglDispatchEvent(puglview, (PuglEvent*)&ev);
    reshaped = false;
  }

  if (![[puglview->impl->drawView window] isVisible]) {
    return;
  }

  PuglEventExpose ev0 = {PUGL_EXPOSE,
                         0,
                         rect.origin.x * scaleFactor,
                         rect.origin.y * scaleFactor,
                         rect.size.width * scaleFactor,
                         rect.size.height * scaleFactor,
                         0};
  if (!puglview->impl->trySurfaceCache) {
    if (!nsRects || (nsRectsCount == 1 && puglview->rects.rectsCount > 1)) {
      // could be that getRectsBeingDrawn is not working (MacOS >= 10.14)
      // see also
      // https://forum.juce.com/t/juce-coregraphics-render-with-multiple-paint-calls-not-working-on-new-mac-mojave/30905
      //     or
      //     https://github.com/steinbergmedia/vstgui/commit/245ba880aa807a17aafac515efaeaa92c13f5093
      puglview->impl->trySurfaceCache = true;
    } 
    if (nsRectsCount == 1 && puglview->rects.rectsCount == 1) {
      PuglRect* r = puglview->rects.rectsList;
      if (   ev0.x < r->x || ev0.x + ev0.width  > r->x + r->width 
          || ev0.y < r->y || ev0.y + ev0.height > r->y + r->height)
      {
        puglview->impl->trySurfaceCache = true;
      }
    }
    if (!puglview->impl->trySurfaceCache) {
      if (puglRectsInit(&puglview->rects2, nsRectsCount)) {
        for (int i = 0; i < nsRectsCount; ++i) {
          PuglRect* r = puglview->rects2.rectsList + i;
          r->x        = nsRects[i].origin.x * scaleFactor;
          r->y        = nsRects[i].origin.y * scaleFactor;
          r->width    = nsRects[i].size.width * scaleFactor;
          r->height   = nsRects[i].size.height * scaleFactor;
          integerRect(r);
        }
        puglview->rects2.rectsCount = nsRectsCount;
      } else {
        puglview->rects2.rectsCount = 0;
      }
    }
  }
  if (puglview->impl->trySurfaceCache) {
    bool useRects2 =
      puglview->rects.rectsCount > 0 &&
      (puglview->rects2.rectsList || puglRectsInit(&puglview->rects2, 4));
    if (useRects2) {
      PuglRects swap   = puglview->rects2;
      puglview->rects2 = puglview->rects;
      puglview->rects  = swap;
    } else {
      puglview->rects2.rectsCount = 0;
    }
    puglview->rects.rectsCount = 0;
  }
  puglview->backend->enter(puglview, &ev0, &puglview->rects2);
  if (puglview->hints[PUGL_DONT_MERGE_RECTS] &&
      puglview->rects2.rectsCount > 0) {
    for (int i = 0; i < puglview->rects2.rectsCount; ++i) {
      PuglRect*       r = puglview->rects2.rectsList + i;
      PuglEventExpose e = {PUGL_EXPOSE,
                           0,
                           r->x,
                           r->y,
                           r->width,
                           r->height,
                           puglview->rects2.rectsCount - 1 - i};
      puglDispatchEventInContext(puglview, (PuglEvent*)&e);
    }
  } else {
    puglDispatchEventInContext(puglview, (PuglEvent*)&ev0);
  }
  puglview->backend->leave(puglview, &ev0, &puglview->rects2);
  puglview->rects2.rectsCount = 0;
}

- (NSSize)intrinsicContentSize
{
  if (puglview && (puglview->reqWidth || puglview->reqHeight)) {
    return sizePoints(puglview, puglview->reqWidth, puglview->reqHeight);
  }

  return NSMakeSize(NSViewNoInstrinsicMetric, NSViewNoInstrinsicMetric);
}

- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)acceptsFirstResponder
{
  return puglview &&
         !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (BOOL)becomeFirstResponder
{
  return puglview &&
         !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (void)handleFocusIn: (NSNotification*)notification
{
  if (puglview) {
    PuglEvent ev  = {{PUGL_FOCUS_IN, 0}};
    ev.focus.mode = PUGL_CROSSING_NORMAL;
    puglDispatchEvent(puglview, &ev);
  }
}
- (void)handleFocusOut: (NSNotification*)notification
{
  if (puglview) {
    PuglEvent ev  = {{PUGL_FOCUS_OUT, 0}};
    ev.focus.mode = PUGL_CROSSING_NORMAL;
    puglDispatchEvent(puglview, &ev);
  }
}

- (void)viewDidMoveToWindow
{
  if (puglview) {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    NSWindow* window = [self window];
    if (window) {
      [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleFocusIn:) 
                                                             name:NSWindowDidBecomeKeyNotification
                                                             object:window];
      [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleFocusOut:) 
                                                             name:NSWindowDidResignKeyNotification
                                                             object:window];
      [window setAcceptsMouseMovedEvents:YES];
      [window makeFirstResponder:self];
    }
  }
}


- (void)setReshaped
{
  reshaped = true;
}

- (void)clipboardReceived
{
  if (puglview && puglview->clipboard.data) {
    PuglEvent ev             = {0};
    ev.received.type         = PUGL_DATA_RECEIVED;
    ev.received.data         = puglview->clipboard.data;
    ev.received.len          = puglview->clipboard.len;
    puglview->clipboard.data = NULL;
    puglview->clipboard.len  = 0;
    puglDispatchEvent(puglview, &ev);
  }
}

static uint32_t
getModifiers(const NSEvent* const ev)
{
  const NSEventModifierFlags modifierFlags = [ev modifierFlags];

  return (((modifierFlags & NSShiftKeyMask) ? PUGL_MOD_SHIFT : 0) |
          ((modifierFlags & NSControlKeyMask) ? PUGL_MOD_CTRL : 0) |
          ((modifierFlags & NSAlternateKeyMask) ? PUGL_MOD_ALT : 0) |
          ((modifierFlags & NSCommandKeyMask) ? PUGL_MOD_SUPER : 0));
}

static PuglKey
keySymToSpecial(const NSEvent* const ev)
{
  NSString* chars = [ev charactersIgnoringModifiers];
  if ([chars length] == 1) {
    switch ([chars characterAtIndex:0]) {
    case NSDeleteCharacter:
      return PUGL_KEY_BACKSPACE;
    case NSTabCharacter:
      return PUGL_KEY_TAB;
    case NSBackTabCharacter:
      return PUGL_KEY_TAB;
    case NSCarriageReturnCharacter:
      return PUGL_KEY_RETURN;
    case NSEnterCharacter:
      return PUGL_KEY_KP_ENTER;
    case 0x001B:
      return PUGL_KEY_ESCAPE;
    case NSDeleteFunctionKey:
      return PUGL_KEY_DELETE;
    case NSF1FunctionKey:
      return PUGL_KEY_F1;
    case NSF2FunctionKey:
      return PUGL_KEY_F2;
    case NSF3FunctionKey:
      return PUGL_KEY_F3;
    case NSF4FunctionKey:
      return PUGL_KEY_F4;
    case NSF5FunctionKey:
      return PUGL_KEY_F5;
    case NSF6FunctionKey:
      return PUGL_KEY_F6;
    case NSF7FunctionKey:
      return PUGL_KEY_F7;
    case NSF8FunctionKey:
      return PUGL_KEY_F8;
    case NSF9FunctionKey:
      return PUGL_KEY_F9;
    case NSF10FunctionKey:
      return PUGL_KEY_F10;
    case NSF11FunctionKey:
      return PUGL_KEY_F11;
    case NSF12FunctionKey:
      return PUGL_KEY_F12;
    case NSLeftArrowFunctionKey:
      return PUGL_KEY_LEFT;
    case NSUpArrowFunctionKey:
      return PUGL_KEY_UP;
    case NSRightArrowFunctionKey:
      return PUGL_KEY_RIGHT;
    case NSDownArrowFunctionKey:
      return PUGL_KEY_DOWN;
    case NSPageUpFunctionKey:
      return PUGL_KEY_PAGE_UP;
    case NSPageDownFunctionKey:
      return PUGL_KEY_PAGE_DOWN;
    case NSHomeFunctionKey:
      return PUGL_KEY_HOME;
    case NSEndFunctionKey:
      return PUGL_KEY_END;
    case NSInsertFunctionKey:
      return PUGL_KEY_INSERT;
    case NSMenuFunctionKey:
      return PUGL_KEY_MENU;
    case NSScrollLockFunctionKey:
      return PUGL_KEY_SCROLL_LOCK;
    case NSClearLineFunctionKey:
      return PUGL_KEY_NUM_LOCK;
    case NSPrintScreenFunctionKey:
      return PUGL_KEY_PRINT_SCREEN;
    case NSPauseFunctionKey:
      return PUGL_KEY_PAUSE;
    default:
      break;
    }
    // SHIFT, CTRL, ALT, and SUPER are handled in [flagsChanged]
  }
  return (PuglKey)0;
}

- (void)updateTrackingAreas
{
  if (trackingArea != nil) {
    [self removeTrackingArea:trackingArea];
    [trackingArea release];
  }

  const int opts = (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                    NSTrackingActiveAlways);
  trackingArea   = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                              options:opts
                                                owner:self
                                             userInfo:nil];
  [self addTrackingArea:trackingArea];
  [super updateTrackingAreas];
  [self.window invalidateCursorRectsForView:self];
}

- (NSPoint)eventLocation:(NSEvent*)event
{
  return nsPointFromPoints(
    puglview, [self convertPoint:[event locationInWindow] fromView:nil]);
}

- (void)resetCursorRects
{
  [super resetCursorRects];
  [self addCursorRect:self.bounds cursor:puglview->impl->cursor];
}

static void
handleCrossing(PuglWrapperView* view, NSEvent* event, const PuglEventType type)
{
  const NSPoint     wloc = [view eventLocation:event];
  const NSPoint     rloc = [NSEvent mouseLocation];
  PuglEventCrossing ev   = {
    type,
    0,
    [event timestamp],
    wloc.x,
    wloc.y,
    rloc.x,
    [[NSScreen mainScreen] frame].size.height - rloc.y,
    getModifiers(event),
    PUGL_CROSSING_NORMAL,
  };

  puglDispatchEvent(view->puglview, (PuglEvent*)&ev);
}

- (void)mouseEntered:(NSEvent*)event
{
  if (!puglview)
    return;

  handleCrossing(self, event, PUGL_POINTER_IN);
  [puglview->impl->cursor set];
  if (puglview->impl->shouldCursorHidden && !puglview->impl->isCursorHidden) {
    [NSCursor hide];
    puglview->impl->isCursorHidden = true;
  }
  puglview->impl->mouseTracked = true;
}

- (void)mouseExited:(NSEvent*)event
{
  if (!puglview)
    return;

  if (puglview->impl->isCursorHidden && !puglview->impl->mouseButtons) {
    [NSCursor unhide];
    puglview->impl->isCursorHidden = false;
  }
  [[NSCursor arrowCursor] set];
  handleCrossing(self, event, PUGL_POINTER_OUT);
  puglview->impl->mouseTracked = false;
}

- (void)cursorUpdate:(NSEvent*)event
{
  (void)event;
  [puglview->impl->cursor set];
}

- (void)mouseMoved:(NSEvent*)event
{
  if (!puglview)
    return;

  const NSPoint   wloc = [self eventLocation:event];
  const NSPoint   rloc = [NSEvent mouseLocation];
  PuglEventMotion ev   = {
    PUGL_MOTION,
    0,
    [event timestamp],
    wloc.x,
    wloc.y,
    rloc.x,
    [[NSScreen mainScreen] frame].size.height - rloc.y,
    getModifiers(event),
  };

  if (puglview->impl->mouseTracked || puglview->impl->mouseButtons) {
    puglDispatchEvent(puglview, (PuglEvent*)&ev);
  }
  if (puglview &&
      !(puglview->impl->mouseTracked || puglview->impl->mouseButtons) &&
      puglview->impl->isCursorHidden) {
    [NSCursor unhide];
    puglview->impl->isCursorHidden = false;
  }
}

- (void)mouseDragged:(NSEvent*)event
{
  [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event
{
  [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event
{
  [self mouseMoved:event];
}

- (void)mouseDown2:(NSEvent*)event button:(uint32_t)buttonNumber
{
  const NSPoint   wloc = [self eventLocation:event];
  const NSPoint   rloc = [NSEvent mouseLocation];
  PuglEventButton ev   = {PUGL_BUTTON_PRESS,
                        0,
                        [event timestamp],
                        wloc.x,
                        wloc.y,
                        rloc.x,
                        [[NSScreen mainScreen] frame].size.height - rloc.y,
                        getModifiers(event),
                        buttonNumber};
  if (1 <= buttonNumber && buttonNumber <= 32) {
    puglview->impl->mouseButtons |= (1 << (buttonNumber - 1));
  }
  puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void)mouseDown:(NSEvent*)event
{
  if (!puglview)
    return;

  if (puglview->transientParent && puglview->hints[PUGL_IS_POPUP]) {
    [NSApp preventWindowOrdering];
  }
  [self mouseDown2:event button:1];
}

- (void)mouseUp2:(NSEvent*)event button:(uint32_t)buttonNumber
{
  if (!puglview)
    return;

  const NSPoint   wloc = [self eventLocation:event];
  const NSPoint   rloc = [NSEvent mouseLocation];
  PuglEventButton ev   = {PUGL_BUTTON_RELEASE,
                        0,
                        [event timestamp],
                        wloc.x,
                        wloc.y,
                        rloc.x,
                        [[NSScreen mainScreen] frame].size.height - rloc.y,
                        getModifiers(event),
                        buttonNumber};
  if (1 <= buttonNumber && buttonNumber <= 32) {
    puglview->impl->mouseButtons &= ~(1 << (buttonNumber - 1));
  }
  puglDispatchEvent(puglview, (PuglEvent*)&ev);

  if (puglview && !puglview->impl->mouseButtons) {
    if (!puglview->impl->mouseTracked) {
      if (puglview->impl->isCursorHidden) {
        [NSCursor unhide];
        puglview->impl->isCursorHidden = false;
      }
      [[NSCursor arrowCursor] set];
    }
  }
}

- (void)mouseUp:(NSEvent*)event
{
  [self mouseUp2:event button:1];
}

- (void)rightMouseDown:(NSEvent*)event
{
  [self mouseDown2:event button:3];
}

- (void)rightMouseUp:(NSEvent*)event
{
  [self mouseUp2:event button:3];
}

- (void)otherMouseDown:(NSEvent*)event
{
  [self mouseDown2:event button:2];
}

- (void)otherMouseUp:(NSEvent*)event
{
  [self mouseUp2:event button:2];
}

- (void)scrollWheel:(NSEvent*)event
{
  if (!puglview)
    return;

  const NSPoint             wloc = [self eventLocation:event];
  const NSPoint             rloc = [NSEvent mouseLocation];
  double                    dx   = [event scrollingDeltaX] * 0.1;
  double                    dy   = [event scrollingDeltaY] * 0.1;
  const PuglScrollDirection dir =
    ((dx == 0.0 && dy > 0.0)
       ? PUGL_SCROLL_UP
       : ((dx == 0.0 && dy < 0.0)
            ? PUGL_SCROLL_DOWN
            : ((dy == 0.0 && dx > 0.0)
                 ? PUGL_SCROLL_LEFT
                 : ((dy == 0.0 && dx < 0.0) ? PUGL_SCROLL_RIGHT
                                            : PUGL_SCROLL_SMOOTH))));
  if (![event hasPreciseScrollingDeltas]) {
    if (dx)
      dx = (dx > 0) ? +1 : -1;
    if (dy)
      dy = (dy > 0) ? +1 : -1;
  }

  const PuglEventScroll ev = {
    PUGL_SCROLL,
    0,
    [event timestamp],
    wloc.x,
    wloc.y,
    rloc.x,
    [[NSScreen mainScreen] frame].size.height - rloc.y,
    getModifiers(event),
    [event hasPreciseScrollingDeltas] ? PUGL_SCROLL_SMOOTH : dir,
    dx,
    dy,
  };

  puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void)keyDown:(NSEvent*)event
{
  if (!event || !puglview ||
      (puglview->hints[PUGL_IGNORE_KEY_REPEAT] && [event isARepeat])) {
    return;
  }

  const NSPoint   wloc  = [self eventLocation:event];
  const NSPoint   rloc  = [NSEvent mouseLocation];
  const PuglKey   spec  = keySymToSpecial(event);
  const NSString* chars = [event charactersIgnoringModifiers];
  const char*     str   = [[chars lowercaseString] UTF8String];
  const uint32_t  code  = (spec ? spec : puglDecodeUTF8((const uint8_t*)str));

  PuglEventKey ev = {
    PUGL_KEY_PRESS,
    0,
    [event timestamp],
    wloc.x,
    wloc.y,
    rloc.x,
    [[NSScreen mainScreen] frame].size.height - rloc.y,
    getModifiers(event),
    [event keyCode],
    (code != 0xFFFD) ? code : 0,
    {0}, 0
  };

  if (!spec) {
    processingKeyEvent = &ev;
    [self interpretKeyEvents:@[event]];
    processingKeyEvent = NULL;
  }
  puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void)keyUp:(NSEvent*)event
{
  if (!puglview)
    return;
  const NSPoint   wloc  = [self eventLocation:event];
  const NSPoint   rloc  = [NSEvent mouseLocation];
  const PuglKey   spec  = keySymToSpecial(event);
  const NSString* chars = [event charactersIgnoringModifiers];
  const char*     str   = [[chars lowercaseString] UTF8String];
  const uint32_t  code  = (spec ? spec : puglDecodeUTF8((const uint8_t*)str));

  PuglEventKey ev = {
    PUGL_KEY_RELEASE,
    0,
    [event timestamp],
    wloc.x,
    wloc.y,
    rloc.x,
    [[NSScreen mainScreen] frame].size.height - rloc.y,
    getModifiers(event),
    [event keyCode],
    (code != 0xFFFD) ? code : 0,
    {0}, 0
  };

  puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (BOOL)hasMarkedText
{
  return [markedText length] > 0;
}

- (NSRange)markedRange
{
  return (([markedText length] > 0) ? NSMakeRange(0, [markedText length] - 1)
                                    : NSMakeRange(NSNotFound, 0));
}

- (NSRange)selectedRange
{
  return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selected
     replacementRange:(NSRange)replacement
{
  (void)selected;
  (void)replacement;
  if (markedText)
    [markedText release];
  markedText =
    ([(NSObject*)string isKindOfClass:[NSAttributedString class]]
       ? [[NSMutableAttributedString alloc] initWithAttributedString:string]
       : [[NSMutableAttributedString alloc] initWithString:string]);
}

- (void)unmarkText
{
  [[markedText mutableString] setString:@""];
}

- (NSArray*)validAttributesForMarkedText
{
  return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                 (NSRangePointer)actual
{
  (void)range;
  (void)actual;
  return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
  (void)point;
  return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actual
{
  (void)range;
  (void)actual;

  const NSRect frame = [self bounds];
  return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void)doCommandBySelector:(SEL)selector
{
  (void)selector;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacement
{
  if (!puglview)
    return;
  (void)replacement;

  NSString* const characters =
    ([(NSObject*)string isKindOfClass:[NSAttributedString class]]
       ? [(NSAttributedString*)string string]
       : (NSString*)string);

  const char* bytes = [characters UTF8String];
  size_t      len   = strlen(bytes);

  if (processingKeyEvent) {
    PuglEventKey* event = processingKeyEvent;
    char*         newInput;
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
  } else {
    NSEvent* const ev    = [NSApp currentEvent];
    const NSPoint  wloc  = [self eventLocation:ev];
    const NSPoint  rloc  = [NSEvent mouseLocation];
    PuglEventKey   event = {PUGL_KEY_PRESS,
                          0,
                          [ev timestamp],
                          wloc.x,
                          wloc.y,
                          rloc.x,
                          [[NSScreen mainScreen] frame].size.height - rloc.y,
                          getModifiers(ev),
                          0,  // keycode
                          0, // key
                          {0}, 0};

    char*          newInput;
    if (len > 8) {
      newInput = malloc(len);
      if (newInput)
        event.input.ptr = newInput;
    } else {
      newInput = event.input.data;
    }
    if (newInput) {
      memcpy(newInput, bytes, len);
      event.inputLength = len;
      puglDispatchEvent(puglview, (PuglEvent*)&ev);
    }
  }
}

- (void)flagsChanged:(NSEvent*)event
{
  if (!puglview)
    return;
  const uint32_t mods    = getModifiers(event);
  PuglEventType  type    = PUGL_NOTHING;
  PuglKey        special = (PuglKey)0;

  if ((mods & PUGL_MOD_SHIFT) != (puglview->impl->mods & PUGL_MOD_SHIFT)) {
    type    = mods & PUGL_MOD_SHIFT ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
    special = PUGL_KEY_SHIFT;
  } else if ((mods & PUGL_MOD_CTRL) != (puglview->impl->mods & PUGL_MOD_CTRL)) {
    type    = mods & PUGL_MOD_CTRL ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
    special = PUGL_KEY_CTRL;
  } else if ((mods & PUGL_MOD_ALT) != (puglview->impl->mods & PUGL_MOD_ALT)) {
    type    = mods & PUGL_MOD_ALT ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
    special = PUGL_KEY_ALT;
  } else if ((mods & PUGL_MOD_SUPER) !=
             (puglview->impl->mods & PUGL_MOD_SUPER)) {
    type    = mods & PUGL_MOD_SUPER ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
    special = PUGL_KEY_SUPER;
  }

  if (special != 0) {
    const NSPoint wloc = [self eventLocation:event];
    const NSPoint rloc = [NSEvent mouseLocation];
    PuglEventKey  ev   = {type,
                       0,
                       [event timestamp],
                       wloc.x,
                       wloc.y,
                       rloc.x,
                       [[NSScreen mainScreen] frame].size.height - rloc.y,
                       mods,
                       [event keyCode],
                       special,
                       {0}, 0};
    puglDispatchEvent(puglview, (PuglEvent*)&ev);
  }

  puglview->impl->mods = mods;
}

- (BOOL)preservesContentInLiveResize
{
  return NO;
}

- (void)viewWillStartLiveResize
{
  [super viewWillStartLiveResize];
  puglDispatchSimpleEvent(puglview, PUGL_LOOP_ENTER);
}

- (void)viewWillDraw
{
  if (!puglview)
    return;
  puglDispatchSimpleEvent(puglview, PUGL_UPDATE);
  [super viewWillDraw];
}

- (void)resizeTick
{
  // redisplay works without this
  // use timer for animations
  // puglPostRedisplay(puglview);
}

- (void)viewDidEndLiveResize
{
  [super viewDidEndLiveResize];
  puglDispatchSimpleEvent(puglview, PUGL_LOOP_LEAVE);
}

@end

@interface PuglWindowDelegate : NSObject<NSWindowDelegate>

- (instancetype)initWithPuglWindow:(PuglWindow*)window;

@end

@implementation PuglWindowDelegate {
  PuglWindow* window;
}

- (instancetype)initWithPuglWindow:(PuglWindow*)puglWindow
{
  if ((self = [super init])) {
    window = puglWindow;
  }

  return self;
}

- (BOOL)windowShouldClose:(id)sender
{
  (void)sender;

  puglDispatchSimpleEvent(window->puglview, PUGL_CLOSE);
  return NO;
}

- (void)windowDidMove:(NSNotification*)notification
{
  (void)notification;

  PuglView* puglview = window->puglview;

  updateViewRect(puglview);

  PuglEventConfigure ev = {
    PUGL_CONFIGURE,
    0,
    puglview->frame.x,
    puglview->frame.y,
    puglview->frame.width,
    puglview->frame.height,
  };
  puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

@end

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

  impl->app = [NSApplication sharedApplication];

  return impl;
}

void
puglFreeWorldInternals(PuglWorld* world)
{
  releaseProcessTimer(world->impl);
  if (world->impl->worldProxy) {
    [world->impl->worldProxy setPuglWorld:NULL];
    [world->impl->worldProxy release];
    world->impl->worldProxy = NULL;
  }
  free(world->impl);

  for (int i = 0; i < 3; ++i) {
    // run the event loop shortly to let closed windows disappear
    int rslt = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
    if (rslt != kCFRunLoopRunHandledSource)
      break;
  }
}

PuglStatus
puglSetClassName(PuglWorld* const world, const char* const name)
{
  puglSetString(&world->className, name);
  return PUGL_SUCCESS;
}

void*
puglGetNativeWorld(PuglWorld* PUGL_UNUSED(world))
{
  CGContextRef contextRef =
    (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  return contextRef;
}

PuglInternals*
puglInitViewInternals(void)
{
  PuglInternals* impl = (PuglInternals*)calloc(1, sizeof(PuglInternals));

  impl->cursor = [[NSCursor arrowCursor] retain];

  return impl;
}

PuglStatus
puglRealize(PuglView* view)
{
  PuglInternals* impl = view->impl;
  if (impl->wrapperView) {
    return PUGL_FAILURE;
  }

  const NSScreen* const screen      = [NSScreen mainScreen];
  const double          scaleFactor = [screen backingScaleFactor];

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

  CGDirectDisplayID displayId = CGMainDisplayID();
  CGDisplayModeRef  mode      = CGDisplayCopyDisplayMode(displayId);

  // Try to get refresh rate from mode (usually fails)
  view->hints[PUGL_REFRESH_RATE] = (int)CGDisplayModeGetRefreshRate(mode);

  CGDisplayModeRelease(mode);
  if (view->hints[PUGL_REFRESH_RATE] == 0) {
    // Get refresh rate from a display link
    // TODO: Keep and actually use the display link for something?
    CVDisplayLinkRef link;
    CVDisplayLinkCreateWithCGDisplay(displayId, &link);

    const CVTime p = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
    const double r = p.timeScale / (double)p.timeValue;
    view->hints[PUGL_REFRESH_RATE] = (int)lrint(r);

    CVDisplayLinkRelease(link);
  }

  const NSRect framePx =
    NSMakeRect(view->reqX, view->reqY, view->reqWidth, view->reqHeight);

  const NSRect framePt = NSMakeRect(framePx.origin.x / scaleFactor,
                                    framePx.origin.y / scaleFactor,
                                    framePx.size.width / scaleFactor,
                                    framePx.size.height / scaleFactor);

  // Create wrapper view to handle input
  impl->wrapperView             = [[PuglWrapperView alloc] init];
  impl->wrapperView->puglview   = view;
  impl->wrapperView->markedText = [[NSMutableAttributedString alloc] init];
  [impl->wrapperView setAutoresizesSubviews:YES];
  [impl->wrapperView initWithFrame:framePt];
  if (view->minWidth) {
    impl->wrapperView->minWidthConstraint = 
        [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                      attribute:NSLayoutAttributeWidth
                                      relatedBy:NSLayoutRelationGreaterThanOrEqual
                                         toItem:nil
                                      attribute:NSLayoutAttributeNotAnAttribute
                                     multiplier:1.0
                                       constant:(CGFloat)view->minWidth / scaleFactor] retain];
      [impl->wrapperView addConstraint:impl->wrapperView->minWidthConstraint];
  }
  if (view->minHeight) {
    impl->wrapperView->minHeightConstraint = 
        [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                      attribute:NSLayoutAttributeHeight
                                      relatedBy:NSLayoutRelationGreaterThanOrEqual
                                         toItem:nil
                                      attribute:NSLayoutAttributeNotAnAttribute
                                     multiplier:1.0
                                       constant:(CGFloat)view->minHeight / scaleFactor] retain];
      [impl->wrapperView addConstraint:impl->wrapperView->minHeightConstraint];
  }
  if (view->maxWidth > 0) {
    impl->wrapperView->maxWidthConstraint = 
        [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                      attribute:NSLayoutAttributeWidth
                                      relatedBy:NSLayoutRelationLessThanOrEqual
                                         toItem:nil
                                      attribute:NSLayoutAttributeNotAnAttribute
                                     multiplier:1.0
                                       constant:(CGFloat)view->maxWidth / scaleFactor] retain];
    [impl->wrapperView addConstraint:impl->wrapperView->maxWidthConstraint];
  }
  if (view->maxHeight > 0) {
    impl->wrapperView->maxHeightConstraint = 
        [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                      attribute:NSLayoutAttributeHeight
                                      relatedBy:NSLayoutRelationLessThanOrEqual
                                         toItem:nil
                                      attribute:NSLayoutAttributeNotAnAttribute
                                     multiplier:1.0
                                       constant:(CGFloat)view->maxHeight / scaleFactor] retain];
    [impl->wrapperView addConstraint:impl->wrapperView->maxHeightConstraint];
  }

  [impl->wrapperView setReshaped];

  // Create draw view to be rendered to
  PuglStatus st = PUGL_SUCCESS;
  if ((st = view->backend->configure(view)) ||
      (st = view->backend->create(view))) {
    return st;
  }

  // Add draw view to wrapper view
  [impl->wrapperView addSubview:impl->drawView];
  [impl->drawView setHidden:NO];

  if (view->parent) {
    [impl->wrapperView setHidden:YES];
    NSView* pview = (NSView*)view->parent;
    [pview addSubview:impl->wrapperView];
    [impl->drawView setHidden:NO];
    [[impl->drawView window] makeFirstResponder:impl->wrapperView];
  } else {
    [impl->wrapperView setHidden:NO];

    unsigned style =
      (NSClosableWindowMask | NSTitledWindowMask | NSMiniaturizableWindowMask);
    if (view->hints[PUGL_IS_POPUP]) {
      style = NSWindowStyleMaskBorderless;
    }
    if (view->hints[PUGL_RESIZABLE]) {
      style |= NSResizableWindowMask;
    }
    NSRect contentRect = rectToScreen([NSScreen mainScreen], framePt);

    PuglWindow* window =
      [[PuglWindow alloc] initWithContentRect:contentRect
                                    styleMask:style
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [window setColorSpace:[NSColorSpace sRGBColorSpace]];
    [window setPuglview:view];

    if (view->title) {
      NSString* titleString =
        [[NSString alloc] initWithBytes:view->title
                                 length:strlen(view->title)
                               encoding:NSUTF8StringEncoding];

      [window setTitle:titleString];
      [titleString release];
    }

    if (view->hints[PUGL_RESIZABLE] && (view->minWidth || view->minHeight)) {
      [window
        setContentMinSize:sizePoints(view, view->minWidth, view->minHeight)];
    }
    if (view->hints[PUGL_RESIZABLE] && (view->maxWidth || view->maxHeight)) {
        NSSize scaled = sizePoints(view, view->maxWidth, view->maxHeight);
        if (view->maxWidth < 0) {
          scaled.width = CGFLOAT_MAX;
        }
        if (view->maxHeight < 0) {
          scaled.height = CGFLOAT_MAX;
        }
        [view->impl->window setContentMaxSize:scaled];
    }
    impl->window = window;

    PuglWindowDelegate* delegate =
      [[PuglWindowDelegate alloc] initWithPuglWindow:window];
    ((NSWindow*)window).delegate = delegate;
    [delegate release];

    if (view->hints[PUGL_RESIZABLE] && (view->minAspectX && view->minAspectY)) {
      [window setContentAspectRatio:sizePoints(view,
                                               view->minAspectX,
                                               view->minAspectY)];
    }

    [window setContentView:impl->wrapperView];

    if (view->hints[PUGL_IS_POPUP]) {
      //[window setLevel:NSPopUpMenuWindowLevel];
      [window setHasShadow:YES];
    }
    [window setFrame:[window frameRectForContentRect:contentRect] display:NO];
  }

  [impl->wrapperView updateTrackingAreas];

  puglDispatchSimpleEvent(view, PUGL_CREATE);

  return PUGL_SUCCESS;
}

PuglStatus
puglShow(PuglView* view)
{
  if (!view->impl->wrapperView) {
    const PuglStatus st = puglRealize(view);
    if (st) {
      return st;
    }
  }

  if (![view->impl->window isVisible]) {
    NSWindow* window = view->impl->window;
    if (window) {
      if (!view->impl->displayed) {
        view->impl->displayed = true;
        if (view->transientParent) {
          NSWindow* parentWindow = [(id)view->transientParent window];
          if (parentWindow) {
            if (!view->impl->posRequested) {
              // center to parent, otherwise if will appear at top left screen
              // corner
              NSRect parentFrame = [parentWindow frame];
              NSRect frame       = [window frame];
              frame.origin.x     = parentFrame.origin.x +
                               parentFrame.size.width / 2 -
                               frame.size.width / 2;
              frame.origin.y = parentFrame.origin.y +
                               parentFrame.size.height / 2 -
                               frame.size.height / 2;
              [window setFrame:frame display:NO];
            }
            [parentWindow addChildWindow:window ordered:NSWindowAbove];
          }
        } else {
          if (!view->impl->posRequested) {
            // center to screen, otherwise if will appear at top left screen
            // corner
            [window center];
          }
        }
      }
      [window setIsVisible:YES];
      if (!(view->transientParent && view->hints[PUGL_IS_POPUP])) {
        [view->world->impl->app activateIgnoringOtherApps:YES];
        [window makeFirstResponder:view->impl->wrapperView];
        [window makeKeyAndOrderFront:window];
      }
    } else {
      [view->impl->wrapperView setHidden:NO];
    }
    updateViewRect(view);

    [view->impl->drawView setNeedsDisplay:YES];
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglHide(PuglView* view)
{
  NSWindow* window = view->impl->window;
  if (window) {
    [view->impl->window setIsVisible:NO];
  } else {
    [view->impl->wrapperView setHidden:YES];
  }
  return PUGL_SUCCESS;
}

void
puglFreeViewInternals(PuglView* view)
{
  if (view) {
    if (view->backend) {
      view->backend->destroy(view);
    }

    if (view->impl) {
      PuglWrapperView* wrapperView = view->impl->wrapperView;
      if (wrapperView) {
        if (wrapperView->trackingArea) {
          [wrapperView->trackingArea release];
          wrapperView->trackingArea = NULL;
        }
        if (wrapperView->markedText) {
          [wrapperView->markedText release];
          wrapperView->markedText = NULL;
        }
        if (wrapperView->minWidthConstraint) {
          [wrapperView->minWidthConstraint release];
          wrapperView->minWidthConstraint = NULL;
        }
        if (wrapperView->minHeightConstraint) {
          [wrapperView->minHeightConstraint release];
          wrapperView->minHeightConstraint = NULL;
        }
        if (wrapperView->maxWidthConstraint) {
          [wrapperView->maxWidthConstraint release];
          wrapperView->maxWidthConstraint = NULL;
        }
        if (wrapperView->maxHeightConstraint) {
          [wrapperView->maxHeightConstraint release];
          wrapperView->maxHeightConstraint = NULL;
        }
        [wrapperView removeFromSuperview];
        wrapperView->puglview = NULL;
        [wrapperView release];
        view->impl->wrapperView = NULL;
      }
      if (view->impl->window) {
        view->impl->window->puglview = NULL;
        [view->impl->window close];
        [view->impl->window release];
        view->impl->window = NULL;
      }
      if (view->impl->cursor) {
        [view->impl->cursor release];
        view->impl->cursor = NULL;
      }
      free(view->impl);
      view->impl = NULL;
    }
  }
}

PuglStatus
puglGrabFocus(PuglView* view)
{
  NSWindow* window = [view->impl->wrapperView window];
  if (window) {
    [window makeKeyWindow];
    [window makeFirstResponder:view->impl->wrapperView];
  }
  return PUGL_SUCCESS;
}

bool
puglHasFocus(const PuglView* view)
{
  PuglInternals* const impl = view->impl;

  return ([[impl->wrapperView window] isKeyWindow] &&
          [[impl->wrapperView window] firstResponder] == impl->wrapperView);
}

PuglStatus
puglRequestAttention(PuglView* view)
{
  if (![view->impl->window isKeyWindow]) {
    [view->world->impl->app requestUserAttention:NSInformationalRequest];
  }

  return PUGL_SUCCESS;
}

void
puglSetProcessFunc(PuglWorld*      world,
                   PuglProcessFunc processFunc,
                   void*           userData)
{
  world->processFunc     = processFunc;
  world->processUserData = userData;
  if (processFunc) {
    if (!world->impl->worldProxy) {
      world->impl->worldProxy = [[PuglWorldProxy alloc] init];
      [world->impl->worldProxy setPuglWorld:world];
    }
  }
}

void
puglSetNextProcessTime(PuglWorld* world, double seconds)
{
  if (seconds >= 0) {
    world->impl->nextProcessTime = puglGetTime(world) + seconds;
    if (!world->impl->worldProxy) {
      world->impl->worldProxy = [[PuglWorldProxy alloc] init];
      [world->impl->worldProxy setPuglWorld:world];
    }
    rescheduleProcessTimer(world);
  } else {
    world->impl->nextProcessTime = -1;
    releaseProcessTimer(world->impl);
  }
}

void
puglAwake(PuglWorld* world)
{
  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];
  if (world->impl->worldProxy) {
    [world->impl->worldProxy
      performSelectorOnMainThread:@selector(awakeCallback)
                       withObject:nil
                    waitUntilDone:NO];
  }
  [localPool release];
}

PuglStatus
puglSendEvent(PuglView* view, const PuglEvent* event)
{
  if (event->type == PUGL_CLIENT) {
    PuglWrapperView* wrapper = view->impl->wrapperView;
    const NSWindow*  window  = [wrapper window];
    const NSRect     rect    = [wrapper frame];
    const NSPoint    center  = {NSMidX(rect), NSMidY(rect)};

    NSEvent* nsevent =
      [NSEvent otherEventWithType:NSApplicationDefined
                         location:center
                    modifierFlags:0
                        timestamp:[[NSProcessInfo processInfo] systemUptime]
                     windowNumber:window.windowNumber
                          context:nil
                          subtype:PUGL_CLIENT
                            data1:(NSInteger)event->client.data1
                            data2:(NSInteger)event->client.data2];

    [view->world->impl->app postEvent:nsevent atStart:false];
    return PUGL_SUCCESS;
  }

  return PUGL_UNSUPPORTED_TYPE;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglWaitForEvent(PuglView* view)
{
  return puglPollEvents(view->world, -1.0);
}
#endif

static void
dispatchClientEvent(PuglWorld* world, NSEvent* ev)
{
  NSWindow* win = [ev window];
  NSPoint   loc = [ev locationInWindow];
  for (size_t i = 0; i < world->numViews; ++i) {
    PuglView*        view    = world->views[i];
    PuglWrapperView* wrapper = view->impl->wrapperView;
    if ([wrapper window] == win && NSPointInRect(loc, [wrapper frame])) {
      PuglEventClient event = {
        PUGL_CLIENT, 0, (uintptr_t)[ev data1], (uintptr_t)[ev data2]};

      puglDispatchEvent(view, (PuglEvent*)&event);
    }
  }
}

PuglStatus
puglUpdate(PuglWorld* world, const double timeout)
{
  NSDate* date =
    ((timeout < 0) ? [NSDate distantFuture]
                   : [NSDate dateWithTimeIntervalSinceNow:timeout]);
  bool wasPolling = world->impl->polling;
  world->impl->polling = true;
  bool hadEvent = false;
  for (NSEvent* ev = NULL;
       (ev = [world->impl->app nextEventMatchingMask:NSAnyEventMask
                                           untilDate:date
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]);) {
    if ([ev type] == NSApplicationDefined && [ev subtype] == PUGL_CLIENT) {
      dispatchClientEvent(world, ev);
    }

    [world->impl->app sendEvent:ev];

    if (!hadEvent) {
      // Now that we've waited and got an event, set the date to now to
      // avoid looping forever
      date = [NSDate date];
    }
    hadEvent = true;
  }
  world->impl->polling = wasPolling;

  // the following does not work, at least not under Mac OS 10.15.5
  // 1.) Even after setting [view->impl->drawView setNeedsDisplay:YES]
  //     [view->impl->drawView needsDisplay] gives NO.
  // 2.) [view->impl->drawView displayIfNeeded] does always invoke drawRect
  //     for NSOpenGLView.
  //	for (size_t i = 0; i < world->numViews; ++i) {
  //		PuglView* const view = world->views[i];
  //
  //                bool needed = [view->impl->drawView needsDisplay];
  //
  //		if ([[view->impl->drawView window] isVisible]) {
  //			puglDispatchSimpleEvent(view, PUGL_UPDATE);
  //		}
  //
  //		[view->impl->drawView displayIfNeeded];
  //	}

  return hadEvent ? PUGL_SUCCESS : PUGL_FAILURE;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglProcessEvents(PuglView* view)
{
  return puglDispatchEvents(view->world);
}
#endif

double
puglGetTime(const PuglWorld* world)
{
  return (mach_absolute_time() / 1e9) - world->startTime;
}

PuglStatus
puglPostRedisplay(PuglView* view)
{
  view->rects.rectsCount = 1;
  PuglRect* r            = view->rects.rectsList;
  r->x                   = 0;
  r->y                   = 0;
  r->width               = ceil(view->frame.width);
  r->height              = ceil(view->frame.height);
  [view->impl->drawView setNeedsDisplay:YES];
  return PUGL_SUCCESS;
}

PuglStatus
puglPostRedisplayRect(PuglView* view, PuglRect rect)
{
  integerRect(&rect);
  if (    rect.x + rect.width  > 0 && rect.x < view->frame.width
       && rect.y + rect.height > 0 && rect.y < view->frame.height)
  {
    if (addRect(&view->rects, &rect)) {
      const NSRect rectPx = rectToNsRect(rect);
      [view->impl->drawView setNeedsDisplayInRect:nsRectToPoints(view, rectPx)];
    } else {
      return puglPostRedisplay(view);
    }
  }
  return PUGL_SUCCESS;
}

PuglNativeView
puglGetNativeWindow(PuglView* view)
{
  return (PuglNativeView)view->impl->wrapperView;
}

PuglStatus
puglSetWindowTitle(PuglView* view, const char* title)
{
  puglSetString(&view->title, title);

  if (view->impl->window) {
    NSString* titleString =
      [[NSString alloc] initWithBytes:title
                               length:strlen(title)
                             encoding:NSUTF8StringEncoding];

    [view->impl->window setTitle:titleString];
    [titleString release];
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

  view->impl->posRequested = true;

  PuglInternals* const impl = view->impl;

  if (!impl->window && !impl->wrapperView)
    return PUGL_SUCCESS;

  const NSRect framePx = rectToNsRect(frame);
  const NSRect framePt = nsRectToPoints(view, framePx);
  if (impl->window) {
    // Resize window to fit new content rect
    const NSRect screenPt = rectToScreen(viewScreen(view), framePt);
    const NSRect winFrame = [impl->window frameRectForContentRect:screenPt];

    [impl->window setFrame:winFrame display:NO];
  }
  // Resize views
  const NSRect sizePx = NSMakeRect(0, 0, frame.width, frame.height);
  const NSRect sizePt = nsRectToPoints(view, sizePx);

  [impl->wrapperView setFrame:(impl->window ? sizePt : framePt)];
  [impl->drawView setFrame:sizePt];

  return PUGL_SUCCESS;
}

PuglStatus
puglSetSize(PuglView* view, int width, int height)
{
  view->reqWidth  = width;
  view->reqHeight = height;

  PuglInternals* const impl = view->impl;

  if (impl->window) {
    NSRect rectPt = [impl->window contentRectForFrameRect:[impl->window frame]];
    NSSize oldSizePt = rectPt.size;
    rectPt.size      = sizePoints(view, width, height);
    rectPt.origin.y += oldSizePt.height - rectPt.size.height;

    // Resize window to fit new content rect
    const NSRect windowFrame = [impl->window frameRectForContentRect:rectPt];

    [impl->window setFrame:windowFrame display:NO];

    // Resize views
    const NSRect sizePx = NSMakeRect(0, 0, width, height);
    const NSRect sizePt = nsRectToPoints(view, sizePx);
    [impl->wrapperView setFrame:sizePt];
    [impl->drawView setFrame:sizePt];
  } else if (impl->wrapperView) {
    NSRect rectPt    = [impl->wrapperView frame];
    NSSize oldSizePt = rectPt.size;
    rectPt.size      = sizePoints(view, width, height);
    // rectPt.origin.y += oldSizePt.height - rectPt.size.height;
    [impl->wrapperView setFrame:rectPt];

    const NSRect sizePx = NSMakeRect(0, 0, width, height);
    const NSRect sizePt = nsRectToPoints(view, sizePx);
    [impl->drawView setFrame:sizePt];
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglSetMinSize(PuglView* const view, const int width, const int height)
{
  const NSScreen* const screen      = [NSScreen mainScreen];
  const double          scaleFactor = [screen backingScaleFactor];

  view->minWidth  = width;
  view->minHeight = height;

  if (view->impl->window && (view->minWidth || view->minHeight)) {
    [view->impl->window
      setContentMinSize:sizePoints(view, view->minWidth, view->minHeight)];
  }
  PuglInternals* impl = view->impl;
  if (impl->wrapperView) {
    if (impl->wrapperView->minHeightConstraint) {
      [impl->wrapperView removeConstraint: impl->wrapperView->minHeightConstraint];
      [impl->wrapperView->minHeightConstraint release];
      impl->wrapperView->minHeightConstraint = NULL;
    }
    
    if (impl->wrapperView->minWidthConstraint) {
      [impl->wrapperView removeConstraint: impl->wrapperView->minWidthConstraint];
      [impl->wrapperView->minWidthConstraint release];
      impl->wrapperView->minWidthConstraint = NULL;
    }
    if (width > 0) {
      impl->wrapperView->minWidthConstraint = 
          [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                        attribute:NSLayoutAttributeWidth
                                        relatedBy:NSLayoutRelationGreaterThanOrEqual
                                           toItem:nil
                                        attribute:NSLayoutAttributeNotAnAttribute
                                       multiplier:1.0
                                         constant:(CGFloat)view->minWidth / scaleFactor] retain];
        [impl->wrapperView addConstraint:impl->wrapperView->minWidthConstraint];
    }
    if (height > 0) {
      impl->wrapperView->minHeightConstraint = 
          [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                        attribute:NSLayoutAttributeHeight
                                        relatedBy:NSLayoutRelationGreaterThanOrEqual
                                           toItem:nil
                                        attribute:NSLayoutAttributeNotAnAttribute
                                       multiplier:1.0
                                         constant:(CGFloat)view->minHeight / scaleFactor] retain];
        [impl->wrapperView addConstraint:impl->wrapperView->minHeightConstraint];
    }
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglSetMaxSize(PuglView* const view, const int width, const int height)
{
  const NSScreen* const screen      = [NSScreen mainScreen];
  const double          scaleFactor = [screen backingScaleFactor];

  view->maxWidth  = width;
  view->maxHeight = height;

  if (view->impl->window && (width || height)) {
    NSSize scaled = sizePoints(view, width, height);
    if (width < 0) {
      scaled.width = CGFLOAT_MAX;
    }
    if (height < 0) {
      scaled.height = CGFLOAT_MAX;
    }

    [view->impl->window setContentMaxSize:scaled];
  }

  PuglInternals* impl = view->impl;
  if (impl->wrapperView) {
    if (impl->wrapperView->maxHeightConstraint) {
      [impl->wrapperView removeConstraint: impl->wrapperView->maxHeightConstraint];
      [impl->wrapperView->maxHeightConstraint release];
      impl->wrapperView->maxHeightConstraint = NULL;
    }
    if (impl->wrapperView->maxWidthConstraint) {
      [impl->wrapperView removeConstraint: impl->wrapperView->maxWidthConstraint];
      [impl->wrapperView->maxWidthConstraint release];
      impl->wrapperView->maxWidthConstraint = NULL;
    }
    if (width > 0) {
      impl->wrapperView->maxWidthConstraint = 
          [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                        attribute:NSLayoutAttributeWidth
                                        relatedBy:NSLayoutRelationLessThanOrEqual
                                           toItem:nil
                                        attribute:NSLayoutAttributeNotAnAttribute
                                       multiplier:1.0
                                         constant:(CGFloat)view->maxWidth / scaleFactor] retain];
        [impl->wrapperView addConstraint:impl->wrapperView->maxWidthConstraint];
    }
    if (height > 0) {
      impl->wrapperView->maxHeightConstraint = 
          [[NSLayoutConstraint constraintWithItem:impl->wrapperView
                                        attribute:NSLayoutAttributeHeight
                                        relatedBy:NSLayoutRelationLessThanOrEqual
                                           toItem:nil
                                        attribute:NSLayoutAttributeNotAnAttribute
                                       multiplier:1.0
                                         constant:(CGFloat)view->maxHeight / scaleFactor] retain];
        [impl->wrapperView addConstraint:impl->wrapperView->maxHeightConstraint];
    }
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

  if (view->impl->window && view->minAspectX && view->minAspectY) {
    [view->impl->window setContentAspectRatio:sizePoints(view,
                                                         view->minAspectX,
                                                         view->minAspectY)];
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeView parent)
{
  if (view->impl->wrapperView) {
    return PUGL_FAILURE;
  }
  view->transientParent = parent;
  return PUGL_SUCCESS;
}

PuglStatus
puglRequestClipboard(PuglView* view)
{
  NSPasteboard* const pasteboard = [NSPasteboard generalPasteboard];

  if ([[pasteboard types] containsObject:NSStringPboardType]) {
    const NSString* str  = [pasteboard stringForType:NSStringPboardType];
    const char*     utf8 = [str UTF8String];

    puglSetBlob(&view->clipboard, utf8, strlen(utf8));

    [view->impl->wrapperView
      performSelectorOnMainThread:@selector(clipboardReceived)
                       withObject:nil
                    waitUntilDone:NO];
  }

  return PUGL_SUCCESS;
}

static NSCursor*
puglGetNsCursor(const PuglCursor cursor)
{
  switch (cursor) {
  case PUGL_CURSOR_ARROW:
  case PUGL_CURSOR_HIDDEN:
    return [NSCursor arrowCursor];
  case PUGL_CURSOR_CARET:
    return [NSCursor IBeamCursor];
  case PUGL_CURSOR_CROSSHAIR:
    return [NSCursor crosshairCursor];
  case PUGL_CURSOR_HAND:
    return [NSCursor pointingHandCursor];
  case PUGL_CURSOR_NO:
    return [NSCursor operationNotAllowedCursor];
  case PUGL_CURSOR_LEFT_RIGHT:
    return [NSCursor resizeLeftRightCursor];
  case PUGL_CURSOR_UP_DOWN:
    return [NSCursor resizeUpDownCursor];
  }

  return NULL;
}

PuglStatus
puglSetCursor(PuglView* view, PuglCursor cursor)
{
  PuglInternals* const impl = view->impl;
  NSCursor*            cur  = puglGetNsCursor(cursor);
  if (!cur) {
    return PUGL_FAILURE;
  }
  if (impl->cursor)
    [impl->cursor release];
  impl->cursor = [cur retain];
  if (cursor == PUGL_CURSOR_HIDDEN) {
    impl->shouldCursorHidden = true;
    if (!impl->isCursorHidden && (impl->mouseTracked || impl->mouseButtons)) {
      [NSCursor hide];
      impl->isCursorHidden = true;
    }
  } else {
    impl->shouldCursorHidden = false;
    if (impl->isCursorHidden) {
      [NSCursor unhide];
      impl->isCursorHidden = false;
    }
  }
  if (impl->mouseTracked || impl->mouseButtons) {
    [cur set];
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetClipboard(PuglWorld*  world,
                 const char* type,
                 const void* data,
                 size_t      len)
{
  NSPasteboard* const pasteboard = [NSPasteboard generalPasteboard];
  const char* const   str        = (const char*)data;

  PuglStatus st = puglSetInternalClipboard(world, type, data, len);
  if (st) {
    return st;
  }

  NSString* nsString = [NSString stringWithUTF8String:str];
  if (nsString) {
    [pasteboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil]
                       owner:nil];

    [pasteboard setString:nsString forType:NSStringPboardType];

    return PUGL_SUCCESS;
  }

  return PUGL_UNKNOWN_ERROR;
}

bool
puglHasClipboard(PuglWorld* world)
{
  return false;
}

double
puglGetScreenScale(PuglView* view)
{
  return [viewScreen(view) backingScaleFactor];
}

double
puglGetDefaultScreenScale(PuglWorld* world)
{
  return [[NSScreen mainScreen] backingScaleFactor];
}
