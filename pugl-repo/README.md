This is a modified copy of Pugl for [LPugl](https://github.com/osch/lua-lpugl), 
a Lua-API based on Pugl.

For original Pugl see:
   * https://drobilla.net/software/pugl
   * https://gitlab.com/lv2/pugl
   * https://github.com/drobilla/pugl

Pugl
====

Pugl (PlUgin Graphics Library) is a minimal portable API for GUIs which is
suitable for use in plugins.  It works on X11, MacOS, and Windows, and
optionally supports OpenGL and Cairo graphics contexts.

Pugl is vaguely similar to libraries like GLUT and GLFW, but with some
distinguishing features:

 * Minimal in scope, providing only a thin interface to isolate
   platform-specific details from applications.

 * Zero dependencies, aside from standard system libraries.

 * Support for embedding in native windows, for example as a plugin or
   component within a larger application that is not based on Pugl.

 * Simple and extensible event-based API that makes dispatching in application
   or toolkit code easy with minimal boilerplate.

 * Suitable not only for continuously rendering applications like games, but
   also event-driven applications that only draw when necessary.

 * Explicit context and no static data whatsoever, so that several instances
   can be used within a single program at once.

 * Small, liberally licensed Free Software implementation that is suitable for
   vendoring and/or static linking.  Pugl can be installed as a library, or
   used by simply copying the headers into a project.

Stability
---------

Pugl is currently being developed towards a long-term stable API.  For the time
being, however, the API may break occasionally.  Please report any relevant
feedback, or file feature requests, so that we can ensure that the released API
is stable for as long as possible.

Distribution
------------

Pugl is designed for flexible distribution.  It can be used by simply including
the source code, or installed and linked against as a static or shared library.
Static linking or direct inclusion is a good idea for plugins that will be
distributed as binaries to avoid dependency problems.

If you are including the code, please use a submodule so that suitable changes
can be merged upstream to keep fragmentation to a minimum.

When installed, Pugl is split into different libraries to keep dependencies
minimal.  The core implementation is separate from graphics backends:

 * The core implementation for a particular platform is in one library:
   `pugl_x11`, `pugl_mac`, or `pugl_win`.  This does not depend on backends or
   their dependencies.

 * Backends for platforms are in separate libraries, which depend on the core:
   `pugl_x11_cairo`, `pugl_x11_gl`, `pugl_mac_cairo`, and so on.

Applications must link against the core and at least one backend.  Normally,
this can be achieved by simply depending on the package `pugl-gl-0` or
`pugl-cairo-0`.  Though it is possible to compile everything into a monolithic
library, distributions should retain this separation so that GL applications
don't depend on Cairo and its dependencies, or vice-versa.

Distributions are encouraged to include static libraries if possible so that
developers can build portable plugin binaries.

Testing
-------

There are a few unit tests included, but unfortunately manual testing is still
required.  The tests and example programs will be built if you pass the
`--test` option when configuring:

    ./waf configure --test

Then, after building, the unit tests can be run:

    ./waf
    ./waf test --gui-tests

Several example programs are included that serve as both manual tests and
demonstrations:

 * `pugl_embed_demo` shows a view embedded in another, and also tests
   requesting attention (which happens after 5 seconds), keyboard focus
   (switched by pressing tab), view moving (with the arrow keys), and view
   resizing (with the arrow keys while shift is held).  This program uses only
   very old OpenGL and should work on any system.

 * `pugl_window_demo` demonstrates multiple top-level windows.

 * `pugl_shader_demo` demonstrates using more modern OpenGL (version 3 or 4)
   where dynamic loading and shaders are required.  It can also be used to test
   performance by passing the number of rectangles to draw on the command line.

 * `pugl_cairo_demo` demonstrates using Cairo on top of the native windowing
   system (without OpenGL), and partial redrawing.

 * `pugl_print_events` is a utility that prints all received events to the
   console in a human readable format.

 * `pugl_cxx_demo` is a simple cube demo that uses the C++ API.

All example programs support several command line options to control various
behaviours, see the output of `--help` for details.  Please file an issue if
any of these programs do not work as expected on your system.

Documentation
-------------

The [API reference](https://lv2.gitlab.io/pugl/) for the latest master is
available online, and can also be built from the source code by configuring
with `--docs`.

 -- David Robillard <d@drobilla.net>
