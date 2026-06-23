-- Shared centred single-line input box for NeonOS UI.
--
-- InputBox.new({
--     label = "New Folder",
--     message = "Name (ASCII recommended; no slash):",
--     callback = function(self, value) ... end,
--     on_cancel = function(self) ... end,
-- })
--
-- The message area grows to fit wrapped text. It is capped only by the
-- physical screen height, so it never draws beyond the framebuffer.

local gfx = require("gfx")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")

local Keys = InputEvent.Keys
local InputBox = {}

local WIDTH = 420
local MARGIN = 10
local PADDING = 12
local LABEL_HEIGHT = 20
local DIVIDER_HEIGHT = 1
local MESSAGE_MIN_HEIGHT = 22
local FIELD_HEIGHT = 28
local FOOTER_HEIGHT = 16
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

local function tail_fit(value, max_chars)
    value = text(value)

    if max_chars <= 0 then
        return ""
    end

    if #value <= max_chars then
        return value
    end

    return value:sub(#value - max_chars + 1)
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

local function draw_label(x, y, value, accent_prefix)
    local shown = text(value)
    local prefix = math.floor(tonumber(accent_prefix) or 1)

    prefix = clamp(prefix, 0, #shown)

    if prefix > 0 then
        gfx.text(x, y, shown:sub(1, prefix), Palette.vermilion, 2)
    end

    if prefix < #shown then
        gfx.text(
            x + prefix * CHAR_WIDTH,
            y,
            shown:sub(prefix + 1),
            Palette.white_smoke,
            2
        )
    end
end

local function geometry(box)
    local screen_w = gfx.width()
    local screen_h = gfx.height()
    local width = math.min(WIDTH, math.max(180, screen_w - MARGIN * 2))
    local content_width = width - PADDING * 2
    local max_chars = math.max(1, math.floor(content_width / CHAR_WIDTH))
    local all_lines = wrapped_lines(box.message, max_chars)
    local fixed_height = PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        FIELD_HEIGHT + FOOTER_HEIGHT + PADDING
    local available_message_height = math.max(
        MESSAGE_MIN_HEIGHT,
        screen_h - MARGIN * 2 - fixed_height
    )
    local max_lines = math.max(
        1,
        math.floor((available_message_height - 4) / CHAR_HEIGHT)
    )
    local lines = visible_lines(all_lines, max_lines)
    local message_height = math.max(
        MESSAGE_MIN_HEIGHT,
        #lines * CHAR_HEIGHT + 4
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

local function invoke(box, callback, ...)
    if type(callback) == "function" then
        callback(box, ...)
    else
        box.visible = false
    end
end

function InputBox.new(options)
    options = type(options) == "table" and options or {}

    return setmetatable({
        label = text(options.label),
        label_accent_prefix = options.label_accent_prefix,
        message = text(options.message),
        value = text(options.value),
        max_length = math.max(1, math.floor(tonumber(options.max_length) or 55)),
        allow_character = options.allow_character,
        callback = options.callback,
        on_cancel = options.on_cancel,
        x = options.x,
        y = options.y,
        visible = options.visible ~= false,
    }, { __index = InputBox })
end

function InputBox:draw()
    local layout = geometry(self)
    local message_top = layout.y + PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        math.floor((layout.message_height - #layout.lines * CHAR_HEIGHT) / 2)
    local field_y = layout.y + PADDING + LABEL_HEIGHT + DIVIDER_HEIGHT +
        layout.message_height
    local footer_y = field_y + FIELD_HEIGHT + 2

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

    draw_label(
        layout.x + PADDING,
        layout.y + PADDING,
        fit(self.label, layout.max_chars),
        self.label_accent_prefix
    )
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
            Palette.cadet_blue,
            2
        )
    end

    gfx.fill_rect(
        layout.x + PADDING,
        field_y,
        layout.content_width,
        FIELD_HEIGHT,
        Palette.rich_black
    )
    gfx.rect(
        layout.x + PADDING,
        field_y,
        layout.content_width,
        FIELD_HEIGHT,
        Palette.dark_grey
    )

    local value_chars = math.max(
        1,
        math.floor((layout.content_width - 10) / CHAR_WIDTH)
    )
    local shown = tail_fit(self.value, value_chars - 1)

    gfx.text(
        layout.x + PADDING + 5,
        field_y + 6,
        shown .. "_",
        Palette.white_smoke,
        2
    )
    gfx.text(
        layout.x + PADDING,
        footer_y,
        "Enter: accept   Tab/Esc: cancel",
        Palette.cadet_blue,
        1
    )
end

function InputBox:handle_event(event)
    if type(event) ~= "table" then
        return false
    end

    if event.type ~= "key" then
        return true
    end

    local key = event.key

    if type(key) ~= "number" then
        return true
    end

    if key == Keys.ENTER then
        invoke(self, self.callback, self.value)
        return true
    end

    if key == Keys.ESCAPE or key == Keys.TAB then
        invoke(self, self.on_cancel)
        return true
    end

    if key == Keys.BACKSPACE then
        self.value = self.value:sub(1, -2)
        return true
    end

    if key >= 32 and key <= 126 and #self.value < self.max_length then
        local character = string.char(key)
        local allowed = true

        if type(self.allow_character) == "function" then
            allowed = self.allow_character(self, character) == true
        end

        if allowed then
            self.value = self.value .. character
        end

        return true
    end

    return true
end

return InputBox
