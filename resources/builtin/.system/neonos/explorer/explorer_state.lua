local fs = require("fs")
local buffer = require("buffer")

local Config = require("explorer_config")
local Util = require("explorer_util")

local ExplorerState = {}

function ExplorerState.new(screen_width, screen_height)
    local state = {
        layout = Config.make_layout(screen_width, screen_height),

        cwd = "0:/",
        items = {},
        selected = 1,
        offset = 1,
        status = "Ready",
        running = true,
        dirty = true,

        picker_mode = nil,
        target_mode = nil,
        error_handler = nil,
    }

    function state.set_status(text)
        state.status = tostring(text or "")
        state.dirty = true
    end

    function state.set_error_handler(handler)
        state.error_handler = handler
    end

    function state.report_error(label, message)
        label = tostring(label or "EXPLORER")
        message = tostring(message or "Unknown error")

        if type(state.error_handler) == "function" then
            state.error_handler(label, message)
            return
        end

        state.set_status(message)
    end

    function state.selected_item()
        return state.items[state.selected]
    end

    function state.clamp_selection()
        if #state.items == 0 then
            state.selected = 1
            state.offset = 1
            return
        end

        if state.selected < 1 then
            state.selected = 1
        elseif state.selected > #state.items then
            state.selected = #state.items
        end

        if state.offset < 1 then
            state.offset = 1
        end

        if state.selected < state.offset then
            state.offset = state.selected
        elseif state.selected >= state.offset + state.layout.page_size then
            state.offset = state.selected - state.layout.page_size + 1
        end

        local max_offset = math.max(
            1,
            #state.items - state.layout.page_size + 1
        )

        if state.offset > max_offset then
            state.offset = max_offset
        end
    end

    function state.load_items(message)
        local ok, result = pcall(fs.listInfo, state.cwd)

        if not ok or type(result) ~= "table" then
            state.items = {}
            state.selected = 1
            state.offset = 1
            state.report_error(
                "CANNOT OPEN FOLDER",
                "Cannot list " .. Util.display_path(state.cwd) .. ":\n" ..
                tostring(result)
            )
            return false
        end

        local new_items = {}

        if not Util.is_root(state.cwd) then
            table.insert(new_items, {
                name = "..",
                path = Util.parent_of(state.cwd),
                isDir = true,
                parent = true,
                size = 0,
            })
        end

        table.sort(result, function(a, b)
            if a.isDir ~= b.isDir then
                return a.isDir
            end

            return string.lower(a.name) < string.lower(b.name)
        end)

        for index = 1, #result do
            local entry = result[index]
            local entry_path = fs.combine(state.cwd, entry.name)

            if not Util.is_hidden_path(entry_path) then
                table.insert(new_items, {
                    name = entry.name,
                    path = entry_path,
                    isDir = entry.isDir == true,
                    parent = false,
                    size = entry.size or 0,
                    readonly = entry.readonly == true,
                })
            end
        end

        state.items = new_items
        state.clamp_selection()

        if message then
            state.set_status(message)
        else
            state.set_status(
                tostring(#state.items - (
                    Util.is_root(state.cwd) and 0 or 1
                )) .. " item(s)"
            )
        end

        return true
    end

    function state.refresh(message)
        state.load_items(message)
        state.dirty = true
    end

    function state.change_selection(delta)
        if #state.items == 0 then
            return
        end

        state.selected = state.selected + delta
        state.clamp_selection()
        state.dirty = true
    end

    function state.open_directory(item)
        if not item then
            state.report_error("OPEN FOLDER", "Nothing is selected.")
            return false
        end

        if not item.isDir then
            state.report_error(
                "OPEN FOLDER",
                "Not a folder: " .. tostring(item.name or "")
            )
            return false
        end

        state.cwd = Util.normalize_root(item.path)
        state.selected = 1
        state.offset = 1
        return state.load_items("Opened " .. Util.display_path(state.cwd))
    end

    function state.go_parent()
        if Util.is_root(state.cwd) then
            return false
        end

        state.cwd = Util.parent_of(state.cwd)
        state.selected = 1
        state.offset = 1
        return state.load_items("Opened " .. Util.display_path(state.cwd))
    end

    function state.finish_picker(path)
        local stored
        local error_message

        if not state.picker_mode then
            return false
        end

        stored, error_message = buffer.set(
            "explorer.pick_result",
            Util.normalize_launch_path(path)
        )

        if stored ~= true then
            local message = "Cannot return the selected path to the caller."

            if error_message ~= nil and error_message ~= "" then
                message = message .. "\n" .. tostring(error_message)
            end

            state.report_error("PICKER ERROR", message)
            return false
        end

        state.running = false
        return true
    end

    function state.parse_picker_args(args)
        local flag = args[1]
        local start

        if flag ~= "--pick-file" and
            flag ~= "--pick-folder" and
            flag ~= "--pick-path" and
            flag ~= "--pick" then
            return
        end

        if flag == "--pick-folder" then
            state.picker_mode = "folder"
        elseif flag == "--pick-path" then
            state.picker_mode = "path"
        else
            state.picker_mode = "file"
        end

        local cleared, clear_error = pcall(buffer.clear, "explorer.pick_result")

        if not cleared then
            state.report_error(
                "PICKER ERROR",
                "Cannot clear the previous picker result:\n" ..
                tostring(clear_error)
            )
        end

        start = Util.normalize_launch_path(args[2] or "0:/")

        if fs.exists(start) and fs.isDir(start) then
            state.cwd = start
        else
            state.cwd = "0:/"
        end

        state.selected = 1
        state.offset = 1
    end

    return state
end

return ExplorerState
