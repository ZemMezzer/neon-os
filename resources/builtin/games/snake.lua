-- NeonOS Snake
--
-- Requires:
--   local gfx = require("gfx")
--   local input = require("input")
--
-- Controls:
--   Menus: Up/Down, Enter, Q
--   Game:  Arrows/WASD, P or Space, R restart, L menu, Q quit
--
-- Uses standard Lua os.clock(), therefore the ARM64 generic-timer time.c
-- implementation must be present in NeonOS.

local gfx = require("gfx")
local input = require("input")

local GRID_W = 30
local GRID_H = 20
local DRAW_INTERVAL = 1 / 60
local MAX_TICKS_PER_FRAME = 5

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR_BG          = 0x000A101A
local COLOR_PANEL       = 0x000E1928
local COLOR_BOARD       = 0x00050C14
local COLOR_BORDER      = 0x00345176
local COLOR_TEXT        = 0x00ECF4FF
local COLOR_MUTED       = 0x0094A9C4
local COLOR_YELLOW      = 0x00FFE65C
local COLOR_GREEN       = 0x0058E69A
local COLOR_HEAD        = 0x0092FFC8
local COLOR_FOOD        = 0x00FF6681
local COLOR_OBSTACLE    = 0x00435C7A
local COLOR_ORANGE      = 0x00FFB65C
local COLOR_RED         = 0x00FF5B70
local COLOR_CYAN        = 0x0063DDFC
local COLOR_BLACK       = 0x00000000

