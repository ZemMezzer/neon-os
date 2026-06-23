local gfx = require("gfx")
local InputEvent = require("ui.input")
local Keys = InputEvent.Keys
local fs = require("fs")
local shell = require("shell")

local Config = require("explorer_config")
local Util = require("explorer_util")
local Palette = require("ui.palette")
local Canvas = require("ui.canvas")
local Context = require("ui.modules.context")
local MessageBox = require("ui.modules.message_box")
local InputBox = require("ui.modules.input_box")

local ExplorerActions = {}

function ExplorerActions.new(state)
    local actions = {}
    local open_selected_item
    local choose_target_folder
    local make_directory
    local rename_selected
    local delete_selected

    local function show_message(label, message, on_close)
        local box

        box = MessageBox.new(label, message, {
            {
                "OK",
                function(self)
                    Canvas.remove(self)
                    state.dirty = true

                    if type(on_close) == "function" then
                        on_close()
                    end
                end,
            },
        })

        Canvas.add(box)
        state.dirty = true
        return box
    end

    state.set_error_handler(show_message)

    local function begin_confirmation(label, message, on_yes, on_no)
        local box

        box = MessageBox.new(label, message, {
            {
                "Yes",
                function(self)
                    Canvas.remove(self)
                    state.dirty = true

                    if type(on_yes) == "function" then
                        on_yes()
                    end
                end,
            },
            {
                "No",
                function(self)
                    Canvas.remove(self)
                    state.dirty = true

                    if type(on_no) == "function" then
                        on_no()
                    end
                end,
            },
        })

        Canvas.add(box)
        state.dirty = true
        return box
    end

    local function begin_input(label, message, initial, on_submit, on_cancel)
        local box

        box = InputBox.new({
            label = label,
            message = message,
            value = initial or "",
            allow_character = function(_, character)
                return character ~= "/" and character ~= "\\"
            end,
            callback = function(self, value)
                Canvas.remove(self)
                state.dirty = true

                if type(on_submit) == "function" then
                    on_submit(value)
                end
            end,
            on_cancel = function(self)
                Canvas.remove(self)
                state.dirty = true

                if type(on_cancel) == "function" then
                    on_cancel()
                end
            end,
        })

        Canvas.add(box)
        state.dirty = true
        return box
    end

    local function open_file_with_association(item)
        if not item or item.parent or item.isDir then
            state.report_error(
                "OPEN FILE",
                "Not a file: " .. tostring(item and item.name or "")
            )
            return
        end

        state.set_status("Opening " .. item.name)
        gfx.clear(Palette.rich_black)
        gfx.present()

        local ok, result = pcall(
            shell.exec,
            "open " .. Util.shell_quote(item.path)
        )

        if not ok then
            state.refresh()
            state.report_error("OPEN FAILED", tostring(result))
        elseif result == 0 then
            state.refresh()
        else
            state.refresh()
            state.report_error(
                "OPEN FAILED",
                "Cannot open " .. item.name ..
                " (status " .. tostring(result) .. ")"
            )
        end
    end

    open_selected_item = function()
        local item = state.selected_item()

        if not item then
            state.report_error("EXPLORER", "Nothing is selected.")
            return
        end

        if state.picker_mode == "folder" then
            if item.parent then
                state.go_parent()
            elseif item.isDir then
                state.open_directory(item)
            else
                state.report_error("SELECT FOLDER", "Select a folder.")
            end
            return
        end

        if item.isDir then
            state.open_directory(item)
        elseif state.picker_mode == "file" or state.picker_mode == "path" then
            state.finish_picker(item.path)
        else
            open_file_with_association(item)
        end
    end

    local function cancel_target()
        state.target_mode = nil
        state.refresh()
    end

    local function perform_target_operation(destination, overwrite)
        local operation = state.target_mode

        if not operation then
            return
        end

        local source = operation.source.path
        local name = operation.source.name
        local kind = operation.kind
        local ok, err = pcall(function()
            if overwrite and fs.exists(destination) then
                fs.delete(destination)
            end

            if kind == "copy" then
                fs.copy(source, destination)
            else
                fs.move(source, destination)
            end
        end)

        state.target_mode = nil

        if ok then
            state.refresh((kind == "copy" and "Copied " or "Moved ") .. name)
        else
            state.refresh()
            state.report_error(
                kind == "copy" and "COPY FAILED" or "MOVE FAILED",
                tostring(err)
            )
        end
    end

    choose_target_folder = function()
        local operation = state.target_mode

        if not operation then
            return
        end

        local destination = fs.combine(state.cwd, operation.source.name)

        if Util.path_equals(destination, operation.source.path) then
            state.report_error(
                "INVALID DESTINATION",
                "The destination is the same as the source."
            )
            return
        end

        if fs.exists(destination) then
            begin_confirmation(
                "OVERWRITE",
                "Overwrite " .. Util.display_path(destination) .. "?",
                function()
                    perform_target_operation(destination, true)
                end
            )
            return
        end

        perform_target_operation(destination, false)
    end

    local function start_copy_or_move(kind)
        local item = state.selected_item()

        if not item then
            state.report_error("EXPLORER", "Nothing is selected.")
            return
        end

        if item.parent then
            state.report_error(
                kind == "copy" and "COPY" or "MOVE",
                "The parent entry cannot be " .. kind .. "d."
            )
            return
        end

        state.target_mode = {
            kind = kind,
            source = item,
        }
        state.set_status("Navigate to destination folder")
    end

    delete_selected = function()
        local item = state.selected_item()

        if not item then
            state.report_error("DELETE", "Nothing is selected.")
            return
        end

        if item.parent then
            state.report_error("DELETE", "The parent entry cannot be deleted.")
            return
        end

        begin_confirmation(
            "DELETE",
            "Delete " .. item.name .. "?",
            function()
                local ok, err = pcall(fs.delete, item.path)

                if ok then
                    state.refresh("Deleted " .. item.name)
                else
                    state.refresh()
                    state.report_error("DELETE FAILED", tostring(err))
                end
            end
        )
    end

    make_directory = function(open_after_create)
        local function ask_for_name(value)
            begin_input(
                "New Folder",
                "Name (ASCII recommended; no slash):",
                value or "",
                function(name)
                    if name == "" then
                        show_message(
                            "NEW FOLDER",
                            "Enter a folder name.",
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    if name:find("/", 1, true) or name:find("\\", 1, true) then
                        show_message(
                            "NEW FOLDER",
                            "Name cannot contain a slash.",
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    local path = fs.combine(state.cwd, name)

                    if fs.exists(path) then
                        show_message(
                            "NEW FOLDER",
                            "Already exists: " .. name,
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    local ok, err = pcall(fs.makeDir, path)

                    if not ok then
                        state.refresh()
                        show_message(
                            "CREATE FOLDER FAILED",
                            tostring(err),
                            function()
                                ask_for_name(name)
                            end
                        )
                    elseif open_after_create then
                        state.cwd = Util.normalize_root(path)
                        state.selected = 1
                        state.offset = 1
                        state.refresh("Created and opened " .. name)
                    else
                        state.refresh("Created " .. name)
                    end
                end
            )
        end

        ask_for_name("")
    end

    rename_selected = function()
        local item = state.selected_item()

        if not item then
            state.report_error("RENAME", "Nothing is selected.")
            return
        end

        if item.parent then
            state.report_error("RENAME", "The parent entry cannot be renamed.")
            return
        end

        local function ask_for_name(value)
            begin_input(
                "Rename",
                "New name for " .. item.name .. ":",
                value or "",
                function(name)
                    if name == "" then
                        show_message(
                            "RENAME",
                            "Enter a new name.",
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    if name:find("/", 1, true) or name:find("\\", 1, true) then
                        show_message(
                            "RENAME",
                            "Name cannot contain a slash.",
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    local destination = fs.combine(Util.parent_of(item.path), name)

                    if Util.path_equals(destination, item.path) then
                        show_message(
                            "RENAME",
                            "The new name is the same as the current name.",
                            function()
                                ask_for_name(name)
                            end
                        )
                        return
                    end

                    local function do_rename(overwrite)
                        local ok, err = pcall(function()
                            if overwrite and fs.exists(destination) then
                                fs.delete(destination)
                            end

                            fs.move(item.path, destination)
                        end)

                        if ok then
                            state.refresh("Renamed to " .. name)
                        else
                            state.refresh()
                            show_message(
                                "RENAME FAILED",
                                tostring(err),
                                function()
                                    ask_for_name(name)
                                end
                            )
                        end
                    end

                    if fs.exists(destination) then
                        begin_confirmation(
                            "OVERWRITE",
                            "Overwrite " .. name .. "?",
                            function()
                                do_rename(true)
                            end,
                            function()
                                ask_for_name(name)
                            end
                        )
                    else
                        do_rename(false)
                    end
                end
            )
        end

        ask_for_name("")
    end

    local function show_info()
        local item = state.selected_item()

        if not item then
            state.report_error("INFO", "Nothing is selected.")
            return
        end

        if item.parent then
            show_message("INFO", "Parent directory")
            return
        end

        local kind = item.isDir and "Directory" or "File"
        local size = item.isDir and "" or ("\nSize: " .. Util.format_size(item.size))
        local readonly = item.readonly and "\nRead-only" or ""

        show_message("INFO", kind .. ": " .. item.name .. size .. readonly)
    end

    local function is_ctrl_enter(key, modifiers)
        return key == Keys.ENTER and
            type(modifiers) == "table" and modifiers.ctrl == true
    end

    local function context_y()
        local selected_row = state.layout.list_top +
            (state.selected - state.offset) * Config.ROW_HEIGHT

        return selected_row - 3
    end

    local function open_context_menu()
        local item = state.selected_item()
        local context = item and item.name or Util.display_path(state.cwd)
        local buttons = {}
        local menu

        local function add_button(label, callback)
            table.insert(buttons, {
                label,
                function(current_menu)
                    Canvas.remove(current_menu)
                    state.dirty = true
                    callback()
                end,
            })
        end

        if state.target_mode then
            if item and item.isDir then
                add_button("Open folder", open_selected_item)
            end
            add_button("Choose this folder", choose_target_folder)
            add_button("Cancel operation", cancel_target)
        elseif state.picker_mode == "folder" then
            if item and item.isDir then
                add_button("Open folder", open_selected_item)
            end
            add_button("Choose current folder", function()
                state.finish_picker(state.cwd)
            end)
            add_button("New folder", function()
                make_directory(true)
            end)
        elseif state.picker_mode == "path" then
            if item then
                add_button(
                    item.isDir and "Open folder" or "Choose file",
                    open_selected_item
                )
            end
            add_button("Choose current folder", function()
                state.finish_picker(state.cwd)
            end)
            add_button("New folder", function()
                make_directory(true)
            end)
        elseif state.picker_mode == "file" then
            if item then
                add_button(
                    item.isDir and "Open folder" or "Choose file",
                    open_selected_item
                )
            end
            add_button("New folder", function()
                make_directory(true)
            end)
        elseif item == nil then
            add_button("New folder", function()
                make_directory(false)
            end)
            add_button("Refresh", function()
                state.refresh()
            end)
        elseif item.parent then
            add_button("Open parent", open_selected_item)
            add_button("New folder", function()
                make_directory(false)
            end)
            add_button("Refresh", function()
                state.refresh()
            end)
        else
            add_button(
                item.isDir and "Open folder" or "Open file",
                open_selected_item
            )
            add_button("Rename", rename_selected)
            add_button("Copy", function()
                start_copy_or_move("copy")
            end)
            add_button("Move", function()
                start_copy_or_move("move")
            end)
            add_button("Delete", delete_selected)
            add_button("Info", show_info)
            add_button("New folder", function()
                make_directory(false)
            end)
            add_button("Refresh", function()
                state.refresh()
            end)
        end

        menu = Context.new("ACTIONS", context, buttons)
        menu.y = context_y()
        Canvas.add(menu)
        state.dirty = true
    end

    local function handle_target_key(key)
        local shortcut = Util.lower_shortcut(key)

        if key == Keys.TAB or key == Keys.ESCAPE or shortcut == Keys.Q then
            cancel_target()
        elseif key == Keys.UP or shortcut == Keys.W then
            state.change_selection(-1)
        elseif key == Keys.DOWN or shortcut == Keys.S then
            state.change_selection(1)
        elseif key == Keys.HOME then
            state.selected = 1
            state.clamp_selection()
            state.dirty = true
        elseif key == Keys.END then
            state.selected = #state.items
            state.clamp_selection()
            state.dirty = true
        elseif key == string.byte("[") then
            state.change_selection(-state.layout.page_size)
        elseif key == string.byte("]") then
            state.change_selection(state.layout.page_size)
        elseif key == Keys.BACKSPACE or key == Keys.LEFT then
            state.go_parent()
        elseif key == Keys.RIGHT or shortcut == Keys.O then
            local item = state.selected_item()

            if item and item.isDir then
                state.open_directory(item)
            else
                state.report_error("SELECT FOLDER", "Select a folder.")
            end
        elseif key == Keys.ENTER then
            choose_target_folder()
        end
    end

    local function handle_browse_key(key, modifiers)
        local shortcut = Util.lower_shortcut(key)

        if state.picker_mode and (
            key == Keys.TAB or key == Keys.ESCAPE or shortcut == Keys.Q
        ) then
            state.running = false
            return
        end

        if not state.picker_mode and (key == Keys.ESCAPE or shortcut == Keys.Q) then
            state.running = false
            return
        end

        if is_ctrl_enter(key, modifiers) then
            open_context_menu()
            return
        end

        if (state.picker_mode == "folder" or state.picker_mode == "path") and
            (key == Keys.SPACE or shortcut == Keys.E or key == Keys.F2) then
            state.finish_picker(state.cwd)
            return
        end

        if key == Keys.UP or shortcut == Keys.W then
            state.change_selection(-1)
        elseif key == Keys.DOWN or shortcut == Keys.S then
            state.change_selection(1)
        elseif key == Keys.HOME then
            state.selected = 1
            state.clamp_selection()
            state.dirty = true
        elseif key == Keys.END then
            state.selected = #state.items
            state.clamp_selection()
            state.dirty = true
        elseif key == string.byte("[") then
            state.change_selection(-state.layout.page_size)
        elseif key == string.byte("]") then
            state.change_selection(state.layout.page_size)
        elseif key == Keys.BACKSPACE or key == Keys.LEFT then
            state.go_parent()
        elseif key == Keys.ENTER then
            open_selected_item()
        elseif key == Keys.RIGHT then
            state.open_directory(state.selected_item())
        elseif state.picker_mode then
            if shortcut == Keys.N then
                make_directory(true)
            end
        elseif shortcut == Keys.E then
            open_selected_item()
        elseif shortcut == Keys.D or key == Keys.DELETE then
            delete_selected()
        elseif shortcut == Keys.N then
            make_directory(false)
        elseif shortcut == Keys.R then
            rename_selected()
        elseif shortcut == Keys.C then
            start_copy_or_move("copy")
        elseif shortcut == Keys.M then
            start_copy_or_move("move")
        elseif shortcut == Keys.F then
            state.refresh()
        elseif shortcut == Keys.I then
            show_info()
        end
    end

    function actions.handle_event(event)
        if type(event) ~= "table" or event.type ~= "key" then
            return false
        end

        if type(event.key) ~= "number" then
            return false
        end

        if state.target_mode then
            handle_target_key(event.key)
        else
            handle_browse_key(event.key, event.modifiers)
        end

        return true
    end

    return actions
end

return ExplorerActions
