local gfx = require("gfx")
local input = require("input")
local fs = require("fs")
local shell = require("shell")
local buffer = require("buffer")

local args = { ... }

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local FONT_SCALE = 1
local FONT_W = 6 * FONT_SCALE
local FONT_H = 8 * FONT_SCALE
local LINE_H = FONT_H + 3

local THEME = {
    bg_lower = 0x00181718,
    bg_bottom = 0x00131315,

    tile_selected = 0x00242424,
    tile_glow = 0x00505052,

    text = 0x00F4F4F4,
    text_dim = 0x00A8A8AA,
    white = 0x00F4F4F4,

    neon = 0x00E02915,
    neon_dim = 0x006B2720,

    tray = 0x00141415,
    tray_border = 0x00333335,
    tray_widget = 0x001D1D1F,
}

local COLOR_BG = THEME.bg_lower
local COLOR_PANEL = THEME.tray
local COLOR_BORDER = THEME.tray_border
local COLOR_TEXT = THEME.text
local COLOR_MUTED = THEME.text_dim
local COLOR_ACCENT = THEME.neon
local COLOR_YELLOW = THEME.neon
local COLOR_RED = THEME.neon
local COLOR_SELECTION = THEME.neon_dim
local COLOR_CURSOR = THEME.white
local COLOR_BLACK = THEME.bg_bottom

local DEFAULT_NOTES_DIR = "0:/notes"
local MAX_UNDO = 40
local TAB_WIDTH = 4

local file_path = nil
local lines = { "" }
local cursor_x = 1
local cursor_y = 1
local scroll_x = 1
local scroll_y = 1
local dirty = false
local running = true
local status = "READY"
local undo_stack = {}
local sel_line = nil
local sel_col = nil
local modal = nil
local needs_draw = true

local function lower_control_key(key)
    if key >= string.byte("A") and key <= string.byte("Z") then
        return key - string.byte("A") + string.byte("a")
    end

    return key
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

local function normalize_path(path)
    path = tostring(path or ""):gsub("\\", "/")

    if path == "" then
        return ""
    end

    if path:sub(1, 2) == "./" then
        path = path:sub(3)
    end

    if path:sub(1, 2) == "0:" then
        if path:sub(3, 3) ~= "/" then
            path = "0:/" .. path:sub(3)
        end
    elseif path:sub(1, 1) == "/" then
        path = "0:" .. path
    else
        path = "0:/" .. path
    end

    while #path > 3 and path:sub(-1) == "/" do
        path = path:sub(1, -2)
    end

    return path
end

local function display_path(path)
    path = normalize_path(path)

    if path == "0:/" or path == "0:" then
        return "/"
    end

    if path:sub(1, 3) == "0:/" then
        return "/" .. path:sub(4)
    end

    return path
end

local function path_dir(path)
    local dir = fs.getDir(path)

    if dir == nil or dir == "" or dir == "0:" then
        return "0:/"
    end

    return normalize_path(dir)
end

local function ensure_directory(path)
    path = normalize_path(path)

    if path == "" or path == "0:/" then
        return true
    end

    if path:sub(1, 3) ~= "0:/" then
        return false, "Invalid directory path"
    end

    local current = "0:"
    local remainder = path:sub(4)

    for part in remainder:gmatch("[^/]+") do
        current = current .. "/" .. part

        if fs.exists(current) then
            if not fs.isDir(current) then
                return false, current .. " is not a folder"
            end
        else
            local ok, err = pcall(fs.makeDir, current)

            if not ok then
                return false, tostring(err)
            end
        end
    end

    return true
end

local function valid_file_name(name)
    name = tostring(name or "")

    if name == "" or name == "." or name == ".." then
        return nil, "Enter a file name"
    end

    if name:find("/", 1, true) or name:find("\\", 1, true) or
       name:find(":", 1, true) or name:find('"', 1, true) then
        return nil, "Name cannot contain slash, colon or quote"
    end

    if not name:find("%.") then
        name = name .. ".txt"
    end

    return name
