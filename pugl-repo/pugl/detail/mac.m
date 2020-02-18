/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>
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

/**
   @file mac.m MacOS implementation.
*/

#define GL_SILENCE_DEPRECATION 1

#include "pugl/detail/implementation.h"
#include "pugl/detail/mac.h"
#include "pugl/pugl.h"
#include "pugl/pugl_stub.h"

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
rectToScreen(NSRect rect)
{
	const double screenHeight = [[NSScreen mainScreen] frame].size.height;

	rect.origin.y = screenHeight - rect.origin.y - rect.size.height;
	return rect;
}

static void
updateViewRect(PuglView* view)
{
	NSWindow* const window = view->impl->window;
	if (window) {
		const double screenHeight = [[NSScreen mainScreen] frame].size.height;
		const NSRect frame        = [window frame];
		const NSRect content      = [window contentRectForFrameRect:frame];

		view->frame.x      = content.origin.x;
		view->frame.y      = screenHeight - content.origin.y - content.size.height;
		view->frame.width  = content.size.width;
		view->frame.height = content.size.height;
	}
}

@interface PuglWorldProxy : NSObject
{
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
- (void) processTimerCallback
{
        if (world) {
                PuglWorldInternals* impl = world->impl;
                if (impl->nextProcessTime >= 0 && impl->nextProcessTime <= puglGetTime(world)) {
                    releaseProcessTimer(impl);
                    impl->nextProcessTime = -1;
                    if (world->processFunc) {
                        world->processFunc(world, world->processUserData);
                        if (world->impl->polling) {
                            NSEvent* event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                                                                location: NSMakePoint(0,0)
                                                            modifierFlags: 0
                                                                timestamp: 0.0
                                                             windowNumber: 0
                                                                  context: nil
                                                                  subtype: 0
                                                                    data1: 0
                                                                    data2: 0];
                            [world->impl->app postEvent:event atStart:false];
                        }
                    }
                }
                else {
                   rescheduleProcessTimer(world);
                }
        }

}
- (void) awakeCallback
{
	if (world) {
		if (world->processFunc) {
		    world->processFunc(world, world->processUserData);
		    if (world->impl->polling) {
		        NSEvent* event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
		                                            location: NSMakePoint(0,0)
                                                       modifierFlags: 0
                                                           timestamp: 0.0
                                                        windowNumber: 0
                                                             context: nil
                                                             subtype: 0
                                                               data1: 0
                                                               data2: 0];
		        [world->impl->app postEvent:event atStart:false];
		    }   
		}
	}
}
- (void) setPuglWorld:(PuglWorld*)w
{
	world = w;
}
@end

@implementation PuglWindow

- (id) initWithContentRect:(NSRect)contentRect
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
	[self setContentSize:NSMakeSize(view->frame.width, view->frame.height)];
}

