# lpugl Documentation

<!-- ---------------------------------------------------------------------------------------- -->
##   Contents
<!-- ---------------------------------------------------------------------------------------- -->
   * [Overview](#overview)
   * [Module Functions](#module-functions)
        * [lpugl.newWorld()](#lpugl_newWorld)
        * [lpugl.world()](#lpugl_world)
        * [lpugl.cairo.newWorld()](#lpugl_cairo_newWorld)
        * [lpugl.cairo.newBackend()](#lpugl_cairo_newBackend)
        * [lpugl.opengl.newWorld()](#lpugl_opengl_newWorld)
        * [lpugl.opengl.newBackend()](#lpugl_opengl_newBackend)
   * [Module Constants](#module-constants)
        * [lpugl.platform](#lpugl_platform)
   * [World Methods](#world-methods)
        * [world:setDefaultBackend()](#world_setDefaultBackend)
        * [world:getDefaultBackend()](#world_getDefaultBackend)
        * [world:id()](#world_id)
        * [world:newView()](#world_newView)
        * [world:hasViews()](#world_hasViews)
        * [world:setProcessFunc()](#world_setProcessFunc)
        * [world:setNextProcessTime()](#world_setNextProcessTime)
        * [world:awake()](#world_awake)
        * [world:getTime()](#world_getTime)
        * [world:setErrorFunc()](#world_setErrorFunc)
        * [world:setClipboard()](#world_setClipboard)
        * [world:hasClipboard()](#world_hasClipboard)
        * [world:close()](#world_close)
   * [View Methods](#view-methods)
        * [view:show()](#view_show)
        * [view:hide()](#view_hide)
        * [view:setEventFunc()](#view_setEventFunc)
        * [view:getLayoutContext()](#view_getLayoutContext)
        * [view:getDrawContext()](#view_getDrawContext)
        * [view:close()](#view_close)
   * [Backend Methods](#backend-methods)
        * [cairoBackend:getLayoutContext()](#cairoBackend_getLayoutContext)

<!-- ---------------------------------------------------------------------------------------- -->
##   Overview
<!-- ---------------------------------------------------------------------------------------- -->
   
LPugl provides three LuaRocks modules 
   * **[lpugl]**        - platform specific base module
   * **[lpugl.cairo]**  - Cairo rendering backend module
   * **[lpugl.opengl]** - OpenGL rendering backend module

Each of these modules provides a Lua package with the same name which can be 
loaded via *require()*.

The backend packages *lpugl.cairo* and *lpugl.opengl* automatically require
the base package *lpugl*. All members from the base package are available in
the backend packages, i.e if you intend to work only with one backend, it is
sufficient just to require the corresponding backend package, i.e.

```lua
local XXX = require("lpugl.cairo")
```
makes all symbols from the packages *lpugl* and *lpugl.cairo* visible under the name *XXX*.


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
  been created with *lpugl.newWorld()*, *lpugl.cairo.newWorld()* or *lpugl.opengl.newWorld()*.
  
  This function can be invoked from any concurrently running thread. The obtained
  restricted world access can only be used to invoke the [*world:awake()*](#world_awake)
  method.

  * *id* - mandatory integer, the world object's id that can be obtained
           by [*world:id()*](#world_id)


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_cairo_newWorld">**`       lpugl.cairo.newWorld(name)
  `**</a>
  
  This does the same as [*lpugl.newWorld()*](#lpugl_newWorld) but does
  also set a Cairo backend as default backend for the created world object.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_cairo_newBackend">**`     lpugl.cairo.newBackend(world)
  `**</a>
  
  Creates a new Cairo backend object for the given world.
  
  * *world* - mandatory world object for the which the backend can be used.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newWorld">**`      lpugl.opengl.newWorld(name)
  `**</a>
  
  This does the same as [*lpugl.newWorld()*](#lpugl_newWorld) but does
  also set an OpenGL backend as default backend for the created world object.


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newBackend">**`    lpugl.opengl.newBackend(world)
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

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_newView">**`              world:newView(initArgs)`**

  Creates a new view object. View objects can be top level windows or can be embedded in other
  views, depending on the parameters given in *initArgs*.

  A view object is owned by the Pugl world for which the view was created. Therefore
  the view object will only be garbage collected if the view is closed explicitly via
  [*view:close()*](#view_close) or if the world becomes garbage collected or is closed
  explicitly via [*world:close()*](#world_close)
  
  * *initArgs* - lua table with initial key-value parameters. Can be ommitted, if a
                 default backend was set via 
                 [*world:setDefaultBackend()*](#world_setDefaultBackend),
                 otherwise at least a backend must be given.
                 
  The parameter *initArgs* may contain the following parameters as key value pairs:
  
  * *backend*      - backend object to be used for the created world.
  * *title*        - title for the created window.
  * *resizable*    - *true* if the created window should be resizable.
  * *parent*       - optional parent view. If given the created view is embedded
                     into the parent view. Otherwise a top level window
                     is created.
  * *popupFor*     - TODO
  * *transientFor* - TODO
  
  The parameters *title* and *resizable* have no effect if *parent* or
  *popupFor* is given.

  Only one of the parameters *parent*, *popupFor* or *transientFor* can be set.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_hasViews">**`                world:hasViews()
  `**</a>
  
  Returns *true* if there are views belonging to this world that have not been closed.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_setProcessFunc">**`          world:setProcessFunc(func)
  `**</a>
  
  Sets the world's process function. The process function is called in the main event loop after 
  the time has elapsed that was given in [*world:setNextProcessTime()*](#world_setNextProcessTime) 
  or if [*world:awake()*](#world_awake) has been called.
  
  * *func*   -  a function that takes no parameters. This function is meant to do periodic 
                or timer triggered processing in the main event loop.
  
  On X11 platform the usage of a process function is equivalent to handle processing in the main
  event loop directly, e.g.
  
  ```lua
  while world:hasViews() do
      if world:pollEvents(timeoutSeconds) then
        world:dispatchEvents()
      end
      ... do some timer dependent processing here...
  end
  
  ```
  
  However Windows and Mac OS platforms have the concept of a *central event queue*
  (on Windows per thread, on Mac per application). On these platforms event
  dispatching can occur outside your own event loop (for example on Windows: event 
  dispatching occurs when a window is dragged or resized). 
  Also on these platforms calling *world:dispatchEvents()* will dispatch events belonging
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
             error object.
  
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

* <a id="world_close">**`                world:close()
  `**</a>
  
  Closes the Pugl world. Closes also all views belonging to this Pugl world.
  This method is also invoked if the last Lua reference to the Pugl world
  is lost and the  world object becomes garbage collected.
  
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

* <a id="view_setEventFunc">**`         view:setEventFunc(func)
  `**</a>
  
  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getLayoutContext">**`     view:getLayoutContext()
  `**</a>
  
  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_getDrawContext">**`       view:getDrawContext()
  `**</a>
  
  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="view_close">**`                view:close()
  `**</a>
  
  Closes the view. Once a view is closed, it cannot be re-opened. 
  
  
TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   Backend Methods
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="cairoBackend_getLayoutContext">**`         cairoBackend:getLayoutContext()
  `**</a>
  
TODO

End of document.

<!-- ---------------------------------------------------------------------------------------- -->

[lpugl]:                    https://luarocks.org/modules/osch/lpugl
[lpugl.cairo]:              https://luarocks.org/modules/osch/lpugl.cairo
[lpugl.opengl]:             https://luarocks.org/modules/osch/lpugl.opengl
[mtmsg]:                    https://github.com/osch/lua-mtmsg#mtmsg
