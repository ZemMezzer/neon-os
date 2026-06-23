local Config = {}

Config.FONT_SCALE = 1
Config.FONT_W = 6 * Config.FONT_SCALE
Config.FONT_H = 8 * Config.FONT_SCALE
Config.LINE_H = Config.FONT_H + 3

Config.DEFAULT_NOTES_DIR = "0:/notes"
Config.MAX_UNDO = 40
Config.TAB_WIDTH = 4

function Config.make_layout(screen_width, screen_height)
    local header_h = 62
    local footer_h = 0
    local gutter_w = 44
    local text_x = gutter_w + 12
    local text_y = header_h + 12
    local rows = math.max(
        1,
        math.floor((screen_height - text_y - footer_h - 8) / Config.LINE_H)
    )
    local cols = math.max(
        1,
        math.floor((screen_width - text_x - 12) / Config.FONT_W)
    )

    return {
        screen_width = screen_width,
        screen_height = screen_height,
        header_h = header_h,
        footer_h = footer_h,
        gutter_w = gutter_w,
        text_x = text_x,
        text_y = text_y,
        rows = rows,
        cols = cols,
    }
end

return Config
