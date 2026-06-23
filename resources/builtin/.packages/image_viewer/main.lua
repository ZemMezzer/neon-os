local gfx = require("gfx")
local shell = require("shell")
local buffer = require("buffer")
local bitmap = require("bitmap")

local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")
local MessageBox = require("ui.modules.message_box")

local Keys = InputEvent.Keys
local args = { ... }

local HEADER_HEIGHT = 36
local MARGIN = 10
local MAX_SCALE = 8
local PAN_STEP = 24

local running = true
local image = nil
local image_path = ""
local image_width = 0
local image_height = 0
local scale = 1
local pan_x = 0
local pan_y = 0
local redraw = true

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end
 
local function normalize_path(path)
    path = tostring(path or ""):gsub("\\", "/")

    if path == "" then
        return ""
    end

    if path:sub(1, 2) == "./" then
        path = path:sub(3)
    end

    if path:sub(1, 2) == "0:" then
        if path:sub(3, 3) ~= "/" then
            path = "0:/" .. path:sub(3)
        end
    elseif path:sub(1, 1) == "/" then
        path = "0:" .. path
    else
        path = "0:/" .. path
    end

    while #path > 3 and path:sub(-1) == "/" do
        path = path:sub(1, -2)
    end

    return path
end

local function display_path(path)
    path = normalize_path(path)

    if path:sub(1, 3) == "0:/" then
        return "/" .. path:sub(4)
    end

    return path
end

local function shell_quote(value)
    value = tostring(value or "")

    if value:find('"', 1, true) then
        return nil
    end

    return '"' .. value .. '"'
end

local function show_error(message)
    local box

    box = MessageBox.new(
        "IMAGE VIEWER",
        tostring(message or "Unknown error"),
        {
            {
                "Close",
                function(self)
                    Canvas.remove(self)
                    running = false
                    redraw = true
                end,
            },
        }
    )

    Canvas.add(box)
    redraw = true
end

local function choose_image_path()
    local command = "open " .. shell_quote("explorer") ..
        " --pick-file " .. shell_quote("0:/")
    local ok
    local result
    local selected
    local buffer_error

    buffer.clear("explorer.pick_result")
    gfx.clear(Palette.rich_black)
    gfx.present()

    ok, result = pcall(shell.exec, command)

    if not ok then
        return nil, "Cannot start Explorer: " .. tostring(result)
    end

    if result ~= 0 then
        return nil, nil
    end

    selected, buffer_error = buffer.take("explorer.pick_result")

    if selected == nil then
        if buffer_error ~= nil and buffer_error ~= "" then
            return nil, "Explorer did not return a file: " .. tostring(buffer_error)
        end

        return nil, nil
    end

    return normalize_path(selected), nil
end

local function load_image(path)
    local loaded
    local error_message

    loaded, error_message = bitmap.load(path)

    if loaded == nil then
        return false, error_message or "Cannot load image"
    end

    image = loaded
    image_path = path
    image_width, image_height = image:size()
    scale = 1
    pan_x = 0
    pan_y = 0
    redraw = true
    return true
end

local function image_origin()
    local screen_w = gfx.width()
    local screen_h = gfx.height()
    local viewport_x = MARGIN
    local viewport_y = HEADER_HEIGHT + MARGIN
    local viewport_w = math.max(1, screen_w - MARGIN * 2)
    local viewport_h = math.max(1, screen_h - viewport_y - MARGIN)
    local draw_w = image_width * scale
    local draw_h = image_height * scale
    local base_x = viewport_x + math.floor((viewport_w - draw_w) / 2)
    local base_y = viewport_y + math.floor((viewport_h - draw_h) / 2)
    local min_x = viewport_x + viewport_w - draw_w
    local min_y = viewport_y + viewport_h - draw_h
    local max_x = viewport_x
    local max_y = viewport_y
    local x
    local y

    if draw_w <= viewport_w then
        pan_x = 0
        x = base_x
    else
        pan_x = clamp(pan_x, min_x - base_x, max_x - base_x)
        x = base_x + pan_x
    end

    if draw_h <= viewport_h then
        pan_y = 0
        y = base_y
    else
        pan_y = clamp(pan_y, min_y - base_y, max_y - base_y)
        y = base_y + pan_y
    end

    return x, y, draw_w, draw_h
end

local function draw_header()
    local screen_w = gfx.width()
    local title = "IMAGE VIEWER"

    gfx.fill_rect(0, 0, screen_w, HEADER_HEIGHT, Palette.onyx)
    gfx.line(
        0,
        HEADER_HEIGHT - 1,
        screen_w - 1,
        HEADER_HEIGHT - 1,
        Palette.dark_grey
    )
    gfx.text(MARGIN, 10, "I", Palette.vermilion, 2)
    gfx.text(MARGIN + 12, 10, title:sub(2), Palette.white_smoke, 2)
end

local function draw_image()
    local x, y, draw_w, draw_h = image_origin()

    gfx.fill_rect(
        x - 1,
        y - 1,
        draw_w + 2,
        draw_h + 2,
        Palette.graphite
    )
    gfx.fill_rect(x, y, draw_w, draw_h, Palette.space_black)
    bitmap.draw(image, x, y, scale)
end

local function draw()
    gfx.clear(Palette.rich_black)

    if image ~= nil then
        draw_header()
        draw_image()
    end

    Canvas.render()
end

local function move_image(dx, dy)
    if image == nil then
        return
    end

    pan_x = pan_x + dx * PAN_STEP
    pan_y = pan_y + dy * PAN_STEP
    redraw = true
end

local function reset_view()
    scale = 1
    pan_x = 0
    pan_y = 0
    redraw = true
end

local function handle_key(key)
    if key == Keys.ESCAPE or key == Keys.Q then
        running = false
    elseif key == Keys.LEFT then
        move_image(1, 0)
    elseif key == Keys.RIGHT then
        move_image(-1, 0)
    elseif key == Keys.UP then
        move_image(0, 1)
    elseif key == Keys.DOWN then
        move_image(0, -1)
    elseif key == Keys.HOME or key == string.byte("0") then
        reset_view()
    elseif key == string.byte("+") or key == string.byte("=") then
        scale = math.min(MAX_SCALE, scale + 1)
        redraw = true
    elseif key == string.byte("-") then
        scale = math.max(1, scale - 1)
        redraw = true
    end
end

Canvas.clear()

local requested_path = normalize_path(args[1])

if requested_path == "" then
    local selected_path
    local picker_error

    selected_path, picker_error = choose_image_path()

    if selected_path == nil then
        if picker_error ~= nil then
            show_error(picker_error)
        else
            running = false
        end
    else
        requested_path = selected_path
    end
end

if running and requested_path ~= "" then
    local loaded
    local load_error

    loaded, load_error = load_image(requested_path)

    if not loaded then
        show_error(
            "Cannot open " .. display_path(requested_path) .. ": " ..
            tostring(load_error)
        )
    end
end

while running do
    local event = InputEvent.poll_latest()

    if event ~= nil then
        if not Canvas.handle_event(event) and event.type == "key" then
            handle_key(event.key)
        end
    end

    if redraw or Canvas.is_dirty() then
        draw()
        redraw = false
    end

    gfx.present()
end

Canvas.clear()
gfx.clear(Palette.rich_black)
gfx.present()
