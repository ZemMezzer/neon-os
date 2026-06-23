local gfx = require("gfx")
local bitmap_ok, bitmap = pcall(require, "bitmap")

local Config = require("desktop_config")
local Util = require("desktop_util")
local Palette = require("ui.palette")

local Render = {}
local screen_w = gfx.width()
local screen_h = gfx.height()

local function grid_rows(top)
    local available_height = screen_h - top - Config.TRAY.height - 8
    local rows = math.floor(
        (available_height + Config.GRID.gap_y) /
        (Config.GRID.tile_h + Config.GRID.gap_y)
    )

    if rows < 1 then
        rows = 1
    end

    return rows
end

local function draw_background()
    local bands = {
        Palette.warm_charcoal,
        Palette.smoky_black,
        Palette.onyx,
        Palette.eerie_black,
        Palette.rich_black,
    }

    local band_h = math.floor(screen_h / #bands) + 1

    for index, color in ipairs(bands) do
        gfx.fill_rect(0, (index - 1) * band_h, screen_w, band_h, color)
    end

    for x = 0, screen_w, 64 do
        gfx.line(x, 0, x, screen_h, Palette.gunmetal)
    end

    for y = 0, screen_h, 64 do
        gfx.line(0, y, screen_w, y, Palette.gunmetal)
    end

    local logo_n = "N"
    local logo_eonos = "eonOS"
    local title = logo_n .. logo_eonos
    local scale = 15
    local logo_w = Util.text_width(title, scale)
    local logo_h = 8 * scale
    local tx = math.floor((screen_w - logo_w) / 2)
    local ty = math.floor((screen_h - logo_h) / 2) - 8
    local first_w = Util.text_width(logo_n, scale)

    gfx.text(tx + 3, ty + 4, logo_n, Palette.near_black, scale)
    gfx.text(
        tx + first_w + 3,
        ty + 4,
        logo_eonos,
        Palette.near_black,
        scale
    )
    gfx.text(tx, ty, logo_n, Palette.vermilion, scale)
    gfx.text(tx + first_w, ty, logo_eonos, Palette.white_smoke, scale)
end

local function draw_app_icon(x, y)
    gfx.fill_rect(x + 4, y + 5, 34, 28, Palette.steel_blue)
    gfx.rect(x + 4, y + 5, 34, 28, Palette.electric_blue)
    gfx.fill_rect(x + 9, y + 10, 24, 16, Palette.midnight_blue)
    gfx.fill_rect(x + 15, y + 35, 12, 3, Palette.electric_blue)
end

local function draw_package_icon(item, x, y)
    if item.bitmap ~= nil and item.bitmap_w > 0 and item.bitmap_h > 0 then
        local draw_x = x + math.floor(
            (Config.ICON_MAX_W - item.bitmap_w) / 2
        )
        local draw_y = y + math.floor(
            (Config.ICON_MAX_H - item.bitmap_h) / 2
        )
        local drawn = bitmap_ok and pcall(
            bitmap.draw,
            item.bitmap,
            draw_x,
            draw_y,
            1
        )

        if drawn then
            return
        end
    end

    draw_app_icon(x, y)
end

local function draw_tile(item, index, selected_index, x, y)
    local selected = index == selected_index

    if selected then
        gfx.fill_rect(
            x,
            y,
            Config.GRID.tile_w,
            Config.GRID.tile_h,
            Palette.charcoal
        )
        gfx.rect(
            x,
            y,
            Config.GRID.tile_w,
            Config.GRID.tile_h,
            Palette.davy_grey
        )
        gfx.rect(
            x - 2,
            y - 2,
            Config.GRID.tile_w + 4,
            Config.GRID.tile_h + 4,
            Palette.davy_grey
        )
    end

    draw_package_icon(
        item,
        x + math.floor((Config.GRID.tile_w - Config.ICON_MAX_W) / 2),
        y + 3
    )

    local title = Util.fit_text(item.title, Config.GRID.tile_w - 6, 1)
    Util.centered_text(
        gfx,
        x,
        y + 52,
        Config.GRID.tile_w,
        title,
        Palette.ice_blue,
        1
    )
end

local function draw_grid(items, selected, left, top)
    local rows = grid_rows(top)

    for index, item in ipairs(items) do
        local zero_index = index - 1
        local column = math.floor(zero_index / rows)
        local row = zero_index % rows
        local x = left + column * (Config.GRID.tile_w + Config.GRID.gap_x)
        local y = top + row * (Config.GRID.tile_h + Config.GRID.gap_y)

        draw_tile(item, index, selected, x, y)
    end
end

local function draw_tray()
    local tray_y = screen_h - Config.TRAY.height
    local x = screen_w - Config.TRAY.padding - 148
    local y = tray_y + Config.TRAY.padding
    local current_time = Util.safe_date("%H:%M", "--:--")
    local current_date = Util.safe_date("%d.%m.%Y", "01.01.1970")

    gfx.fill_rect(
        0,
        tray_y,
        screen_w,
        Config.TRAY.height,
        Palette.ink_black
    )
    gfx.line(0, tray_y, screen_w - 1, tray_y, Palette.dark_grey)
    gfx.fill_rect(
        x,
        y,
        148,
        Config.TRAY.height - Config.TRAY.padding * 2,
        Palette.jet_black
    )
    gfx.rect(
        x,
        y,
        148,
        Config.TRAY.height - Config.TRAY.padding * 2,
        Palette.dark_grey
    )
    gfx.text(x + 10, y + 5, current_time, Palette.alice_blue, 2)
    gfx.text(x + 11, y + 27, current_date, Palette.cadet_blue, 1)
end

local function draw_blurred_desktop(desktop)
    local rows = grid_rows(Config.GRID.desktop_top)

    draw_background()

    for index, _ in ipairs(desktop.icons) do
        local zero_index = index - 1
        local column = math.floor(zero_index / rows)
        local row = zero_index % rows
        local x = Config.GRID.left + column * (
            Config.GRID.tile_w + Config.GRID.gap_x
        )
        local y = Config.GRID.desktop_top + row * (
            Config.GRID.tile_h + Config.GRID.gap_y
        )

        gfx.fill_rect(
            x + 3,
            y + 3,
            Config.GRID.tile_w - 6,
            Config.GRID.tile_h - 6,
            Palette.coal_black
        )
        gfx.fill_rect(x + 20, y + 13, 24, 22, Palette.space_black)
        gfx.fill_rect(x + 11, y + 49, 42, 4, Palette.carbon_black)
    end
end

local function draw_desktop(desktop)
    draw_background()
    draw_grid(
        desktop.icons,
        desktop.desktop_selected,
        Config.GRID.left,
        Config.GRID.desktop_top
    )
    draw_tray()
end

local function draw_launcher(desktop)
    local panel_x = Config.LAUNCHER.left
    local panel_y = Config.LAUNCHER.top
    local panel_w = screen_w - Config.LAUNCHER.left - Config.LAUNCHER.right
    local panel_h = screen_h - Config.TRAY.height - Config.LAUNCHER.top -
        Config.LAUNCHER.bottom_gap

    draw_blurred_desktop(desktop)

    gfx.fill_rect(
        panel_x + 3,
        panel_y + 3,
        panel_w,
        panel_h,
        Palette.void_black
    )
    gfx.fill_rect(
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        Palette.obsidian
    )
    gfx.rect(
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        Palette.graphite
    )
    gfx.text(
        panel_x + Config.LAUNCHER.padding,
        panel_y + 8,
        "PACKAGES",
        Palette.alice_blue,
        1
    )

    draw_grid(
        desktop.packages,
        desktop.package_selected,
        panel_x + Config.LAUNCHER.padding,
        Config.GRID.launcher_top
    )
    draw_tray()
end

function Render.draw_current(desktop)
    if desktop.mode == "launcher" then
        draw_launcher(desktop)
    else
        draw_desktop(desktop)
    end
end

return Render
