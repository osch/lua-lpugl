local lpugl   = require"lpugl_cairo"
local oocairo = require"oocairo"

local floor = math.floor
local ceil  = math.ceil
local abs   = math.abs
local sqrt  = math.sqrt

local unpack = table.unpack or unpack -- for Lua 5.1

----------------------------------------------------------------------------------------------

local REFRESH_PERIOD         = 0.015
local CELL_SIZE              = 50
local SPEED                  = 25
local DRAW_CHANGED_ONLY      = true
local REDISPLAY_CHANGED_ONLY = true
local DONT_MERGE_RECTS       = true
local STATIC_BACKGROUND      = true

----------------------------------------------------------------------------------------------

math.randomseed(os.time())

local world = lpugl.newWorld("example05.lua")
local scale = world:getScreenScale()

local cellWidth, cellHeight = CELL_SIZE*scale, CELL_SIZE*scale
local R = cellWidth*0.4
local speed = SPEED*scale

local initialWidth, initialHeight = 12 * cellWidth, 10 * cellHeight

----------------------------------------------------------------------------------------------

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

    drawBall = function(cairo, x, y, r, gradientIndex)
        drawBallImpl(cairo, x, y, r,
                     unpack(gradientTable[gradientIndex]))
    end
end

----------------------------------------------------------------------------------------------

local objects = {}
local active = {}
local activeCount = 0


local function objects_process(view)
    local t0 = world:getTime()
    for obj,_ in pairs(active) do
        local dt = t0 - obj.t
        if dt > 0 then
            if obj.inflating then
                local r = obj.r + speed * dt
                if r > R then
                    obj.inflating = false
                    obj.r = R - (r - R)
                    if obj.r < 0 then
                        obj.r = R
                    end
                else
                    obj.r = r
                end
            else
                local r = obj.r - speed * dt
                if r < 0 then
                    obj.inflating = true
                    obj.r = -r
                    if obj.r > R then
                        obj.r = 0
                    end
                else
                    obj.r = r
                end
            end
            if REDISPLAY_CHANGED_ONLY then
                view:postRedisplay((obj.j-1) * cellWidth, (obj.i-1) * cellHeight, cellWidth, cellHeight)
            end
            obj.t = t0
        end
    end
    if not REDISPLAY_CHANGED_ONLY and activeCount > 0 then
        view:postRedisplay()
    end
end

local function getObject(i, j)
    local objs = objects[i]; if not objs then objs = {}; objects[i] = objs end
    local obj  = objs[j]; 
    if not obj then 
        obj = { i = i, j = j, r = R, color = math.random(1,3) };
        objs[j] = obj 
    end
    return obj
end

local function objects_mark(rx, ry, rw, rh)
    local i0 = floor(ry / cellHeight) + 1
    local j0 = floor(rx / cellWidth) + 1
    local y = (i0-1) * cellHeight
    local i = i0
    while y < ry + rh do
        local j = j0
        local x = (j0-1) * cellWidth
        while x < rx + rw do
            getObject(i, j).mustDraw = true
            j = j + 1
            x = x + cellWidth
        end
        i = i + 1
        y = y + cellHeight
    end
end

local drawSum   = 0
local drawCount = 0

local function objects_draw(ctx, w, h)
    local i, y = 1, 0
    local drawn = 0
    while y < h do
        local j, x = 1, 0
        while x < w do
            local obj = getObject(i, j)
            if not DRAW_CHANGED_ONLY or obj.mustDraw then
                drawBall(ctx, x + cellWidth/2, y + cellWidth/2, obj.r, obj.color)
                obj.mustDraw = false
                drawn = drawn + 1
            end
            j = j + 1
            x = x + cellWidth
        end
        i = i + 1
        y = y + cellHeight
    end
    drawSum = drawSum + drawn
    drawCount = drawCount + 1
end

local function objects_clicked(bx, by, buttonCounter)
    local i = floor(by / cellHeight) + 1
    local j = floor(bx / cellWidth) + 1
    local obj = getObject(i, j)
    if obj then
        local y0 = (i-1) * cellHeight + cellHeight/2
        local x0 = (j-1) * cellWidth  + cellWidth/2
        local d = math.sqrt((bx-x0)^2+(by-y0)^2)
        if d <= R and (not obj.buttonCounter or buttonCounter > obj.buttonCounter) then
            obj.buttonCounter = buttonCounter
            if not obj.active then
                obj.active = true
                obj.t = world:getTime()
                obj.inflating = false
                active[obj] = true
                activeCount = activeCount + 1
            else
                obj.active = false
                active[obj] = nil
                activeCount = activeCount -1
            end
        end
    end
