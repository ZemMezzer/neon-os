-- Shared centred message box for NeonOS UI.
--
-- Preferred interface:
--
--   local box = MessageBox.new("DELETE", "Delete notes.txt?", {
--       { "Yes", function(self) Canvas.remove(self) end },
--       { "No",  function(self) Canvas.remove(self) end },
--   })
--
-- The message area grows to fit all wrapped lines. Its only limit is the
-- physical screen height; an excessively long message is clipped with an
-- ellipsis instead of being drawn outside the screen.

local gfx = require("gfx")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")

local Keys = InputEvent.Keys
local MessageBox = {}

local MAX_BUTTONS = 3
local WIDTH = 360
local MARGIN = 10
local PADDING = 12
local LABEL_HEIGHT = 20
local DIVIDER_HEIGHT = 1
local MESSAGE_MIN_HEIGHT = 38
local BUTTON_HEIGHT = 26
local BUTTON_GAP = 8
local SHADOW_OFFSET = 3
local CHAR_WIDTH = 12
local CHAR_HEIGHT = 16

local function text(value)
    return tostring(value or "")
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

local function fit(value, max_chars)
    value = text(value)

    if max_chars <= 0 then
        return ""
    end

    if #value <= max_chars then
        return value
    end

    if max_chars <= 3 then
        return value:sub(1, max_chars)
    end

    return value:sub(1, max_chars - 3) .. "..."
end

local function append_word(lines, current, word, max_chars)
    while #word > max_chars do
        if current ~= "" then
            table.insert(lines, current)
            current = ""
        end

        table.insert(lines, word:sub(1, max_chars))
        word = word:sub(max_chars + 1)
    end

    if current == "" then
        return word
    end

    if #current + 1 + #word <= max_chars then
        return current .. " " .. word
    end

    table.insert(lines, current)
    return word
end

local function append_paragraph(lines, paragraph, max_chars)
    local current = ""

    if paragraph == "" then
        table.insert(lines, "")
        return
    end

    for word in paragraph:gmatch("[^%s]+") do
        current = append_word(lines, current, word, max_chars)
    end

    if current ~= "" then
        table.insert(lines, current)
    end
end

local function wrapped_lines(value, max_chars)
    local lines = {}
    local content = text(value):gsub("\r", "")

    for paragraph in (content .. "\n"):gmatch("(.-)\n") do
        append_paragraph(lines, paragraph, max_chars)
    end

    if #lines == 0 then
        table.insert(lines, "")
    end

    return lines
end

