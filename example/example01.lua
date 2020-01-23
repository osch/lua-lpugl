local lpugl = require"lpugl.cairo"

local world = lpugl.newWorld("Cairo Example App")

local view = world:newView {
    title     = "Cairo Example Window",
    resizable = true
}
view:setSize(300, 100)
view:setEventFunc(function(event, ...)
    print(event, ...)
    if event == "EXPOSE" then
        local x, y, w, h = ...
        local cairo  = view:getDrawContext()
        cairo:set_source_rgb(0.9, 0.9, 0.9)
        cairo:rectangle(x, y, w, h)
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
end)

view:show()

while world:hasViews() do
    world:pollEvents()
    world:dispatchEvents()
end