end

----------------------------------------------------------------------------------------------

local lastDisplayTime = 0

local renderStartTime = nil
local renderTime  = 0
local renderCount = 0

local lastRender  = world:getTime()
local frameTime   = 0
local frameCount  = 0

local processTime = 0
local processCount = 0

local blinkingSum = 0
local blinkingCount = 0

local rects = { n = 0 }


local buttonPressed = false
local buttonCounter = 0

local view = world:newView
{
    title          = "example05",
    size           = {initialWidth, initialHeight},
    resizable      = true,
    dontMergeRects = DONT_MERGE_RECTS,
    
    eventFunc = function(view, event, ...)
        if event == "CONFIGURE" then
        elseif event == "EXPOSE" then
            local x, y, w, h, count = ...
            local ctx = view:getDrawContext()
            if DRAW_CHANGED_ONLY then
                objects_mark(x, y, w, h)
            end
            if count == 0 then
                local startTime = world:getTime()
                frameTime = frameTime + startTime - lastRender
                frameCount = frameCount + 1
                lastRender = startTime
        
                local w, h  = view:getSize()
                
                if STATIC_BACKGROUND then
                    local r = (w > h) and w/2 or h/2
                    local pattern = oocairo.pattern_create_radial(w/2, h/2, 0,
                                                                  w/2, h/2, r)
                    pattern:add_color_stop_rgb(0, 1, 1, 1)
                    pattern:add_color_stop_rgb(0.9, 0.6, 0.6, 0.6)
                    pattern:add_color_stop_rgb(1, 0.5, 0.5, 0.5)
                    ctx:set_source(pattern)
                else
                    local c = 0.8 + 0.1*math.sin(startTime*5)
                    ctx:set_source_rgb(c, c, c)
                end
                ctx:rectangle(0, 0, w, h)
                ctx:fill()
                
                objects_draw(ctx, w, h)
                
                renderStartTime = startTime
                world:setNextProcessTime(0)
            end
        elseif event == "BUTTON_PRESS" or event == "MOTION" then
            local bx, by, bn = ...
            if event == "BUTTON_PRESS" and bn == 1  then
                buttonPressed = true
                buttonCounter = buttonCounter + 1
            end
            if buttonPressed then
                objects_clicked(bx, by, buttonCounter)
                if activeCount > 0 then
                    world:awake()
                end
            end
        elseif event == "BUTTON_RELEASE" then
            local bx, by, bn = ...
            if bn == 1 then
                buttonPressed = false
            end
        elseif event == "CLOSE" then
            view:close()
        end
    end
}

view:show()

local lastP = world:getTime()


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
            objects_process(view)
            processTime  = processTime + world:getTime() - startTime
            processCount = processCount + 1
            
            if activeCount > 0 then
                world:setNextProcessTime(REFRESH_PERIOD)
            end
            blinkingSum   = blinkingSum + activeCount
            blinkingCount = blinkingCount + 1
        else
            if activeCount > 0 then
                world:setNextProcessTime(lastDisplayTime + REFRESH_PERIOD - now)
            end
        end

        if startTime > lastP + 2 then
            print(string.format("render: %5.1fms, process: %5.1fms, period: %5.1fms, blinking: %5.1f, drawn: %5.1f", 
                                renderCount > 0 and renderTime/renderCount * 1000 or -1,
                                processCount > 0 and processTime/processCount * 1000 or -1,
                                frameCount > 0 and frameTime/frameCount * 1000 or -1,
                                blinkingCount > 0 and blinkingSum/blinkingCount or -1,
                                drawCount > 0 and drawSum/drawCount or -1))
            renderTime    = 0
            renderCount   = 0
            processTime   = 0
            processCount  = 0
            frameTime     = 0
            frameCount    = 0
            blinkingSum   = 0
            blinkingCount = 0
            drawSum       = 0
            drawCount     = 0
            lastP = startTime
        end
    end
    
end)
world:setNextProcessTime(0)

while world:hasViews() do
    world:update()
end
----------------------------------------------------------------------------------------------
