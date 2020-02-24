/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>
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

/**
   @file x11.c X11 implementation.
*/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "pugl/detail/implementation.h"
#include "pugl/detail/types.h"
#include "pugl/detail/x11.h"
#include "pugl/pugl.h"
#include "pugl/pugl_stub.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <sys/select.h>
#include <sys/time.h>

#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define KEYSYM2UCS_INCLUDED
#include "x11_keysym2ucs.c"

#ifndef MIN
#    define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#    define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#include "pugl/detail/x11_clip.h"

enum WmClientStateMessageAction {
	WM_STATE_REMOVE,
	WM_STATE_ADD,
	WM_STATE_TOGGLE
};

static const long eventMask =
	(ExposureMask | StructureNotifyMask |
	 VisibilityChangeMask | FocusChangeMask |
	 EnterWindowMask | LeaveWindowMask | PointerMotionMask |
	 ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
	 PropertyChangeMask);

PuglStatus
puglInitApplication(const PuglApplicationFlags flags)
{
	if (flags & PUGL_APPLICATION_THREADS) {
		XInitThreads();
	}

	return PUGL_SUCCESS;
}

PuglWorldInternals*
puglInitWorldInternals(void)
{
	Display* display = XOpenDisplay(NULL);
	if (!display) {
		return NULL;
	}
	// XSynchronize(display, True); // for debugging

	PuglWorldInternals* impl = (PuglWorldInternals*)calloc(
		1, sizeof(PuglWorldInternals));

	impl->display = display;

	// Intern the various atoms we will need
	impl->atoms.CLIPBOARD        = XInternAtom(display, "CLIPBOARD", 0);
	impl->atoms.TARGETS          = XInternAtom(display, "TARGETS", 0);
	impl->atoms.INCR             = XInternAtom(display, "INCR", 0);
	impl->atoms.UTF8_STRING      = XInternAtom(display, "UTF8_STRING", 0);
	impl->atoms.WM_PROTOCOLS     = XInternAtom(display, "WM_PROTOCOLS", 0);
	impl->atoms.WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", 0);
	impl->atoms.NET_WM_NAME      = XInternAtom(display, "_NET_WM_NAME", 0);
	impl->atoms.NET_WM_STATE     = XInternAtom(display, "_NET_WM_STATE", 0);
	impl->atoms.NET_WM_STATE_DEMANDS_ATTENTION =
		XInternAtom(display, "_NET_WM_STATE_DEMANDS_ATTENTION", 0);

	// Open input method
	XSetLocaleModifiers("");
	if (!(impl->xim = XOpenIM(display, NULL, NULL, NULL))) {
		XSetLocaleModifiers("@im=");
		if (!(impl->xim = XOpenIM(display, NULL, NULL, NULL))) {
			fprintf(stderr, "warning: XOpenIM failed\n");
		}
	}

	XFlush(display);

	if (pipe(impl->awake_fds) == 0) {
		fcntl(impl->awake_fds[1], F_SETFL,
			fcntl(impl->awake_fds[1], F_GETFL) | O_NONBLOCK);
	} else {
		impl->awake_fds[0] = -1;
	}

	impl->nextProcessTime = -1;
	return impl;
}

static void
initWorldPseudoWin(PuglWorld* world)
{
	PuglWorldInternals* impl = world->impl;
	if (!impl->pseudoWin) {
	    impl->pseudoWin = XCreateSimpleWindow(impl->display, 
	                                          RootWindow(impl->display, DefaultScreen(impl->display)),
	                                          0, 0, 1, 1, 0, 0, 0);
	    XSelectInput(impl->display, impl->pseudoWin, PropertyChangeMask);
	}
}

void*
puglGetNativeWorld(PuglWorld* world)
{
	return world->impl->display;
}

PuglStatus
puglSetClassName(PuglWorld* const world, const char* const name)
{
	puglSetString(&world->className, name);
	return PUGL_SUCCESS;
}

PuglInternals*
puglInitViewInternals(void)
{
	return (PuglInternals*)calloc(1, sizeof(PuglInternals));
}

