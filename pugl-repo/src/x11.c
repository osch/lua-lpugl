/*
  Copyright 2012-2020 David Robillard <d@drobilla.net>
  Copyright 2013 Robin Gareus <robin@gareus.org>
  Copyright 2011-2012 Ben Loftis, Harrison Consoles

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

#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 199309L
#endif

#include "x11.h"

#include "implementation.h"
#include "rect.h"
#include "types.h"

#include "pugl/pugl.h"

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <X11/Xresource.h>
#include <X11/cursorfont.h>

#ifdef HAVE_XRANDR
#  include <X11/extensions/Xrandr.h>
#endif

#ifdef HAVE_XSYNC
#  include <X11/extensions/sync.h>
#  include <X11/extensions/syncconst.h>
#endif

#ifdef HAVE_XCURSOR
#  include <X11/Xcursor/Xcursor.h>
#  include <X11/cursorfont.h>
#endif

#include <sys/select.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define KEYSYM2UCS_INCLUDED
#include "x11_keysym2ucs.c"

#ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#include "x11_clip.h"

#define PUGL_XC_DEFAULT_ARROW XC_left_ptr

enum WmClientStateMessageAction {
  WM_STATE_REMOVE,
  WM_STATE_ADD,
  WM_STATE_TOGGLE
};

PuglStatus
puglInitApplication(PuglApplicationFlags flags)
{
  if (flags & PUGL_APPLICATION_THREADS) {
    XInitThreads();
  }

  return PUGL_SUCCESS;
}

PuglWorldInternals*
puglInitWorldInternals(PuglWorldType type, PuglWorldFlags flags)
{
  if (type == PUGL_PROGRAM && (flags & PUGL_WORLD_THREADS)) {
    XInitThreads();
  }

  Display* display = XOpenDisplay(NULL);
  if (!display) {
    return NULL;
  }
  // XSynchronize(display, True); // for debugging

  PuglWorldInternals* impl =
    (PuglWorldInternals*)calloc(1, sizeof(PuglWorldInternals));
  if (!impl) {
    XCloseDisplay(display);
    return NULL;
  }
  impl->display = display;

  // Intern the various atoms we will need
  impl->atoms.CLIPBOARD        = XInternAtom(display, "CLIPBOARD", 0);
  impl->atoms.TARGETS          = XInternAtom(display, "TARGETS", 0);
  impl->atoms.INCR             = XInternAtom(display, "INCR", 0);
  impl->atoms.UTF8_STRING      = XInternAtom(display, "UTF8_STRING", 0);
  impl->atoms.WM_PROTOCOLS     = XInternAtom(display, "WM_PROTOCOLS", 0);
  impl->atoms.WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  impl->atoms.PUGL_CLIENT_MSG  = XInternAtom(display, "_PUGL_CLIENT_MSG", 0);
  impl->atoms.NET_WM_NAME      = XInternAtom(display, "_NET_WM_NAME", 0);
  impl->atoms.NET_WM_STATE     = XInternAtom(display, "_NET_WM_STATE", 0);
  impl->atoms.NET_WM_STATE_DEMANDS_ATTENTION =
    XInternAtom(display, "_NET_WM_STATE_DEMANDS_ATTENTION", 0);

  // Open input method
  XSetLocaleModifiers("");
  if (!(impl->xim = XOpenIM(display, NULL, NULL, NULL))) {
    XSetLocaleModifiers("@im=");
    impl->xim = XOpenIM(display, NULL, NULL, NULL);
  }

  XFlush(display);

  if (pipe(impl->awake_fds) == 0) {
    fcntl(impl->awake_fds[1],
          F_SETFL,
          fcntl(impl->awake_fds[1], F_GETFL) | O_NONBLOCK);
  } else {
    impl->awake_fds[0] = -1;
  }

  impl->nextProcessTime = -1;

  // Key modifiers
  XModifierKeymap* map = XGetModifierMapping(display);
  if (map) {
    const int max_keypermod = map->max_keypermod;
    for (int i = 0; i < 8; ++i) {
      for (int j = 0; j < max_keypermod; ++j) {
        const KeyCode keyCode = map->modifiermap[i * max_keypermod + j];
        const KeySym  keySym  = XkbKeycodeToKeysym(
          display,
          keyCode,
          0,
          0); // deprecated: XKeycodeToKeysym(display, keyCode, 0);
        switch (keySym) {
        case XK_Shift_L:
        case XK_Shift_R:
          impl->shiftKeyStates |= (1 << i);
          break;

        case XK_Control_L:
        case XK_Control_R:
          impl->controlKeyStates |= (1 << i);
          break;

        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Meta_L:
        case XK_Meta_R:
          impl->altKeyStates |= (1 << i);
          break;

        case XK_Super_L:
        case XK_Super_R:
          impl->superKeyStates |= (1 << i);
          break;

        case XK_Mode_switch:
        case XK_ISO_Level3_Shift:
          impl->altgrKeyStates |= (1 << i);
          break;
        }
      }
    }
    XFreeModifiermap(map);
  } else {
    // Fallback
    /*impl->shiftKeyStates    = ShiftMask;
    impl->controlKeyStates  = ControlMask;
    impl->altKeyStates      = Mod1Mask;
    impl->superKeyStates    = Mod4Mask;
    impl->altgrKeyStates    = Mod5Mask;*/
  }

  return impl;
}

static void
initWorldPseudoWin(PuglWorld* world)
{
  PuglWorldInternals* impl = world->impl;
  if (!impl->pseudoWin) {
    impl->pseudoWin = XCreateSimpleWindow(
      impl->display,
      RootWindow(impl->display, DefaultScreen(impl->display)),
      0,
      0,
      1,
      1,
      0,
      0,
      0);
    XSelectInput(impl->display, impl->pseudoWin, PropertyChangeMask);
  }
}

void*
puglGetNativeWorld(PuglWorld* world)
{
  return world->impl->display;
}

PuglStatus
puglSetClassName(PuglWorld* const world, const char* name)
{
  puglSetString(&world->className, name);
  return PUGL_SUCCESS;
}

PuglInternals*
puglInitViewInternals(void)
{
  PuglInternals* impl = (PuglInternals*)calloc(1, sizeof(PuglInternals));
  if (!impl) {
    return NULL;
  }
  impl->cursorShape = PUGL_XC_DEFAULT_ARROW;

  return impl;
}

