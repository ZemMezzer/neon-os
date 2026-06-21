local gfx = require("gfx")
local input = require("input")
local shell = require("shell")

local launch_args = { ... }

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local FONT_SCALE = 1
local FONT_W = 6 * FONT_SCALE
local FONT_H = 8 * FONT_SCALE

local PAD = 8
local HEADER_H = 0
local FOOTER_H = 26
local MAX_OUTPUT_LINES = 700
local MAX_COMMAND_LENGTH = 240

local THEME = {
    bg_top = 0x00262523,
    bg_upper = 0x00212020,
    bg_mid = 0x001D1C1D,
    bg_lower = 0x00181718,
    bg_bottom = 0x00131315,

    grid = 0x002B2C2E,

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
local COLOR_INPUT = THEME.tray_widget
local COLOR_BORDER = THEME.tray_border
local COLOR_TEXT = THEME.text
local COLOR_CURSOR = THEME.white

local output_lines = {}
local command_history = {}
local history_index = nil
local input_line = ""
local cursor = 1
local scroll_offset = 0
local running = true
local last_status = "READY"
local frame_counter = 0

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

local function trim(text)
    text = tostring(text or "")
    text = text:gsub("^%s+", "")
    text = text:gsub("%s+$", "")
    return text
end

local function ends_with(text, suffix)
    if #suffix > #text then
        return false
    end

    return text:sub(#text - #suffix + 1) == suffix
end

local function layout()
    local output_top = HEADER_H + PAD
    local output_bottom = SCREEN_H - FOOTER_H - PAD
    local output_rows = math.max(
        1,
        math.floor((output_bottom - output_top + 1) / FONT_H)
    )
    local columns = math.max(8, math.floor((SCREEN_W - PAD * 2) / FONT_W))
    local input_columns = math.max(1, columns - 2)

    return {
        output_top = output_top,
        output_bottom = output_bottom,
        output_rows = output_rows,
        columns = columns,
        input_columns = input_columns,
    }
end

local function add_raw_line(text)
    output_lines[#output_lines + 1] = tostring(text or "")

    while #output_lines > MAX_OUTPUT_LINES do
        table.remove(output_lines, 1)
    end
end

local function add_wrapped_line(text)
    local columns = layout().columns
    text = tostring(text or "")

    if text == "" then
        add_raw_line("")
        return
    end

    while #text > columns do
        add_raw_line(text:sub(1, columns))
        text = text:sub(columns + 1)
    end

    add_raw_line(text)
end

local function add_output(text)
    text = tostring(text or "")
    text = text:gsub("\r\n", "\n")
    text = text:gsub("\r", "\n")
    text = text:gsub("\t", "    ")

    local start = 1

    while true do
        local line_end = text:find("\n", start, true)

        if not line_end then
            if start <= #text then
                add_wrapped_line(text:sub(start))
            end

            break
        end

        add_wrapped_line(text:sub(start, line_end - 1))
        start = line_end + 1

        if start > #text then
            break
        end
    end

    scroll_offset = 0
end

local function clear_output()
    output_lines = {}
    scroll_offset = 0
end

local function shell_quote(value)
    value = tostring(value or "")

    if value:find('"', 1, true) then
        return nil, 'Arguments containing " are not supported by the shell parser'
    end

    if value == "" then
        return '""'
    end

    if value:find(" ", 1, true) or value:find("\t", 1, true) then
        return '"' .. value .. '"'
    end

    return value
end

local function remember_command(command)
    if command == "" then
        return
    end

    if command_history[#command_history] ~= command then
        command_history[#command_history + 1] = command
    end

    while #command_history > 100 do
        table.remove(command_history, 1)
    end
end

local function show_history()
    if #command_history == 0 then
        add_output("(history is empty)")
        return
    end

    for index = 1, #command_history do
        add_output(string.format("%3d  %s", index, command_history[index]))
    end
end

local function run_command(command)
    command = trim(command)

    if command == "" then
        return
    end

    if command == "clear" or command == "cls" then
        clear_output()
        last_status = "CLEARED"
        return
    end

    if command == "exit" or command == "quit" then
        running = false
        return
    end

    if command == "history" then
        add_output("> history")
        show_history()
        last_status = "OK"
        return
    end

    add_output("> " .. command)
    remember_command(command)

    if type(shell.exec_capture) ~= "function" then
        add_output("Terminal error: shell.exec_capture() is missing.")
        add_output("Install the updated lua_shell_api.c bridge.")
        last_status = "ERROR"
        return
    end

    local ok, status, output = pcall(shell.exec_capture, command)

    if not ok then
        add_output("Terminal error: " .. tostring(status))
        last_status = "ERROR"
        return
    end

    if output and output ~= "" then
        add_output(output)
    end

    if status == 0 then
        last_status = "OK"
    else
        last_status = "EXIT " .. tostring(status)
    end
end

local function append_character(character)
    if #input_line >= MAX_COMMAND_LENGTH then
        last_status = "COMMAND TOO LONG"
        return
    end

    input_line =
        input_line:sub(1, cursor - 1) ..
        character ..
        input_line:sub(cursor)

    cursor = cursor + #character
end

local function backspace()
    if cursor <= 1 then
        return
    end

    input_line =
        input_line:sub(1, cursor - 2) ..
        input_line:sub(cursor)

    cursor = cursor - 1
end

local function delete_forward()
    if cursor > #input_line then
        return
    end

    input_line =
        input_line:sub(1, cursor - 1) ..
        input_line:sub(cursor + 1)
end

local function history_up()
    if #command_history == 0 then
        return
    end

    if history_index == nil then
        history_index = #command_history
    else
        history_index = math.max(1, history_index - 1)
    end

    input_line = command_history[history_index]
    cursor = #input_line + 1
end

local function history_down()
    if history_index == nil then
        return
    end

    if history_index < #command_history then
        history_index = history_index + 1
        input_line = command_history[history_index]
    else
        history_index = nil
        input_line = ""
    end

    cursor = #input_line + 1
end

local function draw_output(area)
    local max_scroll = math.max(0, #output_lines - area.output_rows)
    scroll_offset = clamp(scroll_offset, 0, max_scroll)

    local start = math.max(
        1,
        #output_lines - area.output_rows - scroll_offset + 1
    )

    for row = 0, area.output_rows - 1 do
        local line = output_lines[start + row]

        if line then
            gfx.text(
                PAD,
                area.output_top + row * FONT_H,
                line,
                COLOR_TEXT,
                FONT_SCALE
            )
        end
    end
end

local function draw_input(area)
    local prompt = "> "
    local available = area.input_columns
    local view_start = 1

    if cursor > available then
        view_start = cursor - available
    end

    if view_start < 1 then
        view_start = 1
    end

    local shown = input_line:sub(view_start, view_start + available - 1)
    local cursor_column = #prompt + cursor - view_start

    gfx.fill_rect(
        0,
        SCREEN_H - FOOTER_H,
        SCREEN_W,
        FOOTER_H,
        COLOR_INPUT
    )
    gfx.line(
        0,
        SCREEN_H - FOOTER_H,
        SCREEN_W - 1,
        SCREEN_H - FOOTER_H,
        COLOR_BORDER
    )

    gfx.text(PAD, SCREEN_H - FOOTER_H + 9, prompt, COLOR_CURSOR, FONT_SCALE)
    gfx.text(
        PAD + #prompt * FONT_W,
        SCREEN_H - FOOTER_H + 9,
        shown,
        COLOR_TEXT,
        FONT_SCALE
    )

    if (frame_counter % 36) < 20 then
        local cursor_x = PAD + cursor_column * FONT_W

        gfx.fill_rect(
            cursor_x,
            SCREEN_H - FOOTER_H + 8,
            2,
            FONT_H + 1,
            COLOR_CURSOR
        )
    end
end

local function draw()
    local area = layout()

    gfx.clear(COLOR_BG)

    draw_output(area)
    draw_input(area)

    gfx.present()
end

local function lower_ascii_key(key)
    if key >= string.byte("A") and key <= string.byte("Z") then
        return key - string.byte("A") + string.byte("a")
    end

    return key
end

local function handle_key(key, modifiers)
    modifiers = modifiers or {}

    if not key then
        return
    end

    local lower = lower_ascii_key(key)

    if key == input.ESCAPE or
       (modifiers.ctrl and lower == string.byte("q")) or
       key == 17 then
        running = false
        return
    end

    if (modifiers.ctrl and lower == string.byte("l")) or key == 12 then
        clear_output()
        last_status = "CLEARED"
        return
    end

    if (modifiers.ctrl and lower == string.byte("c")) or key == 3 then
        input_line = ""
        cursor = 1
        history_index = nil
        last_status = "INPUT CANCELLED"
        return
    end

    if key == input.ENTER then
        local command = input_line

        input_line = ""
        cursor = 1
        history_index = nil
        run_command(command)
        return
    end

    if key == input.BACKSPACE then
        history_index = nil
        backspace()
        return
    end

    if key == input.DELETE then
        history_index = nil
        delete_forward()
        return
    end

    if key == input.LEFT then
        cursor = math.max(1, cursor - 1)
        return
    end

    if key == input.RIGHT then
        cursor = math.min(#input_line + 1, cursor + 1)
        return
    end

    if key == input.HOME then
        cursor = 1
        return
    end

    if key == input.END then
        cursor = #input_line + 1
        return
    end

    if key == input.UP then
        history_up()
        return
    end

    if key == input.DOWN then
        history_down()
        return
    end

    if key == input.PAGE_UP then
        scroll_offset = scroll_offset + layout().output_rows
        return
    end

    if key == input.PAGE_DOWN then
        scroll_offset = math.max(0, scroll_offset - layout().output_rows)
        return
    end

    if modifiers.ctrl or modifiers.alt then
        return
    end

    if key >= 32 and key <= 126 then
        history_index = nil
        append_character(string.char(key))
    end
end

local function startup_command_from_args(argv)
    if #argv == 0 then
        return nil
    end

    local command_name

    if argv[1] == "--" then
        command_name = argv[2]

        if not command_name then
            add_output("Usage: terminal --<command> [arguments...]")
            return nil
        end

        table.remove(argv, 1)

    elseif argv[1]:sub(1, 2) == "--" then
        command_name = argv[1]:sub(3)

        if command_name == "" then
            add_output("Usage: terminal --<command> [arguments...]")
            return nil
        end

        argv[1] = command_name

    elseif ends_with(tostring(argv[1] or ""):lower(), ".sh") then
        command_name = argv[1]

    else
        add_output("Usage: terminal --<command> [arguments...]")
        add_output('Example: terminal --echo "Hello world"')
        return nil
    end

    command_name = tostring(argv[1] or "")

    if ends_with(command_name:lower(), ".sh") then
        if #argv ~= 1 then
            return nil
        end

        local quoted, quote_error = shell_quote(command_name)

        if not quoted then
            add_output("Argument error: " .. quote_error)
            return nil
        end

        return "sh " .. quoted
    end

    local parts = {}

    for index = 1, #argv do
        local quoted, quote_error = shell_quote(argv[index])

        if not quoted then
            add_output("Argument error: " .. quote_error)
            return nil
        end

        parts[#parts + 1] = quoted
    end

    return table.concat(parts, " ")
end

local startup_command = startup_command_from_args(launch_args)
if startup_command then
    run_command(startup_command)
end

while running do
    frame_counter = frame_counter + 1

    local key, modifiers = input.poll()
    handle_key(key, modifiers)

    draw()
end
