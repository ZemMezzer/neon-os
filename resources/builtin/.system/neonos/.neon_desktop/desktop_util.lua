local Util = {}

function Util.safe_date(format, fallback)
    local ok, result = pcall(function()
        return os.date(format)
    end)

    if ok and type(result) == "string" and #result > 0 then
        return result
    end

    return fallback
end

function Util.text_width(text, scale)
    return #text * 6 * scale
end

function Util.centered_text(gfx, x, y, width, text, color, scale)
    local tx = x + math.floor((width - Util.text_width(text, scale)) / 2)
    gfx.text(tx, y, text, color, scale)
end

function Util.fit_text(text, max_width, scale)
    local max_chars = math.floor(max_width / (6 * scale))

    if max_chars < 1 then
        return ""
    end

    if #text <= max_chars then
        return text
    end

    if max_chars <= 3 then
        return string.sub(text, 1, max_chars)
    end

    return string.sub(text, 1, max_chars - 3) .. "..."
end

function Util.trim(value)
    value = tostring(value or "")
    value = value:gsub("^%s+", "")
    value = value:gsub("%s+$", "")
    return value
end

function Util.lower(value)
    return string.lower(tostring(value or ""))
end

function Util.key_is(key, name, numeric_code)
    return key == name or key == numeric_code
end

function Util.is_volume_path(path)
    return type(path) == "string" and path:match("^%d:") ~= nil
end

function Util.normalize_path(path)
    path = Util.trim(path):gsub("\\", "/")

    if path == "" then
        return ""
    end

    if not Util.is_volume_path(path) then
        path = path:gsub("^%./", "")
        path = path:gsub("^/+", "")
        path = "0:/" .. path
    end

    local drive = path:sub(1, 2)
    local rest = path:sub(3)
    local parts = {}

    for part in rest:gmatch("[^/]+") do
        if part ~= "" and part ~= "." then
            if part == ".." then
                if #parts > 0 then
                    table.remove(parts)
                end
            else
                table.insert(parts, part)
            end
        end
    end

    if #parts == 0 then
        return drive .. "/"
    end

    return drive .. "/" .. table.concat(parts, "/")
end

function Util.join_path(left, right)
    left = Util.normalize_path(left)
    right = tostring(right or ""):gsub("\\", "/")
    right = right:gsub("^/+", "")

    if left == "" then
        return Util.normalize_path(right)
    end

    if right == "" then
        return left
    end

    if left:sub(-1) == "/" then
        return Util.normalize_path(left .. right)
    end

    return Util.normalize_path(left .. "/" .. right)
end

function Util.path_key(path)
    return Util.lower(Util.normalize_path(path))
end

function Util.base_name(path)
    path = Util.normalize_path(path)
    return path:match("([^/]+)$") or ""
end

function Util.read_first_line(path)
    local file = io.open(path, "r")

    if file == nil then
        return nil
    end

    local line = file:read("*l")
    file:close()

    line = Util.trim(line)

    if line == "" then
        return nil
    end

    return line
end

function Util.write_shortcut(path, target)
    local file, open_error = io.open(path, "w")

    if file == nil then
        return false, open_error or "cannot create shortcut"
    end

    local wrote, write_error = file:write(target, "\n")
    local closed, close_error = file:close()

    if not wrote then
        return false, write_error or "cannot write shortcut"
    end

    if not closed then
        return false, close_error or "cannot close shortcut"
    end

    return true
end

function Util.shell_quote_argument(value)
    value = tostring(value or "")

    if string.find(value, '"', 1, true) ~= nil then
        return nil
    end

    return '"' .. value .. '"'
end

return Util