local levels = {
    {
        name = "EMPTY FIELD",
        description = "Classic snake without obstacles.",
        build = function()
            return {}
        end,
    },
    {
        name = "CENTER WALL",
        description = "Vertical wall with a gap in the middle.",
        build = function()
            local cells = {}
            local x = math.floor(GRID_W / 2)
            local gap_y = math.floor(GRID_H / 2)

            for y = 2, GRID_H - 1 do
                if math.abs(y - gap_y) > 1 then
                    cells[#cells + 1] = { x = x, y = y }
                end
            end

            return cells
        end,
    },
    {
        name = "SMALL CROSS",
        description = "Cross-shaped barrier at the centre.",
        build = function()
            local cells = {}
            local cx = math.floor(GRID_W / 2)
            local cy = math.floor(GRID_H / 2)

            for x = cx - 5, cx + 5 do
                cells[#cells + 1] = { x = x, y = cy }
            end

            for y = cy - 3, cy + 3 do
                cells[#cells + 1] = { x = cx, y = y }
            end

            return cells
        end,
    },
    {
        name = "FOUR BLOCKS",
        description = "Four obstacle islands leave narrow routes.",
        build = function()
            local cells = {}
            local centers = {
                { x = 8,  y = 6 },
                { x = 23, y = 6 },
                { x = 8,  y = 15 },
                { x = 23, y = 15 },
            }

            for i = 1, #centers do
                local c = centers[i]

                for dy = -1, 1 do
                    for dx = -2, 2 do
                        cells[#cells + 1] = {
                            x = c.x + dx,
                            y = c.y + dy,
                        }
                    end
                end
            end

            return cells
        end,
    },
    {
        name = "CORRIDORS",
        description = "Horizontal corridors with alternating openings.",
        build = function()
            local cells = {}

            for x = 3, GRID_W - 3 do
                if x < 20 then
                    cells[#cells + 1] = { x = x, y = 6 }
                end

                if x > 10 then
                    cells[#cells + 1] = { x = x, y = 11 }
                end

                if x < 19 then
                    cells[#cells + 1] = { x = x, y = 16 }
                end
            end

            return cells
        end,
    },
}

local difficulties = {
    {
        name = "EASY",
        description = "Slow start, gentle acceleration.",
        start_delay = 0.22,
        min_delay = 0.085,
        acceleration = 0.985,
    },
    {
        name = "MEDIUM",
        description = "Normal speed.",
        start_delay = 0.135,
        min_delay = 0.055,
        acceleration = 0.972,
    },
    {
        name = "HARD",
        description = "Fast start and sharper acceleration.",
        start_delay = 0.085,
        min_delay = 0.040,
        acceleration = 0.955,
    },
}

local function now()
    local ok, value = pcall(os.clock)

    if ok and type(value) == "number" then
        return value
    end

    return 0
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

local CELL = math.floor(math.min(
    (SCREEN_W - 84) / GRID_W,
    (SCREEN_H - 178) / GRID_H
))

CELL = clamp(CELL, 8, 26)

local BOARD_W = GRID_W * CELL
local BOARD_H = GRID_H * CELL
local BOARD_X = math.floor((SCREEN_W - BOARD_W) / 2)
local BOARD_Y = 136

if BOARD_Y + BOARD_H > SCREEN_H - 28 then
    BOARD_Y = SCREEN_H - BOARD_H - 28
end

local state = "LEVEL_MENU"
local running = true
local paused = false
local game_over = false
local won = false

local selected_level = 1
local selected_difficulty = 2

local score = 0
local high_score = 0
local delay = difficulties[selected_difficulty].start_delay
local message = "SELECT A LEVEL"

local snake = {}
local food = nil
local obstacles = {}
local obstacle_map = {}

local direction = { x = 1, y = 0 }
local next_direction = { x = 1, y = 0 }

local last_tick = 0
local last_draw = 0
local dirty = true

local function cell_key(x, y)
    return tostring(x) .. ":" .. tostring(y)
end

local function is_obstacle(x, y)
    return obstacle_map[cell_key(x, y)] == true
end

local function snake_contains(x, y, last_index)
    local count = last_index or #snake

    for i = 1, count do
        if snake[i].x == x and snake[i].y == y then
            return true
        end
    end

    return false
end

local function rebuild_obstacle_map()
    obstacle_map = {}

    for i = 1, #obstacles do
        local item = obstacles[i]

        if item.x >= 1 and item.x <= GRID_W and item.y >= 1 and item.y <= GRID_H then
            obstacle_map[cell_key(item.x, item.y)] = true
        end
    end
end

local function find_start_position()
    local preferred_x = math.floor(GRID_W / 2)
    local preferred_y = math.floor(GRID_H / 2)

    local function can_start(x, y)
        return (
            x >= 3 and x <= GRID_W and y >= 1 and y <= GRID_H and
            not is_obstacle(x, y) and
            not is_obstacle(x - 1, y) and
            not is_obstacle(x - 2, y)
        )
    end

    if can_start(preferred_x, preferred_y) then
        return preferred_x, preferred_y
    end

    for y = 1, GRID_H do
        for x = 3, GRID_W do
            if can_start(x, y) then
                return x, y
            end
        end
    end

    return 4, 2
end

local function spawn_food()
    local max_attempts = GRID_W * GRID_H * 3

    for _ = 1, max_attempts do
        local x = math.random(1, GRID_W)
        local y = math.random(1, GRID_H)

        if not is_obstacle(x, y) and not snake_contains(x, y) then
            food = { x = x, y = y }
            return
        end
    end

    -- The snake filled every usable cell.
    food = nil
    won = true
    game_over = true
    paused = false
    message = "BOARD CLEARED"
end

local function reset_game()
    local difficulty = difficulties[selected_difficulty]

    score = 0
    delay = difficulty.start_delay
    paused = false
    game_over = false
    won = false
    message = "GO"

    obstacles = levels[selected_level].build()
    rebuild_obstacle_map()

    local start_x, start_y = find_start_position()

    snake = {
        { x = start_x,     y = start_y },
        { x = start_x - 1, y = start_y },
        { x = start_x - 2, y = start_y },
    }

    direction = { x = 1, y = 0 }
    next_direction = { x = 1, y = 0 }

    spawn_food()
    last_tick = now()
    dirty = true
end

local function set_direction(x, y)
    -- Reject immediate 180-degree turns.
    if direction.x + x == 0 and direction.y + y == 0 then
        return
    end

    next_direction = { x = x, y = y }
end

local function end_game(reason)
    game_over = true
    paused = false
    won = false
    message = reason

    if score > high_score then
        high_score = score
    end

    dirty = true
end

local function step_game()
    if state ~= "GAME" or paused or game_over then
        return
    end

    direction = next_direction

    local head = snake[1]
    local new_head = {
        x = head.x + direction.x,
        y = head.y + direction.y,
    }

    if (
        new_head.x < 1 or new_head.x > GRID_W or
        new_head.y < 1 or new_head.y > GRID_H
    ) then
        end_game("HIT THE WALL")
        return
    end

    if is_obstacle(new_head.x, new_head.y) then
        end_game("HIT AN OBSTACLE")
        return
    end

    local eating = food ~= nil and new_head.x == food.x and new_head.y == food.y
    local body_limit = #snake

    -- Moving onto the current tail is legal if the snake will not grow.
    if not eating then
        body_limit = #snake - 1
    end

    if snake_contains(new_head.x, new_head.y, body_limit) then
        end_game("HIT YOURSELF")
        return
    end

    table.insert(snake, 1, new_head)

    if eating then
        score = score + 1

        if score > high_score then
            high_score = score
        end

        delay = math.max(
            difficulties[selected_difficulty].min_delay,
            delay * difficulties[selected_difficulty].acceleration
        )

        message = "SCORE " .. score
        spawn_food()
    else
        table.remove(snake)
    end

    dirty = true
end

local function pixel_for_cell(x, y)
    return BOARD_X + (x - 1) * CELL,
           BOARD_Y + (y - 1) * CELL
end

local function draw_disc(cx, cy, radius, color)
    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))
        gfx.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)
    end
end

local function draw_text_center(y, text, color, scale)
    scale = scale or 1
    local estimated_width = #text * 8 * scale
    local x = math.floor((SCREEN_W - estimated_width) / 2)

    gfx.text(x, y, text, color, scale)
end

local function draw_menu(title, instructions, options, selected, description)
    gfx.clear(COLOR_BG)

    draw_text_center(34, title, COLOR_YELLOW, 3)
    draw_text_center(88, instructions, COLOR_MUTED, 1)

    local panel_w = math.min(640, SCREEN_W - 96)
    local panel_h = 60 + #options * 54
    local panel_x = math.floor((SCREEN_W - panel_w) / 2)
    local panel_y = math.floor((SCREEN_H - panel_h) / 2) - 16

    gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR_PANEL)
    gfx.rect(panel_x, panel_y, panel_w, panel_h, COLOR_BORDER)

    for i = 1, #options do
        local y = panel_y + 26 + (i - 1) * 54
        local active = i == selected
        local color = active and COLOR_GREEN or COLOR_TEXT

        if active then
            gfx.fill_rect(panel_x + 18, y - 10, panel_w - 36, 38, 0x00182D42)
            gfx.rect(panel_x + 18, y - 10, panel_w - 36, 38, COLOR_GREEN)
            gfx.text(panel_x + 32, y, "> " .. options[i], color, 2)
        else
            gfx.text(panel_x + 32, y, "  " .. options[i], color, 2)
        end
    end

    draw_text_center(SCREEN_H - 52, description, COLOR_CYAN, 1)
    gfx.present()
end

local function draw_game()
    local level = levels[selected_level]
    local difficulty = difficulties[selected_difficulty]

    gfx.clear(COLOR_BG)

    gfx.text(34, 26, "SNAKE", COLOR_YELLOW, 3)
    gfx.text(
        230,
        40,
        "SCORE " .. score .. "   BEST " .. high_score,
        COLOR_TEXT,
        2
    )
    gfx.text(
        34,
        90,
        level.name .. " / " .. difficulty.name,
        COLOR_MUTED,
        1
    )

    if game_over then
        gfx.text(34, 112, "R / ENTER RESTART     L MENU     Q QUIT", COLOR_RED, 1)
    elseif paused then
        gfx.text(34, 112, "PAUSED - P / SPACE RESUME     Q QUIT", COLOR_ORANGE, 1)
    else
        gfx.text(34, 112, "ARROWS / WASD MOVE     P / SPACE PAUSE     Q QUIT", COLOR_MUTED, 1)
    end

    gfx.fill_rect(BOARD_X - 6, BOARD_Y - 6, BOARD_W + 12, BOARD_H + 12, COLOR_BORDER)
    gfx.fill_rect(BOARD_X - 2, BOARD_Y - 2, BOARD_W + 4, BOARD_H + 4, COLOR_BLACK)
    gfx.fill_rect(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, COLOR_BOARD)

    for i = 1, #obstacles do
        local obstacle = obstacles[i]
        local px, py = pixel_for_cell(obstacle.x, obstacle.y)

        gfx.fill_rect(px + 2, py + 2, CELL - 4, CELL - 4, COLOR_OBSTACLE)
        gfx.rect(px + 2, py + 2, CELL - 4, CELL - 4, COLOR_BORDER)
    end

    if food then
        local px, py = pixel_for_cell(food.x, food.y)
        local radius = math.max(3, math.floor(CELL * 0.26))

        draw_disc(
            px + math.floor(CELL / 2),
            py + math.floor(CELL / 2),
            radius,
            COLOR_FOOD
        )
    end

    for i = #snake, 1, -1 do
        local part = snake[i]
        local px, py = pixel_for_cell(part.x, part.y)
        local color = i == 1 and COLOR_HEAD or COLOR_GREEN
        local inset = i == 1 and 2 or 3

        gfx.fill_rect(
            px + inset,
            py + inset,
            CELL - inset * 2,
            CELL - inset * 2,
            color
        )

        if i == 1 then
            gfx.rect(
                px + inset,
                py + inset,
                CELL - inset * 2,
                CELL - inset * 2,
                COLOR_TEXT
            )
        end
    end

    gfx.text(34, SCREEN_H - 28, message, COLOR_MUTED, 1)

    if game_over then
        local panel_w = 330
        local panel_h = 68
        local panel_x = math.floor((SCREEN_W - panel_w) / 2)
        local panel_y = math.floor(BOARD_Y + BOARD_H / 2 - panel_h / 2)
        local color = won and COLOR_GREEN or COLOR_RED
        local label = won and "BOARD CLEARED" or "GAME OVER"

        gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR_BLACK)
        gfx.rect(panel_x, panel_y, panel_w, panel_h, color)
        gfx.text(panel_x + 48, panel_y + 23, label, color, 2)
    elseif paused then
        local panel_w = 220
        local panel_h = 62
        local panel_x = math.floor((SCREEN_W - panel_w) / 2)
        local panel_y = math.floor(BOARD_Y + BOARD_H / 2 - panel_h / 2)

        gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR_BLACK)
        gfx.rect(panel_x, panel_y, panel_w, panel_h, COLOR_ORANGE)
        gfx.text(panel_x + 48, panel_y + 21, "PAUSED", COLOR_ORANGE, 2)
    end

    gfx.present()
