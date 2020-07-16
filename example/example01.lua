local lpugl = require"lpugl_cairo"

local world = lpugl.newWorld("Hello World App")

local view = world:newView 
{
    title     = "Hello World Window",
    size      = {300, 100},
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
            cairo:set_font_size(24)
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
