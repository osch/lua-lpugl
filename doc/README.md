# lpugl Documentation

<!-- ---------------------------------------------------------------------------------------- -->
##   Contents
<!-- ---------------------------------------------------------------------------------------- -->
   * [Overview](#overview)
   * [Module Functions](#module-functions)
        * [lpugl.newWorld()](#lpugl_newWorld)
        * [lpugl.world()](#lpugl_world)
        * [lpugl.btest()](#lpugl_btest)
        * [lpugl_cairo.newWorld()](#lpugl_cairo_newWorld)
        * [lpugl_cairo.newBackend()](#lpugl_cairo_newBackend)
        * [lpugl_opengl.newWorld()](#lpugl_opengl_newWorld)
        * [lpugl_opengl.newBackend()](#lpugl_opengl_newBackend)
   * [Module Constants](#module-constants)
        * [lpugl.platform](#lpugl_platform)
        * [Key modifier flags](#lpugl_MOD_)
   * [World Methods](#world-methods)
        * [world:setDefaultBackend()](#world_setDefaultBackend)
        * [world:getDefaultBackend()](#world_getDefaultBackend)
        * [world:id()](#world_id)
        * [world:newView()](#world_newView)
        * [world:hasViews()](#world_hasViews)
        * [world:update()](#world_update)
        * [world:setProcessFunc()](#world_setProcessFunc)
        * [world:setNextProcessTime()](#world_setNextProcessTime)
        * [world:awake()](#world_awake)
        * [world:getTime()](#world_getTime)
        * [world:setErrorFunc()](#world_setErrorFunc)
        * [world:setLogLevel()](#world_setLogLevel)
        * [world:setLogFunc()](#world_setLogFunc)
        * [world:setClipboard()](#world_setClipboard)
        * [world:hasClipboard()](#world_hasClipboard)
        * [world:getScreenScale()](#world_getScreenScale)
        * [world:close()](#world_close)
        * [world:isClosed()](#world_isClosed)
   * [View Methods](#view-methods)
        * [view:show()](#view_show)
        * [view:hide()](#view_hide)
        * [view:isVisible()](#view_isVisible)
        * [view:setTitle()](#view_setTitle)
        * [view:setMinSize()](#view_setMinSize)
        * [view:setMaxSize()](#view_setMaxSize)
        * [view:setSize()](#view_setSize)
        * [view:getSize()](#view_getSize)
        * [view:setFrame()](#view_setFrame)
        * [view:getFrame()](#view_getFrame)
        * [view:hasFocus()](#view_hasFocus)
        * [view:grabFocus()](#view_grabFocus)
        * [view:getLayoutContext()](#view_getLayoutContext)
        * [view:getDrawContext()](#view_getDrawContext)
        * [view:getScreenScale()](#view_getScreenScale)
        * [view:postRedisplay()](#view_postRedisplay)
        * [view:setCursor()](#view_setCursor)
        * [view:requestClipboard()](#view_requestClipboard)
        * [view:getNativeHandle()](#view_getNativeHandle)
        * [view:close()](#view_close)
        * [view:isClosed()](#view_isClosed)
   * [Backend Methods](#backend-methods)
        * [cairoBackend:getLayoutContext()](#cairoBackend_getLayoutContext)
   * [Event Processing](#event-processing)
        * [CREATE](#event_CREATE)
        * [CONFIGURE](#event_CONFIGURE)
        * [MAP](#event_MAP)
        * [UNMAP](#event_UNMAP)
        * [EXPOSE](#event_EXPOSE)
        * [BUTTON_PRESS](#event_BUTTON_PRESS)
        * [BUTTON_RELEASE](#event_BUTTON_RELEASE)
        * [KEY_PRESS](#event_KEY_PRESS)
        * [KEY_RELEASE](#event_KEY_RELEASE)
        * [POINTER_IN](#event_POINTER_IN)
        * [POINTER_OUT](#event_POINTER_OUT)
        * [MOTION](#event_MOTION)
        * [SCROLL](#event_SCROLL)
        * [FOCUS_IN](#event_FOCUS_IN)
        * [FOCUS_OUT](#event_FOCUS_OUT)
        * [CLOSE](#event_CLOSE)

<!-- ---------------------------------------------------------------------------------------- -->
##   Overview
<!-- ---------------------------------------------------------------------------------------- -->
   
LPugl provides three LuaRocks modules 
   * **[lpugl]**        - platform specific base module
   * **[lpugl_cairo]**  - Cairo rendering backend module
   * **[lpugl_opengl]** - OpenGL rendering backend module

Each of these modules provides a Lua package with the same name which can be 
loaded via *require()*.

The backend packages *lpugl_cairo* and *lpugl_opengl* automatically require
the base package *lpugl*. All members from the base package are available in
the backend packages, i.e if you intend to work only with one backend, it is
sufficient just to require the corresponding backend package, i.e.

```lua
local XXX = require("lpugl_cairo")
```
makes all symbols from the packages *lpugl* and *lpugl_cairo* visible under the name *XXX*.


<!-- ---------------------------------------------------------------------------------------- -->
##   Module Functions
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_newWorld">**`  lpugl.newWorld(name)
  `**</a>
  
  Creates a new Pugl world object with the given name. A world object is used to create
  windows and to drive the event loop. Normally you would only create one world object 
  for an application.
  
  * *name* - mandatory string. This name is also used by some window managers to display
             a name for the group of windows that were created by this world object.
             
  The created Pugl world is subject to garbage collection. If the world object
  is garbage collected, [*world:close()*](#world_close) is invoked which closes
  all views that were created from this world.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_world">**`     lpugl.world(id)
  `**</a>
  
  Creates a lua object for restricted access to an existing world object that has
  been created with *lpugl.newWorld()*, *lpugl_cairo.newWorld()* or *lpugl_opengl.newWorld()*.
  
  This function can be invoked from any concurrently running thread. The obtained
  restricted world access can only be used to invoke the [*world:awake()*](#world_awake)
  method.

  * *id* - mandatory integer, the world object's id that can be obtained
           by [*world:id()*](#world_id)


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_btest">**`     lpugl.btest(...)
  `**</a>
  
  Returns a boolean signaling whether the bitwise AND of its operands is different from zero. 
  
  Can be used to test for [key modifier flags](#lpugl_MOD_) in a key modifier state value.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_cairo_newWorld">**`       lpugl_cairo.newWorld(name)
  `**</a>
  
  This does the same as [*lpugl.newWorld()*](#lpugl_newWorld) but does
  also set a Cairo backend as default backend for the created world object.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_cairo_newBackend">**`     lpugl_cairo.newBackend(world)
  `**</a>
  
  Creates a new Cairo backend object for the given world.
  
  * *world* - mandatory world object for the which the backend can be used.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newWorld">**`      lpugl_opengl.newWorld(name)
  `**</a>
  
  This does the same as [*lpugl.newWorld()*](#lpugl_newWorld) but does
  also set an OpenGL backend as default backend for the created world object.


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newBackend">**`    lpugl_opengl.newBackend(world)
  `**</a>
  
  Creates a new OpenGL backend object for the given world.
  
  * *world* - mandatory world object for the which the backend can be used.
  
TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   Module Constants
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_platform">**`    lpugl.platform
  `**</a>
  
  Platform information, contains the string `"X11"`, `"WIN"` or `"MAC"`. 


* **<a id="lpugl_MOD_">Key modifier flags</a>**

  The current key modifier state (e.g. as delivered in a [*BUTTON_PRESS*](#event_BUTTON_PRESS) event) 
  is an integer value where each key modifier is presented as a bit flag.
  You may use the function [lpugl.btest()](#lpugl_btest) for testing which
  flags are set in a key modifier state.
  
   * **`lpugl.MOD_SHIFT = 1`**   - Shift key 
   * **`lpugl.MOD_CTRL = 2`**    - Control key
   * **`lpugl.MOD_ALT = 4`**     - Alt/Option key
   * **`lpugl.MOD_SUPER = 8`**   - Mod4/Command/Windows key
   * **`lpugl.MOD_ALTGR = 16`**  - AltGr key

   


TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   World Methods
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setDefaultBackend">**`    world:setDefaultBackend(backend)
  `** </a>
  
  Sets the given backend as default backend. The default backend is used for
  all views that are created for this world if no backend is given when
  creating the view via [*world:newView()*](#world_newView).
  
  * *backend* - mandatory backend object

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_getDefaultBackend">**`    world:getDefaultBackend()
  `**</a>
  
  Returns the default backend that was set with [*world:setDefaultBackend()*](#world_setDefaultBackend).

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_id">**`                   world:id()
  `**</a>

  Returns the world objects's id as integer. This id is unique among all world objects
  for the whole process. 
  
  The world's id can be used to trigger events for this world from concurrently running threads 
  by invoking the functions [*lpugl:world()*](#lpugl_world) and [*world:awake()*](#world_awake).

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_newView">**`              world:newView(initArgs)`**

  Creates a new view object. View objects can be top level windows or can be embedded in other
  views, depending on the parameters given in *initArgs*.

  A view object is owned by the Pugl world for which the view was created. Therefore
  the view object will only be garbage collected if the view is closed explicitly via
  [*view:close()*](#view_close) or if the world becomes garbage collected or is closed
  explicitly via [*world:close()*](#world_close)
  
  * *initArgs* - lua table with initial key-value parameters. 
                 
  The parameter *initArgs* may contain the following parameters as key value pairs:
  
  * <a id="newView_backend">**`backend = lpuglBackend`**</a> - backend object to be used for the 
    created view. Can be omitted, if a default backend was set via 
    [*world:setDefaultBackend()*](#world_setDefaultBackend)

  * <a id="newView_title">**`title = titleString`**</a> - optional string, initial title for the 
    created window. This title is displayed in the window title bar if the view is a top level 
    window (i.e. [*parent*](#newView_parent) and [*popupFor*](#newView_popupFor) are not given).
    The title may afterwards be changed using [*view:setTitle()*](#view_setTitle).

  * <a id="newView_size">**`size = {width, height}`**</a> - optional table containing
    width and height as first and second table entry. Sets the initial size for the new view.
    The size may afterwards be changed using [*view:setSize()*](#view_setSize).
    
  * <a id="newView_resizable">**`resizable = flag`**</a> - *true* if the created window should be 
    resizable by the user.

  * <a id="newView_parent">**`parent = view`**</a> - optional parent view. If given the created 
    view is embedded into the parent view. Otherwise a top level window is created. This parameter 
    cannot be combined with [*popupFor*](#newView_popupFor) or [*transientFor*](#newView_transientFor).
    
    The parent view may also be given as a [light userdata] 
    which is then interpreted as a native view handle. This makes it possible to create a 
    *lpugl.view* as a child view into a parent view that is not handled by *lpugl.world*. See
    also [*view:getNativeHandle()*](#view_getNativeHandle)

  * <a id="newView_popupFor">**`popupFor = view`**</a>       - TODO - This parameter 
    cannot be combined with [*parent*](#newView_parent) or [*transientFor*](#newView_transientFor). 

  * <a id="newView_transientFor">**`transientFor = view`**</a>   - TODO - This parameter 
    cannot be combined with [*parent*](#newView_parent) or [*popupFor*](#newView_popupFor).

  * <a id="newView_dontMergeRects">**`dontMergeRects = flag`**</a> - if set to *true*, 
    several [expose events](#event_EXPOSE) will be delivered for one exposure cycle. Otherwise
    all exposed rectangles in the current cycle will be merged to one larger rectangle that will 
    be delivered in one single exposure event.

  * <a id="newView_useDoubleBuffer">**`useDoubleBuffer = flag`**</a> - *true* if the created window 
    should use double buffering. This parameter has only effect for the OpenGL backend.
    If double buffering is set, partial redrawing is not guaranteed to work, i.e. in this case the
    function [view:postRedisplay()](#view_postRedisplay) should only be called without
    specifying a redraw rectangle.

  * <a id="newView_eventFunc">**`eventFunc = func | {func, ...}`**</a>  - sets a function for 
    handling the view's  [event processing](#event-processing). The value for *eventFunc* may
    be a function or a table with it's first entry being the event handling function. The other
    entries with index > 1 in this table are context parameters that are given to the
    event handling function for each invocation.
    
    If for the example the event handling function *func* is set up by the following 
    invocation:
      ```lua
      view = world:newView { eventFunc = {func, "foo1", "foo2"} }
      ```
      and if an [mouse motion event](#event_MOTION) for the view at position 100, 200 occurs, then 
      *func* will be called with the arguments `"foo1", "foo2", view, "MOTION", 100, 200`. If
      the event handling function is set up without context parameters, e.g.:
      ```lua
      view = world:newView { eventFunc = func }
      ```
      the above mouse motion event would lead to an invocation of *func* with the arguments 
      `view, "MOTION", 100, 200`.
      
    

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_hasViews">**`             world:hasViews()
  `**</a>
  
  Returns *true* if there are views belonging to this world that have not been closed.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_update">**`               world:update(timeout)
  `**</a>

  Update by processing events from the window system.

  * *timeout*  - optional float, timeout in seconds  

  If *timeout* is given, this function will wait for *timeout* seconds until
  events from the window system become available. If *timeout* is `nil` or not
  given, this function will block indefinitely until an event occurs.

  As soon as events are available, all events in the queue are processed and this function 
  returns `true`.
  
  If *timeout* is given and there are no events available after *timeout*
  seconds, this function will return `false`.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setProcessFunc">**`          world:setProcessFunc(func)
  `**</a>
  
  Sets the world's process function. The process function is called in the main event loop after 
  the time has elapsed that was given in [*world:setNextProcessTime()*](#world_setNextProcessTime) 
  or if [*world:awake()*](#world_awake) has been called.
  
  * *func*   -  a function that takes no parameters. This function is meant to do periodic 
                or timer triggered processing in the main event loop.
  
  On X11 platform the usage of a process function is equivalent to handling processing in the main
  event loop directly, e.g.
  
  ```lua
  while world:hasViews() do
      world:update(timeoutSeconds)
      ... do some timer dependent processing here...
  end
  
  ```

  However Windows and Mac OS platforms have the concept of a *central event queue*
  (on Windows per thread, on Mac per application). On these platforms event
  dispatching can occur outside your own event loop (for example on Windows: event 
  dispatching occurs when a window is dragged or resized). 
  Also on Mac platforms calling *world:update()* will dispatch events belonging
  to other windows that do not  belong to your Pugl world. This might be interesting if
  LPugl is used within a larger context (e.g. for a plugin GUI inside another application).

  For best cross platform compatibility therefore it is highly reommended to
  register a process function via *world:setProcessFunc()* and use
  [*world:setNextProcessTime()*](#world_setNextProcessTime)  or
  [*world:awake()*](#world_awake) to trigger its execution. 
  This process function is then also invoked if events are dispatched outside your own 
  event loop on Win and Mac platforms.
  
                
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setNextProcessTime">**`          world:setNextProcessTime(seconds)
  `**</a>
  
  Sets the time duration after the world's process function is called that was set via
  [*world:setProcessFunc()*](#world_setProcessFunc).
  
  This timer is not periodic, i.e. this function has to be called again to schedule 
  subsequent invocations of the process function.
  
  * *seconds* - mandatory float, time in seconds after the world's process function is invoked 
                in the main event loop. For seconds < 0 the timer is disabled.
                
                
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_awake">**`                world:awake()
  `**</a>
  
  Can be invoked from concurrently running threads to trigger the invocation of
  the world's process function that was set via [*world:setProcessFunc()*](#world_setProcessFunc) 
  while dispatching events on the GUI thread. 
  
  This can be used for signalling the GUI thread to handle events that are to be
  obtained from application specific channels, for example interthread
  communication via [mtmsg].
  
  To use this feature you have to obtain the world's id by invoking
  [*world:id()*](#world_id) and transfer this integer id to the background thread. On
  the background thread  invoke [*lpugl.world(id)*](#lpugl_world) to obtain a 
  restricted access object on which the method *world:awake()* can be invoked.
  
  This functions returns `true` if the world is open and the awake could be signalled,
  `false` otherwise.
  
  An awake could also be signalled from any non Lua related C code via the *Notify C API*, 
  see [src/notify_capi.h](../src/notify_capi.h),
  i.e. the world object has an associated meta table entry *_capi_notify* delivered by
  the C API function *notify_get_capi()* and the associated C API function *toNotifier()* returns
  a valid pointer for a given LPugl world object. See [example10.lua](../example/example10.lua)
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_getTime">**`              world:getTime()
  `**</a>
  
   Returns the time in seconds as float value.

   This is a monotonically increasing clock with high resolution.  The returned
   time is only useful to compare against other times returned by this
   method, its absolute value has no meaning.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setErrorFunc">**`         world:setErrorFunc(func)
  `**</a>

  Sets the world's error function. The error function is called when an error occurs while
  an event is dispatched or while the world's process functions is invoked. 
  If no error function is set, the error message and stack traceback are printed
  to `stderr` and the process is aborted. 
  
  * *func* - a error reporting function that takes two arguments: first argument is a string
             containing the error message and stack traceback, second argument is the original
             error object that caused the error.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setLogLevel">**`          world:setLogLevel(logLevel)
  `**</a>
  
  * *logLevel* - string, valid values: *"NONE"*, *"ERROR"*, *"WARNING"*, *"INFO"*, *"DEBUG"*.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setLogFunc">**`           world:setLogFunc(func)
  `**</a>
  
  * *func* - log function, this function is invoked with arguments *logLevel*, *logMessage*
             for each log message that has a log level according to the log 
             level set with *world:setLogLevel()*.
  
  If no log function is set, log messages with level *"ERROR"* are written to stderr.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setClipboard">**`         world:setClipboard(text)
  `**</a>
  
  Pastes the given text to the clipboard. On X11 the clipboard content is lost if the Pugl
  world is closed.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_hasClipboard">**`         world:hasClipboard()
  `**</a>
  
  Returns `true` if the clipboard is owned by this Pugl world. This returns always `false`
  on Win and Mac platform. On X11 the clipboard content is lost if the clipboard owning
  application terminates, i.e. if the clipboard owning Pugl world is closed.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_getScreenScale">**`         world:getScreenScale()
  `**</a>
  
  Returns the screen scale factor for the default screen.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_close">**`                world:close()
  `**</a>
  
  Closes the Pugl world. Closes also all views belonging to this Pugl world.
  This method is also invoked if the last Lua reference to the Pugl world
  is lost and the  world object becomes garbage collected.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_isClosed">**`             world:isClosed()
  `**</a>
  
  Returns `true` if the Pugl world was closed.
  
TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   View Methods
<!-- ---------------------------------------------------------------------------------------- -->

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_show">**`                 view:show()
  `**</a>
  
  Makes the view visible. A new created view is invisible by default.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_hide">**`                 view:hide()
  `**</a>
  
  Makes the view invisible.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_isVisible">**`            view:isVisible()
  `**</a>
  
  Returns `true` if the view is visible.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setTitle">**`              view:setTitle(title)
  `**</a>
  
  Sets the title of the window. 
  
  * *title* -  string value. The title is displayed in the window title bar if the view 
               is a top level window (i.e. [*parent*](#newView_parent) and 
               [*popupFor*](#newView_popupFor) are not set).
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setMinSize">**`           view:setMinSize(width, height)
  `**</a>
  
  Sets the minimum size of the view.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setMaxSize">**`           view:setMaxSize(width, height)
  `**</a>
  
  Sets the maximum size of the view.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setSize">**`              view:setSize(width, height)
  `**</a>
  
  Sets the size of the view.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getSize">**`              view:getSize()
  `**</a>
  
  Gets the size of the view. Returns *width*, *height*.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setFrame">**`             view:setFrame(x, y, width, height)
  `**</a>
  
  Sets the position and size of the view.
  
  The position is parent-relative if the view is embedded into a parent view (i.e. 
  *parent* was given in [world:newView()](#world_newView)). Otherwise the position 
  is in absolute screen coordinates (the view is a top level window or popup in 
  these cases).
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getFrame">**`             view:getFrame()
  `**</a>
  
  Gets the position and size of the view.  Returns *x*, *y*, *width*, *height*. 
  
  The position is parent-relative if the view is embedded into a parent view (i.e. 
  *parent* was given in [world:newView()](#world_newView)). Otherwise the position 
  is in absolute screen coordinates (the view is a top level window or popup in 
  these cases).
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_hasFocus">**`             view:hasFocus()
  `**</a>
  
  Returns `true` if the view has keyboard input focus. 

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_grabFocus">**`            view:grabFocus()
  `**</a>
  
  Requests keyboard input focus.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getLayoutContext">**`     view:getLayoutContext()
  `**</a>
  
  Only for Cairo backend: Gets a Cairo context that can be used for rendering off screen 
  surfaces or for layout purposes, e.g. obtaining text extents. This context may
  be used outside or within [exposure event processing](#event_EXPOSE)
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getDrawContext">**`       view:getDrawContext()
  `**</a>
  
  Only for Cairo backend: Gets the Cairo draw context. This should only be used while
  [processing exposure events](#event_EXPOSE). The Cairo draw context has already been set up with 
  clipping for the exposed area of the current exposure cycle.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getScreenScale">**`       view:getScreenScale()
  `**</a>
  
  Returns the screen scale factor for the view.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_postRedisplay">**`        view:postRedisplay([x, y, width, height])
  `**</a>
  
  Request a redisplay for the entire view or the given rectangle within the view.
  
  * *x*, *y*, *width*, *height*  - optional position and size of the rectangle that should be 
                                   redisplayed. If omitted the entire view will be redisplayed.
  
  If a view with OpenGL backend was created with [*useDoubleBuffer=true*](#newView_useDoubleBuffer),
  partial redisplay could lead to undefined behaviour, since the content of the OpenGL back buffer 
  becomes undefined after back and front buffers are swapped. Therefore *view:postRedisplay()* 
  should only be invoked without arguments in this case.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_setCursor">**`        view:setCursor(type)
  `**</a>
  
  Sets the appearance of the mouse cursor.
  
  * *type* mouse cursor type: can be one of the following:
  
    * *"ARROW"*      - Default pointing arrow
    * *"CARET"*      - Caret (I-Beam) for text entry
    * *"CROSSHAIR"*  - Cross-hair
    * *"HAND"*       - Hand with a pointing finger
    * *"NO"*         - Operation not allowed
    * *"LEFT_RIGHT"* - Left/right arrow for horizontal resize
    * *"UP_DOWN"*    - Up/down arrow for vertical resize
    * *"HIDDEN"*     - Invisible cursor


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_requestClipboard">**`     view:requestClipboard()
  `**</a>
  
  TODO

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getNativeHandle">**`      view:getNativeHandle()
  `**</a>
  
  Gives the native handle for the view as a [light userdata]. 
  
  * For X11: This is a `Window`.
  * For MacOS: This is a pointer to an `NSView`.
  * For Windows: This is a `HWND`.

  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_close">**`                view:close()
  `**</a>
  
  Closes the view. Once a view is closed, it cannot be re-opened. 
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_isClosed">**`             view:isClosed()
  `**</a>
  
  Returns `true` if the Pugl view was closed.
  
  
TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   Backend Methods
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="cairoBackend_getLayoutContext">**`         cairoBackend:getLayoutContext()
  `**</a>
  
  Gets a Cairo context that can be used for rendering off screen surfaces or for
  layout purposes, e.g. obtaining text extents. This context may be used
  outside or within [exposure event processing](#event_EXPOSE)

<!-- ---------------------------------------------------------------------------------------- -->
##   Event Processing
<!-- ---------------------------------------------------------------------------------------- -->

  Events for a view object are processed by the event handling function that
  was set as [*eventFunc*](#newView_eventFunc) in [*world:newView()*](#world_newView).
  
  When an event for the view occurs, the event handling function is called with the optional
  context parameters given to [*eventFunc*](#newView_eventFunc) as first arguments. 
  After the context parameters, the view and then the event name followed by event specific 
  paramaters are given to the event handling function.
  
  The event handling function is called with the following possible events (event names 
  followed by event specific parameters):

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_CREATE">**`            "CREATE"
  `**</a>

  View create event.

  This event is sent when a view is realized before it is first displayed,
  with the graphics context entered.  This is typically used for setting up
  the graphics system, for example by loading OpenGL extensions.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_CONFIGURE">**`         "CONFIGURE", x, y, width, height
  `**</a>

  View resize or move event.

  A configure event is sent whenever the view is resized or moved.  When a
  configure event is received, the graphics context is active but not set up
  for drawing.  For example, it is valid to adjust the OpenGL viewport or
  otherwise configure the context, but not to draw anything.
  
  * *x, y*    - new position, parent-relative if the view is embedded into a parent view (i.e. 
                *parent* was given in [*world:newView()*](#world_newView)). Otherwise the position 
                is in absolute screen coordinates (the view is a top level window or popup in 
                these cases).
  * *width*, *height*   - new width and height
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_MAP">**`               "MAP"
  `**</a>

  View show event.

  This event is sent when a view is mapped to the screen and made visible.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_UNMAP">**`             "UNMAP"
  `**</a>

  View hide event.

  This event is sent when a view is unmapped from the screen and made
  invisible.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_EXPOSE">**`            "EXPOSE", x, y, width, height, count, isFirst
  `**</a>

  * *x*, *y*, *width*, *height* - position and size of the rectangle that should be redisplayed
                                  in view relative coordinates.
  * *count*                     - number of pending expose events that are belonging
                                  to the current exposure cycle. *count=0* if this is the last 
                                  expose event in the current cycle.
  * *isFirst*                   - flag with value *true* if this expose event is the first event of 
                                  the current exposure cycle.
  
  If the view was **not** created with [*dontMergeRects=true*](#newView_dontMergeRects), 
  *count* will always be *0* and *isFirst* will always be *true*.
  
  For the Cairo backend, clipping to the exposed area of the current exposure
  cylce is already set up in the cairo context that can be obtained by
  [view:getDrawContext()](#view_getDrawContext).
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_BUTTON_PRESS">**`      "BUTTON_PRESS", x, y, button, state
  `**</a>

  Mouse button press event.
  
  * *x*, *x* - view-relative position of mouse pointer
  * *button* - button number (1=left, 2=middle, 3=right button)
  * *state*  - key modifier state, bitwise OR of [key modifier flags](#lpugl_MOD_)
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_BUTTON_RELEASE">**`    "BUTTON_RELEASE", x, y, button, state
  `**</a>

  Mouse button release event.
  
  * *x*, *x* - view-relative position of mouse pointer
  * *button* - button number (1=left, 2=middle, 3=right button)
  * *state*  - key modifier state, bitwise OR of [key modifier flags](#lpugl_MOD_)
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_KEY_PRESS">**`         "KEY_PRESS", keyName, state, text
  `**</a>

  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_KEY_RELEASE">**`       "KEY_RELEASE", keyName, state, text
  `**</a>

  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_POINTER_IN">**`        "POINTER_IN", x, y
  `**</a>

  Mouse pointer enter event. This event is sent when the pointer enters the view.

  * *x*, *y*  - view-relative position of mouse pointer
  
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_POINTER_OUT">**`       "POINTER_OUT", x, y
  `**</a>

  Mouse pointer leave event. This event is sent when the pointer leaves the view.
  
  * *x*, *y*  - view-relative position of mouse pointer
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_MOTION">**`            "MOTION", x, y
  `**</a>

  Mouse pointer motion event.

  * *x*, *y*  - view-relative position of mouse pointer
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_SCROLL">**`            "SCROLL", dx, dy
  `**</a>

  Scroll event.

  The scroll distance is an arbitrary unit that corresponds to a single tick of a detented 
  mouse wheel. Some devices may send values < 1.0 to allow finer scrolling (not on X11).
  
  * *dx*  - horizontal scroll distance: a positive value indicates that the content of the view
            should be moved to the right (i.e. scroll left), a negative value indicates that 
            the content of the view should be moved to the left (i.e. scroll right).

  * *dy*  - vertical scroll distance: a positive value indicates that the content of the view 
            should be moved down (i.e. scroll up), a negative value indicates that the content 
            of the view should be moved up (i.e. scroll down).


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_FOCUS_IN">**`          "FOCUS_IN"
  `**</a>

  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_FOCUS_OUT">**`         "FOCUS_OUT"
  `**</a>

  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="event_CLOSE">**`             "CLOSE"
  `**</a>
  
  User requests to close the view. The view mustn't close. Instead the view can 
  initiate user interactions, e.g. for saving open documents or cancelling the close
  operation (by just ignoring the event). The view can finally be closed by calling
  method [*view:close()*](#view_close).

<!-- ---------------------------------------------------------------------------------------- -->


End of document.

<!-- ---------------------------------------------------------------------------------------- -->

[lpugl]:                    https://luarocks.org/modules/osch/lpugl
[lpugl_cairo]:              https://luarocks.org/modules/osch/lpugl_cairo
[lpugl_opengl]:             https://luarocks.org/modules/osch/lpugl_opengl
[mtmsg]:                    https://github.com/osch/lua-mtmsg#mtmsg
[light userdata]:           https://www.lua.org/manual/5.4/manual.html#2.1