end

local function draw_current_screen()
    if state == "LEVEL_MENU" then
        local options = {}

        for i = 1, #levels do
            options[i] = levels[i].name
        end

        draw_menu(
            "SNAKE - SELECT LEVEL",
            "UP / DOWN SELECT     ENTER START     Q QUIT",
            options,
            selected_level,
            levels[selected_level].description
        )
    elseif state == "DIFFICULTY_MENU" then
        local options = {}

        for i = 1, #difficulties do
            options[i] = difficulties[i].name
        end

        draw_menu(
            "SNAKE - SELECT DIFFICULTY",
            "UP / DOWN SELECT     ENTER START     Q QUIT",
            options,
            selected_difficulty,
            difficulties[selected_difficulty].description
        )
    else
        draw_game()
    end

    dirty = false
    last_draw = now()
end

local function select_next(count, delta, current)
    current = current + delta

    if current < 1 then
        return count
    end

    if current > count then
        return 1
    end

    return current
end

local function enter_game()
    state = "GAME"
    reset_game()
    dirty = true
end

local function handle_key(key)
    if key == nil then
        return
    end

    if key == input.Q then
        running = false
        return
    end

    if state == "LEVEL_MENU" then
        if key == input.UP or key == input.W then
            selected_level = select_next(#levels, -1, selected_level)
            dirty = true
        elseif key == input.DOWN or key == input.S then
            selected_level = select_next(#levels, 1, selected_level)
            dirty = true
        elseif key == input.ENTER then
            state = "DIFFICULTY_MENU"
            dirty = true
        end

        return
    end

    if state == "DIFFICULTY_MENU" then
        if key == input.UP or key == input.W then
            selected_difficulty = select_next(#difficulties, -1, selected_difficulty)
            dirty = true
        elseif key == input.DOWN or key == input.S then
            selected_difficulty = select_next(#difficulties, 1, selected_difficulty)
            dirty = true
        elseif key == input.ENTER then
            enter_game()
        end

        return
    end

    if key == input.P or key == input.SPACE then
        if not game_over then
            paused = not paused
            last_tick = now()
            dirty = true
        end

        return
    end

    if game_over then
        if key == input.R or key == input.ENTER then
            reset_game()
        elseif key == input.L then
            state = "LEVEL_MENU"
            paused = false
            dirty = true
        end

        return
    end

    if key == input.UP or key == input.W then
        set_direction(0, -1)
    elseif key == input.DOWN or key == input.S then
        set_direction(0, 1)
    elseif key == input.LEFT or key == input.A then
        set_direction(-1, 0)
    elseif key == input.RIGHT or key == input.D then
        set_direction(1, 0)
    end
end

math.randomseed(math.floor(now() * 1000000) + 1337)
math.random()
math.random()
math.random()

last_tick = now()
last_draw = last_tick

while running do
    -- Exactly one non-blocking input read per loop: matches NeonOS input.poll().
    handle_key(input.poll())

    local current = now()

    if state == "GAME" and not paused and not game_over then
        local ticks = 0

        while current - last_tick >= delay and ticks < MAX_TICKS_PER_FRAME do
            step_game()
            last_tick = last_tick + delay
            ticks = ticks + 1
        end

        if ticks == MAX_TICKS_PER_FRAME then
            last_tick = current
        end
    else
        last_tick = current
    end

    if dirty or current - last_draw >= DRAW_INTERVAL then
        draw_current_screen()
    end
end
