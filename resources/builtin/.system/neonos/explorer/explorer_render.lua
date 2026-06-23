local gfx = require("gfx")
local fs = require("fs")

local Config = require("explorer_config")
local Util = require("explorer_util")
local Palette = require("ui.palette")

local Render = {}

local function text_at(x, y, value, color, max_chars)
    if max_chars ~= nil then
        value = Util.fit(value, max_chars)
    end

    gfx.text(
        x,
        y,
        tostring(value or ""),
        color or Palette.white_smoke,
        Config.SCALE
    )
end

local function draw_line(x, y, width, color)
    gfx.fill_rect(x, y, width, 1, color)
end

local function draw_folder_row_icon(x, y, selected)
    local main = selected and Palette.alice_blue or Palette.ice_blue
    local light = selected and Palette.alice_blue or Palette.white_smoke
    local dark = selected and Palette.vermilion or Palette.davy_grey

    gfx.fill_rect(x + 3, y + 3, 8, 3, dark)
    gfx.fill_rect(x + 1, y + 6, 14, 10, main)
    gfx.rect(x + 1, y + 6, 14, 10, dark)
    gfx.fill_rect(x + 3, y + 9, 10, 5, light)
end

local function draw_file_row_icon(x, y, selected)
    local paper = selected and Palette.alice_blue or Palette.white_smoke
    local fold = selected and Palette.vermilion or Palette.ice_blue
    local line = selected and Palette.vermilion or Palette.davy_grey

    gfx.fill_rect(x + 3, y + 1, 10, 15, paper)
    gfx.rect(x + 3, y + 1, 10, 15, line)
    gfx.fill_rect(x + 10, y + 2, 2, 3, fold)
    gfx.line(x + 5, y + 8, x + 11, y + 8, line)
    gfx.line(x + 5, y + 11, x + 11, y + 11, line)
end

local function draw_item_row(state, item, index, y)
    local layout = state.layout
    local selected = index == state.selected
    local foreground = Palette.white_smoke
    local suffix = ""
    local icon_x = Config.PADDING + 4
    local text_x = icon_x + 21

    if selected then
        gfx.fill_rect(
            Config.PADDING,
            y - 1,
            layout.screen_width - Config.PADDING * 2,
            Config.ROW_HEIGHT,
            Palette.charcoal
        )
        gfx.fill_rect(
            Config.PADDING,
            y - 1,
            2,
            Config.ROW_HEIGHT,
            Palette.vermilion
        )
        foreground = Palette.alice_blue
    end

    if item.isDir then
        if item.parent then
            foreground = selected and Palette.alice_blue or Palette.cadet_blue
        else
            suffix = "/"
        end
        draw_folder_row_icon(icon_x, y, selected)
    else
        suffix = "  " .. Util.format_size(item.size)
        draw_file_row_icon(icon_x, y, selected)
    end

    local suffix_width = #suffix * Config.CHAR_WIDTH
    local available_pixels = layout.screen_width - Config.PADDING -
        text_x - suffix_width
    local name_chars = math.max(1, math.floor(available_pixels / Config.CHAR_WIDTH))

    text_at(text_x, y, item.name, foreground, name_chars)

    if suffix ~= "" then
        text_at(
            layout.screen_width - Config.PADDING - suffix_width,
            y,
            suffix,
            foreground
        )
    end
end

function Render.draw_ui(state)
    local layout = state.layout
    local title = "Explorer"

    if state.picker_mode == "file" then
        title = "Explorer - Select file"
    elseif state.picker_mode == "folder" then
        title = "Explorer - Select folder"
    elseif state.picker_mode == "path" then
        title = "Explorer - Select file or folder"
    elseif state.target_mode then
        title = "Explorer - Select " .. state.target_mode.kind .. " target"
    end

    gfx.clear(Palette.rich_black)
    gfx.fill_rect(0, 0, layout.screen_width, Config.HEADER_HEIGHT, Palette.onyx)
    draw_line(0, Config.HEADER_HEIGHT - 1, layout.screen_width, Palette.dark_grey)

    local title_chars = math.floor(
        (layout.screen_width - Config.PADDING * 2) / Config.CHAR_WIDTH
    )
    local title_first = title:sub(1, 1)
    local title_rest = title:sub(2)

    text_at(Config.PADDING, 10, title_first, Palette.vermilion, 1)
    text_at(
        Config.PADDING + Config.CHAR_WIDTH,
        10,
        title_rest,
        Palette.white_smoke,
        title_chars - 1
    )
    text_at(
        Config.PADDING,
        30,
        Util.display_path(state.cwd),
        Palette.white_smoke,
        title_chars
    )

    local detail = state.status

    if not state.target_mode then
        local free_ok, free_space = pcall(fs.getFreeSpace, state.cwd)

        if free_ok then
            detail = detail .. " | free " .. Util.format_size(free_space)
        end
    end

    text_at(Config.PADDING, 50, detail, Palette.cadet_blue, title_chars)

    if #state.items == 0 then
        text_at(
            Config.PADDING + 4,
            layout.list_top + 6,
            "<empty folder>",
            Palette.cadet_blue
        )
    else
        for line = 0, layout.page_size - 1 do
            local index = state.offset + line

            if index > #state.items then
                break
            end

            draw_item_row(
                state,
                state.items[index],
                index,
                layout.list_top + line * Config.ROW_HEIGHT
            )
        end
    end

end

return Render
