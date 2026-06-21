local gfx = require("gfx")
local input = require("input")
local shell = require("shell")

local Desktop = {
    icons = {},
    tray_widgets = {},
    selected = 1
}

local screen_w = gfx.width()
local screen_h = gfx.height()

local COLOR = {
    bg_top = 0x00262523,
    bg_upper = 0x00212020,
    bg_mid = 0x001D1C1D,
    bg_lower = 0x00181718,
    bg_bottom = 0x00131315,

    grid = 0x002B2C2E,

    tile_selected = 0x00242424,
    tile_glow = 0x00505052,

    text = 0x00D5E5F4,
    text_dim = 0x0090A9C0,
    white = 0x00F3FBFF,
    neon = 0x0048D9FF,
    neon_dim = 0x00184B71,

    tray = 0x00141415,
    tray_border = 0x00333335,
    tray_widget = 0x001D1D1F,
}

-- Desktop grid. Icons begin at the upper-left corner.
local GRID = {
    left = 12,
    top = 16,
    tile_w = 64,
    tile_h = 64,
    gap_x = 6,
    gap_y = 6
}

local ICON_SCALE = 0.72

local function sp(value)
    return math.floor(value * ICON_SCALE + 0.5)
end

local function ss(value)
    return math.max(1, sp(value))
end

local ICON_BOX_W = sp(44)

-- Bottom tray. Future widgets are placed from right to left.
local TRAY = {
    height = 56,
    padding = 7,
    gap = 8
}

local KEY_LEFT = 0x100
local KEY_RIGHT = 0x101
local KEY_UP = 0x102
local KEY_DOWN = 0x103

local function safe_date(format, fallback)
    local ok, result = pcall(function()
        return os.date(format) 
    end)

    if ok and type(result) == "string" and #result > 0 then
        return result
    end

    return fallback
end

local function text_width(text, scale)
    return #text * 6 * scale
end

local function centered_text(x, y, width, text, color, scale)
    local tx = x + math.floor((width - text_width(text, scale)) / 2)
    gfx.text(tx, y, text, color, scale)
end

local function fit_text(text, max_width, scale)
    local max_chars = math.floor(max_width / (6 * scale))

    if max_chars < 1 then
        return ""
    end

    if #text <= max_chars then
        return text
    end

    if max_chars <= 3 then
        return string.sub(text, 1, max_chars)
    end

    return string.sub(text, 1, max_chars - 3) .. "..."
end

local function key_is(key, name, numeric_code)
    return key == name or key == numeric_code
end

local function key_is_letter(key, letter)
    return key == letter or key == string.byte(letter)
end

-- Number of icon rows before the next icon starts a new column.
local function grid_rows()
    local available_height = screen_h - GRID.top - TRAY.height - TRAY.gap
    local rows = math.floor(
        (available_height + GRID.gap_y) / (GRID.tile_h + GRID.gap_y)
    )

    if rows < 1 then
        rows = 1
    end

    return rows
end

local function grid_column_count()
    local count = #Desktop.icons
    local rows = grid_rows()

    if count == 0 then
        return 0
    end

    return math.floor((count + rows - 1) / rows)
end

function Desktop.add_icon(title, command, icon_kind)
    if type(title) ~= "string" or #title == 0 then
        return nil
    end

    if type(command) ~= "string" or #command == 0 then
        return nil
    end

    table.insert(Desktop.icons, {
        title = title,
        command = command,
        icon_kind = icon_kind or "app"
    })

    return #Desktop.icons
end

function Desktop.add_tray_widget(name, width, draw_function)
    if type(name) ~= "string" or #name == 0 then
        return nil
    end

    if type(width) ~= "number" or width <= 0 then
        return nil
    end

    if type(draw_function) ~= "function" then
        return nil
    end

    table.insert(Desktop.tray_widgets, {
        name = name,
        width = math.floor(width),
        draw = draw_function
    })

    return #Desktop.tray_widgets
end

function Desktop.icon_at(index)
    return Desktop.icons[index]
end

function Desktop.selected_icon()
    return Desktop.icons[Desktop.selected]
end

function Desktop.set_selected(index)
    local count = #Desktop.icons

    if count == 0 then
        Desktop.selected = 0
        return
    end

    if index < 1 then
        index = 1
    elseif index > count then
        index = count
    end

    Desktop.selected = index
end

function Desktop.select_next()
    local count = #Desktop.icons

    if count == 0 then
        return
    end

    Desktop.selected = Desktop.selected + 1

    if Desktop.selected > count then
        Desktop.selected = 1
    end
end

