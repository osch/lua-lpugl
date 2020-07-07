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
   @file pugl.h
   @brief Pugl API.
*/

#ifndef PUGL_PUGL_H
#define PUGL_PUGL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef PUGL_SHARED
#    ifdef _WIN32
#        define PUGL_LIB_IMPORT __declspec(dllimport)
#        define PUGL_LIB_EXPORT __declspec(dllexport)
#    else
#        define PUGL_LIB_IMPORT __attribute__((visibility("default")))
#        define PUGL_LIB_EXPORT __attribute__((visibility("default")))
#    endif
#    ifdef PUGL_INTERNAL
#        define PUGL_API PUGL_LIB_EXPORT
#    else
#        define PUGL_API PUGL_LIB_IMPORT
#    endif
#else
#    ifndef PUGL_API
#        define PUGL_API
#    endif
#endif

#ifndef PUGL_DISABLE_DEPRECATED
#    if defined(__clang__)
#        define PUGL_DEPRECATED_BY(rep) __attribute__((deprecated("", rep)))
#    elif defined(__GNUC__)
#        define PUGL_DEPRECATED_BY(rep) __attribute__((deprecated("Use " rep)))
#    else
#        define PUGL_DEPRECATED_BY(rep)
#    endif
#endif

#if defined(__GNUC__)
#    define PUGL_CONST_FUNC __attribute__((const))
#else
#    define PUGL_CONST_FUNC
#endif

#ifdef __cplusplus
#	define PUGL_BEGIN_DECLS extern "C" {
#	define PUGL_END_DECLS }
#else
#	define PUGL_BEGIN_DECLS
#	define PUGL_END_DECLS
#endif

PUGL_BEGIN_DECLS

/**
   @defgroup pugl Pugl
   A minimal portable API for embeddable GUIs.
   @{

   @defgroup pugl_c C API
   Public C API.
   @{
*/

/**
   A rectangle.

   This is used to describe things like view position and size.  Pugl generally
   uses coordinates where the top left corner is 0,0.
*/
typedef struct {
	double x;
	double y;
	double width;
	double height;
} PuglRect;

/**
   @defgroup events Events

   Event definitions.

   All updates to the view happen via events, which are dispatched to the
   view's event function by Pugl.  Most events map directly to one from the
   underlying window system, but some are constructed by Pugl itself so there
   is not necessarily a direct correspondence.

   @{
*/

/**
   Keyboard modifier flags.
*/
typedef enum {
	PUGL_MOD_SHIFT = 1,      ///< Shift key
	PUGL_MOD_CTRL  = 1 << 1, ///< Control key
	PUGL_MOD_ALT   = 1 << 2, ///< Alt/Option key
	PUGL_MOD_SUPER = 1 << 3, ///< Mod4/Command/Windows key
	PUGL_MOD_ALTGR = 1 << 4  ///< AltGr key
} PuglMod;

/**
   Bitwise OR of #PuglMod values.
*/
typedef uint32_t PuglMods;

/**
   Keyboard key codepoints.

   All keys are identified by a Unicode code point in PuglEventKey::key.  This
   enumeration defines constants for special keys that do not have a standard
   code point, and some convenience constants for control characters.  Note
   that all keys are handled in the same way, this enumeration is just for
   convenience when writing hard-coded key bindings.

   Keys that do not have a standard code point use values in the Private Use
   Area in the Basic Multilingual Plane (`U+E000` to `U+F8FF`).  Applications
   must take care to not interpret these values beyond key detection, the
   mapping used here is arbitrary and specific to Pugl.
*/
typedef enum {
	// ASCII control codes
	PUGL_KEY_BACKSPACE = 0x08,
	PUGL_KEY_TAB       = 0x09,
	PUGL_KEY_RETURN    = 0x0D,
	PUGL_KEY_ESCAPE    = 0x1B,
	PUGL_KEY_SPACE     = 0x20,
	PUGL_KEY_DELETE    = 0x7F,

	// Unicode Private Use Area
	PUGL_KEY_F1 = 0xE000,
	PUGL_KEY_F2,
	PUGL_KEY_F3,
	PUGL_KEY_F4,
	PUGL_KEY_F5,
	PUGL_KEY_F6,
	PUGL_KEY_F7,
	PUGL_KEY_F8,
	PUGL_KEY_F9,
	PUGL_KEY_F10,
	PUGL_KEY_F11,
	PUGL_KEY_F12,
	PUGL_KEY_LEFT,
	PUGL_KEY_UP,
	PUGL_KEY_RIGHT,
	PUGL_KEY_DOWN,
	PUGL_KEY_PAGE_UP,
	PUGL_KEY_PAGE_DOWN,
	PUGL_KEY_HOME,
	PUGL_KEY_END,
	PUGL_KEY_INSERT,
	PUGL_KEY_SHIFT,
	PUGL_KEY_SHIFT_L = PUGL_KEY_SHIFT,
	PUGL_KEY_SHIFT_R,
	PUGL_KEY_CTRL,
	PUGL_KEY_CTRL_L = PUGL_KEY_CTRL,
	PUGL_KEY_CTRL_R,
	PUGL_KEY_ALT,
	PUGL_KEY_ALT_L = PUGL_KEY_ALT,
	PUGL_KEY_ALT_R,
	PUGL_KEY_SUPER,
	PUGL_KEY_SUPER_L = PUGL_KEY_SUPER,
	PUGL_KEY_SUPER_R,
	PUGL_KEY_MENU,
	PUGL_KEY_CAPS_LOCK,
	PUGL_KEY_SCROLL_LOCK,
	PUGL_KEY_NUM_LOCK,
	PUGL_KEY_PRINT_SCREEN,
	PUGL_KEY_PAUSE,
	
	PUGL_KEY_KP_ENTER,
	PUGL_KEY_KP_HOME,
	PUGL_KEY_KP_LEFT,
	PUGL_KEY_KP_UP,
	PUGL_KEY_KP_RIGHT,
	PUGL_KEY_KP_DOWN,
	PUGL_KEY_KP_PAGE_UP,
	PUGL_KEY_KP_PAGE_DOWN,
	PUGL_KEY_KP_END,
	PUGL_KEY_KP_BEGIN,
	PUGL_KEY_KP_INSERT,
	PUGL_KEY_KP_DELETE,
	PUGL_KEY_KP_MULTIPLY,
	PUGL_KEY_KP_ADD,
	PUGL_KEY_KP_SUBTRACT,
	PUGL_KEY_KP_DIVIDE,
	
	PUGL_KEY_KP_0,
	PUGL_KEY_KP_1,
	PUGL_KEY_KP_2,
	PUGL_KEY_KP_3,
	PUGL_KEY_KP_4,
	PUGL_KEY_KP_5,
	PUGL_KEY_KP_6,
	PUGL_KEY_KP_7,
	PUGL_KEY_KP_8,
	PUGL_KEY_KP_9,
	PUGL_KEY_KP_SEPARATOR
	
} PuglKey;

