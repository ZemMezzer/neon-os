-- NeonOS Explorer v1
--
-- A framebuffer file manager for NeonOS Lua.
-- Requires the built-in modules:
--   local gfx = require("gfx")
--   local input = require("input")
--   local fs = require("fs")
--
-- Controls
--   Up/Down or W/S      move selection
--   Home/End            first / last item
--   [ / ]               previous / next page
--   Enter or Right      open directory
--   Backspace or Left   parent directory
--   D or Delete         delete selected item
--   N                   create directory
--   R                   rename selected item
--   C                   copy selected item
--   M                   move selected item
--   F                   refresh
--   I                   show selected-item information
--   Q                   quit
--
-- In copy/move target selection:
--   Right/O             open selected folder
--   Enter               choose the currently open folder
--   Backspace/Left      parent folder
--   Tab                 cancel
--
-- In text prompts:
--   Enter               confirm
--   Tab                 cancel
--   Backspace           erase one character

local gfx = require("gfx")
local input = require("input")
local fs = require("fs")

local COLOR = {
    black = 0x000000,
    panel = 0x101820,
    panel2 = 0x18232D,
    border = 0x365168,
    text = 0xE8F1F8,
    muted = 0x9CB4C4,
    accent = 0x63C6FF,
    directory = 0x8DDB7A,
    selected_bg = 0x285B7A,
    selected_text = 0xFFFFFF,
    warning = 0xFFD166,
    error = 0xFF7A7A,
    prompt = 0x162B39
}

local SCALE = 2
local CHAR_WIDTH = 6 * SCALE
local CHAR_HEIGHT = 8 * SCALE
local ROW_HEIGHT = 18
local PADDING = 10

local SCREEN_WIDTH = gfx.width()
local SCREEN_HEIGHT = gfx.height()

local HEADER_HEIGHT = 70
local FOOTER_HEIGHT = 74
local LIST_TOP = HEADER_HEIGHT + 8
local LIST_BOTTOM = SCREEN_HEIGHT - FOOTER_HEIGHT - 8
local PAGE_SIZE = math.max(1, math.floor((LIST_BOTTOM - LIST_TOP + 1) / ROW_HEIGHT))

local cwd = "0:/"
local items = {}
local selected = 1
local offset = 1
local status = "Ready"
local running = true
local dirty = true

-- nil while browsing normally.
-- { kind = "copy" | "move", source = item } while choosing a destination.
local target_mode = nil

-- nil, or a modal table for confirmation/text entry.
local modal = nil

local function is_root(path)
    return path == nil or path == "" or path == "/" or path == "0:" or path == "0:/"
end

local function normalize_root(path)
    if is_root(path) then
        return "0:/"
    end

    return path
end

local function display_path(path)
    path = normalize_root(path)

    if path == "0:/" then
        return "0:/"
    end

    return path
end

local function parent_of(path)
    if is_root(path) then
        return "0:/"
    end

    return normalize_root(fs.getDir(path))
end

local function path_equals(left, right)
    local a = tostring(left or ""):gsub("\\", "/")
    local b = tostring(right or ""):gsub("\\", "/")

    while #a > 3 and a:sub(-1) == "/" do
        a = a:sub(1, -2)
    end

    while #b > 3 and b:sub(-1) == "/" do
        b = b:sub(1, -2)
    end

    return a == b
end

local function fit(text, max_chars)
    text = tostring(text or "")

    if max_chars <= 0 then
        return ""
    end

    if #text <= max_chars then
        return text
    end

    if max_chars <= 3 then
        return text:sub(1, max_chars)
    end

    return text:sub(1, max_chars - 3) .. "..."
end

local function text_at(x, y, text, color, max_chars)
    if max_chars then
        text = fit(text, max_chars)
    end

    gfx.text(x, y, tostring(text or ""), color or COLOR.text, SCALE)
end

local function format_size(bytes)
    bytes = tonumber(bytes) or 0

    if bytes >= 1024 * 1024 * 1024 then
        return string.format("%.1fG", bytes / (1024 * 1024 * 1024))
    end

    if bytes >= 1024 * 1024 then
        return string.format("%.1fM", bytes / (1024 * 1024))
    end

    if bytes >= 1024 then
        return string.format("%.1fK", bytes / 1024)
    end

    return tostring(bytes) .. "B"
