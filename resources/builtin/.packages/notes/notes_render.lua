local gfx = require("gfx")

local Config = require("notes_config")
local Util = require("notes_util")
local Palette = require("ui.palette")

local Render = {}

local function draw_notes_logo(x, y, scale)
    gfx.text(x, y, "N", Palette.vermilion, scale)
    gfx.text(x + 6 * scale, y, "OTES", Palette.white_smoke, scale)
end

local function draw_selection_background(state, line_index, line, layout, y)
    if not state.has_selection() then
        return
    end

    local visual = 1

    for index = 1, #line do
        local char_w = state.char_width(line:sub(index, index))

        if state.position_selected(line_index, index) then
            local start = visual
            local finish = visual + char_w
            local visible_start = math.max(start, state.scroll_x)
            local visible_finish = math.min(finish, state.scroll_x + layout.cols)

            if visible_finish > visible_start then
                local x = layout.text_x +
                    (visible_start - state.scroll_x) * Config.FONT_W
                gfx.fill_rect(
                    x,
                    y,
                    (visible_finish - visible_start) * Config.FONT_W,
                    Config.FONT_H,
                    Palette.eerie_black
                )
            end
        end

        visual = visual + char_w
    end
end

function Render.draw_editor(state)
    local layout = state.layout()
    state.ensure_cursor_visible(layout)

    gfx.clear(Palette.eerie_black)
    gfx.fill_rect(
        0,
        0,
        layout.screen_width,
        layout.header_h,
        Palette.ink_black
    )
    gfx.fill_rect(
        0,
        layout.header_h - 1,
        layout.screen_width,
        1,
        Palette.dark_grey
    )

    draw_notes_logo(12, 12, 2)

    local title = Util.display_path(state.file_path)

    if state.dirty then
        title = title .. "  *"
    end

    gfx.text(12, 38, title, Palette.white_smoke, 1)

    local position = "LN " .. state.cursor_y .. "  COL " .. state.cursor_x
    gfx.text(
        layout.screen_width - (#position * Config.FONT_W) - 12,
        40,
        position,
        Palette.cadet_blue,
        1
    )

    for row = 0, layout.rows - 1 do
        local line_index = state.scroll_y + row
        local y = layout.text_y + row * Config.LINE_H

        if line_index <= #state.lines then
            local number = tostring(line_index)
            local number_x = layout.gutter_w - (#number * Config.FONT_W) - 8
            local line = state.lines[line_index] or ""

            gfx.text(number_x, y, number, Palette.cadet_blue, 1)
            gfx.fill_rect(layout.gutter_w, y, 1, Config.FONT_H, Palette.dark_grey)
            draw_selection_background(state, line_index, line, layout, y)

            gfx.text(
                layout.text_x,
                y,
                state.visible_line_text(line, state.scroll_x, layout.cols),
                Palette.white_smoke,
                1
            )
        end
    end

    local cursor_visual = state.visual_col_for_index(
        state.lines[state.cursor_y],
        state.cursor_x
    )
    local cursor_x = layout.text_x +
        (cursor_visual - state.scroll_x) * Config.FONT_W
    local cursor_y = layout.text_y +
        (state.cursor_y - state.scroll_y) * Config.LINE_H

    if cursor_x >= layout.text_x and
        cursor_x < layout.screen_width - 4 and
        cursor_y >= layout.text_y and
        cursor_y < layout.screen_height - layout.footer_h then
        gfx.fill_rect(cursor_x, cursor_y, 2, Config.FONT_H, Palette.alice_blue)
    end

end

return Render