- (BOOL) canBecomeKeyWindow
{
	return puglview && !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (BOOL) canBecomeMainWindow
{
	return puglview && !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

@end

@implementation PuglWrapperView

- (void) resizeWithOldSuperviewSize:(NSSize)oldSize
{
	[super resizeWithOldSuperviewSize:oldSize];

        if (!puglview) return;
        
	const NSRect bounds = [self bounds];
	puglview->backend->resize(puglview, bounds.size.width, bounds.size.height);
}

- (void) dispatchExpose:(NSRect)rect
{
        if (!puglview) return;
        
	if (reshaped) {
		updateViewRect(puglview);

		PuglEventConfigure ev =  {
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

	PuglEventExpose ev = {
		PUGL_EXPOSE,
		0,
		rect.origin.x,
		rect.origin.y,
		rect.size.width,
		rect.size.height,
		0
	};

	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (BOOL) isFlipped
{
	return YES;
}

- (BOOL) acceptsFirstResponder
{
	return puglview && !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (BOOL) becomeFirstResponder
{
	return puglview && !(puglview->transientParent && puglview->hints[PUGL_IS_POPUP]);
}

- (void) setReshaped
{
	reshaped = true;
}

- (void) clipboardReceived
{
	if (puglview && puglview->clipboard.data) {
	    PuglEvent ev = {0};
            ev.received.type = PUGL_DATA_RECEIVED;
            ev.received.data = puglview->clipboard.data;
            ev.received.len  = puglview->clipboard.len;
            puglview->clipboard.data = NULL;
            puglview->clipboard.len = 0;
	    puglDispatchEvent(puglview, &ev);
	}
}

static uint32_t
getModifiers(const NSEvent* const ev)
{
	const NSEventModifierFlags modifierFlags = [ev modifierFlags];

	return (((modifierFlags & NSShiftKeyMask)     ? PUGL_MOD_SHIFT : 0) |
	        ((modifierFlags & NSControlKeyMask)   ? PUGL_MOD_CTRL  : 0) |
	        ((modifierFlags & NSAlternateKeyMask) ? PUGL_MOD_ALT   : 0) |
	        ((modifierFlags & NSCommandKeyMask)   ? PUGL_MOD_SUPER : 0));
}

static PuglKey
keySymToSpecial(const NSEvent* const ev)
{
	NSString* chars = [ev charactersIgnoringModifiers];
	if ([chars length] == 1) {
		switch ([chars characterAtIndex:0]) {
		case NSDeleteCharacter:         return PUGL_KEY_BACKSPACE;
		case NSTabCharacter:            return PUGL_KEY_TAB;
		case NSCarriageReturnCharacter: return PUGL_KEY_RETURN;
		case NSEnterCharacter:          return PUGL_KEY_KP_ENTER;
		case 0x001B:                    return PUGL_KEY_ESCAPE;
		case NSDeleteFunctionKey:       return PUGL_KEY_DELETE;
		case NSF1FunctionKey:           return PUGL_KEY_F1;
		case NSF2FunctionKey:           return PUGL_KEY_F2;
		case NSF3FunctionKey:           return PUGL_KEY_F3;
		case NSF4FunctionKey:           return PUGL_KEY_F4;
		case NSF5FunctionKey:           return PUGL_KEY_F5;
		case NSF6FunctionKey:           return PUGL_KEY_F6;
		case NSF7FunctionKey:           return PUGL_KEY_F7;
		case NSF8FunctionKey:           return PUGL_KEY_F8;
		case NSF9FunctionKey:           return PUGL_KEY_F9;
		case NSF10FunctionKey:          return PUGL_KEY_F10;
		case NSF11FunctionKey:          return PUGL_KEY_F11;
		case NSF12FunctionKey:          return PUGL_KEY_F12;
		case NSLeftArrowFunctionKey:    return PUGL_KEY_LEFT;
		case NSUpArrowFunctionKey:      return PUGL_KEY_UP;
		case NSRightArrowFunctionKey:   return PUGL_KEY_RIGHT;
		case NSDownArrowFunctionKey:    return PUGL_KEY_DOWN;
		case NSPageUpFunctionKey:       return PUGL_KEY_PAGE_UP;
		case NSPageDownFunctionKey:     return PUGL_KEY_PAGE_DOWN;
		case NSHomeFunctionKey:         return PUGL_KEY_HOME;
		case NSEndFunctionKey:          return PUGL_KEY_END;
		case NSInsertFunctionKey:       return PUGL_KEY_INSERT;
		case NSMenuFunctionKey:         return PUGL_KEY_MENU;
		case NSScrollLockFunctionKey:   return PUGL_KEY_SCROLL_LOCK;
		case NSClearLineFunctionKey:    return PUGL_KEY_NUM_LOCK;
		case NSPrintScreenFunctionKey:  return PUGL_KEY_PRINT_SCREEN;
		case NSPauseFunctionKey:        return PUGL_KEY_PAUSE;
		default:                        break;		}
		// SHIFT, CTRL, ALT, and SUPER are handled in [flagsChanged]
	}
	return (PuglKey)0;
}

- (void) updateTrackingAreas
{
	if (trackingArea != nil) {
		[self removeTrackingArea:trackingArea];
		[trackingArea release];
	}

	const int opts = (NSTrackingMouseEnteredAndExited |
	                  NSTrackingMouseMoved |
	                  NSTrackingActiveAlways);
	trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
	                                            options:opts
	                                              owner:self
	                                           userInfo:nil];
	[self addTrackingArea:trackingArea];
	[super updateTrackingAreas];
}

- (NSPoint) eventLocation:(NSEvent*)event
{
	return [self convertPoint:[event locationInWindow] fromView:nil];
}

static void
handleCrossing(PuglWrapperView* view, NSEvent* event, const PuglEventType type)
{
	const NSPoint     wloc = [view eventLocation:event];
	const NSPoint     rloc = [NSEvent mouseLocation];
	PuglEventCrossing ev = {
		type,
		0,
		[event timestamp],
		wloc.x,
		wloc.y,
		rloc.x,
		[[NSScreen mainScreen] frame].size.height - rloc.y,
		getModifiers(event),
		PUGL_CROSSING_NORMAL
	};
	puglDispatchEvent(view->puglview, (PuglEvent*)&ev);
}

- (void) mouseEntered:(NSEvent*)event
{
	mouseEntered = true;
	handleCrossing(self, event, PUGL_ENTER_NOTIFY);
}

- (void) mouseExited:(NSEvent*)event
{
	mouseEntered = false;
	handleCrossing(self, event, PUGL_LEAVE_NOTIFY);
}

- (void) mouseMoved:(NSEvent*)event
{
	const NSPoint   wloc = [self eventLocation:event];
	const NSPoint   rloc = [NSEvent mouseLocation];
	PuglEventMotion ev   = {
		PUGL_MOTION_NOTIFY,
		0,
		[event timestamp],
		wloc.x,
		wloc.y,
		rloc.x,
		[[NSScreen mainScreen] frame].size.height - rloc.y,
		getModifiers(event),
		0,
		1
	};
	if (mouseButtons || mouseEntered) {
	    puglDispatchEvent(puglview, (PuglEvent*)&ev);
	}
}

- (void) mouseDragged:(NSEvent*)event
{
	[self mouseMoved: event];
}

- (void) rightMouseDragged:(NSEvent*)event
{
	[self mouseMoved: event];
}

- (void) otherMouseDragged:(NSEvent*)event
{
	[self mouseMoved: event];
}

- (void) mouseDown2:(NSEvent*)event button:(uint32_t)buttonNumber
{
	const NSPoint   wloc = [self eventLocation:event];
	const NSPoint   rloc = [NSEvent mouseLocation];
	PuglEventButton ev   = {
		PUGL_BUTTON_PRESS,
		0,
		[event timestamp],
		wloc.x,
		wloc.y,
		rloc.x,
		[[NSScreen mainScreen] frame].size.height - rloc.y,
		getModifiers(event),
		buttonNumber
	};
	if (1 <= buttonNumber && buttonNumber <= 32) {
	    mouseButtons |= (1 << (buttonNumber - 1));
	}
	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void) mouseDown:(NSEvent*)event
{
    if (puglview->transientParent && puglview->hints[PUGL_IS_POPUP]) {
        [NSApp preventWindowOrdering];
    }
    [self mouseDown2:event button:1];
}

- (void) mouseUp2:(NSEvent*)event button:(uint32_t)buttonNumber
{
	const NSPoint   wloc = [self eventLocation:event];
	const NSPoint   rloc = [NSEvent mouseLocation];
	PuglEventButton ev   = {
		PUGL_BUTTON_RELEASE,
		0,
		[event timestamp],
		wloc.x,
		wloc.y,
		rloc.x,
		[[NSScreen mainScreen] frame].size.height - rloc.y,
		getModifiers(event),
		buttonNumber
	};
	if (1 <= buttonNumber && buttonNumber <= 32) {
	    mouseButtons &= ~(1 << (buttonNumber - 1));
	}
	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void) mouseUp:(NSEvent*)event
{
    [self mouseUp2:event button:1];
}

- (void) rightMouseDown:(NSEvent*)event
{
	[self mouseDown2:event button:3];
}

- (void) rightMouseUp:(NSEvent*)event
{
	[self mouseUp2:event button:3];
}

- (void) otherMouseDown:(NSEvent*)event
{
	[self mouseDown2:event button:2];
}

- (void) otherMouseUp:(NSEvent*)event
{
	[self mouseUp2:event button:2];
}

- (void) scrollWheel:(NSEvent*)event
{
	const NSPoint   wloc = [self eventLocation:event];
	const NSPoint   rloc = [NSEvent mouseLocation];
	PuglEventScroll ev   = {
		PUGL_SCROLL,
		0,
		[event timestamp],
		wloc.x,
		wloc.y,
		rloc.x,
		[[NSScreen mainScreen] frame].size.height - rloc.y,
		getModifiers(event),
		[event scrollingDeltaX],
		[event scrollingDeltaY]
	};
	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void) keyDown:(NSEvent*)event
{
	if (puglview->hints[PUGL_IGNORE_KEY_REPEAT] && [event isARepeat]) {
		return;
	}

	const NSPoint      wloc  = [self eventLocation:event];
	const NSPoint      rloc  = [NSEvent mouseLocation];
	const PuglKey      spec  = keySymToSpecial(event);
	const NSString*    chars = [event charactersIgnoringModifiers];
	const char*        str   = [[chars lowercaseString] UTF8String];
	const uint32_t     code  = (
		spec ? spec : puglDecodeUTF8((const uint8_t*)str));

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
		(code != 0xFFFD) ? code : 0
	};

	if (!spec) {
	        processingKeyEvent = &ev;
		[self interpretKeyEvents:@[event]];
		processingKeyEvent = NULL;
	}
	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void) keyUp:(NSEvent*)event
{
	const NSPoint      wloc  = [self eventLocation:event];
	const NSPoint      rloc  = [NSEvent mouseLocation];
	const PuglKey      spec  = keySymToSpecial(event);
	const NSString*    chars = [event charactersIgnoringModifiers];
	const char*        str   = [[chars lowercaseString] UTF8String];
	const uint32_t     code  =
		(spec ? spec : puglDecodeUTF8((const uint8_t*)str));

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
		(code != 0xFFFD) ? code : 0
	};
	puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (BOOL) hasMarkedText
{
	return [markedText length] > 0;
}

- (NSRange) markedRange
{
	return (([markedText length] > 0)
	        ? NSMakeRange(0, [markedText length] - 1)
	        : NSMakeRange(NSNotFound, 0));
}

- (NSRange) selectedRange
{
	return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selected
     replacementRange:(NSRange)replacement
{
	(void)selected;
	(void)replacement;
	[markedText release];
	markedText = (
		[string isKindOfClass:[NSAttributedString class]]
		? [[NSMutableAttributedString alloc] initWithAttributedString:string]
		: [[NSMutableAttributedString alloc] initWithString:string]);
}

- (void) unmarkText
{
	[[markedText mutableString] setString:@""];
}

- (NSArray*) validAttributesForMarkedText
{
	return @[];
}

- (NSAttributedString*)
 attributedSubstringForProposedRange:(NSRange)range
                         actualRange:(NSRangePointer)actual
{
	(void)range;
	(void)actual;
	return nil;
}

- (NSUInteger) characterIndexForPoint:(NSPoint)point
{
	(void)point;
	return 0;
}

- (NSRect) firstRectForCharacterRange:(NSRange)range
                          actualRange:(NSRangePointer)actual
{
	(void)range;
	(void)actual;

	const NSRect frame = [(id)puglview bounds];
	return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void) doCommandBySelector:(SEL)selector
{
	(void)selector;
}

- (void) insertText:(id)string
   replacementRange:(NSRange)replacement
{
	(void)replacement;

	NSString* const characters =
		([string isKindOfClass:[NSAttributedString class]]
		 ? [string string]
		 : (NSString*)string);
        
        const char* bytes = [characters UTF8String];
        size_t len = strlen(bytes);

        if (processingKeyEvent) {
            PuglEventKey* event = processingKeyEvent;
            char* newInput;
            if (event->inputLength + len > 8) {
                if (event->inputLength > 8)  {
                    newInput = realloc(event->input.ptr, event->inputLength + len);
                }
                else {
                    newInput = malloc(event->inputLength + len);
                    if (newInput) memcpy(newInput, event->input.data, event->inputLength);
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
            PuglEventKey   event = { PUGL_KEY_PRESS,
                                     0,
                                     [ev timestamp],
                                     wloc.x,
                                     wloc.y,
                                     rloc.x,
                                     [[NSScreen mainScreen] frame].size.height - rloc.y,
                                     getModifiers(ev),
                                     0, // keycode
                                     0xffff }; // key
            char* newInput;
            if (len > 8) {
                newInput = malloc(len);
                if (newInput) event.input.ptr = newInput;
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

- (void) flagsChanged:(NSEvent*)event
{
	const uint32_t mods    = getModifiers(event);
	PuglEventType  type    = PUGL_NOTHING;
	PuglKey        special = 0;

	if ((mods & PUGL_MOD_SHIFT) != (puglview->impl->mods & PUGL_MOD_SHIFT)) {
		type = mods & PUGL_MOD_SHIFT ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
		special = PUGL_KEY_SHIFT;
	} else if ((mods & PUGL_MOD_CTRL) != (puglview->impl->mods & PUGL_MOD_CTRL)) {
		type = mods & PUGL_MOD_CTRL ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
		special = PUGL_KEY_CTRL;
	} else if ((mods & PUGL_MOD_ALT) != (puglview->impl->mods & PUGL_MOD_ALT)) {
		type = mods & PUGL_MOD_ALT ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
		special = PUGL_KEY_ALT;
	} else if ((mods & PUGL_MOD_SUPER) != (puglview->impl->mods & PUGL_MOD_SUPER)) {
		type = mods & PUGL_MOD_SUPER ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE;
		special = PUGL_KEY_SUPER;
	}

	if (special != 0) {
		const NSPoint wloc = [self eventLocation:event];
		const NSPoint rloc = [NSEvent mouseLocation];
		PuglEventKey  ev   = {
			type,
			0,
			[event timestamp],
			wloc.x,
			wloc.y,
			rloc.x,
			[[NSScreen mainScreen] frame].size.height - rloc.y,
			mods,
			[event keyCode],
			special
		};
		puglDispatchEvent(puglview, (PuglEvent*)&ev);
	}

	puglview->impl->mods = mods;
}

- (BOOL) preservesContentInLiveResize
{
	return NO;
}

- (void) viewWillStartLiveResize
{
	[super viewWillStartLiveResize];
}

- (void) resizeTick
{
	puglPostRedisplay(puglview);
}

- (void) urgentTick
{
    [puglview->world->impl->app requestUserAttention:NSInformationalRequest];
}

- (void) viewDidEndLiveResize
{
	[super viewDidEndLiveResize];
}

@end

@interface PuglWindowDelegate : NSObject<NSWindowDelegate>
{
	PuglWindow* window;
}

- (instancetype) initWithPuglWindow:(PuglWindow*)window;

@end

@implementation PuglWindowDelegate

- (instancetype) initWithPuglWindow:(PuglWindow*)puglWindow
{
	if ((self = [super init])) {
		window = puglWindow;
	}

	return self;
}

- (BOOL) windowShouldClose:(id)sender
{
	(void)sender;

	PuglEvent ev = { 0 };
	ev.type = PUGL_CLOSE;
	puglDispatchEvent(window->puglview, &ev);
	return NO;
}

- (void) windowDidMove:(NSNotification*)notification
{
	(void)notification;

	PuglView* puglview = window->puglview;
	
	updateViewRect(puglview);
	
       	PuglEventConfigure ev =  {
       		PUGL_CONFIGURE,
       		0,
		puglview->frame.x,
		puglview->frame.y,
		puglview->frame.width,
		puglview->frame.height,
       	};
        puglDispatchEvent(puglview, (PuglEvent*)&ev);
}

- (void) windowDidBecomeKey:(NSNotification*)notification
{
	(void)notification;

	PuglWrapperView* wrapperView = window->puglview->impl->wrapperView;
	if (wrapperView->urgentTimer) {
		[wrapperView->urgentTimer invalidate];
		[wrapperView->urgentTimer release];
		wrapperView->urgentTimer = NULL;
	}

	PuglEvent ev = { 0 };
	ev.type       = PUGL_FOCUS_IN;
	ev.focus.grab = false;
	puglDispatchEvent(window->puglview, &ev);
}

- (void) windowDidResignKey:(NSNotification*)notification
{
	(void)notification;

	PuglEvent ev = { 0 };
	ev.type       = PUGL_FOCUS_OUT;
	ev.focus.grab = false;
	puglDispatchEvent(window->puglview, &ev);
}

@end

PuglStatus
puglInitApplication(PuglApplicationFlags PUGL_UNUSED(flags))
{
	return PUGL_SUCCESS;
}

PuglWorldInternals*
puglInitWorldInternals(void)
{
	PuglWorldInternals* impl = (PuglWorldInternals*)calloc(
		1, sizeof(PuglWorldInternals));

	impl->app             = [NSApplication sharedApplication];

	return impl;
}

void
puglFreeWorldInternals(PuglWorld* world)
{
	releaseProcessTimer(world->impl);
	if (world->impl->worldProxy) {
                [world->impl->worldProxy setPuglWorld: NULL];
		[world->impl->worldProxy release];
		world->impl->worldProxy = NULL;
	}
	free(world->impl);
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
	CGContextRef contextRef = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
	return contextRef;
}

PuglInternals*
puglInitViewInternals(void)
{
	return (PuglInternals*)calloc(1, sizeof(PuglInternals));
}

static NSLayoutConstraint*
puglConstraint(id item, NSLayoutAttribute attribute, float constant)
{
	return [NSLayoutConstraint
		       constraintWithItem: item
		                attribute: attribute
		                relatedBy: NSLayoutRelationGreaterThanOrEqual
		                   toItem: nil
		                attribute: NSLayoutAttributeNotAnAttribute
		               multiplier: 1.0
		                 constant: constant];
}

PuglStatus
puglCreateWindow(PuglView* view, const char* title)
{
	PuglInternals* impl = view->impl;

	// Create wrapper view to handle input
	impl->wrapperView             = [PuglWrapperView alloc];
	impl->wrapperView->puglview   = view;
	impl->wrapperView->markedText = [[NSMutableAttributedString alloc] init];
	[impl->wrapperView setAutoresizesSubviews:YES];
	[impl->wrapperView initWithFrame:
		     NSMakeRect(0, 0, view->reqWidth, view->reqHeight)];
	[impl->wrapperView addConstraint:
		     puglConstraint(impl->wrapperView, NSLayoutAttributeWidth, view->minWidth)];
	[impl->wrapperView addConstraint:
		     puglConstraint(impl->wrapperView, NSLayoutAttributeHeight, view->minHeight)];
	[impl->wrapperView setReshaped];

	// Create draw view to be rendered to
	int st = 0;
	if ((st = view->backend->configure(view)) ||
	    (st = view->backend->create(view))) {
		return st;
	}

	// Add draw view to wraper view
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
		NSString* titleString = [[NSString alloc]
			                        initWithBytes:title
			                               length:strlen(title)
			                             encoding:NSUTF8StringEncoding];

		const NSRect frame = rectToScreen(
			NSMakeRect(0, 0,
			           view->reqWidth, view->reqHeight));

		unsigned style = (NSClosableWindowMask |
		                  NSTitledWindowMask |
		                  NSMiniaturizableWindowMask );
		if (view->hints[PUGL_IS_POPUP]) {
			style = NSWindowStyleMaskBorderless;
		}
		if (view->hints[PUGL_RESIZABLE]) {
			style |= NSResizableWindowMask;
		}

		id window = [[[PuglWindow alloc]
			initWithContentRect:frame
			          styleMask:style
			            backing:NSBackingStoreBuffered
			              defer:NO
		              ] retain];
		[window setPuglview:view];
		[window setTitle:titleString];
		if (view->hints[PUGL_RESIZABLE] && (view->minWidth || view->minHeight)) {
			[window setContentMinSize:NSMakeSize(view->minWidth,
			                                     view->minHeight)];
		}
		impl->window = window;
		puglSetWindowTitle(view, title);

		((NSWindow*)window).delegate = [[PuglWindowDelegate alloc]
			                  initWithPuglWindow:window];

		if (view->hints[PUGL_RESIZABLE] && (view->minAspectX && view->minAspectY)) {
			[window setContentAspectRatio:NSMakeSize(view->minAspectX,
			                                         view->minAspectY)];
		}

		[window setContentView:impl->wrapperView];
		
		if (view->hints[PUGL_IS_POPUP]) {
			//[window setLevel:NSPopUpMenuWindowLevel];
			[window setHasShadow:YES];
		}
	}

	[impl->wrapperView updateTrackingAreas];

	return 0;
}

PuglStatus
puglShowWindow(PuglView* view)
{
	NSWindow* window = view->impl->window;
	if (window) {
            if (!view->impl->displayed) {
                view->impl->displayed = true;
                if (view->transientParent) {
                    NSWindow* parentWindow = [(id)view->transientParent window];
                    if (parentWindow) {
                        if (!view->impl->posRequested) {
                            // center to parent, otherwise if will appear at top left screen corner
                            NSRect parentFrame = [parentWindow frame];
                            NSRect frame = [window frame];
                            frame.origin.x = parentFrame.origin.x + parentFrame.size.width/2  - frame.size.width/2;
                            frame.origin.y = parentFrame.origin.y + parentFrame.size.height/2 - frame.size.height/2;
                            [window setFrame: frame display:NO];
                        }
                        [parentWindow addChildWindow:window ordered:NSWindowAbove];
                    }
                } else {
                    if (!view->impl->posRequested) {
                        // center to screen, otherwise if will appear at top left screen corner
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
     	} 
     	else {
     	    [view->impl->wrapperView setHidden:NO];
     	}
	updateViewRect(view);
	view->visible = true;

        [view->impl->drawView setNeedsDisplay: YES];
	
	return PUGL_SUCCESS;
}

PuglStatus
puglHideWindow(PuglView* view)
{
        NSWindow* window = view->impl->window;
        if (window) {
            [view->impl->window setIsVisible:NO];
        } else {
            [view->impl->wrapperView setHidden:YES];
        }   
        view->visible = false;
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
			if (wrapperView->urgentTimer) {
				[wrapperView->urgentTimer invalidate];
				[wrapperView->urgentTimer release];
				wrapperView->urgentTimer = NULL;
			}
			if (wrapperView->markedText) {
			    [wrapperView->markedText release];
			    wrapperView->markedText = NULL;
			}
			[wrapperView removeFromSuperview];
			wrapperView->puglview = NULL;
			[wrapperView release];

			if (view->impl->window) {
				view->impl->window->puglview = NULL;
				[view->impl->window close];
				[view->impl->window release];
			}
			free(view->impl);
		}
	}
}

PuglStatus
puglGrabFocus(PuglView* view)
{
	NSWindow* window = [view->impl->wrapperView window];

	[window makeKeyWindow];
	[window makeFirstResponder:view->impl->wrapperView];
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
	        PuglWrapperView* wrapperView = view->impl->wrapperView;
		if (wrapperView->urgentTimer) {
			[wrapperView->urgentTimer invalidate];
			[wrapperView->urgentTimer release];
			wrapperView->urgentTimer = NULL;
		}
		wrapperView->urgentTimer =
		[[NSTimer scheduledTimerWithTimeInterval:2.0
			                                 target:view->impl->wrapperView
			                               selector:@selector(urgentTick)
			                               userInfo:nil
			                                repeats:YES] retain];
	}

	return PUGL_SUCCESS;
}

void
puglSetProcessFunc(PuglWorld* world, PuglProcessFunc processFunc, void* userData)
{
	world->processFunc     = processFunc;
	world->processUserData = userData;
	if (processFunc) {
	    if (!world->impl->worldProxy) {
	        world->impl->worldProxy = [[PuglWorldProxy alloc] init];
	        [world->impl->worldProxy setPuglWorld: world];
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
			[world->impl->worldProxy setPuglWorld: world];
		}
		rescheduleProcessTimer(world);
	} else {
		world->impl->nextProcessTime = -1;
		releaseProcessTimer(world->impl);
	}
}

PuglStatus
puglPollEvents(PuglWorld* world, const double timeout)
{
	world->impl->polling = true;
	
	NSDate* date = ((timeout < 0) ? [NSDate distantFuture] :
	                (timeout == 0) ? nil :
	                [NSDate dateWithTimeIntervalSinceNow:timeout]);

	/* Note that dequeue:NO is broken (it blocks forever even when events are
	   pending), so we work around this by dequeueing the event then posting it
	   back to the front of the queue. */
	NSEvent* event = [world->impl->app
	                     nextEventMatchingMask:NSAnyEventMask
	                     untilDate:date
	                     inMode:NSDefaultRunLoopMode
	                     dequeue:YES];

	world->impl->polling = false;
	if (event) {
		[world->impl->app postEvent:event atStart:true];
		return PUGL_SUCCESS;
	} else {
		return PUGL_FAILURE;
	}
}

void
puglAwake(PuglWorld* world)
{
	NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];
	if (world->impl->worldProxy) {
		[world->impl->worldProxy performSelectorOnMainThread:@selector(awakeCallback)
		                                          withObject:nil
		                                       waitUntilDone:NO];
	
	}
	[localPool release];
}



PuglStatus
puglWaitForEvent(PuglView* view)
{
	return puglPollEvents(view->world, -1.0);
}

PUGL_API PuglStatus
puglDispatchEvents(PuglWorld* world)
{
	for (NSEvent* ev = NULL;
	     (ev = [world->impl->app nextEventMatchingMask:NSAnyEventMask
	                                         untilDate:nil
	                                            inMode:NSDefaultRunLoopMode
	                                           dequeue:YES]);) {
		[world->impl->app sendEvent: ev];
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglProcessEvents(PuglView* view)
{
	return puglDispatchEvents(view->world);
}

double
puglGetTime(const PuglWorld* world)
{
	return (mach_absolute_time() / 1e9) - world->startTime;
}

PuglStatus
puglPostRedisplay(PuglView* view)
{
	[view->impl->drawView setNeedsDisplay: YES];
	return PUGL_SUCCESS;
}

PuglStatus
puglPostRedisplayRect(PuglView* view, const PuglRect rect)
{
	[view->impl->drawView setNeedsDisplayInRect:
		CGRectMake(rect.x, rect.y, rect.width, rect.height)];

	return PUGL_SUCCESS;
}

PuglNativeWindow
puglGetNativeWindow(PuglView* view)
{
	return (PuglNativeWindow)view->impl->wrapperView;
}

PuglStatus
puglSetWindowTitle(PuglView* view, const char* title)
{
	puglSetString(&view->title, title);

	NSString* titleString = [[NSString alloc]
		                        initWithBytes:title
		                               length:strlen(title)
		                             encoding:NSUTF8StringEncoding];

	if (view->impl->window) {
		[view->impl->window setTitle:titleString];
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

	if (impl->window) {
		const NSRect rect = NSMakeRect(frame.x, frame.y, frame.width, frame.height);
		// Resize window to fit new content rect
		const NSRect windowFrame = [
			impl->window frameRectForContentRect:rectToScreen(rect)];

		[impl->window setFrame:windowFrame display:NO];

		// Resize views
		const NSRect drawRect = NSMakeRect(0, 0, frame.width, frame.height);
		[impl->wrapperView setFrame:drawRect];
		[impl->drawView setFrame:drawRect];
	}
	else if (impl->wrapperView) {
		const NSRect rect = NSMakeRect(frame.x, frame.y, frame.width, frame.height);
		[impl->wrapperView setFrame:rect];
		
		const NSRect drawRect = NSMakeRect(0, 0, frame.width, frame.height);
		[impl->drawView setFrame:drawRect];
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetSize(PuglView* view, int width, int height)
{
	view->reqWidth  = width;
	view->reqHeight = height;

	PuglInternals* const impl = view->impl;
	if (impl->window) {
		NSRect rect = [impl->window contentRectForFrameRect: [impl->window frame]];
		rect.origin.y += rect.size.height - height;
		rect.size = NSMakeSize(width, height);
		// Resize window to fit new content rect
		const NSRect windowFrame = [
			impl->window frameRectForContentRect:rect];

		[impl->window setFrame:windowFrame display:NO];

		// Resize views
		const NSRect drawRect = NSMakeRect(0, 0, width, height);
		[impl->wrapperView setFrame:drawRect];
		[impl->drawView setFrame:drawRect];
	}
	else if (impl->wrapperView) {
	        NSRect rect = [impl->wrapperView frame];
	        rect.size = NSMakeSize(width, height);
		[impl->wrapperView setFrame:rect];
		
		const NSRect drawRect = NSMakeRect(0, 0, width, height);
		[impl->drawView setFrame:drawRect];
	}
	return PUGL_SUCCESS;
}

PuglStatus
puglSetMinSize(PuglView* const view, const int width, const int height)
{
	view->minWidth  = width;
	view->minHeight = height;

	if (view->impl->window && (view->minWidth || view->minHeight)) {
		[view->impl->window
		    setContentMinSize:NSMakeSize(view->minWidth, view->minHeight)];
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
		[view->impl->window setContentAspectRatio:NSMakeSize(view->minAspectX,
		                                                     view->minAspectY)];
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeWindow parent)
{
	view->transientParent = parent;
	return PUGL_SUCCESS;
}

PuglStatus
puglRequestClipboard(PuglView* view)
{
	NSPasteboard* const  pasteboard = [NSPasteboard generalPasteboard];

	if ([[pasteboard types] containsObject:NSStringPboardType]) {
		const NSString* str  = [pasteboard stringForType:NSStringPboardType];
		const char*     utf8 = [str UTF8String];

		puglSetBlob(&view->clipboard, utf8, strlen(utf8));

                [view->impl->wrapperView performSelectorOnMainThread:@selector(clipboardReceived)
                                                          withObject:nil
                                                       waitUntilDone:NO];
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetClipboard(PuglWorld*  world,
                 const char* type,
                 const void* data,
                 size_t      len)
{
	NSPasteboard* const  pasteboard = [NSPasteboard generalPasteboard];
	const char* const    str        = (const char*)data;

	PuglStatus st = puglSetInternalClipboard(world, type, data, len);
	if (st) {
		return st;
	}

	[pasteboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil]
	                   owner:nil];

	[pasteboard setString:[NSString stringWithUTF8String:str]
	              forType:NSStringPboardType];

	return PUGL_SUCCESS;
}


bool
puglHasClipboard(PuglWorld*  world)
{
	return false;
}

const PuglBackend*
puglStubBackend(void)
{
	static const PuglBackend backend = {puglStubConfigure,
	                                    puglStubCreate,
	                                    puglStubDestroy,
	                                    puglStubEnter,
	                                    puglStubLeave,
	                                    puglStubResize,
	                                    puglStubGetContext};

	return &backend;
}
