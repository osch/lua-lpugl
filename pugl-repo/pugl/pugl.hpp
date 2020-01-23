/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>

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
   @file pugl.hpp Pugl C++ API wrapper.
*/

#ifndef PUGL_PUGL_HPP
#define PUGL_PUGL_HPP

#include "pugl/pugl.h"
#include "pugl/pugl_gl.h" // FIXME

#include <chrono>
#include <functional>
#include <ratio>
#include <stdexcept>
#include <utility>

/**
   @defgroup puglmm Puglmm
   Pugl C++ API wrapper.
   @{
*/

namespace pugl {

enum class Status {
	success             = PUGL_SUCCESS,
	failure             = PUGL_FAILURE,
	unknownError        = PUGL_UNKNOWN_ERROR,
	badBackend          = PUGL_BAD_BACKEND,
	backendFailed       = PUGL_BACKEND_FAILED,
	registrationFailed  = PUGL_REGISTRATION_FAILED,
	createWindowFailed  = PUGL_CREATE_WINDOW_FAILED,
	setFormatFailed     = PUGL_SET_FORMAT_FAILED,
	createContextFailed = PUGL_CREATE_CONTEXT_FAILED,
	unsupportedType     = PUGL_UNSUPPORTED_TYPE,
};

using ViewHint     = PuglViewHint;
using Rect         = PuglRect;
using NativeWindow = PuglNativeWindow;
using GlFunc       = PuglGlFunc;
using Event        = PuglEvent;

template <PuglEventType t, class Base>
struct TypedEvent : public Base
{
	static constexpr const PuglEventType type = t;
};

using ButtonPressEvent   = TypedEvent<PUGL_BUTTON_PRESS, PuglEventButton>;
using ButtonReleaseEvent = TypedEvent<PUGL_BUTTON_RELEASE, PuglEventButton>;
using ConfigureEvent     = TypedEvent<PUGL_CONFIGURE, PuglEventConfigure>;
using ExposeEvent        = TypedEvent<PUGL_EXPOSE, PuglEventExpose>;
using CloseEvent         = TypedEvent<PUGL_CLOSE, PuglEventClose>;
using KeyPressEvent      = TypedEvent<PUGL_KEY_PRESS, PuglEventKey>;
using KeyReleaseEvent    = TypedEvent<PUGL_KEY_RELEASE, PuglEventKey>;
using TextEvent          = TypedEvent<PUGL_TEXT, PuglEventText>;
using EnterEvent         = TypedEvent<PUGL_ENTER_NOTIFY, PuglEventCrossing>;
using LeaveEvent         = TypedEvent<PUGL_LEAVE_NOTIFY, PuglEventCrossing>;
using MotionEvent        = TypedEvent<PUGL_MOTION_NOTIFY, PuglEventMotion>;
using ScrollEvent        = TypedEvent<PUGL_SCROLL, PuglEventScroll>;
using FocusInEvent       = TypedEvent<PUGL_FOCUS_IN, PuglEventFocus>;
using FocusOutEvent      = TypedEvent<PUGL_FOCUS_OUT, PuglEventFocus>;

static inline const char*
strerror(pugl::Status status)
{
	return puglStrerror(static_cast<PuglStatus>(status));
}

static inline GlFunc
getProcAddress(const char* name)
{
	return puglGetProcAddress(name);
}

class World;

class Clock
{
public:
	using rep        = double;
	using period     = std::ratio<1>;
	using duration   = std::chrono::duration<double>;
	using time_point = std::chrono::time_point<Clock>;

	static constexpr bool is_steady = true;

	explicit Clock(World& world) : _world{world} {}

	time_point now() const;

private:
	const pugl::World& _world;
};

class World
{
public:
	World() : _clock(*this), _world(puglNewWorld())
	{
		if (!_world) {
			throw std::runtime_error("Failed to create pugl::World");
		}
	}

	~World() { puglFreeWorld(_world); }

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&)                 = delete;
	World&& operator=(World&&) = delete;

	Status setClassName(const char* const name)
	{
		return static_cast<Status>(puglSetClassName(_world, name));
	}

	double getTime() const { return puglGetTime(_world); }

