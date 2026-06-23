local fs = require("fs")
local npackages = require("npackages")
local bitmap_ok, bitmap = pcall(require, "bitmap")

local Config = require("desktop_config")
local Util = require("desktop_util")

local Data = {}

local function ensure_desktop_directory()
    local exists_ok, exists = pcall(fs.exists, Config.DESKTOP_DIR)

    if not exists_ok then
        return false, "cannot inspect .desktop"
    end

    if exists then
        local directory_ok, is_directory = pcall(fs.isDir, Config.DESKTOP_DIR)

        if not directory_ok or not is_directory then
            return false, ".desktop is not a folder"
        end

        return true
    end

    local create_ok, created = pcall(fs.makeDir, Config.DESKTOP_DIR)

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
        local an = Util.lower(a.name)
        local bn = Util.lower(b.name)

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

    local path = Util.normalize_path(
        info.path ~= nil and info.path or fallback_path
    )

    if path == "" then
        return nil
    end

    info.path = path

    if info.id == nil or info.id == "" then
        info.id = Util.base_name(path)
    end

    if info.name == nil or info.name == "" then
        info.name = info.id
    end

    return info
end

local function read_package_info(path)
    path = Util.normalize_path(path)

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
        width > Config.ICON_MAX_W or
        height > Config.ICON_MAX_H
    ) then
        return nil, 0, 0
    end

    return image, width, height
end

local function package_item(info, shortcut_path)
    local image, width, height = load_bitmap(info)

    return {
        id = tostring(info.id),
        path = Util.normalize_path(info.path),
        title = tostring(info.name),
        description = tostring(info.description or ""),
        shortcut_path = shortcut_path,
        bitmap = image,
        bitmap_w = width,
        bitmap_h = height,
        on_desktop = shortcut_path ~= nil,
    }
end

local function item_sort(left, right)
    local left_title = Util.lower(left.title)
    local right_title = Util.lower(right.title)

    if left_title == right_title then
        return Util.lower(left.path) < Util.lower(right.path)
    end

    return left_title < right_title
end

local function selected_path(items, index)
    local item = items[index]

    if item == nil then
        return nil
    end

    return Util.path_key(item.path)
end

local function restore_selected(desktop, items, field, previous_path)
    local selected = 1

    if previous_path ~= nil then
        for index, item in ipairs(items) do
            if Util.path_key(item.path) == previous_path then
                selected = index
                break
            end
        end
    end

    if #items == 0 then
        desktop[field] = 0
    else
        desktop[field] = selected
    end
end

local function grid_rows(screen_h, top)
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

