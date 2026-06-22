local gfx = require("gfx")
local input = require("input")
local fs = require("fs")
local shell = require("shell")
local npackages = require("npackages")
local bitmap_ok, bitmap = pcall(require, "bitmap")

local Desktop = {
    icons = {},
    packages = {},
    shortcut_targets = {},
    desktop_selected = 1,
    package_selected = 1,
    mode = "desktop",
    status = "Ready"
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
    blur_tile = 0x00171819,
    blur_icon = 0x00202124,
    blur_text = 0x001B1C1E,
    launcher_panel = 0x00151618,
    launcher_shadow = 0x000D0E10,
    launcher_border = 0x00383A3D,
    text = 0x00D5E5F4,
    text_dim = 0x0090A9C0,
    white = 0x00F3FBFF,
    neon = 0x0048D9FF,
    tray = 0x00141415,
    tray_border = 0x00333335,
    tray_widget = 0x001D1D1F,
}

local GRID = {
    left = 12,
    desktop_top = 16,
    launcher_top = 78,
    tile_w = 64,
    tile_h = 64,
    gap_x = 6,
    gap_y = 6
}

local TRAY = {
    height = 56,
    padding = 7
}

local LAUNCHER = {
    left = 20,
    top = 44,
    right = 20,
    bottom_gap = 12,
    padding = 14,
    header_height = 24
}

local DESKTOP_DIR = "0:/.desktop"
local SHORTCUT_SUFFIX = ".shortcut"
local ICON_MAX_W = 44
local ICON_MAX_H = 44

local KEY_LEFT = 0x100
local KEY_RIGHT = 0x101
local KEY_UP = 0x102
local KEY_DOWN = 0x103
local KEY_DELETE = 0x106
local KEY_ESCAPE = 0x10A
local KEY_F5 = 0x124

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

local function trim(value)
    value = tostring(value or "")
    value = value:gsub("^%s+", "")
    value = value:gsub("%s+$", "")
    return value
end

local function lower(value)
    return string.lower(tostring(value or ""))
end

local function key_is(key, name, numeric_code)
    return key == name or key == numeric_code
end

local function is_volume_path(path)
    return type(path) == "string" and path:match("^%d:") ~= nil
end

local function normalize_path(path)
    path = trim(path):gsub("\\", "/")

    if path == "" then
        return ""
    end

    if not is_volume_path(path) then
        path = path:gsub("^%./", "")
        path = path:gsub("^/+", "")
        path = "0:/" .. path
    end

    local drive = path:sub(1, 2)
    local rest = path:sub(3)
    local parts = {}

    for part in rest:gmatch("[^/]+") do
        if part ~= "" and part ~= "." then
            if part == ".." then
                if #parts > 0 then
                    table.remove(parts)
                end
            else
                table.insert(parts, part)
            end
        end
    end

    if #parts == 0 then
        return drive .. "/"
    end

    return drive .. "/" .. table.concat(parts, "/")
end

local function join_path(left, right)
    left = normalize_path(left)
    right = tostring(right or ""):gsub("\\", "/")
    right = right:gsub("^/+", "")

    if left == "" then
        return normalize_path(right)
    end

    if right == "" then
        return left
    end

    if left:sub(-1) == "/" then
        return normalize_path(left .. right)
    end

    return normalize_path(left .. "/" .. right)
end

local function path_key(path)
    return lower(normalize_path(path))
end

local function base_name(path)
    path = normalize_path(path)
    return path:match("([^/]+)$") or ""
end

local function read_first_line(path)
    local file = io.open(path, "r")

    if file == nil then
        return nil
    end

    local line = file:read("*l")
    file:close()

    line = trim(line)

    if line == "" then
        return nil
    end

    return line
end

local function write_shortcut(path, target)
    local file, open_error = io.open(path, "w")

    if file == nil then
        return false, open_error or "cannot create shortcut"
    end

    local wrote, write_error = file:write(target, "\n")
    local closed, close_error = file:close()

    if not wrote then
        return false, write_error or "cannot write shortcut"
    end

    if not closed then
        return false, close_error or "cannot close shortcut"
    end

    return true