/**
   The type of a PuglEvent.
*/
typedef enum {
	PUGL_NOTHING,        ///< No event
	PUGL_CREATE,         ///< View created, a #PuglEventCreate
	PUGL_DESTROY,        ///< View destroyed, a #PuglEventDestroy
	PUGL_CONFIGURE,      ///< View moved/resized, a #PuglEventConfigure
	PUGL_MAP,            ///< View made visible, a #PuglEventMap
	PUGL_UNMAP,          ///< View made invisible, a #PuglEventUnmap
	PUGL_UPDATE,         ///< View ready to draw, a #PuglEventUpdate
	PUGL_EXPOSE,         ///< View must be drawn, a #PuglEventExpose
	PUGL_CLOSE,          ///< View will be closed, a #PuglEventClose
	PUGL_FOCUS_IN,       ///< Keyboard focus entered view, a #PuglEventFocus
	PUGL_FOCUS_OUT,      ///< Keyboard focus left view, a #PuglEventFocus
	PUGL_KEY_PRESS,      ///< Key pressed, a #PuglEventKey
	PUGL_KEY_RELEASE,    ///< Key released, a #PuglEventKey
	PUGL_POINTER_IN,     ///< Pointer entered view, a #PuglEventCrossing
	PUGL_POINTER_OUT,    ///< Pointer left view, a #PuglEventCrossing
	PUGL_BUTTON_PRESS,   ///< Mouse button pressed, a #PuglEventButton
	PUGL_BUTTON_RELEASE, ///< Mouse button released, a #PuglEventButton
	PUGL_MOTION,         ///< Pointer moved, a #PuglEventMotion
	PUGL_SCROLL,         ///< Scrolled, a #PuglEventScroll
	PUGL_CLIENT,         ///< Custom client message, a #PuglEventClient
	PUGL_MUST_FREE,      ///< puglFreeView() must be called, a #PuglEventAny
	PUGL_DATA_RECEIVED,  ///< Clipboard/Selection data received 

#ifndef PUGL_DISABLE_DEPRECATED
	PUGL_ENTER_NOTIFY PUGL_DEPRECATED_BY("PUGL_POINTER_IN") = PUGL_POINTER_IN,
	PUGL_LEAVE_NOTIFY PUGL_DEPRECATED_BY("PUGL_POINTER_OUT") = PUGL_POINTER_OUT,
	PUGL_MOTION_NOTIFY PUGL_DEPRECATED_BY("PUGL_MOTION") = PUGL_MOTION,
#endif

} PuglEventType;

/**
   Common flags for all event types.
*/
typedef enum {
	PUGL_IS_SEND_EVENT = 1, ///< Event is synthetic
	PUGL_IS_HINT       = 2  ///< Event is a hint (not direct user input)
} PuglEventFlag;

/**
   Bitwise OR of #PuglEventFlag values.
*/
typedef uint32_t PuglEventFlags;

/**
   Reason for a PuglEventCrossing.
*/
typedef enum {
	PUGL_CROSSING_NORMAL, ///< Crossing due to pointer motion
	PUGL_CROSSING_GRAB,   ///< Crossing due to a grab
	PUGL_CROSSING_UNGRAB  ///< Crossing due to a grab release
} PuglCrossingMode;

/**
   Scroll direction.

   Describes the direction of a #PuglEventScroll along with whether the scroll
   is a "smooth" scroll.  The discrete directions are for devices like mouse
   wheels with constrained axes, while a smooth scroll is for those with
   arbitrary scroll direction freedom, like some touchpads.
*/
typedef enum {
	PUGL_SCROLL_UP,    ///< Scroll up
	PUGL_SCROLL_DOWN,  ///< Scroll down
	PUGL_SCROLL_LEFT,  ///< Scroll left
	PUGL_SCROLL_RIGHT, ///< Scroll right
	PUGL_SCROLL_SMOOTH ///< Smooth scroll in any direction
} PuglScrollDirection;

/**
   Common header for all event structs.
*/
typedef struct {
	PuglEventType  type;  ///< Event type
	PuglEventFlags flags; ///< Bitwise OR of #PuglEventFlag values
} PuglEventAny;

/**
   View create event.

   This event is sent when a view is realized before it is first displayed,
   with the graphics context entered.  This is typically used for setting up
   the graphics system, for example by loading OpenGL extensions.

   This event type has no extra fields.
*/
typedef PuglEventAny PuglEventCreate;

/**
   View destroy event.

   This event is the counterpart to #PuglEventCreate, and it is sent when the
   view is being destroyed.  This is typically used for tearing down the
   graphics system, or otherwise freeing any resources allocated when the
   create event was handled.

   This is the last event sent to any view, and immediately after it is
   processed, the view is destroyed and may no longer be used.

   This event type has no extra fields.
*/
typedef PuglEventAny PuglEventDestroy;

/**
   View resize or move event.

   A configure event is sent whenever the view is resized or moved.  When a
   configure event is received, the graphics context is active but not set up
   for drawing.  For example, it is valid to adjust the OpenGL viewport or
   otherwise configure the context, but not to draw anything.
*/
typedef struct {
	PuglEventType  type;   ///< #PUGL_CONFIGURE
	PuglEventFlags flags;  ///< Bitwise OR of #PuglEventFlag values
	double         x;      ///< New parent-relative X coordinate
	double         y;      ///< New parent-relative Y coordinate
	double         width;  ///< New width
	double         height; ///< New height
} PuglEventConfigure;

/**
   View show event.

   This event is sent when a view is mapped to the screen and made visible.

   This event type has no extra fields.
*/
typedef PuglEventAny PuglEventMap;

/**
   View hide event.

   This event is sent when a view is unmapped from the screen and made
   invisible.

   This event type has no extra fields.
*/
typedef PuglEventAny PuglEventUnmap;