static PuglStatus
puglPollX11Socket(PuglWorld* world, const double timeout0)
{
  PuglWorldInternals* impl = world->impl;
  if (XEventsQueued(world->impl->display, QueuedAlready) > 0) {
    // evaluate only awake queue
    const int afd = impl->awake_fds[0];
    if (afd >= 0) {
      fd_set fds;
      int    nfds = afd + 1;
      FD_ZERO(&fds);
      FD_SET(afd, &fds);
      struct timeval tv  = {0, 0};
      int            ret = select(nfds, &fds, NULL, NULL, &tv);
      if (ret > 0 && FD_ISSET(afd, &fds)) {
        impl->needsProcessing = true;
        char buf[128];
        int  PUGL_UNUSED(ignore)
        = read(afd, buf, sizeof(buf));
      }
    }
    return PUGL_SUCCESS;
  }

  const int fd   = ConnectionNumber(impl->display);
  const int afd  = impl->awake_fds[0];
  int       nfds = (fd > afd ? fd : afd) + 1;
  int       ret  = 0;
  fd_set    fds;
  FD_ZERO(&fds); // NOLINT
  FD_SET(fd, &fds);
  if (afd >= 0) {
    FD_SET(afd, &fds);
  }
  double timeout;
  if (impl->nextProcessTime >= 0) {
    timeout = impl->nextProcessTime - puglGetTime(world);
    if (timeout < 0) {
      timeout = 0;
    }
    if (timeout0 >= 0 && timeout0 < timeout) {
      timeout = timeout0;
    }
  } else {
    timeout = timeout0;
  }
  if (timeout < 0.0) {
    ret = select(nfds, &fds, NULL, NULL, NULL);
  } else {
    const long     sec  = (long)timeout;
    const long     usec = (long)((timeout - (double)sec) * 1e6);
    struct timeval tv   = {sec, usec};
    ret                 = select(nfds, &fds, NULL, NULL, &tv);
  }
  bool hasEvents = FD_ISSET(fd, &fds);
  if (ret > 0 && afd >= 0 && FD_ISSET(afd, &fds)) {
    hasEvents             = true;
    impl->needsProcessing = true;
    char buf[128];
    int  PUGL_UNUSED(ignore)
    = read(afd, buf, sizeof(buf));
  }
  if (impl->nextProcessTime >= 0 &&
      impl->nextProcessTime <= puglGetTime(world)) {
    hasEvents             = true;
    impl->needsProcessing = true;
  }
  if (ret < 0) {
    if (errno == EINTR) {
      return PUGL_FAILURE; // was interrupted
    } else {
      return PUGL_UNKNOWN_ERROR;
    }
  } else {
    return hasEvents ? PUGL_SUCCESS : PUGL_FAILURE;
  }
}

static PuglView*
puglFindView(PuglWorld* world, const Window window)
{
  for (size_t i = 0; i < world->numViews; ++i) {
    if (world->views[i]->impl->win == window) {
      return world->views[i];
    }
  }

  return NULL;
}

static PuglStatus
updateSizeHints(const PuglView* view)
{
  if (!view->impl->win) {
    return PUGL_SUCCESS;
  }

  Display*    display   = view->world->impl->display;
  XSizeHints* sizeHints = XAllocSizeHints();

  if (!sizeHints) {
    return PUGL_FAILURE;
  }

  if (!view->hints[PUGL_RESIZABLE]) {
    sizeHints->flags       = PBaseSize | PMinSize | PMaxSize;
    sizeHints->base_width  = (int)view->reqWidth;
    sizeHints->base_height = (int)view->reqHeight;
    sizeHints->min_width   = (int)view->reqWidth;
    sizeHints->min_height  = (int)view->reqHeight;
    sizeHints->max_width   = (int)view->reqWidth;
    sizeHints->max_height  = (int)view->reqHeight;
  } else {
    if (view->reqWidth >= 0 && view->reqHeight >= 0) {
      sizeHints->flags       = PBaseSize;
      sizeHints->base_width  = view->reqWidth;
      sizeHints->base_height = view->reqHeight;
    }
    if (view->minWidth || view->minHeight) {
      sizeHints->flags |= PMinSize;
      sizeHints->min_width  = view->minWidth;
      sizeHints->min_height = view->minHeight;
    }
    if (view->maxWidth || view->maxHeight) {
      sizeHints->flags |= PMaxSize;
      sizeHints->max_width  = view->maxWidth;
      sizeHints->max_height = view->maxHeight;
      if (sizeHints->max_width < 0) {
        sizeHints->max_width = INT_MAX;
      }
      if (sizeHints->max_height < 0) {
        sizeHints->max_height = INT_MAX;
      }
    }
    if (view->minAspectX) {
      sizeHints->flags |= PAspect;
      sizeHints->min_aspect.x = view->minAspectX;
      sizeHints->min_aspect.y = view->minAspectY;
      sizeHints->max_aspect.x = view->maxAspectX;
      sizeHints->max_aspect.y = view->maxAspectY;
    }
  }

  XSetNormalHints(display, view->impl->win, sizeHints);
  XFree(sizeHints);
  return PUGL_SUCCESS;
}

static PuglStatus
puglDefineCursorShape(PuglView* view, unsigned shape)
{
  PuglInternals* const impl    = view->impl;
  PuglWorld* const     world   = view->world;
  Display* const       display = world->impl->display;
  Cursor               cur;
  if (shape == XC_X_cursor) { // HIDDEN
    const char emptyPixmapBytes[] = {0};
    Pixmap     emptyPixmap =
      XCreateBitmapFromData(display, impl->win, emptyPixmapBytes, 1, 1);
    XColor color = {0, 0, 0, 0, 0, 0};
    cur          = XCreatePixmapCursor(
      display, emptyPixmap, emptyPixmap, &color, &color, 0, 0);
    XFreePixmap(display, emptyPixmap);
  } else {
    cur = XCreateFontCursor(display, shape);
  }
  if (cur) {
    XDefineCursor(display, impl->win, cur);
    XFreeCursor(display, cur);
    return PUGL_SUCCESS;
  }

  return PUGL_FAILURE;
}

