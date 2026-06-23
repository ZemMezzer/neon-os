-- Shared context menu.
--
-- Minimal API:
--   local menu = Context.new(
--       "ACTIONS",
--       "games", -- optional context text
--       {
--           { "Open", function(self) ... end },
--           { "Delete", function(self) ... end },
--       }
--   )
--
-- The second argument may be omitted:
--   Context.new("ACTIONS", { { "Refresh", callback } })
--
-- A callback receives the context menu object. This lets the caller decide
-- when to remove it with Canvas.remove(self).

local gfx = require("gfx")
local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")

local Context = {}

local WIDTH = 236
local MARGIN = 10
local PADDING = 8
local LABEL_HEIGHT = 18
local CONTEXT_HEIGHT = 18
local ROW_HEIGHT = 20
local SHADOW_OFFSET = 3
local CHAR_WIDTH = 12

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

local function normalize_buttons(buttons)
    local result = {}

    if type(buttons) ~= "table" then
        return result
    end

    for _, button in ipairs(buttons) do
        if type(button) == "table" then
            table.insert(result, {
                label = text(button[1]),
                callback = button[2],
            })
        end
    end

    return result
end

local function move_selection(menu, delta)
    local count = #menu.buttons

    if count == 0 then
        return
    end

    menu.selected = menu.selected + delta

    if menu.selected < 1 then
        menu.selected = count
    elseif menu.selected > count then
        menu.selected = 1
    end
end

local function geometry(menu)
    local screen_width = gfx.width()
    local screen_height = gfx.height()
    local has_context = menu.context ~= ""
    local header_height = LABEL_HEIGHT + (has_context and CONTEXT_HEIGHT or 0)
    local max_rows = math.max(
        1,
        math.floor(
            (screen_height - MARGIN * 2 - header_height - PADDING * 3) /
            ROW_HEIGHT
        )
    )
    local rows = math.min(math.max(1, #menu.buttons), max_rows)
    local height = PADDING + header_height + PADDING + rows * ROW_HEIGHT + PADDING
    local x = tonumber(menu.x)
    local y = tonumber(menu.y)

    if x == nil then
        x = screen_width - WIDTH - MARGIN
    end

    if y == nil then
        y = math.floor((screen_height - height) / 2)
    end

    x = clamp(
        math.floor(x),
        MARGIN,
        math.max(MARGIN, screen_width - WIDTH - MARGIN)
    )
    y = clamp(
        math.floor(y),
        MARGIN,
        math.max(MARGIN, screen_height - height - MARGIN)
    )

    return {
        x = x,
        y = y,
        width = WIDTH,
        height = height,
        rows = rows,
        header_height = header_height,
        has_context = has_context,
    }
end

local function keep_selected_visible(menu, rows)
    if menu.selected < menu.scroll then
        menu.scroll = menu.selected
    elseif menu.selected >= menu.scroll + rows then
        menu.scroll = menu.selected - rows + 1
    end

    local max_scroll = math.max(1, #menu.buttons - rows + 1)

    if menu.scroll < 1 then
        menu.scroll = 1
    elseif menu.scroll > max_scroll then
        menu.scroll = max_scroll
    end
end

local function draw_label(x, y, label)
    label = text(label)

    if #label > 0 then
        gfx.text(x, y, label:sub(1, 1), Palette.vermilion, 2)
    end

    if #label > 1 then
        gfx.text(x + CHAR_WIDTH, y, label:sub(2), Palette.white_smoke, 2)
    end
end

function Context.new(label, context, buttons)
    if buttons == nil and type(context) == "table" then
        buttons = context
        context = nil
    end

    local menu = {
        label = text(label ~= nil and label or "ACTIONS"),
        context = text(context),
        buttons = normalize_buttons(buttons),
        selected = 1,
        scroll = 1,
        visible = true,
    }

    return setmetatable(menu, { __index = Context })
end

function Context:draw()
    local menu = self
    local box = geometry(menu)
    local content_width = box.width - PADDING * 2
    local max_chars = math.max(1, math.floor(content_width / CHAR_WIDTH))
    local divider_y = box.y + PADDING + box.header_height - 2
    local buttons_y = divider_y + PADDING + 2

    if #menu.buttons == 0 then
        menu.selected = 0
        menu.scroll = 1
    else
        if menu.selected < 1 or menu.selected > #menu.buttons then
            menu.selected = 1
        end
        keep_selected_visible(menu, box.rows)
    end

    gfx.fill_rect(
        box.x + SHADOW_OFFSET,
        box.y + SHADOW_OFFSET,
        box.width,
        box.height,
        Palette.void_black
    )
    gfx.fill_rect(box.x, box.y, box.width, box.height, Palette.obsidian)
    gfx.rect(box.x, box.y, box.width, box.height, Palette.graphite)

    draw_label(
        box.x + PADDING,
        box.y + PADDING,
        fit(menu.label, max_chars)
    )

    if box.has_context then
        gfx.text(
            box.x + PADDING,
            box.y + PADDING + LABEL_HEIGHT,
            fit(menu.context, max_chars),
            Palette.cadet_blue,
            1
        )
    end

    gfx.line(
        box.x + PADDING,
        divider_y,
        box.x + box.width - PADDING,
        divider_y,
        Palette.dark_grey
    )

    if #menu.buttons == 0 then
        gfx.text(
            box.x + PADDING,
            buttons_y,
            "<no actions>",
            Palette.cadet_blue,
            1
        )
        return
    end

    for row = 0, box.rows - 1 do
        local index = menu.scroll + row
        local button = menu.buttons[index]

        if button == nil then
            break
        end

        local row_y = buttons_y + row * ROW_HEIGHT
        local selected = index == menu.selected
        local foreground = selected and Palette.alice_blue or Palette.white_smoke

        if selected then
            gfx.fill_rect(
                box.x + PADDING,
                row_y,
                content_width,
                ROW_HEIGHT - 1,
                Palette.charcoal
            )
            gfx.fill_rect(
                box.x + PADDING,
                row_y,
                2,
                ROW_HEIGHT - 1,
                Palette.vermilion
            )
        end

        gfx.text(
            box.x + PADDING + 7,
            row_y + 2,
            fit(button.label, max_chars - 1),
            foreground,
            2
        )
    end
end

function Context:handle_event(event)
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

    if key == InputEvent.Keys.ESCAPE or key == InputEvent.Keys.TAB or
        (key == InputEvent.Keys.ENTER and
            type(event.modifiers) == "table" and
            event.modifiers.ctrl == true) then
        Canvas.remove(self)
        return true
    end

    if key == InputEvent.Keys.UP or shortcut == string.byte("w") then
        move_selection(self, -1)
        return true
    end

    if key == InputEvent.Keys.DOWN or shortcut == string.byte("s") then
        move_selection(self, 1)
        return true
    end

    if key == InputEvent.Keys.ENTER then
        local button = self.buttons[self.selected]

        if button ~= nil and type(button.callback) == "function" then
            button.callback(self)
        end

        return true
    end

    return true
end

return Context