/**
   View update event.

   This event is sent to every view near the end of a main loop iteration when
   any pending exposures are about to be redrawn.  It is typically used to mark
   regions to expose with puglPostRedisplay() or puglPostRedisplayRect().  For
   example, to continuously animate, a view calls puglPostRedisplay() when an
   update event is received, and it will then shortly receive an expose event.
*/
typedef PuglEventAny PuglEventUpdate;

/**
   Expose event for when a region must be redrawn.

   When an expose event is received, the graphics context is active, and the
   view must draw the entire specified region.  The contents of the region are
   undefined, there is no preservation of anything drawn previously.
*/
typedef struct {
	PuglEventType  type;   ///< #PUGL_EXPOSE
	PuglEventFlags flags;  ///< Bitwise OR of #PuglEventFlag values
	double         x;      ///< View-relative X coordinate
	double         y;      ///< View-relative Y coordinate
	double         width;  ///< Width of exposed region
	double         height; ///< Height of exposed region
	int            count;  ///< Number of expose events to follow
} PuglEventExpose;

/**
   View close event.

   This event is sent when the view is to be closed, for example when the user
   clicks the close button.

   This event type has no extra fields.
*/
typedef PuglEventAny PuglEventClose;

/**
   Keyboard focus event.

   This event is sent whenever the view gains or loses the keyboard focus.  The
   view with the keyboard focus will receive any key press or release events.
*/
typedef struct {
	PuglEventType    type;  ///< #PUGL_FOCUS_IN or #PUGL_FOCUS_OUT
	PuglEventFlags   flags; ///< Bitwise OR of #PuglEventFlag values
	PuglCrossingMode mode;  ///< Reason for focus change
} PuglEventFocus;

/**
   Key press/release event.

   This event combines low-level key press/release and high level 
   text input events.
   
   Keys are represented as Unicode code points, using the "natural" code point
   for the key wherever possible (see @ref PuglKey for details).  The `key`
   field will be set to the code for the pressed key, without any modifiers
   applied (by the shift or control keys).

   If available, this event also represents text input, as the result of the 
   specified key press.  The text input is given as a UTF-8 string.
   
   There is not always a strict correspondence between key press events 
   and text input events, since for example, with some input methods a 
   sequence of several key presses will generate one or more characters.
      
   If a key press is involved in input method handling that could
   lead to a text input event later on, the `key` field is 
   set to 0.

   If text input cannot be associated to a specific key press, the 
   `key` field is set to 0.
*/
typedef struct {
	PuglEventType  type;    ///< #PUGL_KEY_PRESS or #PUGL_KEY_RELEASE
	PuglEventFlags flags;   ///< Bitwise OR of #PuglEventFlag values
	double         time;    ///< Time in seconds
	double         x;       ///< View-relative X coordinate
	double         y;       ///< View-relative Y coordinate
	double         xRoot;   ///< Root-relative X coordinate
	double         yRoot;   ///< Root-relative Y coordinate
	PuglMods       state;   ///< Bitwise OR of #PuglMod flags
	uint32_t       keycode; ///< Raw key code
	uint32_t       key;     ///< Unshifted Unicode character code, or 0
	union {
	    char*      ptr;     ///< Pointer to input data if inputLength > 8
	    char       data[8]; ///< input data if inputLength <= 8
	}          input;       ///< text input as UTF-8 bytes
	size_t     inputLength; ///< text input length in bytes
} PuglEventKey;

/**
   Pointer enter or leave event.

   This event is sent when the pointer enters or leaves the view.  This can
   happen for several reasons (not just the user dragging the pointer over the
   window edge), as described by the `mode` field.
*/
typedef struct {
	PuglEventType    type;  ///< #PUGL_POINTER_IN or #PUGL_POINTER_OUT
	PuglEventFlags   flags; ///< Bitwise OR of #PuglEventFlag values
	double           time;  ///< Time in seconds
	double           x;     ///< View-relative X coordinate
	double           y;     ///< View-relative Y coordinate
	double           xRoot; ///< Root-relative X coordinate
	double           yRoot; ///< Root-relative Y coordinate
	PuglMods         state; ///< Bitwise OR of #PuglMod flags
	PuglCrossingMode mode;  ///< Reason for crossing
} PuglEventCrossing;

/**
   Button press or release event.
*/
typedef struct {
	PuglEventType  type;   ///< #PUGL_BUTTON_PRESS or #PUGL_BUTTON_RELEASE
	PuglEventFlags flags;  ///< Bitwise OR of #PuglEventFlag values
	double         time;   ///< Time in seconds
	double         x;      ///< View-relative X coordinate
	double         y;      ///< View-relative Y coordinate
	double         xRoot;  ///< Root-relative X coordinate
	double         yRoot;  ///< Root-relative Y coordinate
	PuglMods       state;  ///< Bitwise OR of #PuglMod flags
	uint32_t       button; ///< Button number starting from 1
} PuglEventButton;

/**
   Pointer motion event.
*/
typedef struct {
	PuglEventType  type;   ///< #PUGL_MOTION
	PuglEventFlags flags;  ///< Bitwise OR of #PuglEventFlag values
	double         time;   ///< Time in seconds
	double         x;      ///< View-relative X coordinate
	double         y;      ///< View-relative Y coordinate
	double         xRoot;  ///< Root-relative X coordinate
	double         yRoot;  ///< Root-relative Y coordinate
	PuglMods       state;  ///< Bitwise OR of #PuglMod flags
} PuglEventMotion;

/**
   Scroll event.

   The scroll distance is expressed in "lines", an arbitrary unit that
   corresponds to a single tick of a detented mouse wheel.  For example, `dy` =
   1.0 scrolls 1 line up.  Some systems and devices support finer resolution
   and/or higher values for fast scrolls, so programs should handle any value
   gracefully.
*/
typedef struct {
	PuglEventType       type;      ///< #PUGL_SCROLL
	PuglEventFlags      flags;     ///< Bitwise OR of #PuglEventFlag values
	double              time;      ///< Time in seconds
	double              x;         ///< View-relative X coordinate
	double              y;         ///< View-relative Y coordinate
	double              xRoot;     ///< Root-relative X coordinate
	double              yRoot;     ///< Root-relative Y coordinate
	PuglMods            state;     ///< Bitwise OR of #PuglMod flags
	PuglScrollDirection direction; ///< Scroll direction
	double              dx;        ///< Scroll X distance in lines
	double              dy;        ///< Scroll Y distance in lines
} PuglEventScroll;

