local lpugl = require"lpugl_opengl"
local nvg   = require"nvg"
local color = require"nvg.color"
 
local floor = math.floor
local ceil  = math.ceil
local abs   = math.abs
local sqrt  = math.sqrt

local unpack = table.unpack or unpack -- for Lua 5.1

math.randomseed(os.time())

local world = lpugl.newWorld("example04.lua")

local drawBall
do
    local function drawBallImpl(ctx, x, y, r, 
                                rgba1, 
                                rgba2)
        ctx:beginPath()

        local alpha   = 0.7
        local d       = r*0.75

        ctx:arc(x, y, r, 0, math.pi * 2)
        ctx.fillStyle = ctx:radialGradient(x - d, y - d, r - d, 2*r, rgba1, rgba2)
        ctx:fill()
    end

    local alpha   = 0.7

    local gradientTable = {
        { color.rgba(1.0, 0.8, 0.8, alpha),
          color.rgba(1.0, 0.0, 0.0, alpha) },
    
        { color.rgba(0.7, 1.0, 0.7, alpha), 
          color.rgba(0.0, 1.0, 0.0, alpha) },
                      
        { color.rgba(0.7, 0.7, 1.0, alpha),
          color.rgba(0.0, 0.0, 1.0, alpha) },
    }
    drawBall = function(ctx, x, y, r, gradientIndex)
        drawBallImpl(ctx, x, y, r,
                     unpack(gradientTable[gradientIndex]))
    end

end


local initialWidth, initialHeight = 640, 480

local objects_draw
local objects_push
local objects_process
do
    local objects = {}
    
    for i = 1, 500 do
        local obj = {}
        objects[i] = obj
        obj.r  = math.random(10, 30)
        obj.x  = math.random(obj.r, initialWidth  - 2 * obj.r)
        obj.y  = math.random(obj.r, initialHeight - 2 * obj.r)
        obj.g  = math.random(1, 3)
        obj.dx = 5 * math.random() / 20
        obj.dy = 5 * math.random() / 20
    end


    objects_draw = function(ctx, w, h)
        for i = 1, #objects do
            local obj = objects[i]
            drawBall(ctx, obj.x, obj.y, obj.r, obj.g)
        end
    end
    
    objects_push = function(bx, by)
        for i = 1, #objects do
            local obj = objects[i]
            local x, y = obj.x, obj.y
            local bdx, bdy = x - bx, y - by
            local bd = sqrt(bdx^2 + bdy^2)
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
    title     = "example04",
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
    
local ctx = nil

view:setEventFunc(function(event, ...)
    
    if event == "CREATE" then
        ctx = nvg.new("antialias")
    
    elseif event == "EXPOSE" then
        local startTime = world:getTime()
        frameTime = frameTime + startTime - lastRender
        frameCount = frameCount + 1
        lastRender = startTime

        local w, h  = view:getSize()
        ctx:beginFrame(w, h)
        ctx:clear("#ffffff")
        objects_draw(ctx, w, h)
        ctx:endFrame()

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