	Status pollEvents(const double timeout)
	{
		return static_cast<Status>(puglPollEvents(_world, timeout));
	}

	Status dispatchEvents()
	{
		return static_cast<Status>(puglDispatchEvents(_world));
	}

	const PuglWorld* cobj() const { return _world; }
	PuglWorld*       cobj() { return _world; }

	const Clock& clock() { return _clock; }

private:
	Clock            _clock;
	PuglWorld* const _world;
};

inline Clock::time_point
Clock::now() const
{
	return time_point{duration{_world.getTime()}};
}

class ViewBase
{
public:
	explicit ViewBase(World& world)
	    : _world(world), _view(puglNewView(world.cobj()))
	{
		if (!_view) {
			throw std::runtime_error("Failed to create pugl::View");
		}

		puglSetHandle(_view, this);
	}

	~ViewBase() { puglFreeView(_view); }

	ViewBase(const ViewBase&) = delete;
	ViewBase(ViewBase&&)      = delete;
	ViewBase&  operator=(const ViewBase&) = delete;
	ViewBase&& operator=(ViewBase&&) = delete;

	Status setHint(ViewHint hint, int value)
	{
		return static_cast<Status>(puglSetViewHint(_view, hint, value));
	}

	bool getVisible() const { return puglGetVisible(_view); }

	Status postRedisplay()
	{
		return static_cast<Status>(puglPostRedisplay(_view));
	}

	const pugl::World& getWorld() const { return _world; }
	pugl::World&       getWorld() { return _world; }

	Rect getFrame() const { return puglGetFrame(_view); }

	Status setFrame(Rect frame)
	{
		return static_cast<Status>(puglSetFrame(_view, frame));
	}

	Status setMinSize(int width, int height)
	{
		return static_cast<Status>(puglSetMinSize(_view, width, height));
	}

	Status setAspectRatio(int minX, int minY, int maxX, int maxY)
	{
		return static_cast<Status>(
		        puglSetAspectRatio(_view, minX, minY, maxX, maxY));
	}

	Status setWindowTitle(const char* title)
	{
		return static_cast<Status>(puglSetWindowTitle(_view, title));
	}

	Status setParentWindow(NativeWindow parent)
	{
		return static_cast<Status>(puglSetParentWindow(_view, parent));
	}

	Status setTransientFor(NativeWindow parent)
	{
		return static_cast<Status>(puglSetTransientFor(_view, parent));
	}

	Status createWindow(const char* title)
	{
		return static_cast<Status>(puglCreateWindow(_view, title));
	}

	Status showWindow() { return static_cast<Status>(puglShowWindow(_view)); }

	Status hideWindow() { return static_cast<Status>(puglHideWindow(_view)); }

	NativeWindow getNativeWindow() { return puglGetNativeWindow(_view); }

	Status setBackend(const PuglBackend* backend)
	{
		return static_cast<Status>(puglSetBackend(_view, backend));
	}

	void* getContext() { return puglGetContext(_view); }

	void enterContext(bool drawing) { puglEnterContext(_view, drawing); }
	void leaveContext(bool drawing) { puglLeaveContext(_view, drawing); }

	bool hasFocus() const { return puglHasFocus(_view); }

	Status grabFocus() { return static_cast<Status>(puglGrabFocus(_view)); }

	Status requestAttention()
	{
		return static_cast<Status>(puglRequestAttention(_view));
	}

	PuglView* cobj() { return _view; }

protected:
	World&    _world;
	PuglView* _view;
};

template <typename Data>
class View : public ViewBase
{
public:
	template <class E>
	using TypedEventFunc = std::function<pugl::Status(View&, const E&)>;

	using NothingEvent = TypedEvent<PUGL_NOTHING, PuglEvent>;