end

local function ensure_desktop_directory()
    local exists_ok, exists = pcall(fs.exists, DESKTOP_DIR)

    if not exists_ok then
        return false, "cannot inspect .desktop"
    end

    if exists then
        local directory_ok, is_directory = pcall(fs.isDir, DESKTOP_DIR)

        if not directory_ok or not is_directory then
            return false, ".desktop is not a folder"
        end

        return true
    end

    local create_ok, created = pcall(fs.makeDir, DESKTOP_DIR)

    if not create_ok or not created then
        return false, "cannot create .desktop"
    end

    return true
end

local function sorted_entries(path)
    local ok, entries = pcall(fs.listInfo, path)

    if not ok or type(entries) ~= "table" then
        return nil
    end

    table.sort(entries, function(a, b)
        local an = lower(a.name)
        local bn = lower(b.name)

        if an == bn then
            return tostring(a.name or "") < tostring(b.name or "")
        end

        return an < bn
    end)

    return entries
end

local function normalize_package_info(info, fallback_path)
    if type(info) ~= "table" then
        return nil
    end

    local path = normalize_path(info.path ~= nil and info.path or fallback_path)

    if path == "" then
        return nil
    end

    info.path = path

    if info.id == nil or info.id == "" then
        info.id = base_name(path)
    end

    if info.name == nil or info.name == "" then
        info.name = info.id
    end

    return info
end

local function read_package_info(path)
    path = normalize_path(path)

    if path == "" then
        return nil
    end

    local ok, info = pcall(npackages.info, path)

    if not ok then
        return nil
    end

    return normalize_package_info(info, path)
end

local function load_bitmap(info)
    if (
        not bitmap_ok or
        type(bitmap) ~= "table" or
        info.icon_exists ~= true or
        type(info.icon_path) ~= "string" or
        info.icon_path == ""
    ) then
        return nil, 0, 0
    end

    local loaded, image = pcall(bitmap.load, info.icon_path)

    if not loaded or image == nil then
        return nil, 0, 0
    end

    local sized, width, height = pcall(bitmap.size, image)

    if (
        not sized or
        type(width) ~= "number" or
        type(height) ~= "number" or
        width < 1 or
        height < 1 or
        width > ICON_MAX_W or
        height > ICON_MAX_H
    ) then
        return nil, 0, 0
    end

    return image, width, height
end

local function package_item(info, shortcut_path)
    local image, width, height = load_bitmap(info)

    return {
        id = tostring(info.id),
        path = normalize_path(info.path),
        title = tostring(info.name),
        description = tostring(info.description or ""),
        shortcut_path = shortcut_path,
        bitmap = image,
        bitmap_w = width,
        bitmap_h = height,
        on_desktop = shortcut_path ~= nil
    }
end

local function item_sort(left, right)
    local left_title = lower(left.title)
    local right_title = lower(right.title)

    if left_title == right_title then
        return lower(left.path) < lower(right.path)
    end

    return left_title < right_title
end

local function selected_path(items, index)
    local item = items[index]

    if item == nil then
        return nil
    end

    return path_key(item.path)
end

local function restore_selected(items, field, previous_path)
    local selected = 1

    if previous_path ~= nil then
        for index, item in ipairs(items) do
            if path_key(item.path) == previous_path then
                selected = index
                break
            end
        end
    end

    if #items == 0 then
        Desktop[field] = 0
    else
        Desktop[field] = selected
    end
end