PuglStatus
puglRealize(PuglView* view)
{
  PuglInternals* const impl    = view->impl;
  PuglWorld* const     world   = view->world;
  PuglX11Atoms* const  atoms   = &view->world->impl->atoms;
  Display* const       display = world->impl->display;
  const int            screen  = DefaultScreen(display);
  const Window         root    = RootWindow(display, screen);
  const Window         parent  = view->parent ? (Window)view->parent : root;
  XSetWindowAttributes attr    = {0};
  PuglStatus           st      = PUGL_SUCCESS;

  // Ensure that we're unrealized and that a reasonable backend has been set
  if (impl->win) {
    return PUGL_FAILURE;
  }

  if (!view->backend || !view->backend->configure) {
    return PUGL_BAD_BACKEND;
  } else if (view->reqWidth <= 0 && view->reqHeight <= 0) {
    return PUGL_BAD_CONFIGURATION;
  }

  // Don't center top-level windows if a position has not been set, let
  // the window manager do this.

  // Configure the backend to get the visual info
  impl->display = display;
  impl->screen  = screen;
  if ((st = view->backend->configure(view)) || !impl->vi) {
    view->backend->destroy(view);
    return st ? st : PUGL_BACKEND_FAILED;
  }

  // Create a colormap based on the visual info from the backend
  attr.colormap = XCreateColormap(display, parent, impl->vi->visual, AllocNone);

  // Set the event mask to request all of the event types we react to
  attr.event_mask |= ButtonPressMask;
  attr.event_mask |= ButtonReleaseMask;
  attr.event_mask |= EnterWindowMask;
  attr.event_mask |= ExposureMask;
  attr.event_mask |= FocusChangeMask;
  attr.event_mask |= KeyPressMask;
  attr.event_mask |= KeyReleaseMask;
  attr.event_mask |= LeaveWindowMask;
  attr.event_mask |= PointerMotionMask;
  attr.event_mask |= StructureNotifyMask;
  attr.event_mask |= VisibilityChangeMask;
  attr.event_mask |= PropertyChangeMask;

  attr.override_redirect = view->hints[PUGL_IS_POPUP];

  impl->win = XCreateWindow(display,
                            parent,
                            (int)view->reqX,
                            (int)view->reqY,
                            (unsigned)view->reqWidth,
                            (unsigned)view->reqHeight,
                            0,
                            impl->vi->depth,
                            InputOutput,
                            impl->vi->visual,
                            CWColormap | CWEventMask, // | CWOverrideRedirect,
                            &attr);

  bool isTransient = !view->parent && view->transientParent;
  bool isPopup     = !view->parent && view->hints[PUGL_IS_POPUP];

  Atom net_wm_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  Atom net_wm_type_kind =
    isPopup ? XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False)
            : XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  XChangeProperty(display,
                  impl->win,
                  net_wm_type,
                  XA_ATOM,
                  32,
                  PropModeReplace,
                  (unsigned char*)&net_wm_type_kind,
                  1);

  {
    XWMHints* xhints = XAllocWMHints();
    xhints->flags    = InputHint;
    xhints->input    = (isPopup && isTransient) ? False : True;
    XSetWMHints(display, impl->win, xhints);
    XFree(xhints);
  }

  // motif hints: flags, function, decorations, input_mode, status
  long motifHints[5] = {0, 0, 0, 0, 0};
  if (isPopup) {
    motifHints[0] |= 2; // MWM_HINTS_DECORATIONS
    motifHints[2] = 0;  // no decorations
  }
  XChangeProperty(display,
                  impl->win,
                  XInternAtom(display, "_MOTIF_WM_HINTS", False),
                  XInternAtom(display, "_MOTIF_WM_HINTS", False),
                  32,
                  0,
                  (unsigned char*)motifHints,
                  5);

  if (isTransient) {
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    XChangeProperty(display,
                    impl->win,
                    atoms->NET_WM_STATE,
                    XA_ATOM,
                    32,
                    PropModeAppend,
                    (unsigned char*)&wm_state,
                    1);
  }

  // Create the backend drawing context/surface
  if ((st = view->backend->create(view))) {
    return st;
  }

#ifdef HAVE_XRANDR
  // Set refresh rate hint to the real refresh rate
  XRRScreenConfiguration* conf         = XRRGetScreenInfo(display, parent);
  short                   current_rate = XRRConfigCurrentRate(conf);

  view->hints[PUGL_REFRESH_RATE] = current_rate;
  XRRFreeScreenConfigInfo(conf);
#endif

  updateSizeHints(view);

  XClassHint classHint = {world->className, world->className};
  XSetClassHint(display, impl->win, &classHint);

  if (view->title) {
    puglSetWindowTitle(view, view->title);
  }

  if (parent == root) {
    XSetWMProtocols(display, impl->win, &atoms->WM_DELETE_WINDOW, 1);
  }

  if (view->transientParent) {
    XSetTransientForHint(display, impl->win, (Window)view->transientParent);
  }

  // Create input context
  impl->xic = XCreateIC(world->impl->xim,
                        XNInputStyle,
                        XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow,
                        impl->win,
                        XNFocusWindow,
                        impl->win,
                        NULL);

  puglDefineCursorShape(view, impl->cursorShape);

  puglDispatchSimpleEvent(view, PUGL_CREATE);

  return PUGL_SUCCESS;
}

PuglStatus
puglShow(PuglView* view)
{
  PuglStatus st = PUGL_SUCCESS;
  if (!view->impl->win) {
    if ((st = puglRealize(view))) {
      return st;
    }
  }

  Display* display = view->impl->display;
  XMapRaised(display, view->impl->win);

  if (!view->impl->displayed) {
    view->impl->displayed = true;
    if (view->impl->posRequested && view->transientParent) {
      // Position popups and transients to desired position.
      // Additional XMoveResizeWindow after XMapRaised was
      // at least necessary for KDE desktop.
      XMoveResizeWindow(display,
                        view->impl->win,
                        view->reqX,
                        view->reqY,
                        view->reqWidth,
                        view->reqHeight);
    } else if (view->transientParent && view->hints[PUGL_IS_POPUP]) {
      // center popup to parent, otherwise it will appear at top left screen
      // corner (may depend on window manager), usually you shoud have requested
      // a position for popups.
      XWindowAttributes attrs;
      if (XGetWindowAttributes(display, view->transientParent, &attrs)) {
        int    parentX;
        int    parentY;
        int    parentW = attrs.width;
        int    parentH = attrs.height;
        Window ignore;
        XTranslateCoordinates(display,
                              view->transientParent,
                              RootWindow(display, view->impl->screen),
                              0,
                              0,
                              &parentX,
                              &parentY,
                              &ignore);
        view->reqX = parentX + parentW / 2 - view->reqWidth / 2;
        view->reqY = parentY + parentH / 2 - view->reqHeight / 2;
        XMoveResizeWindow(display,
                          view->impl->win,
                          view->reqX,
                          view->reqY,
                          view->reqWidth,
                          view->reqHeight);
      }
    }
    view->impl->posRequested = false;
  }
  return PUGL_SUCCESS;
}

PuglStatus
puglHide(PuglView* view)
{
  XUnmapWindow(view->impl->display, view->impl->win);
  view->impl->displayed = false;

  return PUGL_SUCCESS;
}

void
puglFreeViewInternals(PuglView* view)
{
  if (view && view->impl) {
    if (view->impl->xic) {
      XDestroyIC(view->impl->xic);
    }
    if (view->backend) {
      view->backend->destroy(view);
    }
    if (view->impl->display) {
      XDestroyWindow(view->impl->display, view->impl->win);
    }
    if (view->impl->vi) {
      XFree(view->impl->vi);
    }
    free(view->impl);
  }
}

void
puglFreeWorldInternals(PuglWorld* world)
{
  if (world->impl->pseudoWin) {
    XDestroyWindow(world->impl->display, world->impl->pseudoWin);
  }
  if (world->impl->xim) {
    XCloseIM(world->impl->xim);
  }
  XCloseDisplay(world->impl->display);
  if (world->impl->awake_fds[0] >= 0) {
    close(world->impl->awake_fds[0]);
    close(world->impl->awake_fds[1]);
  }
  free(world->impl);
}