/**
   Custom client message event.

   This can be used to send a custom message to a view, which is delivered via
   the window system and processed in the event loop as usual.  Among other
   things, this makes it possible to wake up the event loop for any reason.
*/
typedef struct {
	PuglEventType  type;  ///< #PUGL_CLIENT
	PuglEventFlags flags; ///< Bitwise OR of #PuglEventFlag values
	uintptr_t      data1; ///< Client-specific data
	uintptr_t      data2; ///< Client-specific data
} PuglEventClient;


/**
   Clipboard/Selection data received event.
*/
typedef struct {
	PuglEventType type;    ///< #PUGL_DATA_RECEIVED.
	uint32_t      flags;   ///< Bitwise OR of PuglEventFlag values.
	const char*   data;
	size_t        len;
} PuglEventReceived;

/**
   View event.

   This is a union of all event types.  The type must be checked to determine
   which fields are safe to access.  A pointer to PuglEvent can either be cast
   to the appropriate type, or the union members used.

   The graphics system may only be accessed when handling certain events.  The
   graphics context is active for #PUGL_CREATE, #PUGL_DESTROY, #PUGL_CONFIGURE,
   and #PUGL_EXPOSE, but only enabled for drawing for #PUGL_EXPOSE.
*/
typedef union {
	PuglEventAny       any;       ///< Valid for all event types
	PuglEventType      type;      ///< Event type
	PuglEventButton    button;    ///< #PUGL_BUTTON_PRESS, #PUGL_BUTTON_RELEASE
	PuglEventConfigure configure; ///< #PUGL_CONFIGURE
	PuglEventExpose    expose;    ///< #PUGL_EXPOSE
	PuglEventKey       key;       ///< #PUGL_KEY_PRESS, #PUGL_KEY_RELEASE
	PuglEventCrossing  crossing;  ///< #PUGL_ENTER_NOTIFY, #PUGL_LEAVE_NOTIFY
	PuglEventMotion    motion;    ///< #PUGL_MOTION_NOTIFY
	PuglEventScroll    scroll;    ///< #PUGL_SCROLL
	PuglEventFocus     focus;     ///< #PUGL_FOCUS_IN, #PUGL_FOCUS_OUT
	PuglEventClient    client;    ///< #PUGL_CLIENT
	PuglEventReceived  received;  ///< #PUGL_DATA_RECEIVED
} PuglEvent;

/**
   @defgroup status Status

   Status codes and error handling.

   @{
*/

/**
   Return status code.
*/
typedef enum {
	PUGL_SUCCESS,               ///< Success
	PUGL_FAILURE,               ///< Non-fatal failure
	PUGL_UNKNOWN_ERROR,         ///< Unknown system error
	PUGL_BAD_BACKEND,           ///< Invalid or missing backend
	PUGL_BAD_CONFIGURATION,     ///< Invalid view configuration
	PUGL_BAD_PARAMETER,         ///< Invalid parameter
	PUGL_BACKEND_FAILED,        ///< Backend initialisation failed
	PUGL_REGISTRATION_FAILED,   ///< Class registration failed
	PUGL_REALIZE_FAILED,        ///< System view realization failed
	PUGL_SET_FORMAT_FAILED,     ///< Failed to set pixel format
	PUGL_CREATE_CONTEXT_FAILED, ///< Failed to create drawing context
	PUGL_UNSUPPORTED_TYPE,      ///< Unsupported data type
} PuglStatus;

/**
   Return a string describing a status code.
*/
PUGL_API
const char*
puglStrerror(PuglStatus status);

/**
   @}
   @defgroup global Global
   Support for top-level applications.
   @{
*/

typedef enum {
	/**
	   Initialise threads.  This calls XInitThreads() on X11, which is
	   required by some drivers, particularly with Vulkan.
	*/
	PUGL_APPLICATION_THREADS = 1,
} PuglApplicationFlag;

/**
   Bitwise OR of PuglApplicationFlag values.
*/
typedef uint32_t PuglApplicationFlags;

/**
   Initialise an application.

   This must be called only once by a program, and before any other pugl
   functions are called (including creating the world).  This is only to be
   used by "top-level" applications at the start of main(), not in libraries or
   plugins.
*/
PUGL_API PuglStatus
puglInitApplication(PuglApplicationFlags flags);

/**
   @}
   @defgroup world World

   The top-level context of a Pugl application or plugin.

   The world contains all library-wide state.  There is no static data in Pugl,
   so it is safe to use multiple worlds in a single process.  This is to
   facilitate plugins or other situations where it is not possible to share a
   world, but a single world should be shared for all views where possible.

   @{
*/

/**
   The "world" of application state.

   The world represents everything that is not associated with a particular
   view.  Several worlds can be created in a single process, but code using
   different worlds must be isolated so they are never mixed.  Views are
   strongly associated with the world they were created in.
*/
typedef struct PuglWorldImpl PuglWorld;

/**
   Handle for the world's opaque user data.
*/
typedef void* PuglWorldHandle;

/**
   The type of a World.
*/
typedef enum {
	PUGL_PROGRAM, ///< Top-level application
	PUGL_MODULE   ///< Plugin or module within a larger application
} PuglWorldType;

/**
   World flags.
*/
typedef enum {
	/**
	   Set up support for threads if necessary.

	   - X11: Calls XInitThreads() which is required for some drivers.
	*/
	PUGL_WORLD_THREADS = 1 << 0
} PuglWorldFlag;

/**
   Bitwise OR of #PuglWorldFlag values.
*/
typedef uint32_t PuglWorldFlags;

/**
   A log message level, compatible with syslog.
*/
typedef enum {
	PUGL_LOG_LEVEL_ERR     = 3, ///< Error
	PUGL_LOG_LEVEL_WARNING = 4, ///< Warning
	PUGL_LOG_LEVEL_INFO    = 6, ///< Informational message
	PUGL_LOG_LEVEL_DEBUG   = 7  ///< Debug message
} PuglLogLevel;

/**
   A function called to report log messages.

   @param world The world that produced this log message.
   @param level Log level.
   @param msg Message string.
*/
typedef void (*PuglLogFunc)(PuglWorld*   world,
                            PuglLogLevel level,
                            const char*  msg);

