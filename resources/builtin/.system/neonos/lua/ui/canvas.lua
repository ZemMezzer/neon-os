-- Generic ordered UI canvas.
-- Objects are duck-typed: an object may implement draw(self) and/or
-- handle_event(self, event). The canvas never needs to know its type.

local Canvas = {
    objects = {},
    dirty = true,
}

local function valid_object(object)
    return type(object) == "table"
end

function Canvas.add(object)
    if not valid_object(object) then
        return nil
    end

    for _, existing in ipairs(Canvas.objects) do
        if existing == object then
            return object
        end
    end

    table.insert(Canvas.objects, object)
    Canvas.dirty = true
    return object
end

function Canvas.remove(object)
    for index, existing in ipairs(Canvas.objects) do
        if existing == object then
            table.remove(Canvas.objects, index)
            Canvas.dirty = true
            return true
        end
    end

    return false
end

function Canvas.contains(object)
    for _, existing in ipairs(Canvas.objects) do
        if existing == object then
            return true
        end
    end

    return false
end

function Canvas.clear()
    if #Canvas.objects > 0 then
        Canvas.objects = {}
        Canvas.dirty = true
    end
end

function Canvas.is_dirty()
    return Canvas.dirty
end

function Canvas.render()
    for _, object in ipairs(Canvas.objects) do
        if object.visible ~= false and type(object.draw) == "function" then
            object:draw()
        end
    end

    Canvas.dirty = false
end

function Canvas.handle_event(event)
    if type(event) ~= "table" then
        return false
    end

    for index = #Canvas.objects, 1, -1 do
        local object = Canvas.objects[index]

        if object.visible ~= false and type(object.handle_event) == "function" then
            if object:handle_event(event) == true then
                Canvas.dirty = true
                return true
            end
        end
    end

    return false
end

return Canvas