static PuglKey
keySymToSpecial(KeySym sym)
{
  switch (sym) {
  case XK_BackSpace:
    return PUGL_KEY_BACKSPACE;
  case XK_Tab:
    return PUGL_KEY_TAB;
  case XK_Return:
    return PUGL_KEY_RETURN;
  case XK_Escape:
    return PUGL_KEY_ESCAPE;
  case XK_space:
    return PUGL_KEY_SPACE;
  case XK_Delete:
    return PUGL_KEY_DELETE;
  case XK_F1:
    return PUGL_KEY_F1;
  case XK_F2:
    return PUGL_KEY_F2;
  case XK_F3:
    return PUGL_KEY_F3;
  case XK_F4:
    return PUGL_KEY_F4;
  case XK_F5:
    return PUGL_KEY_F5;
  case XK_F6:
    return PUGL_KEY_F6;
  case XK_F7:
    return PUGL_KEY_F7;
  case XK_F8:
    return PUGL_KEY_F8;
  case XK_F9:
    return PUGL_KEY_F9;
  case XK_F10:
    return PUGL_KEY_F10;
  case XK_F11:
    return PUGL_KEY_F11;
  case XK_F12:
    return PUGL_KEY_F12;
  case XK_Left:
    return PUGL_KEY_LEFT;
  case XK_Up:
    return PUGL_KEY_UP;
  case XK_Right:
    return PUGL_KEY_RIGHT;
  case XK_Down:
    return PUGL_KEY_DOWN;
  case XK_Page_Up:
    return PUGL_KEY_PAGE_UP;
  case XK_Page_Down:
    return PUGL_KEY_PAGE_DOWN;
  case XK_Home:
    return PUGL_KEY_HOME;
  case XK_End:
    return PUGL_KEY_END;
  case XK_Insert:
    return PUGL_KEY_INSERT;
  case XK_Shift_L:
    return PUGL_KEY_SHIFT_L;
  case XK_Shift_R:
    return PUGL_KEY_SHIFT_R;
  case XK_Control_L:
    return PUGL_KEY_CTRL_L;
  case XK_Control_R:
    return PUGL_KEY_CTRL_R;
  case XK_Alt_L:
    return PUGL_KEY_ALT_L;
  case XK_ISO_Level3_Shift:
  case XK_Alt_R:
    return PUGL_KEY_ALT_R;
  case XK_Super_L:
    return PUGL_KEY_SUPER_L;
  case XK_Super_R:
    return PUGL_KEY_SUPER_R;
  case XK_Menu:
    return PUGL_KEY_MENU;
  case XK_Caps_Lock:
    return PUGL_KEY_CAPS_LOCK;
  case XK_Scroll_Lock:
    return PUGL_KEY_SCROLL_LOCK;
  case XK_Num_Lock:
    return PUGL_KEY_NUM_LOCK;
  case XK_Print:
    return PUGL_KEY_PRINT_SCREEN;
  case XK_Pause:
    return PUGL_KEY_PAUSE;

  case XK_KP_Enter:
    return PUGL_KEY_KP_ENTER;
  case XK_KP_Home:
    return PUGL_KEY_KP_HOME;
  case XK_KP_Left:
    return PUGL_KEY_KP_LEFT;
  case XK_KP_Up:
    return PUGL_KEY_KP_UP;
  case XK_KP_Right:
    return PUGL_KEY_KP_RIGHT;
  case XK_KP_Down:
    return PUGL_KEY_KP_DOWN;
  case XK_KP_Page_Up:
    return PUGL_KEY_KP_PAGE_UP;
  case XK_KP_Page_Down:
    return PUGL_KEY_KP_PAGE_DOWN;
  case XK_KP_End:
    return PUGL_KEY_KP_END;
  case XK_KP_Begin:
    return PUGL_KEY_KP_BEGIN;
  case XK_KP_Insert:
    return PUGL_KEY_KP_INSERT;
  case XK_KP_Delete:
    return PUGL_KEY_KP_DELETE;
  case XK_KP_Multiply:
    return PUGL_KEY_KP_MULTIPLY;
  case XK_KP_Add:
    return PUGL_KEY_KP_ADD;
  case XK_KP_Subtract:
    return PUGL_KEY_KP_SUBTRACT;
  case XK_KP_Divide:
    return PUGL_KEY_KP_DIVIDE;

  case XK_KP_0:
    return PUGL_KEY_KP_0;
  case XK_KP_1:
    return PUGL_KEY_KP_1;
  case XK_KP_2:
    return PUGL_KEY_KP_2;
  case XK_KP_3:
    return PUGL_KEY_KP_3;
  case XK_KP_4:
    return PUGL_KEY_KP_4;
  case XK_KP_5:
    return PUGL_KEY_KP_5;
  case XK_KP_6:
    return PUGL_KEY_KP_6;
  case XK_KP_7:
    return PUGL_KEY_KP_7;
  case XK_KP_8:
    return PUGL_KEY_KP_8;
  case XK_KP_9:
    return PUGL_KEY_KP_9;
  case XK_KP_Separator:
    return PUGL_KEY_KP_SEPARATOR;
  default:
    break;
  }
  return (PuglKey)0;
}

static int
lookupString(XIC xic, XEvent* xevent, char* str, KeySym* sym)
{
  Status status = 0;

#ifdef X_HAVE_UTF8_STRING
  const int n = Xutf8LookupString(xic, &xevent->xkey, str, 8, sym, &status);
#else
  const int n = XmbLookupString(xic, &xevent->xkey, str, 8, sym, &status);
#endif

  return status == XBufferOverflow ? 0 : n;
}

static void
translateKey(PuglView* view, XEvent* xevent, PuglEvent* event)
{
  const unsigned state  = xevent->xkey.state;
  const bool     filter = XFilterEvent(xevent, None);

  event->key.keycode = xevent->xkey.keycode;

  if (!filter) {
    // Lookup unshifted key
    char   ustr[8] = {0};
    KeySym sym     = 0;
    XLookupString(&xevent->xkey, ustr, 8, &sym, NULL);
    PuglKey special = keySymToSpecial(sym);
    if (!special) {
      xevent->xkey.state = 0;
      XLookupString(&xevent->xkey, ustr, 8, &sym, NULL);
      special = keySymToSpecial(sym);
    }
    if (special) {
      event->key.key = special;
    } else {
      long u = keysym2ucs(sym);
      if (u >= 0) {
        event->key.key = u;
      } else {
        event->key.key = 0;
      }
    }
    // Lookup shifted key for possible text event
    xevent->xkey.state = state;

    char      sstr[8] = {0};
    const int sfound  = lookupString(view->impl->xic, xevent, sstr, &sym);
    if (sfound > 0) {
      memcpy(event->key.input.data, sstr, sfound);
      event->key.inputLength = sfound;
    }
  }
}

