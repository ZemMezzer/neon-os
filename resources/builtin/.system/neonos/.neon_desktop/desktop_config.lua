local Config = {}

Config.GRID = {
    left = 12,
    desktop_top = 16,
    launcher_top = 78,
    tile_w = 64,
    tile_h = 64,
    gap_x = 6,
    gap_y = 6,
}

Config.TRAY = {
    height = 56,
    padding = 7,
}

Config.LAUNCHER = {
    left = 20,
    top = 44,
    right = 20,
    bottom_gap = 12,
    padding = 14,
    header_height = 24,
}

Config.DESKTOP_DIR = "0:/.desktop"
Config.SHORTCUT_SUFFIX = ".shortcut"
Config.ICON_MAX_W = 44
Config.ICON_MAX_H = 44

Config.KEY = {
    left = 0x100,
    right = 0x101,
    up = 0x102,
    down = 0x103,
    delete = 0x106,
    escape = 0x10A,
    f5 = 0x124,
}

return Config
