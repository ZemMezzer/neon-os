local fs = require("fs")

local Util = {}

function Util.clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

function Util.lower_control_key(key)
    if type(key) == "number" and
        key >= string.byte("A") and key <= string.byte("Z") then
        return key - string.byte("A") + string.byte("a")
    end

    return key
end

function Util.normalize_path(path)
    path = tostring(path or ""):gsub("\\", "/")

    if path == "" then
        return ""
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

function Util.display_path(path)
    path = Util.normalize_path(path)

    if path == "0:/" or path == "0:" then
        return "/"
    end

    if path:sub(1, 3) == "0:/" then
        return "/" .. path:sub(4)
    end

    return path
end

function Util.path_dir(path)
    local dir = fs.getDir(path)

    if dir == nil or dir == "" or dir == "0:" then
        return "0:/"
    end

    return Util.normalize_path(dir)
end

function Util.ensure_directory(path)
    path = Util.normalize_path(path)

    if path == "" or path == "0:/" then
        return true
    end

    if path:sub(1, 3) ~= "0:/" then
        return false, "Invalid directory path"
    end

    local current = "0:"
    local remainder = path:sub(4)

    for part in remainder:gmatch("[^/]+") do
        current = current .. "/" .. part

        if fs.exists(current) then
            if not fs.isDir(current) then
                return false, current .. " is not a folder"
            end
        else
            local ok, error_message = pcall(fs.makeDir, current)

            if not ok then
                return false, tostring(error_message)
            end
        end
    end

    return true
end

function Util.valid_file_name(name)
    name = tostring(name or "")

    if name == "" or name == "." or name == ".." then
        return nil, "Enter a file name"
    end

    if name:find("/", 1, true) or
        name:find("\\", 1, true) or
        name:find(":", 1, true) or
        name:find('"', 1, true) then
        return nil, "Name cannot contain slash, colon or quote"
    end

    if not name:find("%.") then
        name = name .. ".txt"
    end

    return name
end

function Util.is_valid_file_name_character(character)
    return character ~= "/" and
        character ~= "\\" and
        character ~= ":" and
        character ~= '"'
end

function Util.copy_lines(source)
    local result = {}

    for index = 1, #source do
        result[index] = source[index]
    end

    return result
end

function Util.split_lines(text)
    text = tostring(text or ""):gsub("\r\n", "\n"):gsub("\r", "\n")

    local result = {}

    for line in (text .. "\n"):gmatch("(.-)\n") do
        result[#result + 1] = line
    end

    if #result == 0 then
        result[1] = ""
    end

    return result
end

function Util.read_document(path)
    local handle, error_message = io.open(path, "r")

    if not handle then
        return { "" }, false, error_message
    end

    local text = handle:read("*a") or ""
    handle:close()

    return Util.split_lines(text), true, nil
end

function Util.shell_quote(value)
    return '"' .. tostring(value or ""):gsub('"', "") .. '"'
end

return Util