static uint32_t
translateModifiers(PuglWorldInternals* impl, const unsigned xstate)
{
  return (((xstate & impl->shiftKeyStates) ? PUGL_MOD_SHIFT : 0u) |
          ((xstate & impl->controlKeyStates) ? PUGL_MOD_CTRL : 0u) |
          ((xstate & impl->altKeyStates) ? PUGL_MOD_ALT : 0u) |
          ((xstate & impl->superKeyStates) ? PUGL_MOD_SUPER : 0u) |
          ((xstate & impl->altgrKeyStates) ? PUGL_MOD_ALTGR : 0u));
}

static PuglEvent
translateEvent(PuglView* view, XEvent xevent)
{
  const PuglX11Atoms* atoms = &view->world->impl->atoms;

  PuglEvent event = {{PUGL_NOTHING, 0}};
  event.any.flags = xevent.xany.send_event ? PUGL_IS_SEND_EVENT : 0;

  switch (xevent.type) {
  case ClientMessage:
    if (xevent.xclient.message_type == atoms->WM_PROTOCOLS) {
      const Atom protocol = (Atom)xevent.xclient.data.l[0];
      if (protocol == atoms->WM_DELETE_WINDOW) {
        event.type = PUGL_CLOSE;
      }
    } else if (xevent.xclient.message_type == atoms->PUGL_CLIENT_MSG) {
      event.type         = PUGL_CLIENT;
      event.client.data1 = (uintptr_t)xevent.xclient.data.l[0];
      event.client.data2 = (uintptr_t)xevent.xclient.data.l[1];
    }
    break;
  case VisibilityNotify:
    view->visible = xevent.xvisibility.state != VisibilityFullyObscured;
    break;
  case MapNotify:
    event.type = PUGL_MAP;
    break;
  case UnmapNotify:
    event.type    = PUGL_UNMAP;
    view->visible = false;
    break;
  case ConfigureNotify:
    if (xevent.xconfigure.send_event /* synthetic? */) {
      // The general rule is that coordinates in real ConfigureNotify
      // events are in the parent's space; in synthetic events, they
      // are in the root space. (X Window System Programmer's Guide, 3.4.1.5)
    } else if (!view->parent /* topLevel? */) {
      Display* display = view->impl->display;
      Window   ignore;
      XTranslateCoordinates(display,
                            view->impl->win,
                            RootWindow(display, view->impl->screen),
                            0,
                            0,
                            &xevent.xconfigure.x,
                            &xevent.xconfigure.y,
                            &ignore);
    }
    event.type             = PUGL_CONFIGURE;
    event.configure.x      = xevent.xconfigure.x;
    event.configure.y      = xevent.xconfigure.y;
    event.configure.width  = xevent.xconfigure.width;
    event.configure.height = xevent.xconfigure.height;
    break;
  case Expose:
    event.type          = PUGL_EXPOSE;
    event.expose.x      = xevent.xexpose.x;
    event.expose.y      = xevent.xexpose.y;
    event.expose.width  = xevent.xexpose.width;
    event.expose.height = xevent.xexpose.height;
    event.expose.count  = xevent.xexpose.count;
    break;
  case MotionNotify:
    event.type         = PUGL_MOTION;
    event.motion.time  = (double)xevent.xmotion.time / 1e3;
    event.motion.x     = xevent.xmotion.x;
    event.motion.y     = xevent.xmotion.y;
    event.motion.xRoot = xevent.xmotion.x_root;
    event.motion.yRoot = xevent.xmotion.y_root;
    event.motion.state =
      translateModifiers(view->world->impl, xevent.xmotion.state);
    if (xevent.xmotion.is_hint == NotifyHint) {
      event.motion.flags |= PUGL_IS_HINT;
    }
    break;
  case ButtonPress:
    if (xevent.xbutton.button >= 4 && xevent.xbutton.button <= 7) {
      event.type         = PUGL_SCROLL;
      event.scroll.time  = (double)xevent.xbutton.time / 1e3;
      event.scroll.x     = xevent.xbutton.x;
      event.scroll.y     = xevent.xbutton.y;
      event.scroll.xRoot = xevent.xbutton.x_root;
      event.scroll.yRoot = xevent.xbutton.y_root;
      event.scroll.state =
        translateModifiers(view->world->impl, xevent.xbutton.state);
      event.scroll.dx = 0.0;
      event.scroll.dy = 0.0;
      switch (xevent.xbutton.button) {
      case 4:
        event.scroll.dy        = 1.0;
        event.scroll.direction = PUGL_SCROLL_UP;
        break;
      case 5:
        event.scroll.dy        = -1.0;
        event.scroll.direction = PUGL_SCROLL_DOWN;
        break;
      case 6:
        event.scroll.dx        = 1.0;
        event.scroll.direction = PUGL_SCROLL_LEFT;
        break;
      case 7:
        event.scroll.dx        = -1.0;
        event.scroll.direction = PUGL_SCROLL_RIGHT;
        break;
      }
      // fallthru
    }
    // fallthru
  case ButtonRelease:
    if (xevent.xbutton.button < 4 || xevent.xbutton.button > 7) {
      event.button.type  = ((xevent.type == ButtonPress) ? PUGL_BUTTON_PRESS
                                                         : PUGL_BUTTON_RELEASE);
      event.button.time  = (double)xevent.xbutton.time / 1e3;
      event.button.x     = xevent.xbutton.x;
      event.button.y     = xevent.xbutton.y;
      event.button.xRoot = xevent.xbutton.x_root;
      event.button.yRoot = xevent.xbutton.y_root;
      event.button.state =
        translateModifiers(view->world->impl, xevent.xbutton.state);
      event.button.button = xevent.xbutton.button;
    }
    break;
  case KeyPress:
  case KeyRelease:
    event.type =
      ((xevent.type == KeyPress) ? PUGL_KEY_PRESS : PUGL_KEY_RELEASE);
    event.key.time  = (double)xevent.xkey.time / 1e3;
    event.key.x     = xevent.xkey.x;
    event.key.y     = xevent.xkey.y;
    event.key.xRoot = xevent.xkey.x_root;
    event.key.yRoot = xevent.xkey.y_root;
    event.key.state = translateModifiers(view->world->impl, xevent.xkey.state);
    translateKey(view, &xevent, &event);
    break;
  case EnterNotify:
  case LeaveNotify:
    event.type =
      ((xevent.type == EnterNotify) ? PUGL_POINTER_IN : PUGL_POINTER_OUT);
    event.crossing.time  = (double)xevent.xcrossing.time / 1e3;
    event.crossing.x     = xevent.xcrossing.x;
    event.crossing.y     = xevent.xcrossing.y;
    event.crossing.xRoot = xevent.xcrossing.x_root;
    event.crossing.yRoot = xevent.xcrossing.y_root;
    event.crossing.state =
      translateModifiers(view->world->impl, xevent.xcrossing.state);
    event.crossing.mode = PUGL_CROSSING_NORMAL;
    if (xevent.xcrossing.mode == NotifyGrab) {
      event.crossing.mode = PUGL_CROSSING_GRAB;
    } else if (xevent.xcrossing.mode == NotifyUngrab) {
      event.crossing.mode = PUGL_CROSSING_UNGRAB;
    }
    break;

  case FocusIn:
  case FocusOut:
    if (xevent.xfocus.detail != NotifyPointer) {
      event.type = (xevent.type == FocusIn) ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT;
      event.focus.mode = PUGL_CROSSING_NORMAL;
      if (xevent.xfocus.mode == NotifyGrab) {
        event.focus.mode = PUGL_CROSSING_GRAB;
      } else if (xevent.xfocus.mode == NotifyUngrab) {
        event.focus.mode = PUGL_CROSSING_UNGRAB;
      }
    }
    break;
  default:
    break;
  }

  return event;
}

