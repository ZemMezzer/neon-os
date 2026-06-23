local fs = require("fs")

local Config = require("explorer_config")

local Util = {}

function Util.is_root(path)
    return path == nil or
        path == "" or
        path == "/" or
        path == "0:" or
        path == "0:/"
end

function Util.normalize_root(path)
    if Util.is_root(path) then
        return "0:/"
    end

    return path
end

function Util.display_path(path)
    path = Util.normalize_root(path)

    if path == "0:/" then
        return "0:/"
    end

    return path
end

function Util.parent_of(path)
    if Util.is_root(path) then
        return "0:/"
    end

    return Util.normalize_root(fs.getDir(path))
end

function Util.path_equals(left, right)
    local a = tostring(left or ""):gsub("\\", "/")
    local b = tostring(right or ""):gsub("\\", "/")

    while #a > 3 and a:sub(-1) == "/" do
        a = a:sub(1, -2)
    end

    while #b > 3 and b:sub(-1) == "/" do
        b = b:sub(1, -2)
    end

    return a == b
end

function Util.normalize_launch_path(path)
    path = tostring(path or ""):gsub("\\", "/")

    if path == "" or path == "/" or path == "0:" or path == "0:/" then
        return "0:/"
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

function Util.is_hidden_path(path)
    local normalized = Util.normalize_launch_path(path):lower()
    return Config.HIDDEN_PATHS[normalized] == true
end

function Util.lower_shortcut(key)
    if key >= string.byte("A") and key <= string.byte("Z") then
        return key - string.byte("A") + string.byte("a")
    end

    return key
end

function Util.fit(text, max_chars)
    text = tostring(text or "")

    if max_chars <= 0 then
        return ""
    end

    if #text <= max_chars then
        return text
    end

    if max_chars <= 3 then
        return text:sub(1, max_chars)
    end

    return text:sub(1, max_chars - 3) .. "..."
end

function Util.format_size(bytes)
    bytes = tonumber(bytes) or 0

    if bytes >= 1024 * 1024 * 1024 then
        return string.format("%.1fG", bytes / (1024 * 1024 * 1024))
    end

    if bytes >= 1024 * 1024 then
        return string.format("%.1fM", bytes / (1024 * 1024))
    end

    if bytes >= 1024 then
        return string.format("%.1fK", bytes / 1024)
    end

    return tostring(bytes) .. "B"
end

function Util.shell_quote(value)
    return '"' .. tostring(value or ""):gsub('"', "") .. '"'
end

return Util
