local gfx = require("gfx")
local fs = require("fs")
local shell = require("shell")

local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")
local MessageBox = require("ui.modules.message_box")

local args = { ... }
local archive_path = ""
local running = true

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

local function quote(value)
    value = tostring(value or "")

    if value:find('"', 1, true) then
        return nil
    end

    return '"' .. value .. '"'
end

local function first_line(value)
    value = tostring(value or ""):gsub("\r", "")
    return value:match("([^\n]+)") or ""
end

local function render()
    gfx.clear(Palette.rich_black)
    Canvas.render()
    gfx.present()
end

local function close_application(box)
    Canvas.remove(box)
    running = false
end

local function show_result(label, message)
    local box

    box = MessageBox.new(label, message, {
        { "Close", close_application },
    })

    Canvas.add(box)
end

local install_package

local function ask_to_install()
    local box
    local name = file_name(archive_path)

    box = MessageBox.new(
        "PACKAGE INSTALLER",
        "Install package?\n" .. name,
        {
            {
                "Install",
                function(self)
                    Canvas.remove(self)
                    install_package()
                end,
            },
            { "Cancel", close_application },
        }
    )

    Canvas.add(box)
end

install_package = function()
    local quoted = quote(archive_path)

    if quoted == nil then
        show_result("INVALID PACKAGE PATH", archive_path)
        return
    end

    local progress = MessageBox.new(
        "PACKAGE INSTALLER",
        "Installing\n" .. file_name(archive_path)
    )

    Canvas.add(progress)
    render()

    local ok, status, output = pcall(
        shell.exec_capture,
        "package install " .. quoted
    )

    Canvas.remove(progress)

    if not ok then
        show_result("INSTALLER ERROR", tostring(status))
        return
    end

    if status == 0 then
        local detail = first_line(output)

        if detail == "" then
            detail = file_name(archive_path)
        end

        show_result("PACKAGE INSTALLED", detail)
        return
    end

    local detail = first_line(output)

    if detail == "" then
        detail = "Status " .. tostring(status)
    end

    show_result("INSTALL FAILED", detail)
end

local function begin()
    archive_path = normalize_path(args[1])

    if archive_path == "" then
        show_result(
            "NO PACKAGE FILE",
            "Explorer did not pass an archive path"
        )
        return
    end

    if not is_npkg(archive_path) then
        show_result("NOT A .NPKG FILE", file_name(archive_path))
        return
    end

    local exists_ok, exists = pcall(fs.exists, archive_path)
    local directory_ok, is_directory = pcall(fs.isDir, archive_path)

    if not exists_ok or not exists or (directory_ok and is_directory) then
        show_result("PACKAGE FILE NOT FOUND", archive_path)
        return
    end

    ask_to_install()
end

Canvas.clear()
begin()

while running do
    local event = InputEvent.poll()

    if event ~= nil then
        Canvas.handle_event(event)
    end

    render()
end

gfx.clear(Palette.rich_black)
gfx.present()