/**
   Create a new world.

   @param type The type, which dictates what this world is responsible for.
   @param flags Flags to control world features.
   @return A new world, which must be later freed with puglFreeWorld().
*/
PUGL_API PuglWorld*
puglNewWorld(PuglWorldType type, PuglWorldFlags flags);

/**
   Free a world allocated with puglNewWorld().
*/
PUGL_API void
puglFreeWorld(PuglWorld* world);

/**
   Set the user data for the world.

   This is usually a pointer to a struct that contains all the state which must
   be accessed by several views.

   The handle is opaque to Pugl and is not interpreted in any way.
*/
PUGL_API void
puglSetWorldHandle(PuglWorld* world, PuglWorldHandle handle);

/**
   Get the user data for the world.
*/
PUGL_API PuglWorldHandle
puglGetWorldHandle(PuglWorld* world);

/**
   Return a pointer to the native handle of the world.

   @return
   - X11: A pointer to the `Display`.
   - MacOS: `NULL`.
   - Windows: The `HMODULE` of the calling process.
*/
PUGL_API void*
puglGetNativeWorld(PuglWorld* world);

/**
   Set the function to call to log a message.

   This will be called to report any log messages generated internally by Pugl
   which are enabled according to the log level.
*/
PUGL_API PuglStatus
puglSetLogFunc(PuglWorld* world, PuglLogFunc logFunc);

/**
   Set the level of log messages to emit.

   Any log messages with a level less than or equal to `level` will be emitted.
*/
PUGL_API PuglStatus
puglSetLogLevel(PuglWorld* world, PuglLogLevel level);

/**
   Set the class name of the application.

   This is a stable identifier for the application, used as the window
   class/instance name on X11 and Windows.  It is not displayed to the user,
   but can be used in scripts and by window managers, so it should be the same
   for every instance of the application, but different from other
   applications.
*/
PUGL_API PuglStatus
puglSetClassName(PuglWorld* world, const char* name);

/**
   Return the time in seconds.

   This is a monotonically increasing clock with high resolution.  The returned
   time is only useful to compare against other times returned by this
   function, its absolute value has no meaning.
*/
PUGL_API double
puglGetTime(const PuglWorld* world);

/**
   Update by processing events from the window system.

   This function is a single iteration of the main loop, and should be called
   repeatedly to update all views.

   If `timeout` is zero, then this function will not block.  Plugins should
   always use a timeout of zero to avoid blocking the host.

   If a positive `timeout` is given, then events will be processed for that
   amount of time, starting from when this function was called.

   If a negative `timeout` is given, this function will block indefinitely
   until an event occurs.

   For continuously animating programs, a timeout that is a reasonable fraction
   of the ideal frame period should be used, to minimise input latency by
   ensuring that as many input events are consumed as possible before drawing.

   @return #PUGL_SUCCESS if events are read, #PUGL_FAILURE if not, or an error.
*/
PUGL_API PuglStatus
puglUpdate(PuglWorld* world, double timeout);

/**
   A function called when an timer or awake event occurs for processing common
   things for the given world.
*/
typedef PuglStatus (*PuglProcessFunc)(PuglWorld* world, void* userData);

/**
   Set the world's process function. This function is called when an process 
   timer or awake event occurs.
*/
PUGL_API void
puglSetProcessFunc(PuglWorld* world, PuglProcessFunc processFunc, void* userData);

/**
   Sets the time in seconds when the world's process function will be
   invoked once. This timer is not periodic, i.e. this function has
   to be called again to schedule subsequent invocations.
   For seconds < 0 the timer is disabled.
*/
PUGL_API void
puglSetNextProcessTime(PuglWorld* world, double seconds);

/**
  Can be called from another thread to trigger the world's process function.
*/
PUGL_API void
puglAwake(PuglWorld* world);

/**
   @}

   @defgroup view View

   A drawable region that receives events.

   A view can be thought of as a window, but does not necessarily correspond to
   a top-level window in a desktop environment.  For example, a view can be
   embedded in some other window, or represent an embedded system where there
   is no concept of multiple windows at all.

   @{
*/

/**
   A drawable region that receives events.
*/
typedef struct PuglViewImpl PuglView;

/**
   A graphics backend.

   The backend dictates how graphics are set up for a view, and how drawing is
   performed.  A backend must be set by calling puglSetBackend() before
   realising a view.

   If you are using a local copy of Pugl, it is possible to implement a custom
   backend.  See the definition of `PuglBackendImpl` in the source code for
   details.
*/
typedef struct PuglBackendImpl PuglBackend;

/**
   A native view handle.

   X11: This is a `Window`.

   MacOS: This is a pointer to an `NSView*`.

   Windows: This is a `HWND`.
*/
typedef uintptr_t PuglNativeView;

/**
   Handle for a view's opaque user data.
*/
typedef void* PuglHandle;

/**
   A hint for configuring a view.
*/
typedef enum {
	PUGL_USE_COMPAT_PROFILE,    ///< Use compatible (not core) OpenGL profile
	PUGL_USE_DEBUG_CONTEXT,     ///< True to use a debug OpenGL context
	PUGL_CONTEXT_VERSION_MAJOR, ///< OpenGL context major version
	PUGL_CONTEXT_VERSION_MINOR, ///< OpenGL context minor version
	PUGL_RED_BITS,              ///< Number of bits for red channel
	PUGL_GREEN_BITS,            ///< Number of bits for green channel
	PUGL_BLUE_BITS,             ///< Number of bits for blue channel
	PUGL_ALPHA_BITS,            ///< Number of bits for alpha channel
	PUGL_DEPTH_BITS,            ///< Number of bits for depth buffer
	PUGL_STENCIL_BITS,          ///< Number of bits for stencil buffer
	PUGL_SAMPLES,               ///< Number of samples per pixel (AA)
	PUGL_DOUBLE_BUFFER,         ///< True if double buffering should be used
	PUGL_SWAP_INTERVAL,         ///< Number of frames between buffer swaps
	PUGL_RESIZABLE,             ///< True if view should be resizable
	PUGL_IGNORE_KEY_REPEAT,     ///< True if key repeat events are ignored
	PUGL_IS_POPUP,              ///< True if window is popup window
	PUGL_DONT_MERGE_RECTS,      ///< True if redraw rects are not merged

	PUGL_NUM_VIEW_HINTS
} PuglViewHint;