PuglStatus
puglPollEvents(PuglWorld* world, const double timeout0)
{
	if (XPending(world->impl->display) > 0) {
		return PUGL_SUCCESS;
	}
	PuglWorldInternals* impl = world->impl;

	const int fd   = ConnectionNumber(impl->display);
	const int afd  = impl->awake_fds[0];
	int nfds = (fd > afd ? fd : afd) + 1;
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
		const long     msec = (long)((timeout - (double)sec) * 1e6);
		struct timeval tv   = {sec, msec};
		ret = select(nfds, &fds, NULL, NULL, &tv);
	}
	bool hasEvents = FD_ISSET(fd, &fds);
	if (ret > 0 && afd >= 0 && FD_ISSET(afd, &fds)) {
	    hasEvents = true;
	    impl->needsProcessing = true;
	    char buf[32];
	    int ignore = read(afd, buf, sizeof(buf));
	}
	if (impl->nextProcessTime >= 0 && impl->nextProcessTime <= puglGetTime(world)) {
	    hasEvents = true;
	    impl->needsProcessing = true;
	}
	return ret < 0 ? PUGL_UNKNOWN_ERROR
	               : hasEvents ? PUGL_SUCCESS : PUGL_FAILURE;
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

static XSizeHints*
getSizeHints(const PuglView* view)
{
	XSizeHints* sizeHints = XAllocSizeHints();
	

	if (!view->hints[PUGL_RESIZABLE]) {
		sizeHints->flags      = PMinSize|PMaxSize;
		sizeHints->min_width  = (int)view->reqWidth;
		sizeHints->min_height = (int)view->reqHeight;
		sizeHints->max_width  = (int)view->reqWidth;
		sizeHints->max_height = (int)view->reqHeight;
	} else {
		if (view->minWidth || view->minHeight) {
			sizeHints->flags      = PMinSize;
			sizeHints->min_width  = view->minWidth;
			sizeHints->min_height = view->minHeight;
		}
		if (view->maxWidth || view->maxHeight) {
			sizeHints->flags      |= PMaxSize;
			sizeHints->max_width  = view->maxWidth;
			sizeHints->max_height = view->maxHeight;
			if (sizeHints->max_width  < 0) { sizeHints->max_width  = INT_MAX; }
			if (sizeHints->max_height < 0) { sizeHints->max_height = INT_MAX; }
		}
		if (view->minAspectX) {
			sizeHints->flags        |= PAspect;
			sizeHints->min_aspect.x  = view->minAspectX;
			sizeHints->min_aspect.y  = view->minAspectY;
			sizeHints->max_aspect.x  = view->maxAspectX;
			sizeHints->max_aspect.y  = view->maxAspectY;
		}
	}

	return sizeHints;
}

