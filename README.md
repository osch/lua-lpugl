# lpugl
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Install](https://img.shields.io/badge/Install-LuaRocks-brightgreen.svg)](https://luarocks.org/modules/osch/lpugl)

LPugl is a minimal Lua-API for building GUIs. It is based on [Pugl] (*PlUgin Graphics Library*), 
a minimal portable API for embeddable GUIs.

LPugl provides only a very minimal API. See [lwtk] (*Lua Widget Toolkit*)
for implementing GUI widgets on top of LPugl.

Like Pugl, LPugl does only have explicit context and no static data, so it's
possible to have several instances within the same program and even within 
the same Lua main state. Therefore LPugl is especially suited for building 
GUIs for plugins or components within larger applications. For example 
see [LDPF-Examples]: here LPugl is used for the GUI of audio processing plugins 
with [DPF] (*DISTRHO Plugin Framework*).

#### Supported platforms: 
   * X11
   * Windows
   * Mac OS X 

#### Supported rendering backends: 
   * Cairo  (requires [OOCairo])
   * OpenGL

Thanks to the modular architecture of Pugl, more rendering backends could be
possible in the future. Different rendering backends can be combined in one
application and also in the same window by embedding different view objects.

#### LuaRocks modules:
   * **[lpugl]**        - platform specific base module
   * **[lpugl_cairo]**  - Cairo rendering backend module
   * **[lpugl_opengl]** - OpenGL rendering backend module


#### Further reading:
   * [Documentation](./doc/README.md#lpugl-documentation)
   * [Examples](./example/README.md#lpugl-examples)

## First Example

* Simple example for using the Cairo backend:

    ```lua
    local lpugl = require"lpugl_cairo"
    
    local world = lpugl.newWorld("Hello World App")
    local scale = world:getScreenScale()
    
    local view = world:newView 
    {
        title     = "Hello World Window",
        size      = {300*scale, 100*scale},
        resizable = true,
        
        eventFunc = function(view, event, ...)
            print(event, ...)
            if event == "EXPOSE" then
                local cairo = view:getDrawContext()
                local w, h  = view:getSize()
                cairo:set_source_rgb(0.9, 0.9, 0.9)
                cairo:rectangle(0, 0, w, h)
                cairo:fill()
                cairo:set_source_rgb(0, 0, 0)
                cairo:select_font_face("sans-serif", "normal", "normal")
                cairo:set_font_size(24*scale)
                local text = "Hello World!"
                local ext = cairo:text_extents(text)
                cairo:move_to((w - ext.width)/2, (h - ext.height)/2 + ext.height)
                cairo:show_text(text)
            elseif event == "CLOSE" then
                view:close()
            end
        end
    }
    view:show()
    
    while world:hasViews() do
        world:update()
    end
    ```

[Pugl]:                     https://drobilla.net/software/pugl
[DPF]:                      https://github.com/DISTRHO/DPF
[LDPF-Examples]:            https://github.com/LDPF/LDPF-Examples
[OOCairo]:                  https://luarocks.org/modules/osch/oocairo
[lwtk]:                     https://github.com/osch/lua-lwtk#lwtk---lua-widget-toolkit
[lpugl]:                    https://luarocks.org/modules/osch/lpugl
[lpugl_cairo]:              https://luarocks.org/modules/osch/lpugl_cairo
[lpugl_opengl]:             https://luarocks.org/modules/osch/lpugl_opengl

