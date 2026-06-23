local gfx = require("gfx")
local fs = require("fs")
local shell = require("shell")
local buffer = require("buffer")

local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local InputBox = require("ui.modules.input_box")
local MessageBox = require("ui.modules.message_box")
local Palette = require("ui.palette")

local Config = require("notes_config")
local Util = require("notes_util")

local NotesActions = {}

local Keys = InputEvent.Keys

function NotesActions.new(state)
    local actions = {}

    local function show_message(label, message, on_close)
        local box

        box = MessageBox.new(label, message, {
            {
                "OK",
                function(self)
                    Canvas.remove(self)

                    if type(on_close) == "function" then
                        on_close()
                    end
                end,
            },
        })

        Canvas.add(box)
        state.needs_draw = true
    end

    local function close_after_error(exit_after_error)
        if exit_after_error then
            return function()
                state.running = false
            end
        end

        return nil
    end

    local function show_save_error(message)
        show_message("Save failed", message)
    end

    local function save_document()
        if state.file_path == nil or state.file_path == "" then
            show_save_error("No file path is selected.")
            return false
        end

        local ready, directory_error = Util.ensure_directory(
            Util.path_dir(state.file_path)
        )

        if not ready then
            show_save_error("Cannot create the parent folder:\n" ..
                tostring(directory_error))
            return false
        end

        local handle, open_error = io.open(state.file_path, "w")

        if not handle then
            show_save_error("Cannot open the file for writing:\n" ..
                tostring(open_error))
            return false
        end

        local wrote, write_error = pcall(function()
            for index = 1, #state.lines do
                local ok, error_message = handle:write(state.lines[index] or "")

                if not ok then
                    error(error_message or "Cannot write the file")
                end

                if index < #state.lines then
                    ok, error_message = handle:write("\n")

                    if not ok then
                        error(error_message or "Cannot write the file")
                    end
                end
            end
        end)
        local closed, close_error = handle:close()

        if not wrote then
            show_save_error("Cannot write the file:\n" .. tostring(write_error))
            return false
        end

        if not closed then
            show_save_error("Cannot finish writing the file:\n" ..
                tostring(close_error))
            return false
        end

        state.dirty = false
        state.needs_draw = true
        return true
    end

    local function load_editor_file(path, exit_after_error)
        path = Util.normalize_path(path)

        if path == "" then
            show_message(
                "Open failed",
                "Select a file to open.",
                close_after_error(exit_after_error)
            )
            return false
        end

        local checked, exists = pcall(fs.exists, path)

        if not checked then
            show_message(
                "Open failed",
                "Cannot inspect this path:\n" .. path,
                close_after_error(exit_after_error)
            )
            return false
        end

        if exists then
            local inspected, is_directory = pcall(fs.isDir, path)

            if not inspected then
                show_message(
                    "Open failed",
                    "Cannot inspect this path:\n" .. path,
                    close_after_error(exit_after_error)
                )
                return false
            end

            if is_directory then
                show_message(
                    "Open failed",
                    "Select a file, not a folder.",
                    close_after_error(exit_after_error)
                )
                return false
            end

            local loaded, opened, read_error = Util.read_document(path)

            if not opened then
                show_message(
                    "Open failed",
                    "Cannot read the file:\n" .. tostring(read_error),
                    close_after_error(exit_after_error)
                )
                return false
            end

            state.load_document(path, loaded, true)
            return true
        end

        state.load_document(path, { "" }, false)
        return true
    end

    local function prepare_external_program()
        gfx.clear(Palette.rich_black)
        gfx.present()
    end

    local function choose_in_explorer(kind, start_path, exit_after_error)
        local flag

        if kind == "folder" then
            flag = "--pick-folder"
        elseif kind == "path" then
            flag = "--pick-path"
        else
            flag = "--pick-file"
        end

        local start = Util.normalize_path(start_path or "0:/")
        local ok
        local result
        local picked
        local buffer_error

        buffer.clear("explorer.pick_result")
        prepare_external_program()

        ok, result = pcall(
            shell.exec,
            "open " .. Util.shell_quote("explorer") ..
            " " .. flag .. " " .. Util.shell_quote(start)
        )

        if not ok then
            show_message(
                "Explorer failed",
                tostring(result),
                close_after_error(exit_after_error)
            )
            return nil, false
        end

        if result ~= 0 then
            show_message(
                "Explorer failed",
                "Explorer exited with code " .. tostring(result) .. ".",
                close_after_error(exit_after_error)
            )
            return nil, false
        end

        picked, buffer_error = buffer.take("explorer.pick_result")

        if picked == nil then
            if buffer_error ~= nil and buffer_error ~= "" then
                show_message(
                    "Picker failed",
                    tostring(buffer_error),
                    close_after_error(exit_after_error)
                )
                return nil, false
            end

            return nil, true
        end

        return Util.normalize_path(picked), false
    end

    local function ask_file_name(directory, initial, on_accept, on_cancel)
        local box

        box = InputBox.new({
            label = "New Note",
            message = "Folder: " .. Util.display_path(directory) ..
                "\nEnter a file name:",
            value = initial or "untitled.txt",
            max_length = 80,
            allow_character = function(_, character)
                return Util.is_valid_file_name_character(character)
            end,
            callback = function(self, value)
                local name, error_message = Util.valid_file_name(value)

                if name == nil then
                    show_message(
                        "Invalid file name",
                        error_message or "Enter a valid file name."
                    )
                    return
                end

                Canvas.remove(self)
                on_accept(Util.normalize_path(fs.combine(directory, name)))
            end,
            on_cancel = function(self)
                Canvas.remove(self)

                if type(on_cancel) == "function" then
                    on_cancel()
                end
            end,
        })

        Canvas.add(box)
        state.needs_draw = true
    end

    local function open_from_explorer()
        local picked = choose_in_explorer(
            "file",
            state.file_path ~= nil and Util.path_dir(state.file_path) or
                Config.DEFAULT_NOTES_DIR
        )

        if picked ~= nil then
            load_editor_file(picked)
        end
    end

    local function save_as_from_explorer()
        local folder = choose_in_explorer(
            "folder",
            state.file_path ~= nil and Util.path_dir(state.file_path) or
                Config.DEFAULT_NOTES_DIR
        )

        if folder == nil then
            return
        end

        ask_file_name(
            folder,
            "untitled.txt",
            function(path)
                state.file_path = path
                save_document()
            end
        )
    end

    local function show_exit_confirmation()
        local box

        box = MessageBox.new(
            "Exit",
            "Save changes before exit?",
            {
                {
                    "Save",
                    function(self)
                        Canvas.remove(self)

                        if save_document() then
                            state.running = false
                        end
                    end,
                },
                {
                    "Discard",
                    function(self)
                        Canvas.remove(self)
                        state.running = false
                    end,
                },
                {
                    "Cancel",
                    function(self)
                        Canvas.remove(self)
                    end,
                },
            }
        )

        Canvas.add(box)
        state.needs_draw = true
    end

    local function request_exit()
        if state.dirty then
            show_exit_confirmation()
        else
            state.running = false
        end
    end

    local function show_discard_open_confirmation()
        local box

        box = MessageBox.new(
            "Open",
            "Discard unsaved changes and open another file?",
            {
                {
                    "Open",
                    function(self)
                        Canvas.remove(self)
                        open_from_explorer()
                    end,
                },
                {
                    "Cancel",
                    function(self)
                        Canvas.remove(self)
                    end,
                },
            }
        )

        Canvas.add(box)
        state.needs_draw = true
    end

    local function request_open()
        if state.dirty then
            show_discard_open_confirmation()
        else
            open_from_explorer()
        end
    end

    local function copy_selection()
        if not state.has_selection() then
            show_message("Copy", "Select text before copying.")
            return
        end

        local copied, error_message = buffer.clipboard_set(state.selected_text())

        if copied ~= true then
            show_message(
                "Clipboard failed",
                error_message ~= nil and tostring(error_message) or
                    "Cannot copy the selected text."
            )
        end
    end

    local function cut_selection()
        if not state.has_selection() then
            show_message("Cut", "Select text before cutting.")
            return
        end

        local copied, error_message = buffer.clipboard_set(state.selected_text())

        if copied ~= true then
            show_message(
                "Clipboard failed",
                error_message ~= nil and tostring(error_message) or
                    "Cannot copy the selected text."
            )
            return
        end

        state.delete_selection()
    end

    local function paste_clipboard()
        local clipboard_text, error_message = buffer.clipboard_get()

        if clipboard_text == nil then
            if error_message ~= nil and error_message ~= "" then
                show_message("Clipboard failed", tostring(error_message))
            else
                show_message("Paste", "The clipboard is empty.")
            end

            return
        end

        state.insert_text(clipboard_text)
    end

    function actions.start(args)
        args = type(args) == "table" and args or {}

        local requested = args[1]

        if requested == nil or requested == "" then
            local selected_path, cancelled = choose_in_explorer(
                "path",
                Config.DEFAULT_NOTES_DIR,
                true
            )

            if selected_path == nil then
                if cancelled then
                    state.running = false
                end
                return
            end

            if fs.exists(selected_path) and fs.isDir(selected_path) then
                ask_file_name(
                    selected_path,
                    "untitled.txt",
                    function(path)
                        load_editor_file(path)
                    end,
                    function()
                        state.running = false
                    end
                )
            else
                load_editor_file(selected_path, true)
            end

            return
        end

        local path = Util.normalize_path(requested)

        if fs.exists(path) and fs.isDir(path) then
            local picked, cancelled = choose_in_explorer("file", path, true)

            if picked ~= nil then
                load_editor_file(picked, true)
            elseif cancelled then
                state.running = false
            end

            return
        end

        local ready, error_message = Util.ensure_directory(Util.path_dir(path))

        if not ready then
            show_message(
                "Cannot create folder",
                tostring(error_message),
                function() state.running = false end
            )
            return
        end

        load_editor_file(path, true)
    end

    function actions.handle_event(event)
        if not InputEvent.is_key(event) then
            return false
        end

        local key = event.key
        local modifiers = type(event.modifiers) == "table" and
            event.modifiers or {}
        local control = Util.lower_control_key(key)

        if modifiers.ctrl then
            if control == Keys.A then
                state.select_all()
            elseif control == Keys.C then
                copy_selection()
            elseif control == Keys.X then
                cut_selection()
            elseif control == Keys.V then
                paste_clipboard()
            elseif control == Keys.Z then
                if not state.undo() then
                    show_message("Undo", "Nothing to undo.")
                end
            elseif control == Keys.O then
                request_open()
            elseif control == Keys.S then
                if modifiers.shift then
                    save_as_from_explorer()
                else
                    save_document()
                end
            elseif control == Keys.Q then
                request_exit()
            end

            state.needs_draw = true
            return true
        end

        if key == Keys.ESCAPE then
            request_exit()
        elseif key == Keys.F2 then
            save_document()
        elseif key == Keys.LEFT then
            state.move_cursor(-1, 0, modifiers.shift)
        elseif key == Keys.RIGHT then
            state.move_cursor(1, 0, modifiers.shift)
        elseif key == Keys.UP then
            state.move_cursor(0, -1, modifiers.shift)
        elseif key == Keys.DOWN then
            state.move_cursor(0, 1, modifiers.shift)
        elseif key == Keys.PAGE_UP then
            state.page_move(-state.layout().rows, modifiers.shift)
        elseif key == Keys.PAGE_DOWN then
            state.page_move(state.layout().rows, modifiers.shift)
        elseif key == Keys.HOME then
            state.home_key(modifiers.shift)
        elseif key == Keys.END then
            state.end_key(modifiers.shift)
        elseif key == Keys.BACKSPACE then
            state.backspace()
        elseif key == Keys.DELETE then
            state.delete_forward()
        elseif key == Keys.TAB then
            state.insert_text("\t")
        elseif key == Keys.ENTER then
            state.insert_new_line()
        elseif type(key) == "number" and key >= 32 and key <= 126 then
            state.insert_text(string.char(key))
        end

        state.needs_draw = true
        return true
    end

    return actions
end

return NotesActions