PuglStatus
puglCreateWindow(PuglView* view, const char* title)
{
	PuglInternals* const impl    = view->impl;
	PuglWorld* const     world   = view->world;
	PuglX11Atoms* const  atoms   = &view->world->impl->atoms;
	Display* const       display = world->impl->display;

	impl->display = display;
	impl->screen  = DefaultScreen(display);

	if (!view->backend || !view->backend->configure) {
		return PUGL_BAD_BACKEND;
	}

	PuglStatus st = view->backend->configure(view);
	if (st || !impl->vi) {
		view->backend->destroy(view);
		return st ? st : PUGL_BACKEND_FAILED;
	}

	Window xParent = view->parent ? (Window)view->parent
	                              : RootWindow(display, impl->screen);

	Colormap cmap = XCreateColormap(
		display, xParent, impl->vi->visual, AllocNone);

	XSetWindowAttributes attr = {0};
	attr.colormap   = cmap;
	attr.event_mask = eventMask;
	attr.override_redirect = view->hints[PUGL_IS_POPUP];

	const Window win = impl->win = XCreateWindow(
		display, xParent,
		view->reqX, view->reqY, 
		view->reqWidth, view->reqHeight,
		0, impl->vi->depth, InputOutput,
		impl->vi->visual, CWColormap | CWEventMask,// | CWOverrideRedirect,
		&attr);

        bool isTransient = !view->parent && view->transientParent;
	bool isPopup = !view->parent && view->hints[PUGL_IS_POPUP];

	Atom net_wm_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
	Atom net_wm_type_kind = isPopup
	                         ? XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False)
	                         : XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	XChangeProperty(display, win, net_wm_type,
	                XA_ATOM, 32, PropModeReplace,
	                (unsigned char*)&net_wm_type_kind, 1);

        {
            XWMHints* xhints = XAllocWMHints();
            xhints->flags = InputHint;
            xhints->input = (isPopup && isTransient) ? False : True;
            XSetWMHints(display, win, xhints);
            XFree(xhints);
        }


	// motif hints: flags, function, decorations, input_mode, status
        long motifHints[5] = {0, 0, 0, 0, 0};
        if (isPopup) {
            motifHints[0] |= 2; // MWM_HINTS_DECORATIONS
            motifHints[2] = 0; // no decorations
        }
        XChangeProperty(display, win,
                        XInternAtom(display, "_MOTIF_WM_HINTS", False),
                        XInternAtom(display, "_MOTIF_WM_HINTS", False),
                        32, 0, (unsigned char *)motifHints, 5);

	if (isTransient) {
		Atom wm_state = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
		XChangeProperty(display, win, atoms->NET_WM_STATE,
		                XA_ATOM, 32, PropModeAppend,
		                (unsigned char*)&wm_state, 1);
	}
	                
	if ((st = view->backend->create(view))) {
		return st;
	}

	XSizeHints* sizeHints = getSizeHints(view);
	XSetNormalHints(display, win, sizeHints);
	XFree(sizeHints);

	XClassHint classHint = { world->className, world->className };
	XSetClassHint(display, win, &classHint);

	if (title) {
		puglSetWindowTitle(view, title);
	}

	if (!view->parent) {
		XSetWMProtocols(display, win, &atoms->WM_DELETE_WINDOW, 1);
	}

	if (view->transientParent) {
		XSetTransientForHint(display, win, (Window)(view->transientParent));
	}

	// Create input context
	const XIMStyle im_style = XIMPreeditNothing | XIMStatusNothing;
	if (!(impl->xic = XCreateIC(world->impl->xim,
	                            XNInputStyle,   im_style,
	                            XNClientWindow, win,
	                            XNFocusWindow,  win,
	                            NULL))) {
		fprintf(stderr, "warning: XCreateIC failed\n");
	}
	
	return PUGL_SUCCESS;
}

PuglStatus
puglShowWindow(PuglView* view)
{
	Display* display = view->impl->display;
	XMapRaised(display, view->impl->win);
        
	if (!view->impl->displayed) {
	    view->impl->displayed = true;
	    if (view->impl->posRequested && view->transientParent) {
	        // Position popups and transients to desired position.
	        // Additional XMoveResizeWindow after XMapRaised was
	        // at least necessary for KDE desktop.
                XMoveResizeWindow(display, view->impl->win,
                                  view->reqX,     view->reqY, 
                                  view->reqWidth, view->reqHeight);
            }
            else if (view->transientParent && view->hints[PUGL_IS_POPUP]) {
		// center popup to parent, otherwise it will appear at top left screen corner
		// (may depend on window manager), usually you shoud have requested 
		// a position for popups.
		XWindowAttributes attrs;
                if (XGetWindowAttributes(display, view->transientParent, &attrs)) {
                    int parentX;
                    int parentY;
                    int parentW = attrs.width;
                    int parentH = attrs.height;
                    Window ignore;
                    XTranslateCoordinates(display, view->transientParent,
                                                   RootWindow(display, view->impl->screen),
                                                   0, 0,
                                                   &parentX, &parentY, &ignore);
                    view->reqX = parentX + parentW/2 - view->reqWidth/2;
                    view->reqY = parentY + parentH/2 - view->reqHeight/2;
                    XMoveResizeWindow(display, view->impl->win,
                                      view->reqX,     view->reqY, 
                                      view->reqWidth, view->reqHeight);
                }
            }
	    view->impl->posRequested = false;
	}
	return PUGL_SUCCESS;
}