/**
   A special view hint value.
*/
typedef enum {
	PUGL_DONT_CARE = -1, ///< Use best available value
	PUGL_FALSE     = 0,  ///< Explicitly false
	PUGL_TRUE      = 1   ///< Explicitly true
} PuglViewHintValue;

/**
   A function called when an event occurs.
*/
typedef PuglStatus (*PuglEventFunc)(PuglView* view, const PuglEvent* event);

/**
   @name Setup
   Functions for creating and destroying a view.
   @{
*/

/**
   Create a new view.

   A newly created view does not correspond to a real system view or window.
   It must first be configured, then the system view can be created with
   puglRealize().
*/
PUGL_API PuglView*
puglNewView(PuglWorld* world);

/**
   Free a view created with puglNewView().
*/
PUGL_API void
puglFreeView(PuglView* view);

/**
   Return the world that `view` is a part of.
*/
PUGL_API PuglWorld*
puglGetWorld(PuglView* view);

/**
   Set the user data for a view.

   This is usually a pointer to a struct that contains all the state which must
   be accessed by a view.  Everything needed to process events should be stored
   here, not in static variables.

   The handle is opaque to Pugl and is not interpreted in any way.
*/
PUGL_API void
puglSetHandle(PuglView* view, PuglHandle handle);

/**
   Get the user data for a view.
*/
PUGL_API PuglHandle
puglGetHandle(PuglView* view);

/**
   Set the graphics backend to use for a view.

   This must be called once to set the graphics backend before calling
   puglRealize().

   Pugl includes the following backends:

   - puglGlBackend(), declared in pugl_gl.h
   - puglCairoBackend(), declared in pugl_cairo.h

   Note that backends are modular and not compiled into the main Pugl library
   to avoid unnecessary dependencies.  To use a particular backend,
   applications must link against the appropriate backend library, or be sure
   to compile in the appropriate code if using a local copy of Pugl.
*/
PUGL_API PuglStatus
puglSetBackend(PuglView* view, const PuglBackend* backend);

/**
   Set the function to call when an event occurs.
*/
PUGL_API PuglStatus
puglSetEventFunc(PuglView* view, PuglEventFunc eventFunc);

/**
   Set a hint to configure view properties.

   This only has an effect when called before puglRealize().
*/
PUGL_API PuglStatus
puglSetViewHint(PuglView* view, PuglViewHint hint, int value);

/**
   @}
   @anchor frame
   @name Frame
   Functions for working with the position and size of a view.
   @{
*/

/**
   Get the current position and size of the view.

   The position is in screen coordinates with an upper left origin.
*/
PUGL_API PuglRect
puglGetFrame(const PuglView* view);

/**
   Set the current position and size of the view.

   The position is in screen coordinates with an upper left origin.
*/
PUGL_API PuglStatus
puglSetFrame(PuglView* view, PuglRect frame);

/**
   Set the current size of the view.
   
   This could also be called before puglResize() to set the default size of the
   view, which will be the initial size of the window if this is a top level
   view.
*/
PUGL_API PuglStatus
puglSetSize(PuglView* view, int width, int height);

/**
   Set the minimum size of the view.

   If an initial minimum size is known, this should be called before
   puglRealize() to avoid stutter, though it can be called afterwards as well.
*/
PUGL_API PuglStatus
puglSetMinSize(PuglView* view, int width, int height);

/**
   Set the maximum size of the view.

   If an initial maximum size is known, this should be called before
   puglRealize() to avoid stutter, though it can be called afterwards as well.
*/
PUGL_API PuglStatus
puglSetMaxSize(PuglView* view, int width, int height);

/**
   Set the view aspect ratio range.

   The x and y values here represent a ratio of width to height.  To set a
   fixed aspect ratio, set the minimum and maximum values to the same ratio.

   Note that setting different minimum and maximum constraints does not
   currenty work on MacOS (the minimum is used), so only setting a fixed aspect
   ratio works properly across all platforms.

   If an initial aspect ratio is known, this should be called before
   puglRealize() to avoid stutter, though it can be called afterwards as well.
*/
PUGL_API PuglStatus
puglSetAspectRatio(PuglView* view, int minX, int minY, int maxX, int maxY);

/**
   @}
   @name Windows
   Functions for working with system views and the window hierarchy.
   @{
*/

/**
   Set the title of the window.

   This only makes sense for non-embedded views that will have a corresponding
   top-level window, and sets the title, typically displayed in the title bar
   or in window switchers.
*/
PUGL_API PuglStatus
puglSetWindowTitle(PuglView* view, const char* title);

/**
   Set the parent window for embedding a view in an existing window.

   This must be called before puglRealize(), reparenting is not supported.
*/
PUGL_API PuglStatus
puglSetParentWindow(PuglView* view, PuglNativeView parent);

/**
   Set the transient parent of the window.

   Set this for transient children like dialogs, to have them properly
   associated with their parent window.  This should be called before
   puglRealize().

   A view can either have a parent (for embedding) or a transient parent (for
   top-level windows like dialogs), but not both.
*/
PUGL_API PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeView parent);

/**
   Realise a view by creating a corresponding system view or window.

   After this call, the (initially invisible) underlying system view exists and
   can be accessed with puglGetNativeWindow().  There is currently no
   corresponding unrealize function, the system view will be destroyed along
   with the view when puglFreeView() is called.

   The view should be fully configured using the above functions before this is
   called.  This function may only be called once per view.
*/
PUGL_API PuglStatus
puglRealize(PuglView* view);

/**
   Show the view.

   If the view has not yet been realized, the first call to this function will
   do so automatically.

   If the view is currently hidden, it will be shown and possibly raised to the
   top depending on the platform.
*/
PUGL_API PuglStatus
puglShowWindow(PuglView* view);

/**
   Hide the current window.
*/
PUGL_API PuglStatus
puglHideWindow(PuglView* view);

/**
   Return true iff the view is currently visible.
*/
PUGL_API bool
puglGetVisible(const PuglView* view);

/**
   Return the native window handle.
*/
PUGL_API PuglNativeView
puglGetNativeWindow(PuglView* view);

/**
   @}
   @name Graphics
   Functions for working with the graphics context and scheduling redisplays.
   @{
*/

