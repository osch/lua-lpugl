local lpugl   = require"lpugl_cairo"
local oocairo = require"oocairo"

local floor = math.floor
local ceil  = math.ceil
local abs   = math.abs
local sqrt  = math.sqrt

local unpack = table.unpack or unpack -- for Lua 5.1

math.randomseed(os.time())

local world = lpugl.newWorld("example03.lua")
local scale = world:getScreenScale()

local fillCache
local drawBall
do
    local function drawBallImpl(cairo, x, y, r, 
                                r1, g1, b1, 
                                r2, g2, b2)
        local alpha   = 0.7
        local d       = r*0.75
        local pattern = oocairo.pattern_create_radial(x - d, y - d, r - d, 
                                                      x - d, y - d, 2*r)
        pattern:add_color_stop_rgba(0, r1, g1, b1, alpha)
        pattern:add_color_stop_rgba(1, r2, g2, b2, alpha)
        cairo:set_source(pattern)
        cairo:arc(x, y, r, 0, math.pi * 2)
        cairo:fill();
    end
    
    local gradientTable = {
        { 1.0, 0.8, 0.8, 
          1.0, 0.0, 0.0 },
    
        { 0.7, 1.0, 0.7, 
          0.0, 1.0, 0.0 },
                      
        { 0.7, 0.7, 1.0,
          0.0, 0.0, 1.0 },
    }
    
    local cache     = {}
    local use_cache = true
    local padding   = 10 
    
    fillCache = function(gradientIndex, r)
        local c2 = cache[gradientIndex]; if not c2 then c2 = {}; cache[gradientIndex] = c2 end
        if not c2[r] then
            local layoutTarget = world:getDefaultBackend():getLayoutContext():get_target()
            local surface = oocairo.surface_create_similar(layoutTarget, "color-alpha", 2*(r+padding), 2*(r+padding))
            local cairo = oocairo.context_create(surface)
            drawBallImpl(cairo, r + padding, r + padding, r,
                         unpack(gradientTable[gradientIndex]))
            c2[r] = surface
        end
    end
    
    local bug_workaround = true  -- enlarge surface and clip to prevent strange border artefacts when  
                                 -- painting the surface on non integer coordinates (bug in cairo?)

    drawBall = function(cairo, x, y, r, gradientIndex)
        if use_cache then
            local cached = cache[gradientIndex][r]
            if bug_workaround then
                cairo:save()
                cairo:rectangle(floor(x - r - padding/2), floor(y - r - padding/2), ceil(2*r+padding), ceil(2*r+padding))
                cairo:clip()
            end
            cairo:set_source(cached, x - r - padding, y - r - padding)
            cairo:paint()
            if bug_workaround then
                cairo:restore()
            end
        else
            drawBallImpl(cairo, x, y, r,
                         unpack(gradientTable[gradientIndex]))
        end
    end
end

local initialWidth, initialHeight = 640*scale, 480*scale

local objects_draw
local objects_push
local objects_process
do
    local objects = {}
    
    for i = 1, 500 do
        local obj = {}
        objects[i] = obj
        obj.r  = math.random(floor(10*scale), floor(30*scale))
        obj.x  = math.random(obj.r, floor(initialWidth  - 2 * obj.r))
        obj.y  = math.random(obj.r, floor(initialHeight - 2 * obj.r))
        obj.g  = math.random(1, 3)
        obj.dx = 5 * math.random() / 20 * scale
        obj.dy = 5 * math.random() / 20 * scale
        fillCache(obj.g, obj.r)
    end


    objects_draw = function(cairo, w, h)
        for i = 1, #objects do
            local obj = objects[i]
            drawBall(cairo, obj.x, obj.y, obj.r, obj.g)
        end
    end
    
    objects_push = function(bx, by)
        for i = 1, #objects do
            local obj = objects[i]
            local x, y = obj.x, obj.y
            local bdx, bdy = x - bx, y - by
            local bd = sqrt(bdx^2 + bdy^2)/scale
            local a = (bd + 0.01)^-1.5 
            obj.dx = obj.dx + a * bdx
            obj.dy = obj.dy + a * bdy
        end
    end

    local lastT = world:getTime()

    objects_process = function(w, h)
        local currT = world:getTime()
        local dt = (currT - lastT) * 1000

        for i = 1, #objects do
            local obj = objects[i]
            local x, y = obj.x, obj.y
            x = x + obj.dx * dt
            if x + obj.r > w then
                x = w - obj.r
                obj.dx = -obj.dx
            elseif x - obj.r < 0 then
                x = obj.r
                obj.dx = -obj.dx
            end
            y = y + obj.dy * dt
            if y + obj.r > h then
                y = h - obj.r
                obj.dy = -obj.dy
            elseif y - obj.r < 0 then
                y = obj.r
                obj.dy = -obj.dy
            end
            obj.x, obj.y = x, y

            obj.dx = obj.dx * (1 - dt * 0.0001)
            obj.dy = obj.dy * (1 - dt * 0.0001)
        end
        lastT = currT
    end
end

local view = world:newView {
    title     = "example03",
    resizable = true
}
view:setSize(initialWidth, initialHeight)

local lastDisplayTime = 0

local renderStartTime = nil
local renderTime  = 0
local renderCount = 0

local lastRender  = 0
local frameTime   = 0
local frameCount  = 0

local processTime = 0
local processCount = 0
    


view:setEventFunc(function(event, ...)
    if event == "EXPOSE" then
        local startTime = world:getTime()
        frameTime = frameTime + startTime - lastRender
        frameCount = frameCount + 1
        lastRender = startTime

        local cairo = view:getDrawContext()
        local w, h  = view:getSize()
        
        cairo:set_source_rgb(1.0, 1.0, 1.0)
        cairo:rectangle(0, 0, w, h)
        cairo:fill()
        
        objects_draw(cairo, w, h)
        
        renderStartTime = startTime
        world:setNextProcessTime(0)

    elseif event == "BUTTON_PRESS" then
        local bx, by, bn = ...
        if bn == 1 then
            objects_push(bx, by)
        end
    elseif event == "CLOSE" then
        view:close()
    end
end)
view:show()

local lastP = world:getTime()

local REFRESH_PERIOD = 0.015

world:setProcessFunc(function()
    if not view:isClosed() then
        local startTime = world:getTime()

        if renderStartTime then
            renderTime  = renderTime + (startTime - renderStartTime)
            renderCount = renderCount + 1
            lastDisplayTime = renderStartTime
            renderStartTime = nil
        end

        local now = world:getTime()
        if now >= lastDisplayTime + REFRESH_PERIOD then
            objects_process(view:getSize())
            processTime  = processTime + world:getTime() - startTime
            processCount = processCount + 1
            
            view:postRedisplay()
            world:setNextProcessTime(REFRESH_PERIOD)
        else
            world:setNextProcessTime(lastDisplayTime + REFRESH_PERIOD - now)
        end

        if startTime > lastP + 2 and renderCount > 0 and processCount > 0 then
            print(string.format("render: %5.1fms, process: %5.1fms, period: %5.1fms", 
                                renderTime/renderCount * 1000,
                                processTime/processCount * 1000,
                                frameTime/frameCount * 1000))
            renderTime  = 0
            renderCount = 0
            processTime  = 0
            processCount = 0
            frameTime    = 0
            frameCount   = 0
            lastP = startTime
        end
    end
    
end)
world:setNextProcessTime(0)

while world:hasViews() do
    world:update()
end
