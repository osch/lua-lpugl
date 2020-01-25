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
   * [World Methods](#world-methods)
        * [world:setDefaultBackend()](#world_setDefaultBackend)
        * world:getDefaultBackend()
       	* [world:id()](#world_id)
       	* [world:newView()](#world_newView)
       	* world:hasViews()
       	* world:viewList()
       	* world:awake()
       	* world:close()
   * [View Methods](#view-methods)

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
  also set a new Cairo backend as default backend for the created world object.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_cairo_newBackend">**`     lpugl.cairo.newBackend(world)
  `**</a>
  
  Creates a new Cairo backend object for the given world.
  
  * *world* - mandatory world object for the which the backend can be used.
  

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newWorld">**`      lpugl.opengl.newWorld(name)
  `**</a>
  
  This does the same as [*lpugl.newWorld()*](#lpugl_newWorld) but does
  also set a new OpenGL backend as default backend for the created world object.


<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lpugl_opengl_newBackend">**`    lpugl.opengl.newBackend(world)
  `**</a>
  
  Creates a new OpenGL backend object for the given world.
  
  * *world* - mandatory world object for the which the backend can be used.
  
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

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_id">**`                   world:id()
  `**</a>

  Returns the world objects's id as integer. This id is unique among all world objects
  for the whole process.

<!-- ---------------------------------------------------------------------------------------- -->

* <a id="world_newView">**`              world:newView(initArgs)`**

  Creates a new view object. View objects can be top level windows or can be embedded in other
  views, depending on the parameters given in *initArgs*.

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

* <a id="world_awake">**`                world:awake()
  `**</a>
  
  Can be invoked from concurrently running threads to trigger the invocation of
  the world's process function while running the event lopp on the gui thread
  (i.e. the thread where the world was created). 
  
  This can be used for signalling the gui thread to handle events that are to be
  obtained from application specific channels, for example interthread
  communication via [mtmsg].
  
  To use this feature you have to obtain the world's id by invoking
  [*world:id()*](#world_id) and transfer this integer id to the background thread. On
  the background thread  invoke [*lpugl.world(id)*](#lpugl_world) to obtain a 
  restricted access object on which the method *world:id()* can be invoked.

TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   View Methods
<!-- ---------------------------------------------------------------------------------------- -->

TODO

End of document.

<!-- ---------------------------------------------------------------------------------------- -->

[lpugl]:                    https://luarocks.org/modules/osch/lpugl
[lpugl.cairo]:              https://luarocks.org/modules/osch/lpugl.cairo
[lpugl.opengl]:             https://luarocks.org/modules/osch/lpugl.opengl
[mtmsg]:                    https://github.com/osch/lua-mtmsg#mtmsg
