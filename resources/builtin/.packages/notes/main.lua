local gfx = require("gfx")

local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local Palette = require("ui.palette")

local NotesDocument = require("notes_document")
local NotesActions = require("notes_actions")
local Render = require("notes_render")

local state = NotesDocument.new(gfx.width(), gfx.height())
local actions = NotesActions.new(state)

Canvas.clear()
actions.start({ ... })

while state.running do
    local event = InputEvent.poll()

    if event ~= nil and not Canvas.handle_event(event) then
        actions.handle_event(event)
    end

    if state.needs_draw or Canvas.is_dirty() then
        Render.draw_editor(state)
        Canvas.render()
        state.needs_draw = false
    end

    gfx.present()
end

Canvas.clear()
gfx.clear(Palette.rich_black)
gfx.present()