PuglStatus
puglGrabFocus(PuglView* view)
{
  XSetInputFocus(
    view->impl->display, view->impl->win, RevertToNone, CurrentTime);
  return PUGL_SUCCESS;
}

bool
puglHasFocus(const PuglView* view)
{
  int    revertTo      = 0;
  Window focusedWindow = 0;
  XGetInputFocus(view->impl->display, &focusedWindow, &revertTo);
  return focusedWindow == view->impl->win;
}

PuglStatus
puglRequestAttention(PuglView* view)
{
  PuglInternals* const      impl  = view->impl;
  const PuglX11Atoms* const atoms = &view->world->impl->atoms;
  XEvent                    event = {0};

  event.type                 = ClientMessage;
  event.xclient.window       = impl->win;
  event.xclient.format       = 32;
  event.xclient.message_type = atoms->NET_WM_STATE;
  event.xclient.data.l[0]    = WM_STATE_ADD;
  event.xclient.data.l[1]    = (long)atoms->NET_WM_STATE_DEMANDS_ATTENTION;
  event.xclient.data.l[2]    = 0;
  event.xclient.data.l[3]    = 1;
  event.xclient.data.l[4]    = 0;

  const Window root = RootWindow(impl->display, impl->screen);
  XSendEvent(impl->display,
             root,
             False,
             SubstructureNotifyMask | SubstructureRedirectMask,
             &event);

  return PUGL_SUCCESS;
}

void
puglSetProcessFunc(PuglWorld*      world,
                   PuglProcessFunc processFunc,
                   void*           userData)
{
  world->processFunc     = processFunc;
  world->processUserData = userData;
}

void
puglSetNextProcessTime(PuglWorld* world, double seconds)
{
  if (seconds >= 0) {
    world->impl->nextProcessTime = puglGetTime(world) + seconds;
  } else {
    world->impl->nextProcessTime = -1;
  }
}

static XEvent
puglEventToX(PuglView* view, const PuglEvent* event)
{
  XEvent xev          = {0};
  xev.xany.send_event = True;

  switch (event->type) {
  case PUGL_EXPOSE: {
    const double x = floor(event->expose.x);
    const double y = floor(event->expose.y);
    const double w = ceil(event->expose.x + event->expose.width) - x;
    const double h = ceil(event->expose.y + event->expose.height) - y;

    xev.xexpose.type    = Expose;
    xev.xexpose.serial  = 0;
    xev.xexpose.display = view->impl->display;
    xev.xexpose.window  = view->impl->win;
    xev.xexpose.x       = (int)x;
    xev.xexpose.y       = (int)y;
    xev.xexpose.width   = (int)w;
    xev.xexpose.height  = (int)h;
    break;
  }

  case PUGL_CLIENT:
    xev.xclient.type         = ClientMessage;
    xev.xclient.serial       = 0;
    xev.xclient.send_event   = True;
    xev.xclient.display      = view->impl->display;
    xev.xclient.window       = view->impl->win;
    xev.xclient.message_type = view->world->impl->atoms.PUGL_CLIENT_MSG;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = (long)event->client.data1;
    xev.xclient.data.l[1]    = (long)event->client.data2;
    break;

  default:
    break;
  }

  return xev;
}

