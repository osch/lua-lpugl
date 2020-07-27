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

/**
   @file mac.h
   @brief Shared definitions for MacOS implementation.
*/

#ifndef PUGL_DETAIL_MAC_H
#define PUGL_DETAIL_MAC_H

#include "pugl/pugl.h"

#import <Cocoa/Cocoa.h>

#include <stdint.h>

@interface PuglWrapperView : NSView<NSTextInputClient>

- (void) dispatchExpose:(NSRect)rect rects:(const NSRect*)rects count:(int)c;
- (void) setReshaped;
- (void) clipboardReceived;

@end

@interface PuglWindow : NSWindow

- (void) setPuglview:(PuglView*)view;

@end

struct PuglWorldInternalsImpl {
	NSApplication*     app;
	double             nextProcessTime;
	id                 worldProxy;
	NSTimer*           processTimer;
	bool               polling;
};

struct PuglInternalsImpl {
	NSApplication*   app;
	PuglWrapperView* wrapperView;
	NSView*          drawView;
	NSCursor*        cursor;
	PuglWindow*      window;
	uint32_t         mods;
	uint32_t         mouseButtons;
	bool             posRequested;
	bool             displayed;
	bool             mouseTracked;
	bool             shouldCursorHidden;
	bool             isCursorHidden;
};

#endif // PUGL_DETAIL_MAC_H
