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
   @file implementation.h
   @brief Shared declarations for implementation.
*/

#ifndef PUGL_DETAIL_IMPLEMENTATION_H
#define PUGL_DETAIL_IMPLEMENTATION_H

#include "pugl/detail/types.h"
#include "pugl/pugl.h"

#include <stddef.h>
#include <stdint.h>

#ifndef PUGL_API_PRIVATE
#   define PUGL_API_PRIVATE
#endif

PUGL_BEGIN_DECLS

/// Set `blob` to `data` with length `len`, reallocating if necessary
PUGL_API_PRIVATE
void puglSetBlob(PuglBlob* dest, const void* data, size_t len);

/// Reallocate and set `*dest` to `string`
PUGL_API_PRIVATE
void puglSetString(char** dest, const char* string);

/// Allocate and initialise world internals (implemented once per platform)
PUGL_API_PRIVATE 
PuglWorldInternals*
puglInitWorldInternals(PuglWorldType type, PuglWorldFlags flags);

/// Destroy and free world internals (implemented once per platform)
PUGL_API_PRIVATE 
void puglFreeWorldInternals(PuglWorld* world);

/// Allocate and initialise view internals (implemented once per platform)
PUGL_API_PRIVATE 
PuglInternals* puglInitViewInternals(void);

/// Destroy and free view internals (implemented once per platform)
PUGL_API_PRIVATE 
void puglFreeViewInternals(PuglView* view);

/// Return the Unicode code point for `buf` or the replacement character
PUGL_API_PRIVATE 
uint32_t puglDecodeUTF8(const uint8_t* buf);

/// Dispatch an event with a simple `type` to `view`
PUGL_API_PRIVATE 
void puglDispatchSimpleEvent(PuglView* view, PuglEventType type);

/// Dispatch `event` to `view` while already in the graphics context
PUGL_API_PRIVATE 
void puglDispatchEventInContext(PuglView* view, PuglEvent* event);

/// Dispatch `event` to `view`, entering graphics context if necessary
PUGL_API_PRIVATE 
void puglDispatchEvent(PuglView* view, PuglEvent* event);

/// Set internal (stored in view) clipboard contents
PUGL_API_PRIVATE 
const void*
puglGetInternalClipboard(const PuglView* view, const char** type, size_t* len);

/// Set internal (stored in view) clipboard contents
PUGL_API_PRIVATE 
PuglStatus
puglSetInternalClipboard(PuglWorld*  world,
                         const char* type,
                         const void* data,
                         size_t      len);

PUGL_API_PRIVATE 
bool puglRectsInit(PuglRects* rects, int capacity);

PUGL_API_PRIVATE 
bool puglRectsAppend(PuglRects* rects, PuglRect* rect);

PUGL_API_PRIVATE 
void puglRectsFree(PuglRects* rects);

PUGL_END_DECLS

#endif // PUGL_DETAIL_IMPLEMENTATION_H