PuglStatus
puglHideWindow(PuglView* view)
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
	case XK_BackSpace:   return PUGL_KEY_BACKSPACE;
	case XK_Tab:         return PUGL_KEY_TAB;
	case XK_Return:      return PUGL_KEY_RETURN;
	case XK_Escape:      return PUGL_KEY_ESCAPE;
	case XK_space:       return PUGL_KEY_SPACE;
	case XK_Delete:      return PUGL_KEY_DELETE;
	case XK_F1:          return PUGL_KEY_F1;
	case XK_F2:          return PUGL_KEY_F2;
	case XK_F3:          return PUGL_KEY_F3;
	case XK_F4:          return PUGL_KEY_F4;
	case XK_F5:          return PUGL_KEY_F5;
	case XK_F6:          return PUGL_KEY_F6;
	case XK_F7:          return PUGL_KEY_F7;
	case XK_F8:          return PUGL_KEY_F8;
	case XK_F9:          return PUGL_KEY_F9;
	case XK_F10:         return PUGL_KEY_F10;
	case XK_F11:         return PUGL_KEY_F11;
	case XK_F12:         return PUGL_KEY_F12;
	case XK_Left:        return PUGL_KEY_LEFT;
	case XK_Up:          return PUGL_KEY_UP;
	case XK_Right:       return PUGL_KEY_RIGHT;
	case XK_Down:        return PUGL_KEY_DOWN;
	case XK_Page_Up:     return PUGL_KEY_PAGE_UP;
	case XK_Page_Down:   return PUGL_KEY_PAGE_DOWN;
	case XK_Home:        return PUGL_KEY_HOME;
	case XK_End:         return PUGL_KEY_END;
	case XK_Insert:      return PUGL_KEY_INSERT;
	case XK_Shift_L:     return PUGL_KEY_SHIFT_L;
	case XK_Shift_R:     return PUGL_KEY_SHIFT_R;
	case XK_Control_L:   return PUGL_KEY_CTRL_L;
	case XK_Control_R:   return PUGL_KEY_CTRL_R;
	case XK_Alt_L:       return PUGL_KEY_ALT_L;
	case XK_ISO_Level3_Shift:
	case XK_Alt_R:       return PUGL_KEY_ALT_R;
	case XK_Super_L:     return PUGL_KEY_SUPER_L;
	case XK_Super_R:     return PUGL_KEY_SUPER_R;
	case XK_Menu:        return PUGL_KEY_MENU;
	case XK_Caps_Lock:   return PUGL_KEY_CAPS_LOCK;
	case XK_Scroll_Lock: return PUGL_KEY_SCROLL_LOCK;
	case XK_Num_Lock:    return PUGL_KEY_NUM_LOCK;
	case XK_Print:       return PUGL_KEY_PRINT_SCREEN;
	case XK_Pause:       return PUGL_KEY_PAUSE;

	case XK_KP_Enter:    return PUGL_KEY_KP_ENTER;
	case XK_KP_Home:     return PUGL_KEY_KP_HOME;
	case XK_KP_Left:     return PUGL_KEY_KP_LEFT;
	case XK_KP_Up:       return PUGL_KEY_KP_UP;
	case XK_KP_Right:    return PUGL_KEY_KP_RIGHT;
	case XK_KP_Down:     return PUGL_KEY_KP_DOWN;
	case XK_KP_Page_Up:  return PUGL_KEY_KP_PAGE_UP;
	case XK_KP_Page_Down:return PUGL_KEY_KP_PAGE_DOWN;
	case XK_KP_End:      return PUGL_KEY_KP_END;
	case XK_KP_Begin:    return PUGL_KEY_KP_BEGIN;
	case XK_KP_Insert:   return PUGL_KEY_KP_INSERT;
	case XK_KP_Delete:   return PUGL_KEY_KP_DELETE;
	case XK_KP_Multiply: return PUGL_KEY_KP_MULTIPLY;
	case XK_KP_Add:      return PUGL_KEY_KP_ADD;
	case XK_KP_Subtract: return PUGL_KEY_KP_SUBTRACT;
	case XK_KP_Divide:   return PUGL_KEY_KP_DIVIDE;

	default: break;
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
	        xevent->xkey.state = 0;
                char          ustr[8] = {0};
                KeySym        sym     = 0;
                XLookupString(&xevent->xkey, ustr, 8, &sym, NULL);
                const PuglKey special = keySymToSpecial(sym);
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
translateModifiers(const unsigned xstate)
{
	return (((xstate & ShiftMask)   ? PUGL_MOD_SHIFT  : 0) |
	        ((xstate & ControlMask) ? PUGL_MOD_CTRL   : 0) |
	        ((xstate & Mod1Mask)    ? PUGL_MOD_ALT    : 0) |
	        ((xstate & Mod4Mask)    ? PUGL_MOD_SUPER  : 0));
}