PuglStatus
puglSendEvent(PuglView* view, const PuglEvent* event)
{
  XEvent xev = puglEventToX(view, event);

  if (xev.type) {
    if (XSendEvent(view->impl->display, view->impl->win, False, 0, &xev)) {
      return PUGL_SUCCESS;
    }

    return PUGL_UNKNOWN_ERROR;
  }

  return PUGL_UNSUPPORTED_TYPE;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglWaitForEvent(PuglView* view)
{
  XEvent xevent;
  XPeekEvent(view->impl->display, &xevent);
  return PUGL_SUCCESS;
}
#endif

static void
mergeExposeEvents(PuglEventExpose* dst, const PuglRect* src)
{
  if (!dst->type) {
    dst->type   = PUGL_EXPOSE;
    dst->flags  = 0;
    dst->x      = src->x;
    dst->y      = src->y;
    dst->width  = src->width;
    dst->height = src->height;
    dst->count  = 0;
  } else {
    const double max_x = MAX(dst->x + dst->width, src->x + src->width);
    const double max_y = MAX(dst->y + dst->height, src->y + src->height);

    dst->x      = MIN(dst->x, src->x);
    dst->y      = MIN(dst->y, src->y);
    dst->width  = max_x - dst->x;
    dst->height = max_y - dst->y;
  }
}

static void
addPendingExpose(PuglView* view, const PuglEventExpose* expose)
{
  PuglRect rect = {expose->x, expose->y, expose->width, expose->height};
  integerRect(&rect);
  bool added = addRect(&view->rects, &rect);
  mergeExposeEvents(&view->impl->pendingExpose.expose, &rect);
  if (!added) {
    view->rects.rectsList[0].x      = view->impl->pendingExpose.expose.x;
    view->rects.rectsList[0].y      = view->impl->pendingExpose.expose.y;
    view->rects.rectsList[0].width  = view->impl->pendingExpose.expose.width;
    view->rects.rectsList[0].height = view->impl->pendingExpose.expose.height;
    view->rects.rectsCount          = 1;
  }
}

void
puglAwake(PuglWorld* world)
{
  if (world->impl->awake_fds[0] >= 0) {
    char c = 0;
    int  PUGL_UNUSED(ignore)
    = write(world->impl->awake_fds[1], &c, 1);
  }
}

/// Flush pending configure and expose events for all views
static void
flushExposures(PuglWorld* world)
{
  for (size_t i = 0; i < world->numViews; ++i) {
    PuglView* const view = world->views[i];

    if (view->visible) {
      puglDispatchSimpleEvent(view, PUGL_UPDATE);
    }

    PuglEvent configure = view->impl->pendingConfigure;
    PuglEvent expose    = view->impl->pendingExpose;

    view->impl->pendingConfigure.type = PUGL_NOTHING;
    view->impl->pendingExpose.type    = PUGL_NOTHING;

    if (configure.type && !expose.type) {
      view->backend->enter(view, NULL, NULL);
      puglDispatchEventInContext(view, &configure);
      view->backend->leave(view, NULL, NULL);
    } else if (expose.type) {
      bool useRects2 =
        view->rects.rectsCount > 0 &&
        (view->rects2.rectsList || puglRectsInit(&view->rects2, 4));
      if (useRects2) {
        PuglRects swap = view->rects2;
        view->rects2   = view->rects;
        view->rects    = swap;
      }
      view->rects.rectsCount = 0;

      view->backend->enter(
        view, &expose.expose, useRects2 ? &view->rects2 : NULL);
      if (configure.type) {
        puglDispatchEventInContext(view, &configure);
      }
      if (view->hints[PUGL_DONT_MERGE_RECTS] && useRects2) {
        int n = view->rects2.rectsCount;
        for (int j = 0; j < n; ++j) {
          PuglRect*       r = view->rects2.rectsList + j;
          PuglEventExpose e = {
            PUGL_EXPOSE, 0, r->x, r->y, r->width, r->height, n - 1 - j};
          puglDispatchEventInContext(view, (PuglEvent*)&e);
        }
      } else {
        puglDispatchEventInContext(view, &expose);
      }
      view->backend->leave(
        view, &expose.expose, useRects2 ? &view->rects2 : NULL);
      if (useRects2) {
        view->rects2.rectsCount = 0;
      }
    }
  }
}

static PuglStatus
puglDispatchX11Events(PuglWorld* world)
{
  bool wasDispatchingEvents      = world->impl->dispatchingEvents;
  world->impl->dispatchingEvents = true;

  PuglWorldInternals* impl    = world->impl;
  Display*            display = impl->display;

  if (impl->syncState == 2) {
    if (XLastKnownRequestProcessed(display) < impl->syncSerial) {
      XSync(
        display,
        False); // wait, we are too fast: previous serial0 still not processed
    }
    impl->syncState = 0;
  }

  if (impl->needsProcessing || (impl->nextProcessTime >= 0 &&
                                impl->nextProcessTime <= puglGetTime(world))) {
    impl->needsProcessing = false;
    impl->nextProcessTime = -1;
    if (world->processFunc)
      world->processFunc(world, world->processUserData);
  }
  const PuglX11Atoms* const atoms = &world->impl->atoms;

  unsigned long serial0 = NextRequest(display);

  // Process all queued events
  while (XEventsQueued(display, QueuedAfterReading) > 0) {
    XEvent xevent;
    XNextEvent(display, &xevent);

    if (xevent.xany.window == impl->pseudoWin) {
      if (xevent.type == SelectionClear) {
        puglSetBlob(&world->clipboard, NULL, 0);
      } else if (xevent.type == SelectionRequest) {
        handleSelectionRequestForOwner(impl->display,
                                       atoms,
                                       &xevent.xselectionrequest,
                                       &world->clipboard,
                                       &impl->incrTarget);
      }
      continue;
    } else if (xevent.type == PropertyNotify &&
               xevent.xany.window == impl->incrTarget.win &&
               xevent.xproperty.state == PropertyDelete) {
      handlePropertyNotifyForOwner(
        impl->display, &world->clipboard, &impl->incrTarget);
      continue;
    }
    PuglView* view = puglFindView(world, xevent.xany.window);
    if (!view) {
      continue;
    }

    // Handle special events
    PuglInternals* const impl = view->impl;
    if (xevent.type == KeyRelease && view->hints[PUGL_IGNORE_KEY_REPEAT]) {
      XEvent next;
      if (XCheckTypedWindowEvent(display, impl->win, KeyPress, &next) &&
          next.type == KeyPress && next.xkey.time == xevent.xkey.time &&
          next.xkey.keycode == xevent.xkey.keycode) {
        continue;
      }
    } else if (xevent.type == FocusIn) {
      XSetICFocus(impl->xic);
    } else if (xevent.type == FocusOut) {
      XUnsetICFocus(impl->xic);
    } else if (xevent.type == SelectionClear) {
      puglSetBlob(&view->clipboard, NULL, 0);
      continue;
    } else if (xevent.type == SelectionNotify && impl->clipboardRequested) {
      handleSelectionNotifyForRequestor(view, &xevent.xselection);
      continue;
    } else if (xevent.type == PropertyNotify &&
               xevent.xproperty.atom == XA_PRIMARY &&
               xevent.xproperty.state == PropertyNewValue &&
               impl->incrClipboardRequest) {
      handlePropertyNotifyForRequestor(view);
      continue;
    }

    // Translate X11 event to Pugl event
    PuglEvent event = translateEvent(view, xevent);

    if (event.type == PUGL_EXPOSE) {
      addPendingExpose(view, &event.expose);
    } else if (event.type == PUGL_CONFIGURE) {
      // Expand configure event to be dispatched after loop
      view->impl->pendingConfigure = event;
      view->frame.x                = event.configure.x;
      view->frame.y                = event.configure.y;
      view->frame.width            = event.configure.width;
      view->frame.height           = event.configure.height;
    } else if (event.type == PUGL_MAP && view->parent) {
      XWindowAttributes attrs;
      XGetWindowAttributes(view->impl->display, view->impl->win, &attrs);

      PuglEventConfigure configure = {PUGL_CONFIGURE,
                                      0,
                                      (double)attrs.x,
                                      (double)attrs.y,
                                      (double)attrs.width,
                                      (double)attrs.height};

      puglDispatchEvent(view, (PuglEvent*)&configure);
      puglDispatchEvent(view, &event);
    } else {
      // Dispatch event to application immediately
      puglDispatchEvent(view, &event);
    }
  }
  if (!wasDispatchingEvents) {
    flushExposures(world);
    impl->dispatchingEvents = false;
  }

  XFlush(display);

  if (impl->syncState == 0) {
    if (NextRequest(display) != serial0) {
      impl->syncState  = 1; // start waiting for serial0
      impl->syncSerial = serial0;
    }
  } else if (impl->syncState == 1) {
    if (NextRequest(display) != serial0) {
      impl->syncState = 2; // next invocation will XSync for previous serial0
    }
  }
  return PUGL_SUCCESS;
}

#ifndef PUGL_DISABLE_DEPRECATED
PuglStatus
puglProcessEvents(PuglView* view)
{
  return puglUpdate(view->world, 0.0);
}
#endif

PuglStatus
puglUpdate(PuglWorld* world, double timeout)
{
  const double startTime = puglGetTime(world);
  PuglStatus   st        = PUGL_SUCCESS;

  XFlush(world->impl->display);

  if (timeout < 0.0) {
    st = puglPollX11Socket(world, timeout);
    st = st ? st : puglDispatchX11Events(world);
  } else if (timeout <= 0.001) {
    st = puglDispatchX11Events(world);
  } else {
    const double endTime = startTime + timeout - 0.001;
    for (double t = startTime; t < endTime; t = puglGetTime(world)) {
      if ((st = puglPollX11Socket(world, endTime - t)) ||
          (st = puglDispatchX11Events(world))) {
        break;
      }
    }
  }

  return st;
}

double
puglGetTime(const PuglWorld* world)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0) -
         world->startTime;
}