/**
   Get the graphics context.

   This is a backend-specific context used for drawing if the backend graphics
   API requires one.  It is only available during an expose.

   @return
   - OpenGL: `NULL`.
   - Cairo: A pointer to a
     [`cairo_t`](http://www.cairographics.org/manual/cairo-cairo-t.html).
*/
PUGL_API void*
puglGetContext(PuglView* view);

/**
   Request a redisplay for the entire view.

   This will cause an expose event to be dispatched later.  If called from
   within the event handler, the expose should arrive at the end of the current
   event loop iteration, though this is not strictly guaranteed on all
   platforms.  If called elsewhere, an expose will be enqueued to be processed
   in the next event loop iteration.
*/
PUGL_API PuglStatus
puglPostRedisplay(PuglView* view);

/**
   Request a redisplay of the given rectangle within the view.

   This has the same semantics as puglPostRedisplay(), but allows giving a
   precise region for redrawing only a portion of the view.
*/
PUGL_API PuglStatus
puglPostRedisplayRect(PuglView* view, PuglRect rect);

/**
   Get the screen scaling factor for the view.
*/
PUGL_API double
puglGetScreenScale(PuglView* view);

/**
   Get the screen scaling factor for the main display.
*/
PUGL_API double
puglGetDefaultScreenScale(PuglWorld* world);


/**
   @}
   @anchor interaction
   @name Interaction
   Functions for interacting with the user.
   @{
*/

/**
   A mouse cursor type.

   This is a portable subset of mouse cursors that exist on X11, MacOS, and
   Windows.
*/
typedef enum {
	PUGL_CURSOR_ARROW,      ///< Default pointing arrow
	PUGL_CURSOR_CARET,      ///< Caret (I-Beam) for text entry
	PUGL_CURSOR_CROSSHAIR,  ///< Cross-hair
	PUGL_CURSOR_HAND,       ///< Hand with a pointing finger
	PUGL_CURSOR_NO,         ///< Operation not allowed
	PUGL_CURSOR_LEFT_RIGHT, ///< Left/right arrow for horizontal resize
	PUGL_CURSOR_UP_DOWN,    ///< Up/down arrow for vertical resize
} PuglCursor;

/**
   Grab the keyboard input focus.
*/
PUGL_API PuglStatus
puglGrabFocus(PuglView* view);

/**
   Return whether `view` has the keyboard input focus.
*/
PUGL_API bool
puglHasFocus(const PuglView* view);

/**
   Set the clipboard contents.

   This sets the system clipboard contents, which can be retrieved with
   puglGetClipboard() or pasted into other applications.

   @param world The world.
   @param type The MIME type of the data, "text/plain" is assumed if NULL.
   @param data The data to copy to the clipboard.
   @param len The length of data in bytes.
*/
PUGL_API PuglStatus
puglSetClipboard(PuglWorld*  world,
                 const char* type,
                 const void* data,
                 size_t      len);

/**
  Returns true iff the given world owns the clipboard. This
  is always false on Win and Mac.
*/
PUGL_API bool
puglHasClipboard(PuglWorld*  world);

/**
   Request clipboard contents.
   
   The clipboard content will be received via PUGL_DATA_RECEIVED event.

   @param view The view.
*/
PUGL_API PuglStatus
puglRequestClipboard(PuglView* view);

/**
   Set the mouse cursor.

   This changes the system cursor that is displayed when the pointer is inside
   the view.  May fail if setting the cursor is not supported on this system,
   for example if compiled on X11 without Xcursor support.
 */
PUGL_API PuglStatus
puglSetCursor(PuglView* view, PuglCursor cursor);

/**
   Request user attention.

   This hints to the system that the window or application requires attention
   from the user.  The exact effect depends on the platform, but is usually
   something like a flashing task bar entry or bouncing application icon.
*/
PUGL_API PuglStatus
puglRequestAttention(PuglView* view);

/**
   Send an event to a view via the window system.

   If supported, the event will be delivered to the view via the event loop
   like other events.  Note that this function only works for certain event
   types, and will return PUGL_UNSUPPORTED_TYPE for events that are not
   supported.
*/
PUGL_API PuglStatus
puglSendEvent(PuglView* view, const PuglEvent* event);

/**
   @}
*/

#ifndef PUGL_DISABLE_DEPRECATED

/**
   @}
   @name Deprecated API
   @{
*/

/**
   A native window handle.

   X11: This is a `Window`.

   MacOS: This is a pointer to an `NSView*`.

   Windows: This is a `HWND`.
*/
typedef uintptr_t PuglNativeWindow;

/**
   Create a Pugl application and view.

   To create a window, call the various puglInit* functions as necessary, then
   call puglRealize().

   @deprecated Use puglNewApp() and puglNewView().

   @param pargc Pointer to argument count (currently unused).
   @param argv  Arguments (currently unused).
   @return A newly created view.
*/
static inline PUGL_DEPRECATED_BY("puglNewView") PuglView*
puglInit(const int* pargc, char** argv)
{
	(void)pargc;
	(void)argv;

	return puglNewView(puglNewWorld(PUGL_MODULE, 0));
}

/**
   Destroy an app and view created with `puglInit()`.

   @deprecated Use puglFreeApp() and puglFreeView().
*/
static inline PUGL_DEPRECATED_BY("puglFreeView") void
puglDestroy(PuglView* view)
{
	PuglWorld* const world = puglGetWorld(view);

	puglFreeView(view);
	puglFreeWorld(world);
}

/**
   Set the window class name before creating a window.
*/
static inline PUGL_DEPRECATED_BY("puglSetClassName") void
puglInitWindowClass(PuglView* view, const char* name)
{
	puglSetClassName(puglGetWorld(view), name);
}

/**
   Set the window size before creating a window.

   @deprecated Use puglSetFrame().
*/
static inline PUGL_DEPRECATED_BY("puglSetFrame") void
puglInitWindowSize(PuglView* view, int width, int height)
{
	PuglRect frame = puglGetFrame(view);

	frame.width = width;
	frame.height = height;

	puglSetFrame(view, frame);
}

/**
   Set the minimum window size before creating a window.
*/
static inline PUGL_DEPRECATED_BY("puglSetMinSize") void
puglInitWindowMinSize(PuglView* view, int width, int height)
{
	puglSetMinSize(view, width, height);
}

