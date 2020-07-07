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
   @file pugl_gl.h
   @brief OpenGL-specific API.
*/

#ifndef PUGL_PUGL_GL_H
#define PUGL_PUGL_GL_H

#include "pugl/pugl.h"

PUGL_BEGIN_DECLS

/**
   @defgroup gl OpenGL
   OpenGL graphics support.
   @ingroup pugl_c
   @{
*/

/**
   OpenGL extension function.
*/
typedef void (*PuglGlFunc)(void);

/**
   Return the address of an OpenGL extension function.
*/
PUGL_API PuglGlFunc
puglGetProcAddress(const char* name);

/**
   OpenGL graphics backend.

   Pass the return value to puglSetBackend() to draw to a view with OpenGL.
*/
PUGL_API const PuglBackend*
puglGlBackend(void);

/**
   Return a pointer to the native handle of the world.

   On X11, this returns a pointer to the Display.
   On OSX, this returns CGContextRef for current GraphicsPort.
   On Windows, this returns a handle to the calling process module.
*/
PUGL_API void*
puglGlBackendGetNativeWorld(PuglWorld* world);


PUGL_END_DECLS

/**
   @}
*/

#endif // PUGL_PUGL_GL_H
