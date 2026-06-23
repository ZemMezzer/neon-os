local Config = require("notes_config")
local Util = require("notes_util")

local Document = {}

function Document.new(screen_width, screen_height)
    local state = {
        screen_width = screen_width,
        screen_height = screen_height,

        file_path = nil,
        lines = { "" },
        cursor_x = 1,
        cursor_y = 1,
        scroll_x = 1,
        scroll_y = 1,
        dirty = false,
        running = true,
        undo_stack = {},
        sel_line = nil,
        sel_col = nil,
        needs_draw = true,
    }

    local function line_length(y)
        return #(state.lines[y] or "")
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

    local function clear_selection()
        state.sel_line = nil
        state.sel_col = nil
    end

    local function selection_range()
        if not state.has_selection() then
            return nil
        end

        if compare_pos(
            state.sel_line,
            state.sel_col,
            state.cursor_y,
            state.cursor_x
        ) <= 0 then
            return state.sel_line, state.sel_col, state.cursor_y, state.cursor_x
        end

        return state.cursor_y, state.cursor_x, state.sel_line, state.sel_col
    end

    local function begin_selection(old_y, old_x, extend)
        if extend then
            if state.sel_line == nil then
                state.sel_line = old_y
                state.sel_col = old_x
            end
        else
            clear_selection()
        end
    end

    local function finish_selection()
        if state.sel_line and compare_pos(
            state.sel_line,
            state.sel_col,
            state.cursor_y,
            state.cursor_x
        ) == 0 then
            clear_selection()
        end
    end

    local function push_undo()
        state.undo_stack[#state.undo_stack + 1] = {
            lines = Util.copy_lines(state.lines),
            cursor_x = state.cursor_x,
            cursor_y = state.cursor_y,
            scroll_x = state.scroll_x,
            scroll_y = state.scroll_y,
        }

        if #state.undo_stack > Config.MAX_UNDO then
            table.remove(state.undo_stack, 1)
        end
    end

    local function delete_selection_raw()
        local start_y, start_x, end_y, end_x = selection_range()

        if not start_y then
            return false
        end

        if start_y == end_y then
            local line = state.lines[start_y] or ""
            state.lines[start_y] = line:sub(1, start_x - 1) .. line:sub(end_x)
        else
            local first = state.lines[start_y] or ""
            local last = state.lines[end_y] or ""
            state.lines[start_y] = first:sub(1, start_x - 1) .. last:sub(end_x)

            for y = end_y, start_y + 1, -1 do
                table.remove(state.lines, y)
            end
        end

        state.cursor_y = start_y
        state.cursor_x = start_x
        clear_selection()
        state.clamp_cursor()
        return true
    end

    function state.layout()
        return Config.make_layout(state.screen_width, state.screen_height)
    end

    function state.line_length(y)
        return line_length(y)
    end

    function state.clamp_cursor()
        state.cursor_y = Util.clamp(state.cursor_y, 1, #state.lines)
        state.cursor_x = Util.clamp(
            state.cursor_x,
            1,
            line_length(state.cursor_y) + 1
        )
    end

    function state.has_selection()
        return state.sel_line ~= nil and compare_pos(
            state.sel_line,
            state.sel_col,
            state.cursor_y,
            state.cursor_x
        ) ~= 0
    end

    function state.clear_selection()
        clear_selection()
    end

    function state.selection_range()
        return selection_range()
    end

    function state.position_selected(y, x)
        local start_y, start_x, end_y, end_x = selection_range()

        if not start_y then
            return false
        end

        return compare_pos(start_y, start_x, y, x) <= 0 and
            compare_pos(y, x, end_y, end_x) < 0
    end

    function state.load_document(path, loaded_lines, exists)
        state.file_path = path
        state.lines = loaded_lines
        state.cursor_x = 1
        state.cursor_y = 1
        state.scroll_x = 1
        state.scroll_y = 1
        state.dirty = false
        clear_selection()
        state.undo_stack = {}
        state.needs_draw = true
        state.clamp_cursor()
    end

    function state.undo()
        local previous = table.remove(state.undo_stack)

        if not previous then
            return false
        end

        state.lines = Util.copy_lines(previous.lines)
        state.cursor_x = previous.cursor_x
        state.cursor_y = previous.cursor_y
        state.scroll_x = previous.scroll_x
        state.scroll_y = previous.scroll_y
        clear_selection()
        state.dirty = true
        state.needs_draw = true
        state.clamp_cursor()
        return true
    end

    function state.selected_text()
        local start_y, start_x, end_y, end_x = selection_range()

        if not start_y then
            return ""
        end

        if start_y == end_y then
            return (state.lines[start_y] or ""):sub(start_x, end_x - 1)
        end

        local result = { (state.lines[start_y] or ""):sub(start_x) }

        for y = start_y + 1, end_y - 1 do
            result[#result + 1] = state.lines[y] or ""
        end

        result[#result + 1] = (state.lines[end_y] or ""):sub(1, end_x - 1)
        return table.concat(result, "\n")
    end

    function state.delete_selection()
        if not state.has_selection() then
            return false
        end

        push_undo()
        delete_selection_raw()
        state.dirty = true
        state.needs_draw = true
        return true
    end

    function state.insert_text(text)
        push_undo()

        if state.has_selection() then
            delete_selection_raw()
        end

        local parts = Util.split_lines(text)

        if #parts == 1 then
            local line = state.lines[state.cursor_y] or ""
            state.lines[state.cursor_y] = line:sub(1, state.cursor_x - 1) ..
                parts[1] .. line:sub(state.cursor_x)
            state.cursor_x = state.cursor_x + #parts[1]
        else
            local line = state.lines[state.cursor_y] or ""
            local before = line:sub(1, state.cursor_x - 1)
            local after = line:sub(state.cursor_x)

            state.lines[state.cursor_y] = before .. parts[1]

            for index = 2, #parts do
                table.insert(
                    state.lines,
                    state.cursor_y + index - 1,
                    parts[index]
                )
            end

            state.cursor_y = state.cursor_y + #parts - 1
            state.lines[state.cursor_y] =
                (state.lines[state.cursor_y] or "") .. after
            state.cursor_x = #parts[#parts] + 1
        end

        clear_selection()
        state.dirty = true
        state.needs_draw = true
        state.clamp_cursor()
    end

    function state.insert_new_line()
        push_undo()

        if state.has_selection() then
            delete_selection_raw()
        end

        local line = state.lines[state.cursor_y] or ""
        local before = line:sub(1, state.cursor_x - 1)
        local after = line:sub(state.cursor_x)

        state.lines[state.cursor_y] = before
        table.insert(state.lines, state.cursor_y + 1, after)
        state.cursor_y = state.cursor_y + 1
        state.cursor_x = 1
        clear_selection()
        state.dirty = true
        state.needs_draw = true
    end

    function state.backspace()
        if state.delete_selection() then
            return
        end

        if state.cursor_y == 1 and state.cursor_x == 1 then
            return
        end

        push_undo()

        if state.cursor_x > 1 then
            local line = state.lines[state.cursor_y] or ""
            state.lines[state.cursor_y] = line:sub(1, state.cursor_x - 2) ..
                line:sub(state.cursor_x)
            state.cursor_x = state.cursor_x - 1
        else
            local previous_len = line_length(state.cursor_y - 1)
            state.lines[state.cursor_y - 1] =
                (state.lines[state.cursor_y - 1] or "") ..
                (state.lines[state.cursor_y] or "")
            table.remove(state.lines, state.cursor_y)
            state.cursor_y = state.cursor_y - 1
            state.cursor_x = previous_len + 1
        end

        state.dirty = true
        state.needs_draw = true
        state.clamp_cursor()
    end

    function state.delete_forward()
        if state.delete_selection() then
            return
        end

        local line = state.lines[state.cursor_y] or ""

        if state.cursor_x > #line and state.cursor_y >= #state.lines then
            return
        end

        push_undo()

        if state.cursor_x <= #line then
            state.lines[state.cursor_y] = line:sub(1, state.cursor_x - 1) ..
                line:sub(state.cursor_x + 1)
        else
            state.lines[state.cursor_y] = line ..
                (state.lines[state.cursor_y + 1] or "")
            table.remove(state.lines, state.cursor_y + 1)
        end

        state.dirty = true
        state.needs_draw = true
        state.clamp_cursor()
    end

    function state.move_cursor(dx, dy, extend)
        local old_y = state.cursor_y
        local old_x = state.cursor_x

        begin_selection(old_y, old_x, extend)

        if dy ~= 0 then
            state.cursor_y = Util.clamp(state.cursor_y + dy, 1, #state.lines)
            state.cursor_x = math.min(old_x, line_length(state.cursor_y) + 1)
        else
            state.cursor_x = state.cursor_x + dx

            if state.cursor_x < 1 then
                if state.cursor_y > 1 then
                    state.cursor_y = state.cursor_y - 1
                    state.cursor_x = line_length(state.cursor_y) + 1
                else
                    state.cursor_x = 1
                end
            elseif state.cursor_x > line_length(state.cursor_y) + 1 then
                if state.cursor_y < #state.lines then
                    state.cursor_y = state.cursor_y + 1
                    state.cursor_x = 1
                else
                    state.cursor_x = line_length(state.cursor_y) + 1
                end
            end
        end

        state.clamp_cursor()
        finish_selection()
        state.needs_draw = true
    end

    function state.page_move(amount, extend)
        local old_y = state.cursor_y
        local old_x = state.cursor_x

        begin_selection(old_y, old_x, extend)
        state.cursor_y = Util.clamp(state.cursor_y + amount, 1, #state.lines)
        state.cursor_x = math.min(old_x, line_length(state.cursor_y) + 1)
        finish_selection()
        state.needs_draw = true
    end

    function state.home_key(extend)
        local old_y = state.cursor_y
        local old_x = state.cursor_x

        begin_selection(old_y, old_x, extend)
        state.cursor_x = 1
        finish_selection()
        state.needs_draw = true
    end

    function state.end_key(extend)
        local old_y = state.cursor_y
        local old_x = state.cursor_x

        begin_selection(old_y, old_x, extend)
        state.cursor_x = line_length(state.cursor_y) + 1
        finish_selection()
        state.needs_draw = true
    end

    function state.select_all()
        state.sel_line = 1
        state.sel_col = 1
        state.cursor_y = #state.lines
        state.cursor_x = line_length(state.cursor_y) + 1
        state.needs_draw = true
    end

    function state.char_width(character)
        if character == "\t" then
            return Config.TAB_WIDTH
        end

        return 1
    end

    function state.visual_col_for_index(line, index)
        local visual = 1

        line = tostring(line or "")
        index = Util.clamp(index or 1, 1, #line + 1)

        for position = 1, index - 1 do
            visual = visual + state.char_width(line:sub(position, position))
        end

        return visual
    end

    function state.ensure_cursor_visible(layout)
        local visual_x = state.visual_col_for_index(
            state.lines[state.cursor_y],
            state.cursor_x
        )

        if state.cursor_y < state.scroll_y then
            state.scroll_y = state.cursor_y
        elseif state.cursor_y > state.scroll_y + layout.rows - 1 then
            state.scroll_y = state.cursor_y - layout.rows + 1
        end

        if visual_x < state.scroll_x then
            state.scroll_x = visual_x
        elseif visual_x > state.scroll_x + layout.cols - 1 then
            state.scroll_x = visual_x - layout.cols + 1
        end

        if state.scroll_y < 1 then
            state.scroll_y = 1
        end

        if state.scroll_x < 1 then
            state.scroll_x = 1
        end
    end

    function state.visible_line_text(line, start_visual, width)
        local output = {}
        local visual = 1

        for index = 1, #line do
            local character = line:sub(index, index)
            local char_w = state.char_width(character)

            for unit = 1, char_w do
                local current = visual + unit - 1

                if current >= start_visual and current < start_visual + width then
                    if character == "\t" then
                        output[#output + 1] = " "
                    else
                        output[#output + 1] = character
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

    return state
end

return Document