/**
   Set the window aspect ratio range before creating a window.

   The x and y values here represent a ratio of width to height.  To set a
   fixed aspect ratio, set the minimum and maximum values to the same ratio.

   Note that setting different minimum and maximum constraints does not
   currenty work on MacOS (the minimum is used), so only setting a fixed aspect
   ratio works properly across all platforms.
*/
static inline PUGL_DEPRECATED_BY("puglSetAspectRatio") void
puglInitWindowAspectRatio(PuglView* view,
                          int       minX,
                          int       minY,
                          int       maxX,
                          int       maxY)
{
	puglSetAspectRatio(view, minX, minY, maxX, maxY);
}

/**
   Set transient parent before creating a window.

   On X11, parent must be a Window.
   On OSX, parent must be an NSView*.
*/
static inline PUGL_DEPRECATED_BY("puglSetTransientFor") void
puglInitTransientFor(PuglView* view, uintptr_t parent)
{
	puglSetTransientFor(view, (PuglNativeWindow)parent);
}

/**
   Enable or disable resizing before creating a window.

   @deprecated Use puglSetViewHint() with #PUGL_RESIZABLE.
*/
static inline PUGL_DEPRECATED_BY("puglSetViewHint") void
puglInitResizable(PuglView* view, bool resizable)
{
	puglSetViewHint(view, PUGL_RESIZABLE, resizable);
}

/**
   Get the current size of the view.

   @deprecated Use puglGetFrame().

*/
static inline PUGL_DEPRECATED_BY("puglGetFrame") void
puglGetSize(PuglView* view, int* width, int* height)
{
	const PuglRect frame = puglGetFrame(view);

	*width  = (int)frame.width;
	*height = (int)frame.height;
}

/**
   Ignore synthetic repeated key events.

   @deprecated Use puglSetViewHint() with #PUGL_IGNORE_KEY_REPEAT.
*/
static inline PUGL_DEPRECATED_BY("puglSetViewHint") void
puglIgnoreKeyRepeat(PuglView* view, bool ignore)
{
	puglSetViewHint(view, PUGL_IGNORE_KEY_REPEAT, ignore);
}

/**
   Set a hint before creating a window.

   @deprecated Use puglSetWindowHint().
*/
static inline PUGL_DEPRECATED_BY("puglSetViewHint") void
puglInitWindowHint(PuglView* view, PuglViewHint hint, int value)
{
	puglSetViewHint(view, hint, value);
}

/**
   Set the parent window before creating a window (for embedding).

   @deprecated Use puglSetWindowParent().
*/
static inline PUGL_DEPRECATED_BY("puglSetParentWindow") void
puglInitWindowParent(PuglView* view, PuglNativeWindow parent)
{
	puglSetParentWindow(view, parent);
}

/**
   Set the graphics backend to use.

   @deprecated Use puglSetBackend().
*/
static inline PUGL_DEPRECATED_BY("puglSetBackend") int
puglInitBackend(PuglView* view, const PuglBackend* backend)
{
	return (int)puglSetBackend(view, backend);
}

/**
   Realise a view by creating a corresponding system view or window.

   The view should be fully configured using the above functions before this is
   called.  This function may only be called once per view.

   @deprecated Use puglRealize(), or just show the view.
*/
static inline PUGL_DEPRECATED_BY("puglRealize") PuglStatus
puglCreateWindow(PuglView* view, const char* title)
{
	puglSetWindowTitle(view, title);
	return puglRealize(view);
}

/**
   Block and wait for an event to be ready.

   This can be used in a loop to only process events via puglProcessEvents when
   necessary.  This function will block indefinitely if no events are
   available, so is not appropriate for use in programs that need to perform
   regular updates (e.g. animation).

   @deprecated Use puglPollEvents().
*/
PUGL_API PUGL_DEPRECATED_BY("puglPollEvents") PuglStatus
puglWaitForEvent(PuglView* view);

/**
   Process all pending window events.

   This handles input events as well as rendering, so it should be called
   regularly and rapidly enough to keep the UI responsive.  This function does
   not block if no events are pending.

   @deprecated Use puglDispatchEvents().
*/
PUGL_API PUGL_DEPRECATED_BY("puglDispatchEvents") PuglStatus
puglProcessEvents(PuglView* view);

/**
   Poll for events that are ready to be processed.

   This polls for events that are ready for any view in the world, potentially
   blocking depending on `timeout`.

   @param world The world to poll for events.

   @param timeout Maximum time to wait, in seconds.  If zero, the call returns
   immediately, if negative, the call blocks indefinitely.

   @return #PUGL_SUCCESS if events are read, #PUGL_FAILURE if not, or an error.

   @deprecated Use puglUpdate().
*/
PUGL_API PUGL_DEPRECATED_BY("puglUpdate") PuglStatus
puglPollEvents(PuglWorld* world, double timeout);

/**
   Dispatch any pending events to views.

   This processes all pending events, dispatching them to the appropriate
   views.  View event handlers will be called in the scope of this call.  This
   function does not block, if no events are pending then it will return
   immediately.

   @deprecated Use puglUpdate().
*/
PUGL_API PUGL_DEPRECATED_BY("puglUpdate") PuglStatus
puglDispatchEvents(PuglWorld* world);

#endif  /* PUGL_DISABLE_DEPRECATED */

/**
   Enter the graphics context.

   Note that, unlike some similar libraries, Pugl automatically enters and
   leaves the graphics context when required and application should not
   normally do this.  Drawing in Pugl is only allowed during the processing of
   an expose event.

   However, this can be used to enter the graphics context elsewhere, for
   example to call any GL functions during setup.

   @param view The view being entered.
   @param drawing If true, prepare for drawing.

   @deprecated Set up graphics when a #PUGL_CREATE event is received.
*/
PUGL_API /*PUGL_DEPRECATED_BY("PUGL_CREATE")*/ PuglStatus
puglEnterContext(PuglView* view, bool drawing);

/**
   Leave the graphics context.

   This must be called after puglEnterContext() with a matching `drawing`
   parameter.

   @param view The view being left.
   @param drawing If true, finish drawing, for example by swapping buffers.

   @deprecated Shut down graphics when a #PUGL_DESTROY event is received.
*/
PUGL_API /*PUGL_DEPRECATED_BY("PUGL_DESTROY")*/ PuglStatus
puglLeaveContext(PuglView* view, bool drawing);


/**
   @}
   @}
   @}
*/

PUGL_END_DECLS

#endif  /* PUGL_PUGL_H */