PuglStatus
puglPostRedisplay(PuglView* view)
{
  const PuglRect rect = {0, 0, view->frame.width, view->frame.height};

  return puglPostRedisplayRect(view, rect);
}

PuglStatus
puglPostRedisplayRect(PuglView* view, PuglRect rect)
{
  const PuglEventExpose event = {
    PUGL_EXPOSE, 0, rect.x, rect.y, rect.width, rect.height, 0};

  if (view->world->impl->dispatchingEvents) {
    // Currently dispatching events, add/expand expose for the loop end
    addPendingExpose(view, &event);
  } else if (view->visible) {
    // Not dispatching events, send an X expose so we wake up next time
    return puglSendEvent(view, (const PuglEvent*)&event);
  }

  return PUGL_SUCCESS;
}

PuglNativeView
puglGetNativeWindow(PuglView* view)
{
  return (PuglNativeView)view->impl->win;
}

PuglStatus
puglSetWindowTitle(PuglView* view, const char* title)
{
  Display*                  display = view->world->impl->display;
  const PuglX11Atoms* const atoms   = &view->world->impl->atoms;

  puglSetString(&view->title, title);

  if (view->impl->win) {
    XStoreName(display, view->impl->win, title);
    XChangeProperty(display,
                    view->impl->win,
                    atoms->NET_WM_NAME,
                    atoms->UTF8_STRING,
                    8,
                    PropModeReplace,
                    (const uint8_t*)title,
                    (int)strlen(title));
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

  if (view->impl->win) {
    updateSizeHints(view);
    if (!XMoveResizeWindow(view->world->impl->display,
                           view->impl->win,
                           (int)frame.x,
                           (int)frame.y,
                           (unsigned)frame.width,
                           (unsigned)frame.height)) {
      return PUGL_UNKNOWN_ERROR;
    }
  }

  view->frame = frame;
  return PUGL_SUCCESS;
}

PuglStatus
puglSetSize(PuglView* view, int width, int height)
{
  view->reqWidth  = width;
  view->reqHeight = height;

  if (view->impl->win) {
    updateSizeHints(view);
    XResizeWindow(view->world->impl->display, view->impl->win, width, height);
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetMinSize(PuglView* const view, const int width, const int height)
{
  view->minWidth  = width;
  view->minHeight = height;

  return updateSizeHints(view);
}

PuglStatus
puglSetMaxSize(PuglView* const view, const int width, const int height)
{
  view->maxWidth  = width;
  view->maxHeight = height;
  return updateSizeHints(view);
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

  return updateSizeHints(view);
}

PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeView parent)
{
  Display* display = view->world->impl->display;

  view->transientParent = parent;

  if (view->impl->win) {
    XSetTransientForHint(
      display, view->impl->win, (Window)view->transientParent);
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglRequestClipboard(PuglView* const view)
{
  PuglInternals* const      impl  = view->impl;
  const PuglX11Atoms* const atoms = &view->world->impl->atoms;

  // Clear internal selection
  puglSetBlob(&view->clipboard, NULL, 0);

  impl->clipboardRequested = 1;
  XConvertSelection(impl->display,
                    atoms->CLIPBOARD,
                    atoms->UTF8_STRING,
                    XA_PRIMARY,
                    impl->win,
                    CurrentTime);
  return PUGL_SUCCESS;
}

PuglStatus
puglSetClipboard(PuglWorld* const  world,
                 const char* const type,
                 const void* const data,
                 const size_t      len)
{
  PuglWorldInternals* const impl  = world->impl;
  const PuglX11Atoms* const atoms = &impl->atoms;

  if (!impl->pseudoWin) {
    initWorldPseudoWin(world);
  }
  if (impl->incrTarget.win) {
    XSelectInput(impl->display, impl->incrTarget.win, 0);
    impl->incrTarget.win = 0;
    impl->incrTarget.pos = 0;
  }
  PuglStatus st = puglSetInternalClipboard(world, type, data, len);
  if (st) {
    return st;
  }

  XSetSelectionOwner(
    impl->display, atoms->CLIPBOARD, impl->pseudoWin, CurrentTime);
  return PUGL_SUCCESS;
}

bool
puglHasClipboard(PuglWorld* world)
{
  return world->clipboard.data != NULL;
}

static const unsigned cursor_nums[] = {
  PUGL_XC_DEFAULT_ARROW, // ARROW
  XC_xterm,              // CARET
  XC_crosshair,          // CROSSHAIR
  XC_hand2,              // HAND
  XC_pirate,             // NO
  XC_sb_h_double_arrow,  // LEFT_RIGHT
  XC_sb_v_double_arrow,  // UP_DOWN
  XC_X_cursor,           // HIDDEN
};

PuglStatus
puglSetCursor(PuglView* view, PuglCursor cursor)
{
  PuglInternals* const impl  = view->impl;
  const unsigned       index = (unsigned)cursor;
  const unsigned       count = sizeof(cursor_nums) / sizeof(cursor_nums[0]);
  if (index >= count) {
    return PUGL_BAD_PARAMETER;
  }

  const unsigned shape = cursor_nums[index];
  if (!impl->win || impl->cursorShape == shape) {
    return PUGL_SUCCESS;
  }

  impl->cursorShape = cursor_nums[index];

  return puglDefineCursorShape(view, impl->cursorShape);
}

double
puglGetScreenScale(PuglView* view)
{
  return puglGetDefaultScreenScale(view->world);
}

double
puglGetDefaultScreenScale(PuglWorld* world)
{
  double rslt = 1.0;

  PuglWorldInternals* impl           = world->impl;
  char*               resourceString = XResourceManagerString(impl->display);
  if (resourceString) {
    XrmInitialize();
    XrmDatabase db = XrmGetStringDatabase(resourceString);
    if (db) {
      XrmValue value;
      char*    type = NULL;
      if (XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True) {
        if (value.addr) {
          double dpi = atof(value.addr);
          if (dpi > 0) {
            rslt = ((int)((dpi / 96) * 100 + 0.5)) / (double)100;
          }
        }
      }
      XrmDestroyDatabase(db);
    }
  }
  return rslt;
}