function Desktop.load_shortcuts()
    local previous_path = selected_path(Desktop.icons, Desktop.desktop_selected)
    local ready, error_message = ensure_desktop_directory()

    Desktop.icons = {}
    Desktop.shortcut_targets = {}

    if not ready then
        Desktop.status = error_message
        Desktop.desktop_selected = 0
        return false
    end

    local entries = sorted_entries(DESKTOP_DIR)

    if entries == nil then
        Desktop.status = "Cannot read .desktop"
        Desktop.desktop_selected = 0
        return false
    end

    local seen = {}

    for _, entry in ipairs(entries) do
        if entry.isDir ~= true then
            local shortcut_path = join_path(DESKTOP_DIR, entry.name)
            local target = read_first_line(shortcut_path)
            local info = read_package_info(target)

            if info ~= nil then
                local key = path_key(info.path)

                if not seen[key] then
                    seen[key] = true
                    Desktop.shortcut_targets[key] = shortcut_path
                    table.insert(Desktop.icons, package_item(info, shortcut_path))
                end
            end
        end
    end

    table.sort(Desktop.icons, item_sort)
    restore_selected(Desktop.icons, "desktop_selected", previous_path)
    Desktop.status = tostring(#Desktop.icons) .. " shortcut(s)"
    return true
end

function Desktop.load_packages()
    local previous_path = selected_path(Desktop.packages, Desktop.package_selected)
    local called, packages, error_message = pcall(npackages.list)

    Desktop.packages = {}

    if not called then
        Desktop.status = "Cannot list packages"
        Desktop.package_selected = 0
        return false
    end

    if type(packages) ~= "table" then
        Desktop.status = tostring(error_message or "Cannot list packages")
        Desktop.package_selected = 0
        return false
    end

    local seen = {}

    for _, info in ipairs(packages) do
        info = normalize_package_info(info, nil)

        if (
            info ~= nil and
            tostring(info.id):sub(1, 1) ~= "."
        ) then
            local key = path_key(info.path)

            if not seen[key] then
                seen[key] = true
                local shortcut_path = Desktop.shortcut_targets[key]
                table.insert(
                    Desktop.packages,
                    package_item(info, shortcut_path)
                )
            end
        end
    end

    table.sort(Desktop.packages, item_sort)
    restore_selected(Desktop.packages, "package_selected", previous_path)
    Desktop.status = tostring(#Desktop.packages) .. " package(s)"
    return true
end

local function sanitize_shortcut_name(name)
    name = tostring(name or ""):gsub("[^%w%-%_]", "_")

    if name == "" then
        return "package"
    end

    return name
end

function Desktop.add_shortcut(item)
    if item == nil then
        return false
    end

    local key = path_key(item.path)

    if Desktop.shortcut_targets[key] ~= nil then
        Desktop.status = item.title .. " is already on the desktop"
        return false
    end

    local ready, error_message = ensure_desktop_directory()

    if not ready then
        Desktop.status = error_message
        return false
    end

    local stem = sanitize_shortcut_name(item.id)
    local index = 1
    local shortcut_path

    repeat
        local suffix = index == 1 and "" or "-" .. tostring(index)
        shortcut_path = join_path(
            DESKTOP_DIR,
            stem .. suffix .. SHORTCUT_SUFFIX
        )
        index = index + 1
    until not fs.exists(shortcut_path)

    local wrote, write_error = write_shortcut(shortcut_path, item.path)

    if not wrote then
        Desktop.status = "Cannot add shortcut: " .. tostring(write_error)
        return false
    end

    Desktop.load_shortcuts()
    Desktop.status = "Added " .. item.title
    return true
end

function Desktop.remove_selected_shortcut()
    local item = Desktop.icons[Desktop.desktop_selected]

    if item == nil or item.shortcut_path == nil then
        return false
    end

    local removed, result = pcall(fs.delete, item.shortcut_path)

    if not removed or result ~= true then
        Desktop.status = "Cannot remove shortcut"
        return false
    end

    Desktop.load_shortcuts()
    Desktop.status = "Removed " .. item.title
    return true
end

function Desktop.open_launcher()
    Desktop.load_shortcuts()
    Desktop.load_packages()
    Desktop.mode = "launcher"
end

function Desktop.close_launcher()
    Desktop.load_shortcuts()
    Desktop.mode = "desktop"
end

function Desktop.current_items()
    if Desktop.mode == "launcher" then
        return Desktop.packages, "package_selected", GRID.launcher_top
    end

    return Desktop.icons, "desktop_selected", GRID.desktop_top
end

function Desktop.current_selected()
    local items, field = Desktop.current_items()
    return items[Desktop[field]]
end

function Desktop.set_selected(index)
    local items, field = Desktop.current_items()

    if #items == 0 then
        Desktop[field] = 0
        return
    end

    if index < 1 then
        index = 1
    elseif index > #items then
        index = #items
    end

    Desktop[field] = index
end

function Desktop.select_next()
    local items, field = Desktop.current_items()

    if #items == 0 then
        return
    end

    Desktop[field] = Desktop[field] + 1

    if Desktop[field] > #items then
        Desktop[field] = 1
    end
end

local function grid_rows(top)
    local available_height = screen_h - top - TRAY.height - 8
    local rows = math.floor(
        (available_height + GRID.gap_y) / (GRID.tile_h + GRID.gap_y)
    )

    if rows < 1 then
        rows = 1
    end

    return rows
end

function Desktop.move_selection(dx, dy)
    local items, field, top = Desktop.current_items()
    local count = #items

    if count == 0 then
        return
    end

    local rows = grid_rows(top)
    local columns = math.floor((count + rows - 1) / rows)
    local zero_index = Desktop[field] - 1
    local current_column = math.floor(zero_index / rows)
    local current_row = zero_index % rows
    local target = Desktop[field]

    if dy ~= 0 then
        local column_first = current_column * rows + 1
        local column_last = math.min(column_first + rows - 1, count)

        target = Desktop[field] + dy

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

local function draw_app_icon(x, y)
    gfx.fill_rect(x + 4, y + 5, 34, 28, 0x0029537A)
    gfx.rect(x + 4, y + 5, 34, 28, COLOR.neon)
    gfx.fill_rect(x + 9, y + 10, 24, 16, 0x000B1930)
    gfx.fill_rect(x + 15, y + 35, 12, 3, COLOR.neon)
end

local function draw_package_icon(item, x, y)
    if item.bitmap ~= nil and item.bitmap_w > 0 and item.bitmap_h > 0 then
        local draw_x = x + math.floor((ICON_MAX_W - item.bitmap_w) / 2)
        local draw_y = y + math.floor((ICON_MAX_H - item.bitmap_h) / 2)
        local drawn = pcall(bitmap.draw, item.bitmap, draw_x, draw_y, 1)

        if drawn then
            return
        end
    end

    draw_app_icon(x, y)
end

local function draw_tile(item, index, selected_index, x, y)
    local selected = index == selected_index

    if selected then
        gfx.fill_rect(x, y, GRID.tile_w, GRID.tile_h, COLOR.tile_selected)
        gfx.rect(x, y, GRID.tile_w, GRID.tile_h, COLOR.tile_glow)
        gfx.rect(x - 2, y - 2, GRID.tile_w + 4, GRID.tile_h + 4, COLOR.tile_glow)
    end

    draw_package_icon(
        item,
        x + math.floor((GRID.tile_w - ICON_MAX_W) / 2),
        y + 3
    )

    local title = fit_text(item.title, GRID.tile_w - 6, 1)
    centered_text(x, y + 52, GRID.tile_w, title, COLOR.text, 1)
end

local function draw_grid(items, selected, left, top)
    local rows = grid_rows(top)

    for index, item in ipairs(items) do
        local zero_index = index - 1
        local column = math.floor(zero_index / rows)
        local row = zero_index % rows
        local x = left + column * (GRID.tile_w + GRID.gap_x)
        local y = top + row * (GRID.tile_h + GRID.gap_y)

        draw_tile(item, index, selected, x, y)
    end
end

local function draw_tray()
    local tray_y = screen_h - TRAY.height
    local x = screen_w - TRAY.padding - 148
    local y = tray_y + TRAY.padding
    local current_time = safe_date("%H:%M", "--:--")
    local current_date = safe_date("%d.%m.%Y", "01.01.1970")

    gfx.fill_rect(0, tray_y, screen_w, TRAY.height, COLOR.tray)
    gfx.line(0, tray_y, screen_w - 1, tray_y, COLOR.tray_border)
    gfx.fill_rect(x, y, 148, TRAY.height - TRAY.padding * 2, COLOR.tray_widget)
    gfx.rect(x, y, 148, TRAY.height - TRAY.padding * 2, COLOR.tray_border)
    gfx.text(x + 10, y + 5, current_time, COLOR.white, 2)
    gfx.text(x + 11, y + 27, current_date, COLOR.text_dim, 1)
end

local function draw_blurred_desktop()
    local rows = grid_rows(GRID.desktop_top)

    draw_background()

    for index, _ in ipairs(Desktop.icons) do
        local zero_index = index - 1
        local column = math.floor(zero_index / rows)
        local row = zero_index % rows
        local x = GRID.left + column * (GRID.tile_w + GRID.gap_x)
        local y = GRID.desktop_top + row * (GRID.tile_h + GRID.gap_y)

        gfx.fill_rect(x + 3, y + 3, GRID.tile_w - 6, GRID.tile_h - 6, COLOR.blur_tile)
        gfx.fill_rect(x + 20, y + 13, 24, 22, COLOR.blur_icon)
        gfx.fill_rect(x + 11, y + 49, 42, 4, COLOR.blur_text)
    end
end

local function draw_desktop()
    draw_background()
    draw_grid(
        Desktop.icons,
        Desktop.desktop_selected,
        GRID.left,
        GRID.desktop_top
    )
    draw_tray()
end

local function draw_launcher()
    local panel_x = LAUNCHER.left
    local panel_y = LAUNCHER.top
    local panel_w = screen_w - LAUNCHER.left - LAUNCHER.right
    local panel_h = screen_h - TRAY.height - LAUNCHER.top - LAUNCHER.bottom_gap

    draw_blurred_desktop()

    gfx.fill_rect(
        panel_x + 3,
        panel_y + 3,
        panel_w,
        panel_h,
        COLOR.launcher_shadow
    )
    gfx.fill_rect(
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        COLOR.launcher_panel
    )
    gfx.rect(
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        COLOR.launcher_border
    )
    gfx.text(
        panel_x + LAUNCHER.padding,
        panel_y + 8,
        "PACKAGES",
        COLOR.white,
        1
    )

    draw_grid(
        Desktop.packages,
        Desktop.package_selected,
        panel_x + LAUNCHER.padding,
        GRID.launcher_top
    )
    draw_tray()
end

local function draw_current()
    if Desktop.mode == "launcher" then
        draw_launcher()
    else
        draw_desktop()
    end
end

local function shell_quote_argument(value)
    value = tostring(value or "")

    if string.find(value, '"', 1, true) ~= nil then
        return nil
    end

    return '"' .. value .. '"'
end

local function open_item(item)
    if item == nil then
        return
    end

    local target = shell_quote_argument(item.path)

    if target == nil then
        Desktop.status = "Invalid package path"
        return
    end

    draw_current()
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

function Desktop.open_selected()
    open_item(Desktop.current_selected())
end

Desktop.load_shortcuts()

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
        elseif Desktop.mode == "desktop" then
            if key_is(key, "f5", KEY_F5) then
                Desktop.open_launcher()
            elseif key_is(key, "delete", KEY_DELETE) then
                Desktop.remove_selected_shortcut()
            elseif key_is(key, "enter", 10) or key_is(key, "space", 32) then
                Desktop.open_selected()
            end
        else
            if key_is(key, "f5", KEY_F5) or key_is(key, "escape", KEY_ESCAPE) then
                Desktop.close_launcher()
            elseif key_is(key, "enter", 10) then
                Desktop.open_selected()
            elseif key_is(key, "space", 32) then
                Desktop.add_shortcut(Desktop.current_selected())
                Desktop.load_packages()
            end
        end
    end

    draw_current()
    gfx.present()
end