-- Navigation follows the visible, column-first layout.
function Desktop.move_selection(dx, dy)
    local count = #Desktop.icons

    if count == 0 then
        return
    end

    local rows = grid_rows()
    local columns = grid_column_count()
    local zero_index = Desktop.selected - 1
    local current_column = math.floor(zero_index / rows)
    local current_row = zero_index % rows
    local target = Desktop.selected

    if dy ~= 0 then
        local column_first = current_column * rows + 1
        local column_last = math.min(column_first + rows - 1, count)

        target = Desktop.selected + dy

        if target < column_first then
            target = column_last
        elseif target > column_last then
            target = column_first
        end
    elseif dx ~= 0 and columns > 0 then
        local target_column = current_column + dx

        if target_column < 0 then
            target_column = columns - 1
        elseif target_column >= columns then
            target_column = 0
        end

        target = target_column * rows + current_row + 1

        if target > count then
            target = count
        end
    end

    Desktop.set_selected(target)
end

local function draw_background()
    local bands = {
        COLOR.bg_top,
        COLOR.bg_upper,
        COLOR.bg_mid,
        COLOR.bg_lower,
        COLOR.bg_bottom
    }

    local band_h = math.floor(screen_h / #bands) + 1

    for index, color in ipairs(bands) do
        gfx.fill_rect(0, (index - 1) * band_h, screen_w, band_h, color)
    end

    for x = 0, screen_w, 64 do
        gfx.line(x, 0, x, screen_h, COLOR.grid)
    end

    for y = 0, screen_h, 64 do
        gfx.line(0, y, screen_w, y, COLOR.grid)
    end

    local logo_n = "N"
    local logo_eonos = "eonOS"
    local title = logo_n .. logo_eonos
    local scale = 15

    local logo_w = text_width(title, scale)
    local logo_h = 8 * scale

    local tx = math.floor((screen_w - logo_w) / 2)
    local ty = math.floor((screen_h - logo_h) / 2) - 8

    local first_w = text_width(logo_n, scale)

    gfx.text(tx + 3, ty + 4, logo_n, 0x00101012, scale)
    gfx.text(tx + first_w + 3, ty + 4, logo_eonos, 0x00101012, scale)

    gfx.text(tx, ty, logo_n, 0x00E02915, scale)
    gfx.text(tx + first_w, ty, logo_eonos, 0x00F4F4F4, scale)
end

local function draw_folder_icon(x, y)
    gfx.fill_rect(x + sp(6), y + sp(5), ss(20), ss(8), 0x00A97828)
    gfx.fill_rect(x + sp(2), y + sp(13), ss(40), ss(26), 0x00F2C75C)
    gfx.rect(x + sp(2), y + sp(13), ss(40), ss(26), 0x00A97828)
    gfx.fill_rect(x + sp(6), y + sp(17), ss(32), ss(17), 0x00FFE39A)
end

local function fill_circle(cx, cy, radius, color)
    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))

        gfx.fill_rect(
            cx - dx,
            cy + dy,
            dx * 2 + 1,
            1,
            color
        )
    end
end

local function draw_clock_icon(x, y)
    local ring = 0x00E02915
    local face = 0x00F4F4F4
    local hand = 0x00313B40

    local cx = x + sp(22)
    local cy = y + sp(22)

    local radius = math.max(1, sp(18))
    local border = math.max(1, sp(2))

    fill_circle(cx, cy, radius, ring)

    fill_circle(cx, cy, radius - border, face)

    local thickness = math.max(2, sp(4))
    local up_length = sp(12)
    local right_length = sp(12)

    gfx.fill_rect(
        cx - math.floor(thickness / 2),
        cy - up_length,
        thickness,
        up_length + 1,
        hand
    )

    gfx.fill_rect(
        cx,
        cy - math.floor(thickness / 2),
        right_length + 1,
        thickness,
        hand
    )

    fill_circle(cx, cy, math.max(1, sp(3)), hand)
end

local function draw_notes_icon(x, y)
    gfx.fill_rect(x + sp(4), y + sp(2), ss(32), ss(39), 0x00E6EDF4)
    gfx.rect(x + sp(4), y + sp(2), ss(32), ss(39), 0x008DA8C0)
    gfx.fill_rect(x + sp(16), y, ss(8), ss(5), 0x00FF8A5D)

    for line = 0, 3 do
        gfx.line(
            x + sp(8),
            y + sp(12 + line * 6),
            x + sp(30),
            y + sp(12 + line * 6),
            0x0076BEE7
        )
    end
end

local function draw_app_icon(x, y)
    gfx.fill_rect(x + sp(4), y + sp(5), ss(34), ss(28), 0x0029537A)
    gfx.rect(x + sp(4), y + sp(5), ss(34), ss(28), COLOR.neon)
    gfx.fill_rect(x + sp(9), y + sp(10), ss(24), ss(16), 0x000B1930)
    gfx.fill_rect(x + sp(15), y + sp(35), ss(12), ss(3), COLOR.neon)
end

local function draw_terminal_icon(x, y)
    gfx.fill_rect(x + sp(4), y + sp(5), ss(34), ss(28), 0x0029537A)
    gfx.rect(x + sp(4), y + sp(5), ss(34), ss(28), COLOR.neon)

    gfx.fill_rect(x + sp(7), y + sp(7), ss(2), ss(2),  0x00A0D8FF)
    gfx.fill_rect(x + sp(10), y + sp(7), ss(2), ss(2),  0x00A0D8FF)
    gfx.fill_rect(x + sp(13), y + sp(7), ss(2), ss(2),  0x00A0D8FF)

    gfx.fill_rect(x + sp(6),  y + sp(12), ss(30), ss(18), 0x000B1930)
