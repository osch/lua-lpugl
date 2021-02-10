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

/*
  Note that this header includes Vulkan headers, so if you are writing a
  program or plugin that dynamically loads vulkan, you should first define
  `VK_NO_PROTOTYPES` before including it.
*/

#ifndef PUGL_VULKAN_H
#define PUGL_VULKAN_H

#include "pugl/pugl.h"

#include <vulkan/vulkan_core.h>

#include <stdint.h>

PUGL_BEGIN_DECLS

/**
   @defgroup vulkan Vulkan
   Vulkan graphics support.

   Vulkan support differs from OpenGL because almost all most configuration is
   done using the Vulkan API itself, rather than by setting view hints to
   configure the context.  Pugl only provides a minimal loader for loading the
   Vulkan library, and a portable function to create a Vulkan surface for a
   view, which hides the platform-specific implementation details.

   @ingroup pugl
   @{
*/

/**
   Dynamic Vulkan loader.

   This can be used to dynamically load the Vulkan library.  Applications or
   plugins should not link against the Vulkan library, but instead use this at
   runtime.  This ensures that things will work on as many systems as possible,
   and allows errors to be handled gracefully.

   This is not a "loader" in the sense of loading all the required Vulkan
   functions (which is the application's responsibility), but just a minimal
   implementation to portably load the Vulkan library and get the two functions
   that are used to load everything else.

   Note that this owns the loaded Vulkan library, so it must outlive all use of
   the Vulkan API.

   @see https://www.khronos.org/registry/vulkan/specs/1.0/html/chap4.html
*/
typedef struct PuglVulkanLoaderImpl PuglVulkanLoader;

/**
   Create a new dynamic loader for Vulkan functions.

   This dynamically loads the Vulkan library and gets the load functions from
   it.

   @return A new Vulkan loader, or null on failure.
*/
PUGL_API
PuglVulkanLoader*
puglNewVulkanLoader(PuglWorld* world);

/**
   Free a loader created with puglNewVulkanLoader().

   Note that this closes the Vulkan library, so no Vulkan objects or API may be
   used after this is called.
*/
PUGL_API
void
puglFreeVulkanLoader(PuglVulkanLoader* loader);

/**
   Return the `vkGetInstanceProcAddr` function.

   @return Null if the Vulkan library does not contain this function (which is
   unlikely and indicates a broken system).
*/
PUGL_API
PFN_vkGetInstanceProcAddr
puglGetInstanceProcAddrFunc(const PuglVulkanLoader* loader);

/**
   Return the `vkGetDeviceProcAddr` function.

   @return Null if the Vulkan library does not contain this function (which is
   unlikely and indicates a broken system).
*/
PUGL_API
PFN_vkGetDeviceProcAddr
puglGetDeviceProcAddrFunc(const PuglVulkanLoader* loader);

/**
   Return the Vulkan instance extensions required to draw to a PuglView.

   This simply returns static strings, it does not access Vulkan or the window
   system.  The returned array always contains at least "VK_KHR_surface".

   @param[out] count The number of extensions in the returned array.
   @return An array of extension name strings.
*/
PUGL_API
const char* const*
puglGetInstanceExtensions(uint32_t* count);

/**
   Create a Vulkan surface for a Pugl view.

   @param vkGetInstanceProcAddr Accessor for Vulkan functions.
   @param view The view the surface is to be displayed on.
   @param instance The Vulkan instance.
   @param allocator Vulkan allocation callbacks, may be NULL.
   @param[out] surface Pointed to a newly created Vulkan surface.
   @return `VK_SUCCESS` on success, or a Vulkan error code.
*/
PUGL_API
VkResult
puglCreateSurface(PFN_vkGetInstanceProcAddr    vkGetInstanceProcAddr,
                  PuglView*                    view,
                  VkInstance                   instance,
                  const VkAllocationCallbacks* allocator,
                  VkSurfaceKHR*                surface);

/**
   Vulkan graphics backend.

   Pass the returned value to puglSetBackend() to draw to a view with Vulkan.
*/
PUGL_CONST_API
const PuglBackend*
puglVulkanBackend(void);

/**
   @}
*/

PUGL_END_DECLS

#endif // PUGL_VULKAN_H
