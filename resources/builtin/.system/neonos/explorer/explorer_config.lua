local Config = {}

Config.HIDDEN_PATHS = {
    ["0:/.system"] = true,
    ["0:/.packages"] = true,
    ["0:/.desktop"] = true,
}

Config.SCALE = 2
Config.CHAR_WIDTH = 6 * Config.SCALE
Config.CHAR_HEIGHT = 8 * Config.SCALE
Config.ROW_HEIGHT = 18
Config.PADDING = 10
Config.HEADER_HEIGHT = 70

function Config.make_layout(screen_width, screen_height)
    local list_top = Config.HEADER_HEIGHT + 8
    local list_bottom = screen_height - 8
    local page_size = math.max(
        1,
        math.floor((list_bottom - list_top + 1) / Config.ROW_HEIGHT)
    )

    return {
        screen_width = screen_width,
        screen_height = screen_height,
        list_top = list_top,
        list_bottom = list_bottom,
        page_size = page_size,
    }
end

return Config
