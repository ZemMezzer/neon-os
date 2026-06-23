local gfx = require("gfx")
local Palette = require("ui.palette")
local Canvas = require("ui.canvas")
local InputEvent = require("ui.input")
local ExplorerState = require("explorer_state")
local ExplorerActions = require("explorer_actions")
local Render = require("explorer_render")

local state = ExplorerState.new(gfx.width(), gfx.height())
local actions = ExplorerActions.new(state)

Canvas.clear()
state.parse_picker_args({ ... })
state.refresh("Ready")

while state.running do
    local event = InputEvent.poll()

    if event ~= nil then
        if not Canvas.handle_event(event) then
            actions.handle_event(event)
        end
    end

    if state.dirty or Canvas.is_dirty() then
        Render.draw_ui(state)
        Canvas.render()
        state.dirty = false
    end

    gfx.present()
end

Canvas.clear()
gfx.clear(Palette.rich_black)
gfx.present()
