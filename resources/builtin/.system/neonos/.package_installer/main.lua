local gfx = require("gfx")
local input = require("input")
local fs = require("fs")
local shell = require("shell")

local args = { ... }

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR = {
    bg = 0x00181718,
    panel = 0x00141415,
    border = 0x00333335,
    text = 0x00F4F4F4,
    muted = 0x00A8A8AA,
    accent = 0x00E02915,
    selected = 0x00242424,
    selected_border = 0x00505052,
    button = 0x001D1D1F,
}

local archive_path = tostring(args[1] or "")
local selected = 2
local state = "confirm"
local title = ""
local detail = ""
local running = true

local function key_is(key, name, numeric)
    return key == name or key == numeric
end

local function lower_key(key)
    if type(key) == "number" and key >= string.byte("A") and key <= string.byte("Z") then
        return key - string.byte("A") + string.byte("a")
    end

    return key
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

local function file_name(path)
    return tostring(path or ""):match("([^/]+)$") or ""
end

local function is_npkg(path)
    return string.lower(tostring(path or "")):sub(-5) == ".npkg"
end

local function fit_text(text, max_chars)
    text = tostring(text or "")

    if max_chars <= 0 then
        return ""
    end

    if #text <= max_chars then
        return text
    end

    if max_chars <= 3 then
        return text:sub(1, max_chars)
    end

    return text:sub(1, max_chars - 3) .. "..."
end

local function quote(value)
    value = tostring(value or "")

    if value:find('"', 1, true) then
        return nil
    end

    return '"' .. value .. '"'
end

local function first_line(text)
    text = tostring(text or ""):gsub("\r", "")
    return text:match("([^\n]+)") or ""
end

local function draw_button(x, y, width, label, active)
    local background = active and COLOR.selected or COLOR.button
    local border = active and COLOR.selected_border or COLOR.border
    local label_x = x + math.floor((width - #label * 6) / 2)

    gfx.fill_rect(x, y, width, 28, background)
    gfx.rect(x, y, width, 28, border)
    gfx.text(label_x, y + 10, label, active and COLOR.text or COLOR.muted, 1)
end

local function draw()
    local panel_w = math.min(500, SCREEN_W - 80)
    local panel_h = 178
    local panel_x = math.floor((SCREEN_W - panel_w) / 2)
    local panel_y = math.floor((SCREEN_H - panel_h) / 2)
    local max_chars = math.floor((panel_w - 32) / 6)

    gfx.clear(COLOR.bg)
    gfx.fill_rect(panel_x - 2, panel_y - 2, panel_w + 4, panel_h + 4, COLOR.border)
    gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR.panel)
    gfx.text(panel_x + 16, panel_y + 16, "P", COLOR.accent, 2)
    gfx.text(panel_x + 28, panel_y + 16, "ACKAGE INSTALLER", COLOR.text, 2)

    if state == "confirm" then
        local button_w = 96
        local gap = 12
        local buttons_w = button_w * 2 + gap
        local buttons_x = panel_x + math.floor((panel_w - buttons_w) / 2)

        gfx.text(panel_x + 16, panel_y + 56, "INSTALL PACKAGE?", COLOR.text, 1)
        gfx.text(
            panel_x + 16,
            panel_y + 78,
            fit_text(file_name(archive_path), max_chars),
            COLOR.text,
            1
        )
        gfx.text(
            panel_x + 16,
            panel_y + 98,
            fit_text(archive_path, max_chars),
            COLOR.muted,
            1
        )

        draw_button(buttons_x, panel_y + 132, button_w, "YES", selected == 1)
        draw_button(
            buttons_x + button_w + gap,
            panel_y + 132,
            button_w,
            "NO",
            selected == 2
        )
        return
    end

    gfx.text(panel_x + 16, panel_y + 58, title, state == "success" and COLOR.text or COLOR.accent, 1)
    gfx.text(
        panel_x + 16,
        panel_y + 82,
        fit_text(detail, max_chars),
        COLOR.muted,
        1
    )
    gfx.text(panel_x + 16, panel_y + 142, "ENTER OR ESC TO CLOSE", COLOR.muted, 1)
end

local function install()
    local quoted = quote(archive_path)
    local ok
    local result
    local output

    if quoted == nil then
        state = "result"
        title = "INVALID PACKAGE PATH"
        detail = archive_path
        return
    end

    draw()
    gfx.present()

    ok, result, output = pcall(
        shell.exec_capture,
        "package install " .. quoted
    )

    state = "result"

    if not ok then
        title = "INSTALLER ERROR"
        detail = tostring(result)
        return
    end

    if result == 0 then
        state = "success"
        title = "PACKAGE INSTALLED"
        detail = first_line(output)
    else
        title = "INSTALL FAILED: " .. tostring(result)
        detail = first_line(output)
    end
end

archive_path = normalize_path(archive_path)

if archive_path == "" then
    state = "result"
    title = "NO PACKAGE FILE"
    detail = "Explorer did not pass an archive path"
elseif not is_npkg(archive_path) then
    state = "result"
    title = "NOT A .NPKG FILE"
    detail = file_name(archive_path)
else
    local exists_ok, exists = pcall(fs.exists, archive_path)
    local dir_ok, is_dir = pcall(fs.isDir, archive_path)

    if not exists_ok or not exists or (dir_ok and is_dir) then
        state = "result"
        title = "PACKAGE FILE NOT FOUND"
        detail = archive_path
    end
end

while running do
    local key = input.poll()

    if key ~= nil then
        local control = lower_key(key)

        if state == "confirm" then
            if key_is(key, "left", input.LEFT) or key_is(key, "right", input.RIGHT) then
                selected = selected == 1 and 2 or 1
            elseif control == string.byte("y") or control == "y" then
                install()
            elseif control == string.byte("n") or control == "n" then
                running = false
            elseif key_is(key, "escape", input.ESCAPE) then
                running = false
            elseif key_is(key, "enter", input.ENTER) then
                if selected == 1 then
                    install()
                else
                    running = false
                end
            end
        elseif key_is(key, "enter", input.ENTER) or key_is(key, "escape", input.ESCAPE) then
            running = false
        end
    end

    draw()
    gfx.present()
end

gfx.clear(COLOR.bg)
gfx.present()