end

local function selected_item()
    return items[selected]
end

local function clamp_selection()
    if #items == 0 then
        selected = 1
        offset = 1
        return
    end

    if selected < 1 then
        selected = 1
    elseif selected > #items then
        selected = #items
    end

    if offset < 1 then
        offset = 1
    end

    if selected < offset then
        offset = selected
    elseif selected >= offset + PAGE_SIZE then
        offset = selected - PAGE_SIZE + 1
    end

    local max_offset = math.max(1, #items - PAGE_SIZE + 1)

    if offset > max_offset then
        offset = max_offset
    end
end

local function set_status(text)
    status = tostring(text or "")
    dirty = true
end

local function load_items(message)
    local ok, result = pcall(fs.listInfo, cwd)

    if not ok then
        items = {}
        selected = 1
        offset = 1
        set_status("Cannot list " .. display_path(cwd) .. ": " .. tostring(result))
        return false
    end

    local new_items = {}

    if not is_root(cwd) then
        table.insert(new_items, {
            name = "..",
            path = parent_of(cwd),
            isDir = true,
            parent = true,
            size = 0
        })
    end

    table.sort(result, function(a, b)
        if a.isDir ~= b.isDir then
            return a.isDir
        end

        return string.lower(a.name) < string.lower(b.name)
    end)

    for index = 1, #result do
        local entry = result[index]

        table.insert(new_items, {
            name = entry.name,
            path = fs.combine(cwd, entry.name),
            isDir = entry.isDir == true,
            parent = false,
            size = entry.size or 0,
            readonly = entry.readonly == true
        })
    end

    items = new_items
    clamp_selection()

    if message then
        set_status(message)
    else
        set_status(tostring(#items - (is_root(cwd) and 0 or 1)) .. " item(s)")
    end

    return true
end

local function refresh(message)
    load_items(message)
    dirty = true
end

local function change_selection(delta)
    if #items == 0 then
        return
    end

    selected = selected + delta
    clamp_selection()
    dirty = true
end

local function open_directory(item)
    if not item then
        set_status("Nothing selected")
        return
    end

    if not item.isDir then
        set_status("Selected file: " .. item.name)
        return
    end

    cwd = normalize_root(item.path)
    selected = 1
    offset = 1
    refresh("Opened " .. display_path(cwd))
end

local function go_parent()
    if is_root(cwd) then
        set_status("Already at volume root")
        return
    end

    cwd = parent_of(cwd)
    selected = 1
    offset = 1
    refresh("Opened " .. display_path(cwd))
end

local function draw_line(x, y, width, color)
    gfx.fill_rect(x, y, width, 1, color)
end

local function draw_item_row(item, index, y)
    local is_selected = index == selected
    local foreground = COLOR.text
    local prefix = "    "
    local suffix = ""

    if is_selected then
        gfx.fill_rect(PADDING, y - 1, SCREEN_WIDTH - PADDING * 2, ROW_HEIGHT, COLOR.selected_bg)
        foreground = COLOR.selected_text
    end

    if item.parent then
        prefix = "<UP> "
        foreground = is_selected and COLOR.selected_text or COLOR.muted
    elseif item.isDir then
        prefix = "[D]  "
        suffix = "/"
        foreground = is_selected and COLOR.selected_text or COLOR.directory
    else
        suffix = "  " .. format_size(item.size)
    end

    local available_chars = math.floor((SCREEN_WIDTH - PADDING * 2) / CHAR_WIDTH)
    local left_text = prefix .. item.name
    local suffix_chars = #suffix
    local name_chars = math.max(1, available_chars - suffix_chars)

    text_at(PADDING + 4, y, left_text, foreground, name_chars)

    if suffix ~= "" then
        local suffix_x = SCREEN_WIDTH - PADDING - #suffix * CHAR_WIDTH
        text_at(suffix_x, y, suffix, foreground)
    end
end

local function draw_modal()
    if not modal then
        return
    end

    local box_width = SCREEN_WIDTH - 120
    local box_height = 104
    local box_x = math.floor((SCREEN_WIDTH - box_width) / 2)
    local box_y = math.floor((SCREEN_HEIGHT - box_height) / 2)

    gfx.fill_rect(box_x - 2, box_y - 2, box_width + 4, box_height + 4, COLOR.border)
    gfx.fill_rect(box_x, box_y, box_width, box_height, COLOR.prompt)

    local max_chars = math.floor((box_width - 24) / CHAR_WIDTH)

    if modal.type == "confirm" then
        text_at(box_x + 12, box_y + 14, fit(modal.text, max_chars), COLOR.warning)
        text_at(box_x + 12, box_y + 52, "Y / Enter = yes    N / Tab = no", COLOR.text, max_chars)
        return
    end

    if modal.type == "text" then
        text_at(box_x + 12, box_y + 12, modal.title, COLOR.accent, max_chars)
        text_at(box_x + 12, box_y + 34, fit(modal.hint, max_chars), COLOR.muted)

        gfx.fill_rect(box_x + 10, box_y + 56, box_width - 20, 26, COLOR.black)
        text_at(box_x + 14, box_y + 62, fit(modal.value, max_chars - 2), COLOR.text)
        text_at(box_x + 12, box_y + 86, "Enter = accept    Tab = cancel", COLOR.muted, max_chars)
    end
end

local function draw_ui()
    gfx.clear(COLOR.black)

    gfx.fill_rect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR.panel)
    draw_line(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOR.border)

    local title = "NeonOS Explorer"
    if target_mode then
        title = "NeonOS Explorer - Select " .. target_mode.kind .. " target"
    end

    local title_chars = math.floor((SCREEN_WIDTH - PADDING * 2) / CHAR_WIDTH)
    text_at(PADDING, 10, title, COLOR.accent, title_chars)
    text_at(PADDING, 30, display_path(cwd), COLOR.text, title_chars)

    local detail = status
    if not target_mode then
        local free_ok, free_or_error = pcall(fs.getFreeSpace, cwd)
        if free_ok then
            detail = detail .. " | free " .. format_size(free_or_error)
        end
    end

    text_at(PADDING, 50, detail, COLOR.muted, title_chars)

    if #items == 0 then
        text_at(PADDING + 4, LIST_TOP + 6, "<empty folder>", COLOR.muted)
    else
        for line = 0, PAGE_SIZE - 1 do
            local index = offset + line
            if index > #items then
                break
            end

            draw_item_row(items[index], index, LIST_TOP + line * ROW_HEIGHT)
        end
    end

    gfx.fill_rect(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, COLOR.panel2)
    draw_line(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, COLOR.border)

    local footer_chars = math.floor((SCREEN_WIDTH - PADDING * 2) / CHAR_WIDTH)

    if target_mode then
        text_at(PADDING, SCREEN_HEIGHT - 62, "Right/O: open folder   Enter: choose this folder", COLOR.text, footer_chars)
        text_at(PADDING, SCREEN_HEIGHT - 42, "Backspace/Left: parent   Tab: cancel", COLOR.muted, footer_chars)
        text_at(PADDING, SCREEN_HEIGHT - 22, "Source: " .. target_mode.source.name, COLOR.warning, footer_chars)
    else
        text_at(PADDING, SCREEN_HEIGHT - 62, "Enter/Right open  Backspace/Left parent  W/S or arrows move", COLOR.text, footer_chars)
        text_at(PADDING, SCREEN_HEIGHT - 42, "D delete  N mkdir  R rename  C copy  M move  F refresh", COLOR.muted, footer_chars)
        text_at(PADDING, SCREEN_HEIGHT - 22, "I info  [/] page  Home/End bounds  Q quit", COLOR.muted, footer_chars)
    end

    draw_modal()
end

local function begin_confirm(text, on_yes, on_no)
    modal = {
        type = "confirm",
        text = text,
        on_yes = on_yes,
        on_no = on_no
    }
    dirty = true
end

local function begin_text(title, hint, initial, on_accept)
    modal = {
        type = "text",
        title = title,
        hint = hint,
        value = initial or "",
        on_accept = on_accept
    }
    dirty = true
end

local function cancel_target(message)
    target_mode = nil
    refresh(message or "Operation cancelled")
end

local function perform_target_operation(destination, overwrite)
    local operation = target_mode

    if not operation then
        return
    end

    local source = operation.source.path
    local name = operation.source.name
    local kind = operation.kind

    local ok, err = pcall(function()
        if overwrite and fs.exists(destination) then
            fs.delete(destination)
        end

        if kind == "copy" then
            fs.copy(source, destination)
        else
            fs.move(source, destination)
        end
    end)

    target_mode = nil

    if ok then
        refresh((kind == "copy" and "Copied " or "Moved ") .. name)
    else
        refresh((kind == "copy" and "Copy failed: " or "Move failed: ") .. tostring(err))
    end
end

local function choose_target_folder()
    if not target_mode then
        return
    end

    local destination = fs.combine(cwd, target_mode.source.name)

    if path_equals(destination, target_mode.source.path) then
        set_status("Destination is the same as the source")
        return
    end

    if fs.exists(destination) then
        local shown = display_path(destination)
        begin_confirm("Overwrite " .. shown .. "?", function()
            modal = nil
            perform_target_operation(destination, true)
        end, function()
            modal = nil
            set_status("Operation cancelled")
        end)
        return
    end

    perform_target_operation(destination, false)
end

local function start_copy_or_move(kind)
    local item = selected_item()

    if not item then
        set_status("Nothing selected")
        return
    end

    if item.parent then
        set_status("Cannot " .. kind .. " parent entry")
        return
    end

    target_mode = {
        kind = kind,
        source = item
    }

    set_status("Navigate to destination folder")
end

local function delete_selected()
    local item = selected_item()

    if not item then
        set_status("Nothing selected")
        return
    end

    if item.parent then
        set_status("Cannot delete parent entry")
        return
    end

    begin_confirm("Delete " .. item.name .. "?", function()
        local ok, err = pcall(fs.delete, item.path)
        modal = nil

        if ok then
            refresh("Deleted " .. item.name)
        else
            refresh("Delete failed: " .. tostring(err))
        end
    end, function()
        modal = nil
        set_status("Delete cancelled")
    end)
end

local function make_directory()
    begin_text(
        "Create directory",
        "Name (ASCII recommended; no slash):",
        "",
        function(name)
            modal = nil

            if name == "" then
                set_status("Create directory cancelled")
                return
            end

            if name:find("/", 1, true) or name:find("\\", 1, true) then
                set_status("Name cannot contain a slash")
                return
            end

            local path = fs.combine(cwd, name)

            if fs.exists(path) then
                set_status("Already exists: " .. name)
                return
            end

            local ok, err = pcall(fs.makeDir, path)

            if ok then
                refresh("Created " .. name)
            else
                refresh("Create failed: " .. tostring(err))
            end
        end
    )
end

local function rename_selected()
    local item = selected_item()

    if not item then
        set_status("Nothing selected")
        return
    end

    if item.parent then
        set_status("Cannot rename parent entry")
        return
    end

    begin_text(
        "Rename " .. item.name,
        "New name (ASCII recommended; no slash):",
        "",
        function(name)
            modal = nil

            if name == "" then
                set_status("Rename cancelled")
                return
            end

            if name:find("/", 1, true) or name:find("\\", 1, true) then
                set_status("Name cannot contain a slash")
                return
            end

            local destination = fs.combine(parent_of(item.path), name)

            if path_equals(destination, item.path) then
                set_status("Same name")
                return
            end

            local function do_rename(overwrite)
                local ok, err = pcall(function()
                    if overwrite and fs.exists(destination) then
                        fs.delete(destination)
                    end

                    fs.move(item.path, destination)
                end)

                if ok then
                    refresh("Renamed to " .. name)
                else
                    refresh("Rename failed: " .. tostring(err))
                end
            end

            if fs.exists(destination) then
                begin_confirm("Overwrite " .. name .. "?", function()
                    modal = nil
                    do_rename(true)
                end, function()
                    modal = nil
                    set_status("Rename cancelled")
                end)
            else
                do_rename(false)
            end
        end
    )
end

local function show_info()
    local item = selected_item()

    if not item then
        set_status("Nothing selected")
        return
    end

    if item.parent then
        set_status("Parent directory")
        return
    end

    local kind = item.isDir and "directory" or "file"
    local size = item.isDir and "" or (", " .. format_size(item.size))
    local readonly = item.readonly and ", read-only" or ""

    set_status(kind .. size .. readonly .. ": " .. item.name)
end

local function handle_modal_key(key)
    if modal.type == "confirm" then
        if key == input.Y or key == input.ENTER then
            local callback = modal.on_yes
            if callback then
                callback()
            else
                modal = nil
            end
            dirty = true
            return
        end

        if key == input.N or key == input.TAB then
            local callback = modal.on_no
            if callback then
                callback()
            else
                modal = nil
            end
            dirty = true
        end

        return
    end

    if modal.type == "text" then
        if key == input.ENTER then
            local callback = modal.on_accept
            local value = modal.value
            if callback then
                callback(value)
            else
                modal = nil
            end
            dirty = true
            return
        end

        if key == input.TAB then
            modal = nil
            set_status("Cancelled")
            return
        end

        if key == input.BACKSPACE then
            modal.value = modal.value:sub(1, -2)
            dirty = true
            return
        end

        if key >= 32 and key <= 126 then
            local character = string.char(key)

            if character ~= "/" and character ~= "\\" and #modal.value < 55 then
                modal.value = modal.value .. character
                dirty = true
            end
        end
    end
end

local function handle_target_key(key)
    if key == input.TAB or key == input.Q then
        cancel_target("Operation cancelled")
        return
    end

    if key == input.UP or key == input.W then
        change_selection(-1)
        return
    end

    if key == input.DOWN or key == input.S then
        change_selection(1)
        return
    end

    if key == input.HOME then
        selected = 1
        clamp_selection()
        dirty = true
        return
    end

    if key == input.END then
        selected = #items
        clamp_selection()
        dirty = true
        return
    end

    if key == string.byte("[") then
        change_selection(-PAGE_SIZE)
        return
    end

    if key == string.byte("]") then
        change_selection(PAGE_SIZE)
        return
    end

    if key == input.BACKSPACE or key == input.LEFT then
        go_parent()
        return
    end

    if key == input.RIGHT or key == input.O then
        local item = selected_item()
        if item and item.isDir then
            open_directory(item)
        else
            set_status("Select a folder")
        end
        return
    end

    if key == input.ENTER then
        choose_target_folder()
    end
end

local function handle_browse_key(key)
    if key == input.Q then
        running = false
        return
    end

    if key == input.UP or key == input.W then
        change_selection(-1)
        return
    end

    if key == input.DOWN or key == input.S then
        change_selection(1)
        return
    end

    if key == input.HOME then
        selected = 1
        clamp_selection()
        dirty = true
        return
    end

    if key == input.END then
        selected = #items
        clamp_selection()
        dirty = true
        return
    end

    if key == string.byte("[") then
        change_selection(-PAGE_SIZE)
        return
    end

    if key == string.byte("]") then
        change_selection(PAGE_SIZE)
        return
    end

    if key == input.BACKSPACE or key == input.LEFT then
        go_parent()
        return
    end

    if key == input.ENTER or key == input.RIGHT then
        open_directory(selected_item())
        return
    end

    if key == input.D or key == input.DELETE then
        delete_selected()
        return
    end

    if key == input.N then
        make_directory()
        return
    end

    if key == input.R then
        rename_selected()
        return
    end

    if key == input.C then
        start_copy_or_move("copy")
        return
    end

    if key == input.M then
        start_copy_or_move("move")
        return
    end

    if key == input.F then
        refresh("Refreshed")
        return
    end

    if key == input.I then
        show_info()
    end
end

local function handle_key(key)
    if modal then
        handle_modal_key(key)
    elseif target_mode then
        handle_target_key(key)
    else
        handle_browse_key(key)
    end
end

refresh("Ready")

while running do
    local key = input.poll()

    if key ~= nil then
        handle_key(key)
    end

    if dirty then
        draw_ui()
        dirty = false
    end

    gfx.present()
end

gfx.clear(COLOR.black)
gfx.present()