local function visible_lines(lines, max_lines)
    local result = {}
    local count = math.min(#lines, max_lines)

    for index = 1, count do
        result[index] = lines[index]
    end

    if #lines > max_lines and count > 0 then
        local last = result[count]

        if #last > 3 then
            result[count] = last:sub(1, #last - 3) .. "..."
        else
            result[count] = "..."
        end
    end

    return result
end

local function normalize_buttons(source)
    local buttons = {}

    if type(source) ~= "table" then
        return buttons
    end

    for _, entry in ipairs(source) do
        if #buttons >= MAX_BUTTONS then
            break
        end

        if type(entry) == "table" then
            local label = entry.label
            local callback = entry.callback

            if label == nil then
                label = entry[1]
            end

            if callback == nil then
                callback = entry[2]
            end

            if label ~= nil then
                table.insert(buttons, {
                    label = text(label),
                    callback = callback,
                    cancel = entry.cancel == true,
                    default = entry.default == true,
                    disabled = entry.disabled == true,
                })
            end
        end
    end

    return buttons
end

local function enabled(button)
    return type(button) == "table" and button.disabled ~= true
end

local function default_button(buttons, requested)
    if requested ~= nil then
        requested = math.floor(tonumber(requested) or 0)

        if requested >= 1 and requested <= #buttons and
            enabled(buttons[requested]) then
            return requested
        end
    end

    for index, button in ipairs(buttons) do
        if button.default == true and enabled(button) then
            return index
        end
    end

    -- With the compact array API the conventional safe choice is last.
    for index = #buttons, 1, -1 do
        if enabled(buttons[index]) then
            return index
        end
    end

    return 0
end

local function ensure_selection(box)
    if #box.buttons == 0 then
        box.selected = 0
        return
    end

    if box.selected < 1 or box.selected > #box.buttons or
        not enabled(box.buttons[box.selected]) then
        box.selected = default_button(box.buttons, box.initial_selected)
    end
end

local function move_selection(box, direction)
    local count = #box.buttons

    if count == 0 then
        return false
    end

    ensure_selection(box)

    local index = box.selected

    for _ = 1, count do
        index = index + direction

        if index < 1 then
            index = count
        elseif index > count then
            index = 1
        end

        if enabled(box.buttons[index]) then
            box.selected = index
            return true
        end
    end

    return false
end

local function cancel_button(box)
    for _, button in ipairs(box.buttons) do
        if button.cancel == true and enabled(button) then
            return button
        end
    end

    return box.buttons[#box.buttons]
end

local function geometry(box)
    local screen_w = gfx.width()
    local screen_h = gfx.height()
    local width = math.min(WIDTH, math.max(160, screen_w - MARGIN * 2))
    local content_width = width - PADDING * 2
    local max_chars = math.max(1, math.floor(content_width / CHAR_WIDTH))
    local all_lines = wrapped_lines(box.message, max_chars)
    local buttons_height = #box.buttons > 0 and BUTTON_HEIGHT or 0
    local button_spacing = #box.buttons > 0 and PADDING or 0
    local fixed_height = PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        button_spacing + buttons_height + PADDING
    local available_message_height = math.max(
        MESSAGE_MIN_HEIGHT,
        screen_h - MARGIN * 2 - fixed_height
    )
    local max_lines = math.max(
        1,
        math.floor((available_message_height - 8) / CHAR_HEIGHT)
    )
    local lines = visible_lines(all_lines, max_lines)
    local message_height = math.max(
        MESSAGE_MIN_HEIGHT,
        #lines * CHAR_HEIGHT + 8
    )
    local height = fixed_height + message_height
    local x = tonumber(box.x)
    local y = tonumber(box.y)

    if x == nil then
        x = math.floor((screen_w - width) / 2)
    end

    if y == nil then
        y = math.floor((screen_h - height) / 2)
    end

    x = clamp(math.floor(x), MARGIN, math.max(MARGIN, screen_w - width - MARGIN))
    y = clamp(math.floor(y), MARGIN, math.max(MARGIN, screen_h - height - MARGIN))

    return {
        x = x,
        y = y,
        width = width,
        height = height,
        content_width = content_width,
        max_chars = max_chars,
        lines = lines,
        message_height = message_height,
    }
end

local function draw_label(x, y, value)
    local shown = fit(value, 24)

    if #shown > 0 then
        gfx.text(x, y, shown:sub(1, 1), Palette.vermilion, 2)
    end

    if #shown > 1 then
        gfx.text(x + CHAR_WIDTH, y, shown:sub(2), Palette.white_smoke, 2)
    end
end

function MessageBox.new(label, message, buttons)
    local selected = nil
    local x = nil
    local y = nil

    -- Backwards compatibility with MessageBox.new({ ... }).
    if type(label) == "table" and message == nil and buttons == nil then
        local options = label
        label = options.label
        message = options.message
        buttons = options.buttons
        selected = options.selected
        x = options.x
        y = options.y
    end

    local box = {
        label = text(label),
        message = text(message),
        buttons = normalize_buttons(buttons),
        selected = 0,
        initial_selected = selected,
        x = x,
        y = y,
        visible = true,
    }

    ensure_selection(box)
    return setmetatable(box, { __index = MessageBox })
end

function MessageBox:draw()
    local layout = geometry(self)
    local message_top = layout.y + PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        math.floor((layout.message_height - #layout.lines * CHAR_HEIGHT) / 2)

    ensure_selection(self)

    gfx.fill_rect(
        layout.x + SHADOW_OFFSET,
        layout.y + SHADOW_OFFSET,
        layout.width,
        layout.height,
        Palette.void_black
    )
    gfx.fill_rect(
        layout.x,
        layout.y,
        layout.width,
        layout.height,
        Palette.obsidian
    )
    gfx.rect(
        layout.x,
        layout.y,
        layout.width,
        layout.height,
        Palette.graphite
    )

    draw_label(layout.x + PADDING, layout.y + PADDING, self.label)
    gfx.line(
        layout.x + PADDING,
        layout.y + PADDING + LABEL_HEIGHT,
        layout.x + layout.width - PADDING,
        layout.y + PADDING + LABEL_HEIGHT,
        Palette.dark_grey
    )

    for index, line in ipairs(layout.lines) do
        local line_x = layout.x + math.floor(
            (layout.width - #line * CHAR_WIDTH) / 2
        )

        gfx.text(
            line_x,
            message_top + (index - 1) * CHAR_HEIGHT,
            line,
            Palette.white_smoke,
            2
        )
    end

    if #self.buttons == 0 then
        return
    end

    local button_y = layout.y + layout.height - PADDING - BUTTON_HEIGHT
    local total_gap = (#self.buttons - 1) * BUTTON_GAP
    local button_width = math.floor(
        (layout.content_width - total_gap) / #self.buttons
    )

    for index, button in ipairs(self.buttons) do
        local button_x = layout.x + PADDING + (index - 1) *
            (button_width + BUTTON_GAP)
        local selected = index == self.selected
        local foreground = button.disabled and Palette.davy_grey or Palette.white_smoke

        if selected then
            gfx.fill_rect(
                button_x,
                button_y,
                button_width,
                BUTTON_HEIGHT,
                Palette.charcoal
            )
            gfx.rect(
                button_x,
                button_y,
                button_width,
                BUTTON_HEIGHT,
                Palette.vermilion
            )
            foreground = Palette.alice_blue
        else
            gfx.fill_rect(
                button_x,
                button_y,
                button_width,
                BUTTON_HEIGHT,
                Palette.jet_black
            )
            gfx.rect(
                button_x,
                button_y,
                button_width,
                BUTTON_HEIGHT,
                Palette.dark_grey
            )
        end

        local shown = fit(
            button.label,
            math.max(1, math.floor((button_width - 8) / CHAR_WIDTH))
        )
        local text_x = button_x + math.floor(
            (button_width - #shown * CHAR_WIDTH) / 2
        )

        gfx.text(text_x, button_y + 5, shown, foreground, 2)
    end
end

function MessageBox:handle_event(event)
    if type(event) ~= "table" or event.type ~= "key" then
        return false
    end

    local key = event.key

    if type(key) ~= "number" then
        return true
    end

    local shortcut = key

    if key >= string.byte("A") and key <= string.byte("Z") then
        shortcut = key - string.byte("A") + string.byte("a")
    end

    if #self.buttons == 0 then
        return true
    end

    if key == Keys.LEFT or key == Keys.UP or
        shortcut == string.byte("a") or shortcut == string.byte("w") then
        move_selection(self, -1)
        return true
    end

    if key == Keys.RIGHT or key == Keys.DOWN or
        shortcut == string.byte("d") or shortcut == string.byte("s") then
        move_selection(self, 1)
        return true
    end

    if key == Keys.ESCAPE or key == Keys.TAB then
        local button = cancel_button(self)

        if button ~= nil and type(button.callback) == "function" then
            button.callback(self, button)
        end
        return true
    end

    if key == Keys.ENTER then
        local button = self.buttons[self.selected]

        if enabled(button) and type(button.callback) == "function" then
            button.callback(self, button)
        end
        return true
    end

    return true
end

return MessageBox