end

local function draw_icon_graphic(kind, x, y)
    if kind == "folder" then
        draw_folder_icon(x, y)
    elseif kind == "clock" then
        draw_clock_icon(x, y)
    elseif kind == "notes" then
        draw_notes_icon(x, y)
    elseif kind == "terminal" then
        draw_terminal_icon(x, y)
    else
        draw_app_icon(x, y)
    end
end

local function draw_tile(icon, index, x, y)
    local selected = (index == Desktop.selected)

    if selected then
        gfx.fill_rect(x, y, GRID.tile_w, GRID.tile_h, COLOR.tile_selected)
        gfx.rect(x, y, GRID.tile_w, GRID.tile_h, COLOR.tile_glow)
        gfx.rect(x - 2, y - 2, GRID.tile_w + 4, GRID.tile_h + 4, COLOR.tile_glow)
    end

    draw_icon_graphic(icon.icon_kind, x + math.floor((GRID.tile_w - ICON_BOX_W) / 2), y + 8)

    local title = fit_text(icon.title, GRID.tile_w - 6, 1)
    centered_text(x, y + 52, GRID.tile_w, title, COLOR.text, 1)
end

-- Icon order is top-to-bottom, then starts a new column to the right.
local function draw_icon_grid()
    local rows = grid_rows()

    for index, icon in ipairs(Desktop.icons) do
        local zero_index = index - 1
        local column = math.floor(zero_index / rows)
        local row = zero_index % rows

        local x = GRID.left + column * (GRID.tile_w + GRID.gap_x)
        local y = GRID.top + row * (GRID.tile_h + GRID.gap_y)

        draw_tile(icon, index, x, y)
    end
end

local function draw_tray()
    local tray_y = screen_h - TRAY.height
    local widget_height = TRAY.height - TRAY.padding * 2
    local right = screen_w - TRAY.padding

    gfx.fill_rect(0, tray_y, screen_w, TRAY.height, COLOR.tray)
    gfx.line(0, tray_y, screen_w - 1, tray_y, COLOR.tray_border)

    -- New widgets stack leftward from the right screen edge.
    for index = #Desktop.tray_widgets, 1, -1 do
        local widget = Desktop.tray_widgets[index]

        right = right - widget.width

        if right >= TRAY.padding then
            widget.draw(right, tray_y + TRAY.padding, widget.width, widget_height)
        end

        right = right - TRAY.gap
    end
end

local function draw_clock_tray_widget(x, y, width, height)
    local current_time = safe_date("%H:%M", "--:--")
    local current_date = safe_date("%d.%m.%Y", "01.01.1970")

    gfx.fill_rect(x, y, width, height, COLOR.tray_widget)
    gfx.rect(x, y, width, height, COLOR.tray_border)

    gfx.text(x + 10, y + 5, current_time, COLOR.white, 2)
    gfx.text(x + 11, y + 27, current_date, COLOR.text_dim, 1)
end

local function draw_desktop()
    draw_background()
    draw_icon_grid()
    draw_tray()
end

function Desktop.open_selected()
    local icon = Desktop.selected_icon()

    if icon == nil then
        return
    end

    draw_desktop()
    gfx.present()
    shell.exec(icon.command)
end

-- Tray widgets. Add future widgets after this line.
Desktop.add_tray_widget("clock", 148, draw_clock_tray_widget)

-- Desktop icons. They fill downward before creating a new column.
Desktop.add_icon("Explorer", "explorer", "folder")
Desktop.add_icon("Notes", "notes", "notes")
Desktop.add_icon("Time", "time", "clock")
Desktop.add_icon("Terminal", "terminal", "terminal")

while true do
    local key = input.poll()

    if key ~= nil then
        if key_is(key, "left", KEY_LEFT) then
            Desktop.move_selection(-1, 0)
        elseif key_is(key, "right", KEY_RIGHT) then
            Desktop.move_selection(1, 0)
        elseif key_is(key, "up", KEY_UP) then
            Desktop.move_selection(0, -1)
        elseif key_is(key, "down", KEY_DOWN) then
            Desktop.move_selection(0, 1)
        elseif key_is(key, "tab", 9) then
            Desktop.select_next()
        elseif key_is(key, "enter", 10) or key_is(key, "space", 32) then
            Desktop.open_selected()
        elseif key_is_letter(key, "e") then
            Desktop.set_selected(1)
            Desktop.open_selected()
        elseif key_is_letter(key, "t") then
            Desktop.set_selected(2)
            Desktop.open_selected()
        elseif key_is_letter(key, "n") then
            Desktop.set_selected(3)
            Desktop.open_selected()
        end
    end

    draw_desktop()
    gfx.present()
end