function Data.new(screen_h)
    local desktop = {
        icons = {},
        packages = {},
        shortcut_targets = {},
        desktop_selected = 1,
        package_selected = 1,
        mode = "desktop",
        status = "Ready",
        screen_h = screen_h,
    }

    function desktop.load_shortcuts()
        local previous_path = selected_path(
            desktop.icons,
            desktop.desktop_selected
        )
        local ready, error_message = ensure_desktop_directory()

        desktop.icons = {}
        desktop.shortcut_targets = {}

        if not ready then
            desktop.status = error_message
            desktop.desktop_selected = 0
            return false
        end

        local entries = sorted_entries(Config.DESKTOP_DIR)

        if entries == nil then
            desktop.status = "Cannot read .desktop"
            desktop.desktop_selected = 0
            return false
        end

        local seen = {}

        for _, entry in ipairs(entries) do
            if entry.isDir ~= true then
                local shortcut_path = Util.join_path(Config.DESKTOP_DIR, entry.name)
                local target = Util.read_first_line(shortcut_path)
                local info = read_package_info(target)

                if info ~= nil then
                    local key = Util.path_key(info.path)

                    if not seen[key] then
                        seen[key] = true
                        desktop.shortcut_targets[key] = shortcut_path
                        table.insert(
                            desktop.icons,
                            package_item(info, shortcut_path)
                        )
                    end
                end
            end
        end

        table.sort(desktop.icons, item_sort)
        restore_selected(
            desktop,
            desktop.icons,
            "desktop_selected",
            previous_path
        )
        desktop.status = tostring(#desktop.icons) .. " shortcut(s)"
        return true
    end

    function desktop.load_packages()
        local previous_path = selected_path(
            desktop.packages,
            desktop.package_selected
        )
        local called, packages, error_message = pcall(npackages.list)

        desktop.packages = {}

        if not called then
            desktop.status = "Cannot list packages"
            desktop.package_selected = 0
            return false
        end

        if type(packages) ~= "table" then
            desktop.status = tostring(error_message or "Cannot list packages")
            desktop.package_selected = 0
            return false
        end

        local seen = {}

        for _, info in ipairs(packages) do
            info = normalize_package_info(info, nil)

            if info ~= nil and tostring(info.id):sub(1, 1) ~= "." then
                local key = Util.path_key(info.path)

                if not seen[key] then
                    seen[key] = true
                    local shortcut_path = desktop.shortcut_targets[key]
                    table.insert(
                        desktop.packages,
                        package_item(info, shortcut_path)
                    )
                end
            end
        end

        table.sort(desktop.packages, item_sort)
        restore_selected(
            desktop,
            desktop.packages,
            "package_selected",
            previous_path
        )
        desktop.status = tostring(#desktop.packages) .. " package(s)"
        return true
    end

    function desktop.add_shortcut(item)
        if item == nil then
            return false
        end

        local key = Util.path_key(item.path)

        if desktop.shortcut_targets[key] ~= nil then
            desktop.status = item.title .. " is already on the desktop"
            return false
        end

        local ready, error_message = ensure_desktop_directory()

        if not ready then
            desktop.status = error_message
            return false
        end

        local stem = tostring(item.id or ""):gsub("[^%w%-%_]", "_")

        if stem == "" then
            stem = "package"
        end

        local index = 1
        local shortcut_path

        repeat
            local suffix = index == 1 and "" or "-" .. tostring(index)
            shortcut_path = Util.join_path(
                Config.DESKTOP_DIR,
                stem .. suffix .. Config.SHORTCUT_SUFFIX
            )
            index = index + 1
        until not fs.exists(shortcut_path)

        local wrote, write_error = Util.write_shortcut(shortcut_path, item.path)

        if not wrote then
            desktop.status = "Cannot add shortcut: " .. tostring(write_error)
            return false
        end

        desktop.load_shortcuts()
        desktop.status = "Added " .. item.title
        return true
    end

    function desktop.remove_selected_shortcut()
        local item = desktop.icons[desktop.desktop_selected]

        if item == nil or item.shortcut_path == nil then
            return false
        end

        local removed, result = pcall(fs.delete, item.shortcut_path)

        if not removed or result ~= true then
            desktop.status = "Cannot remove shortcut"
            return false
        end

        desktop.load_shortcuts()
        desktop.status = "Removed " .. item.title
        return true
    end

    function desktop.open_launcher()
        desktop.load_shortcuts()
        desktop.load_packages()
        desktop.mode = "launcher"
    end

    function desktop.close_launcher()
        desktop.load_shortcuts()
        desktop.mode = "desktop"
    end

    function desktop.current_items()
        if desktop.mode == "launcher" then
            return desktop.packages, "package_selected", Config.GRID.launcher_top
        end

        return desktop.icons, "desktop_selected", Config.GRID.desktop_top
    end

    function desktop.current_selected()
        local items, field = desktop.current_items()
        return items[desktop[field]]
    end

    function desktop.set_selected(index)
        local items, field = desktop.current_items()

        if #items == 0 then
            desktop[field] = 0
            return
        end

        if index < 1 then
            index = 1
        elseif index > #items then
            index = #items
        end

        desktop[field] = index
    end

    function desktop.select_next()
        local items, field = desktop.current_items()

        if #items == 0 then
            return
        end

        desktop[field] = desktop[field] + 1

        if desktop[field] > #items then
            desktop[field] = 1
        end
    end

    function desktop.move_selection(dx, dy)
        local items, field, top = desktop.current_items()
        local count = #items

        if count == 0 then
            return
        end

        local rows = grid_rows(desktop.screen_h, top)
        local columns = math.floor((count + rows - 1) / rows)
        local zero_index = desktop[field] - 1
        local current_column = math.floor(zero_index / rows)
        local current_row = zero_index % rows
        local target = desktop[field]

        if dy ~= 0 then
            local column_first = current_column * rows + 1
            local column_last = math.min(column_first + rows - 1, count)

            target = desktop[field] + dy

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

        desktop.set_selected(target)
    end

    return desktop
end

return Data