end

local function layout()
    local header_h = 62
    local footer_h = 26
    local gutter_w = 44
    local text_x = gutter_w + 12
    local text_y = header_h + 12
    local rows = math.max(1, math.floor((SCREEN_H - text_y - footer_h - 8) / LINE_H))
    local cols = math.max(1, math.floor((SCREEN_W - text_x - 12) / FONT_W))

    return {
        header_h = header_h,
        footer_h = footer_h,
        gutter_w = gutter_w,
        text_x = text_x,
        text_y = text_y,
        rows = rows,
        cols = cols,
    }
end

local function copy_lines(source)
    local result = {}

    for i = 1, #source do
        result[i] = source[i]
    end

    return result
end

local function split_lines(text)
    text = tostring(text or ""):gsub("\r\n", "\n"):gsub("\r", "\n")

    local result = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        result[#result + 1] = line
    end

    if #result == 0 then
        result[1] = ""
    end

    return result
end

local function read_document(path)
    local handle, err = io.open(path, "r")

    if not handle then
        return { "" }, false, err
    end

    local text = handle:read("*a") or ""
    handle:close()

    return split_lines(text), true, nil
end

local function write_document()
    local ok, err = ensure_directory(path_dir(file_path))

    if not ok then
        status = "SAVE FAILED: " .. tostring(err)
        return false
    end

    local handle, open_err = io.open(file_path, "w")

    if not handle then
        status = "SAVE FAILED: " .. tostring(open_err)
        return false
    end

    for i = 1, #lines do
        handle:write(lines[i] or "")

        if i < #lines then
            handle:write("\n")
        end
    end

    handle:close()
    dirty = false
    status = "SAVED"
    return true
end

local function line_length(y)
    return #(lines[y] or "")
end

local function clamp_cursor()
    cursor_y = clamp(cursor_y, 1, #lines)
    cursor_x = clamp(cursor_x, 1, line_length(cursor_y) + 1)
end

local function compare_pos(a_line, a_col, b_line, b_col)
    if a_line < b_line then
        return -1
    elseif a_line > b_line then
        return 1
    elseif a_col < b_col then
        return -1
    elseif a_col > b_col then
        return 1
    end

    return 0
end

local function has_selection()
    return sel_line ~= nil and compare_pos(sel_line, sel_col, cursor_y, cursor_x) ~= 0
end

local function clear_selection()
    sel_line = nil
    sel_col = nil
end

local function selection_range()
    if not has_selection() then
        return nil
    end

    if compare_pos(sel_line, sel_col, cursor_y, cursor_x) <= 0 then
        return sel_line, sel_col, cursor_y, cursor_x
    end

    return cursor_y, cursor_x, sel_line, sel_col
end

local function position_selected(y, x)
    local start_y, start_x, end_y, end_x = selection_range()

    if not start_y then
        return false
    end

    return compare_pos(start_y, start_x, y, x) <= 0 and
           compare_pos(y, x, end_y, end_x) < 0
end

local function begin_selection(old_y, old_x, extend)
    if extend then
        if sel_line == nil then
            sel_line = old_y
            sel_col = old_x
        end
    else
        clear_selection()
    end
end

local function finish_selection()
    if sel_line and compare_pos(sel_line, sel_col, cursor_y, cursor_x) == 0 then
        clear_selection()
    end
end

local function push_undo()
    undo_stack[#undo_stack + 1] = {
        lines = copy_lines(lines),
        cursor_x = cursor_x,
        cursor_y = cursor_y,
        scroll_x = scroll_x,
        scroll_y = scroll_y,
    }

    if #undo_stack > MAX_UNDO then
        table.remove(undo_stack, 1)
    end
end

local function undo()
    local state = table.remove(undo_stack)

    if not state then
        status = "NOTHING TO UNDO"
        return
    end

    lines = copy_lines(state.lines)
    cursor_x = state.cursor_x
    cursor_y = state.cursor_y
    scroll_x = state.scroll_x
    scroll_y = state.scroll_y
    clear_selection()
    dirty = true
    status = "UNDO"
    clamp_cursor()
end

local function selected_text()
    local start_y, start_x, end_y, end_x = selection_range()

    if not start_y then
        return ""
    end

    if start_y == end_y then
        return (lines[start_y] or ""):sub(start_x, end_x - 1)
    end

    local result = { (lines[start_y] or ""):sub(start_x) }

    for y = start_y + 1, end_y - 1 do
        result[#result + 1] = lines[y] or ""
    end

    result[#result + 1] = (lines[end_y] or ""):sub(1, end_x - 1)
    return table.concat(result, "\n")
end

local function delete_selection_raw()
    local start_y, start_x, end_y, end_x = selection_range()

    if not start_y then
        return false
    end

    if start_y == end_y then
        local line = lines[start_y] or ""
        lines[start_y] = line:sub(1, start_x - 1) .. line:sub(end_x)
    else
        local first = lines[start_y] or ""
        local last = lines[end_y] or ""
        lines[start_y] = first:sub(1, start_x - 1) .. last:sub(end_x)

        for y = end_y, start_y + 1, -1 do
            table.remove(lines, y)
        end
    end

    cursor_y = start_y
    cursor_x = start_x
    clear_selection()
    clamp_cursor()
    return true
end

local function delete_selection()
    if not has_selection() then
        return false
    end

    push_undo()
    delete_selection_raw()
    dirty = true
    return true
end

local function insert_text(text)
    push_undo()

    if has_selection() then
        delete_selection_raw()
    end

    local parts = split_lines(text)

    if #parts == 1 then
        local line = lines[cursor_y] or ""
        lines[cursor_y] = line:sub(1, cursor_x - 1) .. parts[1] .. line:sub(cursor_x)
        cursor_x = cursor_x + #parts[1]
    else
        local line = lines[cursor_y] or ""
        local before = line:sub(1, cursor_x - 1)
        local after = line:sub(cursor_x)

        lines[cursor_y] = before .. parts[1]

        for i = 2, #parts do
            table.insert(lines, cursor_y + i - 1, parts[i])
        end

        cursor_y = cursor_y + #parts - 1
        lines[cursor_y] = (lines[cursor_y] or "") .. after
        cursor_x = #parts[#parts] + 1
    end

    clear_selection()
    dirty = true
    clamp_cursor()
end

local function insert_new_line()
    push_undo()

    if has_selection() then
        delete_selection_raw()
    end

    local line = lines[cursor_y] or ""
    local before = line:sub(1, cursor_x - 1)
    local after = line:sub(cursor_x)

    lines[cursor_y] = before
    table.insert(lines, cursor_y + 1, after)
    cursor_y = cursor_y + 1
    cursor_x = 1
    clear_selection()
    dirty = true
end

local function backspace()
    if delete_selection() then
        return
    end

    if cursor_y == 1 and cursor_x == 1 then
        return
    end

    push_undo()

    if cursor_x > 1 then
        local line = lines[cursor_y] or ""
        lines[cursor_y] = line:sub(1, cursor_x - 2) .. line:sub(cursor_x)
        cursor_x = cursor_x - 1
    else
        local previous_len = line_length(cursor_y - 1)
        lines[cursor_y - 1] = (lines[cursor_y - 1] or "") .. (lines[cursor_y] or "")
        table.remove(lines, cursor_y)
        cursor_y = cursor_y - 1
        cursor_x = previous_len + 1
    end

    dirty = true
    clamp_cursor()
end

local function delete_forward()
    if delete_selection() then
        return
    end

    local line = lines[cursor_y] or ""

    if cursor_x > #line and cursor_y >= #lines then
        return
    end

    push_undo()

    if cursor_x <= #line then
        lines[cursor_y] = line:sub(1, cursor_x - 1) .. line:sub(cursor_x + 1)
    else
        lines[cursor_y] = line .. (lines[cursor_y + 1] or "")
        table.remove(lines, cursor_y + 1)
    end

    dirty = true
    clamp_cursor()
end

local function move_cursor(dx, dy, extend)
    local old_y = cursor_y
    local old_x = cursor_x

    begin_selection(old_y, old_x, extend)

    if dy ~= 0 then
        cursor_y = clamp(cursor_y + dy, 1, #lines)
        cursor_x = math.min(old_x, line_length(cursor_y) + 1)
    else
        cursor_x = cursor_x + dx

        if cursor_x < 1 then
            if cursor_y > 1 then
                cursor_y = cursor_y - 1
                cursor_x = line_length(cursor_y) + 1
            else
                cursor_x = 1
            end
        elseif cursor_x > line_length(cursor_y) + 1 then
            if cursor_y < #lines then
                cursor_y = cursor_y + 1
                cursor_x = 1
            else
                cursor_x = line_length(cursor_y) + 1
            end
        end
    end

    clamp_cursor()
    finish_selection()
end

local function page_move(amount, extend)
    local old_y = cursor_y
    local old_x = cursor_x

    begin_selection(old_y, old_x, extend)
    cursor_y = clamp(cursor_y + amount, 1, #lines)
    cursor_x = math.min(old_x, line_length(cursor_y) + 1)
    finish_selection()
end

local function home_key(extend)
    local old_y = cursor_y
    local old_x = cursor_x

    begin_selection(old_y, old_x, extend)
    cursor_x = 1
    finish_selection()
end

local function end_key(extend)
    local old_y = cursor_y
    local old_x = cursor_x

    begin_selection(old_y, old_x, extend)
    cursor_x = line_length(cursor_y) + 1
    finish_selection()
end

local function copy_selection()
    local copied
    local error_message

    if not has_selection() then
        status = "NO SELECTION"
        return
    end

    copied, error_message = buffer.clipboard_set(selected_text())

    if copied ~= true then
        status = "CLIPBOARD FAILED"

        if error_message ~= nil and error_message ~= "" then
            status = status .. ": " .. tostring(error_message)
        end

        return
    end

    status = "COPIED"
end

local function cut_selection()
    local copied
    local error_message

    if not has_selection() then
        status = "NO SELECTION"
        return
    end

    copied, error_message = buffer.clipboard_set(selected_text())

    if copied ~= true then
        status = "CLIPBOARD FAILED"

        if error_message ~= nil and error_message ~= "" then
            status = status .. ": " .. tostring(error_message)
        end

        return
    end

    delete_selection()
    status = "CUT"
end

local function paste_clipboard()
    local clipboard_text
    local error_message

    clipboard_text, error_message = buffer.clipboard_get()

    if clipboard_text == nil then
        if error_message ~= nil and error_message ~= "" then
            status = "CLIPBOARD FAILED: " .. tostring(error_message)
        else
            status = "CLIPBOARD EMPTY"
        end

        return
    end

    insert_text(clipboard_text)
    status = "PASTED"
end

local function select_all()
    sel_line = 1
    sel_col = 1
    cursor_y = #lines
    cursor_x = line_length(cursor_y) + 1
    status = "SELECTED ALL"
end

local function char_width(ch)
    if ch == "\t" then
        return TAB_WIDTH
    end

    return 1
end

local function visual_col_for_index(line, index)
    local visual = 1
    line = tostring(line or "")
    index = clamp(index or 1, 1, #line + 1)

    for i = 1, index - 1 do
        visual = visual + char_width(line:sub(i, i))
    end

    return visual
end

local function ensure_cursor_visible(lay)
    local visual_x = visual_col_for_index(lines[cursor_y], cursor_x)

    if cursor_y < scroll_y then
        scroll_y = cursor_y
    elseif cursor_y > scroll_y + lay.rows - 1 then
        scroll_y = cursor_y - lay.rows + 1
    end

    if visual_x < scroll_x then
        scroll_x = visual_x
    elseif visual_x > scroll_x + lay.cols - 1 then
        scroll_x = visual_x - lay.cols + 1
    end

    if scroll_y < 1 then
        scroll_y = 1
    end

    if scroll_x < 1 then
        scroll_x = 1
    end
end

local function visible_line_text(line, start_visual, width)
    local output = {}
    local visual = 1

    for i = 1, #line do
        local ch = line:sub(i, i)
        local char_w = char_width(ch)

        for unit = 1, char_w do
            local current = visual + unit - 1

            if current >= start_visual and current < start_visual + width then
                if ch == "\t" then
                    output[#output + 1] = " "
                else
                    output[#output + 1] = ch
                end
            end
        end

        visual = visual + char_w

        if visual >= start_visual + width then
            break
        end
    end

    return table.concat(output)
end

local function draw_selection_background(line_index, line, lay, y)
    if not has_selection() then
        return
    end

    local visual = 1

    for index = 1, #line do
        local char_w = char_width(line:sub(index, index))

        if position_selected(line_index, index) then
            local start = visual
            local finish = visual + char_w
            local visible_start = math.max(start, scroll_x)
            local visible_finish = math.min(finish, scroll_x + lay.cols)

            if visible_finish > visible_start then
                local x = lay.text_x + (visible_start - scroll_x) * FONT_W
                gfx.fill_rect(x, y, (visible_finish - visible_start) * FONT_W, FONT_H, COLOR_SELECTION)
            end
        end

        visual = visual + char_w
    end
end

local function draw_notes_logo(x, y, scale)
    gfx.text(x, y, "N", COLOR_ACCENT, scale)
    gfx.text(x + 6 * scale, y, "OTES", COLOR_TEXT, scale)
end

local function draw_modal(lay)
    if not modal then
        return
    end

    local box_w = math.min(430, SCREEN_W - 80)
    local box_h = 88
    local box_x = math.floor((SCREEN_W - box_w) / 2)
    local box_y = math.floor((SCREEN_H - box_h) / 2)

    gfx.fill_rect(box_x - 2, box_y - 2, box_w + 4, box_h + 4, COLOR_BORDER)
    gfx.fill_rect(box_x, box_y, box_w, box_h, COLOR_PANEL)

    if modal.type == "exit" then
        gfx.text(box_x + 14, box_y + 14, "SAVE CHANGES BEFORE EXIT?", COLOR_YELLOW, 1)
        gfx.text(box_x + 14, box_y + 44, "Y/ENTER SAVE    N/TAB DISCARD    ESC CANCEL", COLOR_TEXT, 1)
        return
    end

    if modal.type == "discard_open" then
        gfx.text(box_x + 14, box_y + 14, "DISCARD UNSAVED CHANGES?", COLOR_YELLOW, 1)
        gfx.text(box_x + 14, box_y + 44, "Y/ENTER OPEN EXPLORER    N/TAB/ESC CANCEL", COLOR_TEXT, 1)
        return
    end

    if modal.type == "new_file" then
        gfx.text(box_x + 14, box_y + 12, "NEW NOTE IN " .. display_path(modal.dir), COLOR_ACCENT, 1)
        gfx.fill_rect(box_x + 12, box_y + 33, box_w - 24, 24, COLOR_BLACK)
        gfx.rect(box_x + 12, box_y + 33, box_w - 24, 24, COLOR_BORDER)
        gfx.text(box_x + 18, box_y + 41, modal.value, COLOR_TEXT, 1)
        gfx.text(box_x + 14, box_y + 66, "ENTER CREATE    TAB/ESC CANCEL", COLOR_MUTED, 1)
    end
end

local function draw_editor()
    local lay = layout()
    ensure_cursor_visible(lay)

    gfx.clear(COLOR_BG)
    gfx.fill_rect(0, 0, SCREEN_W, lay.header_h, COLOR_PANEL)
    gfx.fill_rect(0, lay.header_h - 1, SCREEN_W, 1, COLOR_BORDER)

    draw_notes_logo(12, 12, 2)

    local title = display_path(file_path)
    if dirty then
        title = title .. "  *"
    end

    gfx.text(12, 38, title, COLOR_TEXT, 1)

    local position_text = "LN " .. cursor_y .. "  COL " .. cursor_x
    gfx.text(SCREEN_W - (#position_text * FONT_W) - 12, 40, position_text, COLOR_MUTED, 1)

    for row = 0, lay.rows - 1 do
        local line_index = scroll_y + row
        local y = lay.text_y + row * LINE_H

        if line_index <= #lines then
            local number = tostring(line_index)
            local number_x = lay.gutter_w - (#number * FONT_W) - 8
            local line = lines[line_index] or ""

            gfx.text(number_x, y, number, COLOR_MUTED, 1)
            gfx.fill_rect(lay.gutter_w, y, 1, FONT_H, COLOR_BORDER)

            draw_selection_background(line_index, line, lay, y)

            local visible = visible_line_text(line, scroll_x, lay.cols)
            gfx.text(lay.text_x, y, visible, COLOR_TEXT, 1)
        end
    end

    local cursor_visual = visual_col_for_index(lines[cursor_y], cursor_x)
    local cursor_screen_x = lay.text_x + (cursor_visual - scroll_x) * FONT_W
    local cursor_screen_y = lay.text_y + (cursor_y - scroll_y) * LINE_H

    if cursor_screen_x >= lay.text_x and cursor_screen_x < SCREEN_W - 4 and
       cursor_screen_y >= lay.text_y and cursor_screen_y < SCREEN_H - lay.footer_h then
        gfx.fill_rect(cursor_screen_x, cursor_screen_y, 2, FONT_H, COLOR_CURSOR)
    end

    gfx.fill_rect(0, SCREEN_H - lay.footer_h, SCREEN_W, lay.footer_h, COLOR_PANEL)
    gfx.fill_rect(0, SCREEN_H - lay.footer_h, SCREEN_W, 1, COLOR_BORDER)
    gfx.text(12, SCREEN_H - 19, status, COLOR_MUTED, 1)

    draw_modal(lay)
end

local function draw_new_file_prompt(dir, value)
    local lay = layout()

    gfx.clear(COLOR_BG)
    draw_notes_logo(18, 24, 3)
    gfx.text(18, 74, "CREATE A NEW NOTE", COLOR_TEXT, 2)
    gfx.text(18, 110, "FOLDER: " .. display_path(dir), COLOR_MUTED, 1)

    local box_w = math.min(520, SCREEN_W - 72)
    local box_x = math.floor((SCREEN_W - box_w) / 2)
    local box_y = math.floor(SCREEN_H / 2 - 40)

    gfx.fill_rect(box_x - 2, box_y - 2, box_w + 4, 82, COLOR_BORDER)
    gfx.fill_rect(box_x, box_y, box_w, 82, COLOR_PANEL)
    gfx.text(box_x + 14, box_y + 12, "FILE NAME", COLOR_TEXT, 1)
    gfx.fill_rect(box_x + 12, box_y + 32, box_w - 24, 25, COLOR_BLACK)
    gfx.rect(box_x + 12, box_y + 32, box_w - 24, 25, COLOR_BORDER)
    gfx.text(box_x + 18, box_y + 40, value, COLOR_TEXT, 1)
    gfx.text(box_x + 14, box_y + 64, "ENTER CREATE    TAB/ESC CANCEL", COLOR_MUTED, 1)
end

local function ask_new_file(dir)
    local value = "untitled.txt"
    local prompt_status = ""
    local dirty_prompt = true

    while true do
        if dirty_prompt then
            draw_new_file_prompt(dir, value)

            if prompt_status ~= "" then
                gfx.text(18, SCREEN_H - 28, prompt_status, COLOR_RED, 1)
            end

            dirty_prompt = false
        end

        local key, mods = input.poll()
        mods = mods or {}

        if key ~= nil then
            if key == input.ENTER then
                local name, err = valid_file_name(value)

                if name then
                    return normalize_path(fs.combine(dir, name))
                end

                prompt_status = err or "INVALID FILE NAME"
                dirty_prompt = true
            elseif key == input.TAB or key == input.ESCAPE then
                return nil
            elseif key == input.BACKSPACE then
                value = value:sub(1, -2)
                dirty_prompt = true
            elseif not mods.ctrl and key >= 32 and key <= 126 then
                local char = string.char(key)

                if char ~= "/" and char ~= "\\" and char ~= ":" and char ~= '"' and #value < 80 then
                    value = value .. char
                    dirty_prompt = true
                end
            end
        end

        gfx.present()
    end
end

local function shell_quote(value)
    return '"' .. tostring(value or ""):gsub('"', "") .. '"'
end

local function choose_in_explorer(kind, start_path)
    local flag

    if kind == "folder" then
        flag = "--pick-folder"
    elseif kind == "path" then
        flag = "--pick-path"
    else
        flag = "--pick-file"
    end

    local start = normalize_path(start_path or "0:/")
    local ok
    local result
    local picked
    local buffer_error

    buffer.clear("explorer.pick_result")

    gfx.clear(COLOR_BG)
    gfx.present()

    ok, result = pcall(
        shell.exec,
        "open " .. shell_quote("explorer") ..
        " " .. flag ..
        " " .. shell_quote(start)
    )

    if not ok then
        status = "EXPLORER FAILED: " .. tostring(result)
        needs_draw = true
        return nil
    end

    if result ~= 0 then
        status = "EXPLORER EXITED: " .. tostring(result)
        needs_draw = true
        return nil
    end

    picked, buffer_error = buffer.take("explorer.pick_result")

    if picked == nil then
        if buffer_error ~= nil then
            status = "PICKER FAILED: " .. tostring(buffer_error)
            needs_draw = true
        end

        return nil
    end

    return normalize_path(picked)
end

local function load_editor_file(path)
    local loaded
    local exists
    local load_error

    path = normalize_path(path)

    if path == "" or (fs.exists(path) and fs.isDir(path)) then
        status = "SELECT A FILE"
        needs_draw = true
        return false
    end

    loaded, exists, load_error = read_document(path)

    lines = loaded
    file_path = path
    cursor_x = 1
    cursor_y = 1
    scroll_x = 1
    scroll_y = 1
    dirty = false
    clear_selection()
    undo_stack = {}

    if exists then
        status = "OPENED"
    else
        status = "NEW FILE"
    end

    needs_draw = true
    return true
end

local function open_from_explorer()
    local picked = choose_in_explorer("file", path_dir(file_path or "0:/"))

    if not picked then
        status = "OPEN CANCELLED"
        needs_draw = true
        return
    end

    load_editor_file(picked)
end

local function save_as_from_explorer()
    local folder = choose_in_explorer("folder", path_dir(file_path or "0:/"))

    if not folder then
        status = "SAVE AS CANCELLED"
        needs_draw = true
        return
    end

    local new_path = ask_new_file(folder)

    if not new_path then
        status = "SAVE AS CANCELLED"
        needs_draw = true
        return
    end

    file_path = new_path

    if write_document() then
        status = "SAVED AS " .. display_path(file_path)
    end

    needs_draw = true
end

local function request_open()
    if dirty then
        modal = { type = "discard_open" }
        needs_draw = true
        return
    end

    open_from_explorer()
end

local function request_exit()
    if dirty then
        modal = { type = "exit" }
    else
        running = false
    end

    needs_draw = true
end

local function handle_exit_modal(key)
    local control = lower_control_key(key)

    if control == input.Y or key == input.ENTER then
        if write_document() then
            running = false
        end
        modal = nil
        needs_draw = true
        return
    end

    if control == input.N or key == input.TAB then
        modal = nil
        running = false
        return
    end

    if key == input.ESCAPE then
        modal = nil
        status = "EXIT CANCELLED"
        needs_draw = true
    end
end

local function handle_open_modal(key)
    local control = lower_control_key(key)

    if control == input.Y or key == input.ENTER then
        modal = nil
        open_from_explorer()
        return
    end

    if control == input.N or key == input.TAB or key == input.ESCAPE then
        modal = nil
        status = "OPEN CANCELLED"
        needs_draw = true
    end
end

local function handle_editor_key(key, mods)
    mods = mods or {}

    if modal and modal.type == "exit" then
        handle_exit_modal(key)
        return
    end

    if modal and modal.type == "discard_open" then
        handle_open_modal(key)
        return
    end

    local control = lower_control_key(key)

    if mods.ctrl then
        if control == input.A then
            select_all()
        elseif control == input.C then
            copy_selection()
        elseif control == input.X then
            cut_selection()
        elseif control == input.V then
            paste_clipboard()
        elseif control == input.Z then
            undo()
        elseif control == input.O then
            request_open()
        elseif control == input.S then
            if mods.shift then
                save_as_from_explorer()
            else
                write_document()
            end
        elseif control == input.Q then
            request_exit()
        end

        needs_draw = true
        return
    end

    if key == input.ESCAPE then
        request_exit()
    elseif key == input.F2 then
        write_document()
    elseif key == input.LEFT then
        move_cursor(-1, 0, mods.shift)
    elseif key == input.RIGHT then
        move_cursor(1, 0, mods.shift)
    elseif key == input.UP then
        move_cursor(0, -1, mods.shift)
    elseif key == input.DOWN then
        move_cursor(0, 1, mods.shift)
    elseif key == input.PAGE_UP then
        page_move(-layout().rows, mods.shift)
    elseif key == input.PAGE_DOWN then
        page_move(layout().rows, mods.shift)
    elseif key == input.HOME then
        home_key(mods.shift)
    elseif key == input.END then
        end_key(mods.shift)
    elseif key == input.BACKSPACE then
        backspace()
    elseif key == input.DELETE then
        delete_forward()
    elseif key == input.TAB then
        insert_text("\t")
    elseif key == input.ENTER then
        insert_new_line()
    elseif key >= 32 and key <= 126 then
        insert_text(string.char(key))
    end

    needs_draw = true
end

local function resolve_target_path()
    local requested = args[1]

    if requested == nil or requested == "" then
        local selected_path = choose_in_explorer("path", "0:/")

        if not selected_path then
            return nil
        end

        if fs.exists(selected_path) and fs.isDir(selected_path) then
            return ask_new_file(selected_path)
        end

        return selected_path
    end

    local path = normalize_path(requested)

    if fs.exists(path) and fs.isDir(path) then
        return choose_in_explorer("file", path)
    end

    local ok, err = ensure_directory(path_dir(path))
    if not ok then
        status = "CANNOT CREATE FOLDER: " .. tostring(err)
        return nil
    end

    return path
end

file_path = resolve_target_path()

if file_path then
    load_editor_file(file_path)

    while running do
        local key, mods = input.poll()

        if key ~= nil then
            handle_editor_key(key, mods)
        end

        if needs_draw then
            draw_editor()
            needs_draw = false
        end

        gfx.present()
    end
end

gfx.clear(COLOR_BLACK)
gfx.present()
