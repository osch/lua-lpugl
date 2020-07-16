local lpugl = require"lpugl_opengl"
local gl    = require"luagl"
local glu   = require"luaglu"

local world = lpugl.newWorld("OpenGL Example App")

local view = world:newView 
{
    title     = "OpenGL Example Window",
    size      = {300, 100},
    resizable = true,

    eventFunc = function(view, event, ...)
        print(event, ...)
        if event == "EXPOSE" then
            local w, h  = view:getSize()
            gl.Viewport(0, 0, w, h)
            gl.MatrixMode('PROJECTION')
            gl.LoadIdentity()
            glu.Perspective(80, w / h, 1, 5000)
            gl.MatrixMode('MODELVIEW')
            gl.Clear('COLOR_BUFFER_BIT,DEPTH_BUFFER_BIT')
            gl.LoadIdentity()
            gl.Translate(-1.5, 0, -6)
            gl.Begin('TRIANGLES')
              gl.Vertex( 0,  1, 0)
              gl.Vertex(-1, -1, 0)
              gl.Vertex( 1, -1, 0)
            gl.End()
            gl.Translate(3, 0, 0)
            gl.Begin('QUADS')
              gl.Vertex(-1,  1, 0)
              gl.Vertex( 1,  1, 0)
              gl.Vertex( 1, -1, 0)
              gl.Vertex(-1, -1, 0)
            gl.End()
        elseif event == "CLOSE" then
            view:close()
        end
    end
}

view:show()

while world:hasViews() do
    world:update()
end