	using EventFuncs = std::tuple<TypedEventFunc<NothingEvent>,
	                              TypedEventFunc<ButtonPressEvent>,
	                              TypedEventFunc<ButtonReleaseEvent>,
	                              TypedEventFunc<ConfigureEvent>,
	                              TypedEventFunc<ExposeEvent>,
	                              TypedEventFunc<CloseEvent>,
	                              TypedEventFunc<KeyPressEvent>,
	                              TypedEventFunc<KeyReleaseEvent>,
	                              TypedEventFunc<TextEvent>,
	                              TypedEventFunc<EnterEvent>,
	                              TypedEventFunc<LeaveEvent>,
	                              TypedEventFunc<MotionEvent>,
	                              TypedEventFunc<ScrollEvent>,
	                              TypedEventFunc<FocusInEvent>,
	                              TypedEventFunc<FocusOutEvent>>;

	using EventFunc = std::function<pugl::Status(View&, const PuglEvent&)>;

	explicit View(World& world) : ViewBase{world}, _data{}
	{
		puglSetEventFunc(_view, _onEvent);
	}

	View(World& world, Data data) : ViewBase{world}, _data{data}
	{
		puglSetEventFunc(_view, _onEvent);
	}

	template <class HandledEvent>
	Status setEventFunc(
	        std::function<pugl::Status(View&, const HandledEvent&)> handler)
	{
		std::get<HandledEvent::type>(_eventFuncs) = handler;

		return Status::success;
	}

	template <class HandledEvent>
	Status setEventFunc(pugl::Status (*handler)(View&, const HandledEvent&))
	{
		std::get<HandledEvent::type>(_eventFuncs) = handler;

		return Status::success;
	}

	const Data& getData() const { return _data; }
	Data&       getData() { return _data; }

private:
	static PuglStatus _onEvent(PuglView* view, const PuglEvent* event)
	{
		View* self = static_cast<View*>(puglGetHandle(view));

		return static_cast<PuglStatus>(self->dispatchEvent(*event));
	}

	Status dispatchEvent(const PuglEvent& event)
	{
		switch (event.type) {
		case PUGL_NOTHING: return Status::success;
		case PUGL_BUTTON_PRESS:
			return dispatchTypedEvent(
			        static_cast<const ButtonPressEvent&>(event.button));
		case PUGL_BUTTON_RELEASE:
			return dispatchTypedEvent(
			        static_cast<const ButtonReleaseEvent&>(event.button));
		case PUGL_CONFIGURE:
			return dispatchTypedEvent(
			        static_cast<const ConfigureEvent&>(event.configure));
		case PUGL_EXPOSE:
			return dispatchTypedEvent(
			        static_cast<const ExposeEvent&>(event.expose));
		case PUGL_CLOSE:
			return dispatchTypedEvent(
			        static_cast<const CloseEvent&>(event.close));
		case PUGL_KEY_PRESS:
			return dispatchTypedEvent(
			        static_cast<const KeyPressEvent&>(event.key));
		case PUGL_KEY_RELEASE:
			return dispatchTypedEvent(
			        static_cast<const KeyReleaseEvent&>(event.key));
		case PUGL_TEXT:
			return dispatchTypedEvent(
			        static_cast<const TextEvent&>(event.text));
		case PUGL_ENTER_NOTIFY:
			return dispatchTypedEvent(
			        static_cast<const EnterEvent&>(event.crossing));
		case PUGL_LEAVE_NOTIFY:
			return dispatchTypedEvent(
			        static_cast<const LeaveEvent&>(event.crossing));
		case PUGL_MOTION_NOTIFY:
			return dispatchTypedEvent(
			        static_cast<const MotionEvent&>(event.motion));
		case PUGL_SCROLL:
			return dispatchTypedEvent(
			        static_cast<const ScrollEvent&>(event.scroll));
		case PUGL_FOCUS_IN:
			return dispatchTypedEvent(
			        static_cast<const FocusInEvent&>(event.focus));
		case PUGL_FOCUS_OUT:
			return dispatchTypedEvent(
			        static_cast<const FocusOutEvent&>(event.focus));
		}

		return Status::failure;
	}

	template <class E>
	Status dispatchTypedEvent(const E& event)
	{
		auto& handler = std::get<E::type>(_eventFuncs);
		if (handler) {
			return handler(*this, event);
		}

		return Status::success;
	}

	Data       _data;
	EventFuncs _eventFuncs;
};

} // namespace pugl

/**
   @}
*/

#endif /* PUGL_PUGL_HPP */
