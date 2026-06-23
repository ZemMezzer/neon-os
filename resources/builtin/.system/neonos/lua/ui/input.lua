-- Shared UI input and event helpers.
-- Native key constants are exported by the system input module itself.
-- Re-export that table so every UI module sees all present and future keys.

local raw_input = require("input")

local InputEvent = {}

-- Example: InputEvent.Keys.ENTER, InputEvent.Keys.LEFT,
-- InputEvent.Keys.F12, InputEvent.Keys.A.
-- This is intentionally the original system table, not a copied subset.
InputEvent.Keys = raw_input

local function normalized_modifiers(modifiers)
    if type(modifiers) == "table" then
        return modifiers
    end

    return {}
end

function InputEvent.key(key, modifiers)
    if type(key) ~= "number" then
        return nil
    end

    return {
        type = "key",
        key = key,
        modifiers = normalized_modifiers(modifiers),
    }
end

function InputEvent.mouse_down(x, y, button, modifiers)
    return {
        type = "mouse_down",
        x = tonumber(x) or 0,
        y = tonumber(y) or 0,
        button = button,
        modifiers = normalized_modifiers(modifiers),
    }
end

function InputEvent.mouse_up(x, y, button, modifiers)
    return {
        type = "mouse_up",
        x = tonumber(x) or 0,
        y = tonumber(y) or 0,
        button = button,
        modifiers = normalized_modifiers(modifiers),
    }
end

function InputEvent.mouse_move(x, y, modifiers)
    return {
        type = "mouse_move",
        x = tonumber(x) or 0,
        y = tonumber(y) or 0,
        modifiers = normalized_modifiers(modifiers),
    }
end

function InputEvent.poll()
    local key, modifiers = raw_input.poll()

    if key == nil then
        return nil
    end

    return InputEvent.key(key, modifiers)
end

function InputEvent.is_key(event)
    return type(event) == "table" and event.type == "key"
end

return InputEvent
