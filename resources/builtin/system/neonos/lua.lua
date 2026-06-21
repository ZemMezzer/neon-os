local shell = require("shell")

local file_path = arg[1]

if type(file_path) ~= "string" or #file_path == 0 then
    return
end

shell.exec('lua "' .. file_path .. '"')
