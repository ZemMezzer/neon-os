local gfx = require("gfx")
local input = require("input")
local shell = require("shell")

local Config = require("desktop_config")
local Util = require("desktop_util")
local DesktopData = require("desktop_data")
local Render = require("desktop_render")

local Desktop = DesktopData.new(gfx.height())

local function open_item(item)
    if item == nil then
        return
    end

    local target = Util.shell_quote_argument(item.path)

    if target == nil then
        Desktop.status = "Invalid package path"
        return
    end

    Render.draw_current(Desktop)
    gfx.present()

    local ok, result = pcall(shell.exec, "open " .. target)

    Desktop.load_shortcuts()

    if Desktop.mode == "launcher" then
        Desktop.load_packages()
    end

    if not ok then
        Desktop.status = "Open failed"
    elseif result ~= 0 then
        Desktop.status = "Cannot open " .. item.title
    else
        Desktop.status = "Returned from " .. item.title
    end
end

Desktop.load_shortcuts()

while true do
    local key = input.poll()

    if key ~= nil then
        if Util.key_is(key, "left", Config.KEY.left) then
            Desktop.move_selection(-1, 0)
        elseif Util.key_is(key, "right", Config.KEY.right) then
            Desktop.move_selection(1, 0)
        elseif Util.key_is(key, "up", Config.KEY.up) then
            Desktop.move_selection(0, -1)
        elseif Util.key_is(key, "down", Config.KEY.down) then
            Desktop.move_selection(0, 1)
        elseif Util.key_is(key, "tab", 9) then
            Desktop.select_next()
        elseif Desktop.mode == "desktop" then
            if Util.key_is(key, "f5", Config.KEY.f5) then
                Desktop.open_launcher()
            elseif Util.key_is(key, "delete", Config.KEY.delete) then
                Desktop.remove_selected_shortcut()
            elseif Util.key_is(key, "enter", 10) or
                Util.key_is(key, "space", 32) then
                open_item(Desktop.current_selected())
            end
        else
            if Util.key_is(key, "f5", Config.KEY.f5) or
                Util.key_is(key, "escape", Config.KEY.escape) then
                Desktop.close_launcher()
            elseif Util.key_is(key, "enter", 10) then
                open_item(Desktop.current_selected())
            elseif Util.key_is(key, "space", 32) then
                Desktop.add_shortcut(Desktop.current_selected())
                Desktop.load_packages()
            end
        end
    end

    Render.draw_current(Desktop)
    gfx.present()
end