static PuglEvent
translateEvent(PuglView* view, XEvent xevent)
{
	const PuglX11Atoms* atoms = &view->world->impl->atoms;

	PuglEvent event = {0};
	event.any.flags = xevent.xany.send_event ? PUGL_IS_SEND_EVENT : 0;

	switch (xevent.type) {
	case ClientMessage:
		if (xevent.xclient.message_type == atoms->WM_PROTOCOLS) {
			const Atom protocol = (Atom)xevent.xclient.data.l[0];
			if (protocol == atoms->WM_DELETE_WINDOW) {
				event.type = PUGL_CLOSE;
			}
		}
		break;
	case VisibilityNotify:
		view->visible = xevent.xvisibility.state != VisibilityFullyObscured;
		break;
	case MapNotify: {
		break;
		/*
		XWindowAttributes attrs = {0};
		XGetWindowAttributes(view->impl->display, view->impl->win, &attrs);
		event.type             = PUGL_CONFIGURE;
		event.configure.x      = attrs.x;
		event.configure.y      = attrs.y;
		event.configure.width  = attrs.width;
		event.configure.height = attrs.height;
		printf("Map %d %d\n", attrs.x, attrs.y);
		break;*/
	}
	case UnmapNotify:
		view->visible = false;
		break;
	case ConfigureNotify:
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
		event.type           = PUGL_MOTION_NOTIFY;
		event.motion.time    = xevent.xmotion.time / 1e3;
		event.motion.x       = xevent.xmotion.x;
		event.motion.y       = xevent.xmotion.y;
		event.motion.xRoot   = xevent.xmotion.x_root;
		event.motion.yRoot   = xevent.xmotion.y_root;
		event.motion.state   = translateModifiers(xevent.xmotion.state);
		event.motion.isHint  = (xevent.xmotion.is_hint == NotifyHint);
		break;
	case ButtonPress:
		if (xevent.xbutton.button >= 4 && xevent.xbutton.button <= 7) {
			event.type           = PUGL_SCROLL;
			event.scroll.time    = xevent.xbutton.time / 1e3;
			event.scroll.x       = xevent.xbutton.x;
			event.scroll.y       = xevent.xbutton.y;
			event.scroll.xRoot   = xevent.xbutton.x_root;
			event.scroll.yRoot   = xevent.xbutton.y_root;
			event.scroll.state   = translateModifiers(xevent.xbutton.state);
			event.scroll.dx      = 0.0;
			event.scroll.dy      = 0.0;
			switch (xevent.xbutton.button) {
			case 4: event.scroll.dy =  1.0; break;
			case 5: event.scroll.dy = -1.0; break;
			case 6: event.scroll.dx = -1.0; break;
			case 7: event.scroll.dx =  1.0; break;
			}
			// fallthru
		}
		// fallthru
	case ButtonRelease:
		if (xevent.xbutton.button < 4 || xevent.xbutton.button > 7) {
			event.button.type   = ((xevent.type == ButtonPress)
			                       ? PUGL_BUTTON_PRESS
			                       : PUGL_BUTTON_RELEASE);
			event.button.time   = xevent.xbutton.time / 1e3;
			event.button.x      = xevent.xbutton.x;
			event.button.y      = xevent.xbutton.y;
			event.button.xRoot  = xevent.xbutton.x_root;
			event.button.yRoot  = xevent.xbutton.y_root;
			event.button.state  = translateModifiers(xevent.xbutton.state);
			event.button.button = xevent.xbutton.button;
		}
		break;
	case KeyPress:
	case KeyRelease:
		event.type       = ((xevent.type == KeyPress)
		                    ? PUGL_KEY_PRESS
		                    : PUGL_KEY_RELEASE);
		event.key.time   = xevent.xkey.time / 1e3;
		event.key.x      = xevent.xkey.x;
		event.key.y      = xevent.xkey.y;
		event.key.xRoot  = xevent.xkey.x_root;
		event.key.yRoot  = xevent.xkey.y_root;
		event.key.state  = translateModifiers(xevent.xkey.state);
		translateKey(view, &xevent, &event);
		break;
	case EnterNotify:
	case LeaveNotify:
		event.type            = ((xevent.type == EnterNotify)
		                         ? PUGL_ENTER_NOTIFY
		                         : PUGL_LEAVE_NOTIFY);
		event.crossing.time   = xevent.xcrossing.time / 1e3;
		event.crossing.x      = xevent.xcrossing.x;
		event.crossing.y      = xevent.xcrossing.y;
		event.crossing.xRoot  = xevent.xcrossing.x_root;
		event.crossing.yRoot  = xevent.xcrossing.y_root;
		event.crossing.state  = translateModifiers(xevent.xcrossing.state);
		event.crossing.mode   = PUGL_CROSSING_NORMAL;
		if (xevent.xcrossing.mode == NotifyGrab) {
			event.crossing.mode = PUGL_CROSSING_GRAB;
		} else if (xevent.xcrossing.mode == NotifyUngrab) {
			event.crossing.mode = PUGL_CROSSING_UNGRAB;
		}
		break;

	case FocusIn:
	case FocusOut:
		event.type = (xevent.type == FocusIn) ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT;
		event.focus.grab = (xevent.xfocus.mode != NotifyNormal);
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
	event.xclient.data.l[1]    = atoms->NET_WM_STATE_DEMANDS_ATTENTION;
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
puglSetProcessFunc(PuglWorld* world, PuglProcessFunc processFunc, void* userData)
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




PuglStatus
puglWaitForEvent(PuglView* view)
{
	XEvent xevent;
	XPeekEvent(view->impl->display, &xevent);
	return PUGL_SUCCESS;
}

static bool
exposeEventsIntersect(const PuglEvent* a, const PuglEvent* b)
{
	return !(a->expose.x + a->expose.width < b->expose.x ||
	         b->expose.x + b->expose.width < a->expose.x ||
	         a->expose.y + a->expose.height < b->expose.y ||
	         b->expose.y + b->expose.height < a->expose.y);
}

static void
mergeExposeEvents(PuglEvent* dst, const PuglEvent* src)
{
	if (!dst->type) {
		*dst = *src;
	} else {
		const double max_x = MAX(dst->expose.x + dst->expose.width,
		                         src->expose.x + src->expose.width);
		const double max_y = MAX(dst->expose.y + dst->expose.height,
		                         src->expose.y + src->expose.height);

		dst->expose.x      = MIN(dst->expose.x, src->expose.x);
		dst->expose.y      = MIN(dst->expose.y, src->expose.y);
		dst->expose.width  = max_x - dst->expose.x;
		dst->expose.height = max_y - dst->expose.y;
		dst->expose.count  = MIN(dst->expose.count, src->expose.count);
	}
}

static void
addPendingExpose(PuglView* view, const PuglEvent* expose)
{
	if (view->impl->pendingConfigure.type ||
	    (view->impl->pendingExpose.type &&
	     exposeEventsIntersect(&view->impl->pendingExpose, expose))) {
		// Pending configure or an intersecting expose, expand it
		mergeExposeEvents(&view->impl->pendingExpose, expose);
	} else {
		if (view->impl->pendingExpose.type) {
			// Pending non-intersecting expose, dispatch it now
			// This isn't ideal, but avoids needing to maintain an expose list
			puglEnterContext(view, true);
			puglDispatchEvent(view, &view->impl->pendingExpose);
			puglLeaveContext(view, true);
		}

		view->impl->pendingExpose = *expose;
	}
}

static void
flushPendingConfigure(PuglView* view)
{
	PuglEvent* const configure = &view->impl->pendingConfigure;

	if (configure->type) {
		view->frame.x = configure->configure.x;
		view->frame.y = configure->configure.y;

		if (configure->configure.width != view->frame.width ||
		    configure->configure.height != view->frame.height) {
			view->frame.width  = configure->configure.width;
			view->frame.height = configure->configure.height;

			view->backend->resize(view,
			                      (int)view->frame.width,
			                      (int)view->frame.height);
		}

		view->eventFunc(view, configure);
		configure->type = 0;
	}
}

void
puglAwake(PuglWorld* world)
{
	if (world->impl->awake_fds[0] >= 0) {
		char c = 0;
		int ignore = write(world->impl->awake_fds[1], &c, 1);
	}
}

PUGL_API PuglStatus
puglDispatchEvents(PuglWorld* world)
{
	PuglWorldInternals* impl = world->impl;
	if (    impl->needsProcessing
	    || (impl->nextProcessTime >= 0 && impl->nextProcessTime <= puglGetTime(world)))
	{
	    impl->needsProcessing = false;
	    impl->nextProcessTime = -1;
	    if (world->processFunc) world->processFunc(world, world->processUserData);
	}
	const PuglX11Atoms* const atoms = &world->impl->atoms;

	// Flush just once at the start to fill event queue
	Display* display = world->impl->display;
	XFlush(display);

        bool wasDispatchingEvents = world->impl->dispatchingEvents;
	world->impl->dispatchingEvents = true;

	// Process all queued events (locally, without flushing or reading)
	while (XEventsQueued(display, QueuedAlready) > 0) {
		XEvent xevent;
		XNextEvent(display, &xevent);

		if (xevent.xany.window == impl->pseudoWin) {
		    if (xevent.type == SelectionClear) {
		        puglSetBlob(&world->clipboard, NULL, 0);
		    }
		    else if (xevent.type == SelectionRequest) {
		        handleSelectionRequestForOwner(impl->display,
                                                       atoms,
                                                       &xevent.xselectionrequest,
                                                       &world->clipboard,
                                                       &impl->incrTarget);
		    }
		    continue;
		}
		else if (xevent.type == PropertyNotify 
		      && xevent.xany.window == impl->incrTarget.win
		      && xevent.xproperty.state == PropertyDelete) 
		{
		    handlePropertyNotifyForOwner(impl->display,
		                                 &world->clipboard,
		                                 &impl->incrTarget);
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
			    next.type == KeyPress &&
			    next.xkey.time == xevent.xkey.time &&
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
                        handleSelectionNotifyForRequestor(view,
                                                          &xevent.xselection);
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
			// Expand expose event to be dispatched after loop
			addPendingExpose(view, &event);
		} else if (event.type == PUGL_CONFIGURE) {
			// Expand configure event to be dispatched after loop
			view->impl->pendingConfigure = event;
		} else {
			// Dispatch event to application immediately
			puglDispatchEvent(view, &event);
		}
	}

	if (!wasDispatchingEvents) {
		// Flush pending configure and expose events for all views
		for (size_t i = 0; i < world->numViews; ++i) {
			PuglView* const  view      = world->views[i];
			PuglEvent* const configure = &view->impl->pendingConfigure;
			PuglEvent* const expose    = &view->impl->pendingExpose;
	
			if (configure->type || expose->type) {
				const bool mustExpose = expose->type && expose->expose.count == 0;
				puglEnterContext(view, mustExpose);
	
				flushPendingConfigure(view);
	
				if (mustExpose) {
					view->eventFunc(view, &view->impl->pendingExpose);
				}
	
				puglLeaveContext(view, mustExpose);
				configure->type = 0;
				expose->type    = 0;
			}
		}
		world->impl->dispatchingEvents = wasDispatchingEvents;
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
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((double)ts.tv_sec + ts.tv_nsec / 1000000000.0) - world->startTime;
}

PuglStatus
puglPostRedisplay(PuglView* view)
{
	const PuglRect rect = { 0, 0, view->frame.width, view->frame.height };

	return puglPostRedisplayRect(view, rect);
}

PuglStatus
puglPostRedisplayRect(PuglView* view, PuglRect rect)
{
	if (view->world->impl->dispatchingEvents) {
		// Currently dispatching events, add/expand expose for the loop end
		const PuglEventExpose event = {
			PUGL_EXPOSE, 0, rect.x, rect.y, rect.width, rect.height, 0
		};

		addPendingExpose(view, (const PuglEvent*)&event);
	} else if (view->visible) {
		// Not dispatching events, send an X expose so we wake up next time
		const int x = (int)floor(rect.x);
		const int y = (int)floor(rect.y);
		const int w = (int)ceil(rect.x + rect.width) - x;
		const int h = (int)ceil(rect.y + rect.height) - y;

		XExposeEvent ev = {Expose, 0, True,
		                   view->impl->display, view->impl->win,
		                   x, y,
		                   w, h,
		                   0};

		XSendEvent(view->impl->display, view->impl->win, False, 0, (XEvent*)&ev);
	}

	return PUGL_SUCCESS;
}

PuglNativeWindow
puglGetNativeWindow(PuglView* view)
{
	return (PuglNativeWindow)view->impl->win;
}

PuglStatus
puglSetWindowTitle(PuglView* view, const char* title)
{
	Display*                  display = view->world->impl->display;
	const PuglX11Atoms* const atoms   = &view->world->impl->atoms;

	puglSetString(&view->title, title);
	XStoreName(display, view->impl->win, title);
	XChangeProperty(display, view->impl->win, atoms->NET_WM_NAME,
	                atoms->UTF8_STRING, 8, PropModeReplace,
	                (const uint8_t*)title, (int)strlen(title));

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
	    XSizeHints* sizeHints = getSizeHints(view);
	    XSetNormalHints(view->world->impl->display, view->impl->win, sizeHints);
	    XFree(sizeHints);
	    XMoveResizeWindow(view->world->impl->display, view->impl->win,
	                      (int)frame.x, (int)frame.y,
	                      (int)frame.width, (int)frame.height);
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetSize(PuglView* view, int width, int height)
{
	view->reqWidth  = width;
	view->reqHeight = height;
	
	if (view->impl->win) {
	    XSizeHints* sizeHints = getSizeHints(view);
	    XSetNormalHints(view->world->impl->display, view->impl->win, sizeHints);
	    XFree(sizeHints);
	    XResizeWindow(view->world->impl->display, view->impl->win, width, height);
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetMinSize(PuglView* const view, const int width, const int height)
{
	Display* display = view->world->impl->display;

	view->minWidth  = width;
	view->minHeight = height;

	if (view->impl->win) {
		XSizeHints* sizeHints = getSizeHints(view);
		XSetNormalHints(display, view->impl->win, sizeHints);
		XFree(sizeHints);
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetMaxSize(PuglView* const view, const int width, const int height)
{
	Display* display = view->world->impl->display;

	view->maxWidth  = width;
	view->maxHeight = height;

	if (view->impl->win) {
		XSizeHints* sizeHints = getSizeHints(view);
		XSetNormalHints(display, view->impl->win, sizeHints);
		XFree(sizeHints);
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
	Display* display = view->world->impl->display;

	view->minAspectX = minX;
	view->minAspectY = minY;
	view->maxAspectX = maxX;
	view->maxAspectY = maxY;

	if (view->impl->win) {
		XSizeHints* sizeHints = getSizeHints(view);
		XSetNormalHints(display, view->impl->win, sizeHints);
		XFree(sizeHints);
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglSetTransientFor(PuglView* view, PuglNativeWindow parent)
{
	Display* display = view->world->impl->display;

	view->transientParent = parent;

	if (view->impl->win) {
		XSetTransientForHint(display, view->impl->win,
		                     (Window)view->transientParent);
	}

	return PUGL_SUCCESS;
}

PuglStatus
puglRequestClipboard(PuglView* const    view)
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

	XSetSelectionOwner(impl->display, atoms->CLIPBOARD, impl->pseudoWin, CurrentTime);
	return PUGL_SUCCESS;
}

bool
puglHasClipboard(PuglWorld*  world)
{
    return world->clipboard.data != NULL;
}


const PuglBackend*
puglStubBackend(void)
{
	static const PuglBackend backend = {puglX11StubConfigure,
	                                    puglStubCreate,
	                                    puglStubDestroy,
	                                    puglStubEnter,
	                                    puglStubLeave,
	                                    puglStubResize,
	                                    puglStubGetContext};

	return &backend;
}
