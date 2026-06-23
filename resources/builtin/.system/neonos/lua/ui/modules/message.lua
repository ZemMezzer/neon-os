local gfx = require("gfx")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")

local Message = {}

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

local function wrapped_lines(value, max_chars)
    local words = {}
    local lines = {}
    local current = ""

    value = text(value)

    for word in value:gmatch("[^%s]+") do
        table.insert(words, word)
    end

    if #words == 0 then
        return { "" }
    end

    for _, word in ipairs(words) do
        if #word > max_chars then
            word = fit(word, max_chars)
        end

        if current == "" then
            current = word
        elseif #current + 1 + #word <= max_chars then
            current = current .. " " .. word
        else
            table.insert(lines, current)
            current = word
        end
    end

    if current ~= "" then
        table.insert(lines, current)
    end

    while #lines > 3 do
        local last = table.remove(lines)
        lines[#lines] = fit(lines[#lines] .. " " .. last, max_chars)
    end

    return lines
end

local function enabled(button)
    return type(button) == "table" and button.disabled ~= true
end

local function normalize_buttons(source)
    local buttons = {}

    if type(source) ~= "table" then
        return buttons
    end

    for _, source_button in ipairs(source) do
        if #buttons >= MAX_BUTTONS then
            break
        end

        local button = {}

        if type(source_button) == "table" then
            for key, value in pairs(source_button) do
                button[key] = value
            end
        else
            button.label = text(source_button)
            button.disabled = true
        end

        button.label = text(button.label)
        table.insert(buttons, button)
    end

    return buttons
end

local function default_button(buttons)
    for index, button in ipairs(buttons) do
        if button.default == true and enabled(button) then
            return index
        end
    end

    for index, button in ipairs(buttons) do
        if enabled(button) then
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
        box.selected = default_button(box.buttons)
    end
end

local function move_selection(box, direction)
    local count = #box.buttons

    if count == 0 then
        return false
    end

    ensure_selection(box)

    local index = box.selected
    local attempts = 0

    repeat
        index = index + direction

        if index < 1 then
            index = count
        elseif index > count then
            index = 1
        end

        attempts = attempts + 1

        if enabled(box.buttons[index]) then
            box.selected = index
            return true
        end
    until attempts >= count

    return false
end

local function cancel_button(box)
    for _, button in ipairs(box.buttons) do
        if button.cancel == true and enabled(button) then
            return button
        end
    end

    return nil
end

local function geometry(box)
    local screen_w = gfx.width()
    local screen_h = gfx.height()
    local width = math.min(WIDTH, math.max(160, screen_w - MARGIN * 2))
    local content_width = width - PADDING * 2
    local max_chars = math.max(1, math.floor(content_width / CHAR_WIDTH))
    local lines = wrapped_lines(box.message, max_chars)
    local message_height = math.max(
        MESSAGE_MIN_HEIGHT,
        #lines * CHAR_HEIGHT + 8
    )
    local button_count = math.max(1, #box.buttons)
    local height = PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT + message_height +
        BUTTON_HEIGHT + PADDING
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
        button_count = button_count,
    }
end

local function draw_label(x, y, value)
    value = fit(value, 24)

    if #value > 0 then
        gfx.text(x, y, value:sub(1, 1), Palette.vermilion, 2)
    end

    if #value > 1 then
        gfx.text(x + CHAR_WIDTH, y, value:sub(2), Palette.white_smoke, 2)
    end
end

function Message.new(options)
    options = type(options) == "table" and options or {}

    local box = {
        label = text(options.label),
        message = text(options.message),
        buttons = normalize_buttons(options.buttons),
        selected = math.floor(tonumber(options.selected) or 0),
        x = options.x,
        y = options.y,
    }

    ensure_selection(box)
    return setmetatable(box, { __index = Message })
end

function Message:draw()
    local box = self
    local layout = geometry(box)
    local message_top
    local button_y

    ensure_selection(box)

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

    draw_label(layout.x + PADDING, layout.y + PADDING, box.label)
    gfx.line(
        layout.x + PADDING,
        layout.y + PADDING + LABEL_HEIGHT,
        layout.x + layout.width - PADDING,
        layout.y + PADDING + LABEL_HEIGHT,
        Palette.dark_grey
    )

    message_top = layout.y + PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        math.floor((layout.message_height - #layout.lines * CHAR_HEIGHT) / 2)

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

    button_y = layout.y + layout.height - PADDING - BUTTON_HEIGHT

    if #box.buttons == 0 then
        return
    end

    local total_gap = (#box.buttons - 1) * BUTTON_GAP
    local button_width = math.floor(
        (layout.content_width - total_gap) / #box.buttons
    )

    for index, button in ipairs(box.buttons) do
        local button_x = layout.x + PADDING + (index - 1) *
            (button_width + BUTTON_GAP)
        local selected = index == box.selected
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

function Message:handle_event(event)
    if type(event) ~= "table" or event.type ~= "key" then
        return false
    end

    local key = event.key

    if type(key) ~= "number" then
        return false
    end

    local shortcut = key

    if key >= string.byte("A") and key <= string.byte("Z") then
        shortcut = key - string.byte("A") + string.byte("a")
    end

    if key == InputEvent.Keys.LEFT or key == InputEvent.Keys.UP or
        shortcut == string.byte("a") or shortcut == string.byte("w") then
        move_selection(self, -1)
        return true
    end

    if key == InputEvent.Keys.RIGHT or key == InputEvent.Keys.DOWN or
        shortcut == string.byte("d") or shortcut == string.byte("s") then
        move_selection(self, 1)
        return true
    end

    if key == InputEvent.Keys.ESCAPE or key == InputEvent.Keys.TAB then
        local button = cancel_button(self)

        if button ~= nil and type(button.callback) == "function" then
            button.callback(self, button)
        end
        return true
    end

    if key == InputEvent.Keys.ENTER then
        ensure_selection(self)

        local button = self.buttons[self.selected]

        if enabled(button) and type(button.callback) == "function" then
            button.callback(self, button)
        end
        return true
    end

    return true
end

return Message
