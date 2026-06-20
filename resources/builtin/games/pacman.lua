-- NeonOS Pac-Man
-- Ported for:
--   local gfx = require("gfx")
--   local input = require("input")
--
-- Controls:
--   Arrows / WASD  Move
--   P              Pause
--   R              Restart after game over
--   Q              Quit
--
-- This version uses os.clock() from the ordinary Lua standard library for
-- game timing. The graphics and keyboard APIs are NeonOS-specific.

local gfx = require("gfx")
local input = require("input")

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR_BG          = 0x000A101C
local COLOR_BOARD       = 0x00030A14
local COLOR_BORDER      = 0x001D3D70
local COLOR_WALL        = 0x003E79FF
local COLOR_WALL_EDGE   = 0x008AB4FF
local COLOR_TEXT        = 0x00F3F7FF
local COLOR_MUTED       = 0x008EA4C6
local COLOR_YELLOW      = 0x00FFE65C
local COLOR_DOT         = 0x00F6F4DB
local COLOR_POWER       = 0x00FF9BC7
local COLOR_FRIGHTENED  = 0x006DA8FF
local COLOR_RED         = 0x00FF5C70
local COLOR_PINK        = 0x00FF8EC7
local COLOR_ORANGE      = 0x00FFB65C
local COLOR_CYAN        = 0x005EEBFF
local COLOR_BLACK       = 0x00000000

local MAP_TEMPLATE = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o#  #.#   #.##.#   #.#  #o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##          ##.#     ",
    "######.##.###  ###.##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o...#................#...o#",
    "############################",
}

local MAP_H = #MAP_TEMPLATE
local MAP_W = #MAP_TEMPLATE[1]

local TILE = 22
local BOARD_W = MAP_W * TILE
local BOARD_H = MAP_H * TILE
local BOARD_X = math.floor((SCREEN_W - BOARD_W) / 2)
local BOARD_Y = 150

if BOARD_Y + BOARD_H > SCREEN_H - 18 then
    BOARD_Y = SCREEN_H - BOARD_H - 18
end

local dirs = {
    up = { x = 0, y = -1 },
    down = { x = 0, y = 1 },
    left = { x = -1, y = 0 },
    right = { x = 1, y = 0 },
}

local dir_names = { "up", "down", "left", "right" }

local running = true
local paused = false
local game_over = false

local score = 0
local lives = 3
local wave = 1
local message = "EAT ALL DOTS"

local tick_time = 0.14
local tick_count = 0
local power_ticks = 0
local dots_left = 0

local player = nil
local ghosts = {}
local tiles = {}

local function now()
    local ok, value = pcall(os.clock)

    if ok and type(value) == "number" then
        return value
    end

    return 0
end

local function normalize_x(x)
    if x < 1 then
        return MAP_W
    end

    if x > MAP_W then
        return 1
    end

    return x
end

local function tile_at(x, y)
    if x < 1 or x > MAP_W or y < 1 or y > MAP_H then
        return "#"
    end

    return tiles[y][x]
end

local function set_tile(x, y, value)
    if x >= 1 and x <= MAP_W and y >= 1 and y <= MAP_H then
        tiles[y][x] = value
    end
end

local function target_position(x, y, dir_name)
    local dir = dirs[dir_name]

    if not dir then
        return x, y
    end

    return normalize_x(x + dir.x), y + dir.y
end

local function can_move_from(x, y, dir_name)
    local nx, ny = target_position(x, y, dir_name)

    return tile_at(nx, ny) ~= "#"
end

local function opposite_dir(dir_name)
    if dir_name == "up" then
        return "down"
    elseif dir_name == "down" then
        return "up"
    elseif dir_name == "left" then
        return "right"
    elseif dir_name == "right" then
        return "left"
    end

    return nil
end

local function distance_to_player(x, y)
    return math.abs(player.x - x) + math.abs(player.y - y)
end

local function cell_pixel(x, y)
    return BOARD_X + (x - 1) * TILE, BOARD_Y + (y - 1) * TILE
end

local function draw_disc(cx, cy, radius, color)
    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))
        gfx.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)
    end
end

local function draw_pacman(x, y, dir_name)
    local px, py = cell_pixel(x, y)
    local cx = px + math.floor(TILE / 2)
    local cy = py + math.floor(TILE / 2)

    draw_disc(cx, cy, 8, COLOR_YELLOW)

    if dir_name == "right" then
        gfx.fill_rect(cx + 2, cy - 3, 8, 7, COLOR_BOARD)
    elseif dir_name == "left" then
        gfx.fill_rect(cx - 9, cy - 3, 8, 7, COLOR_BOARD)
    elseif dir_name == "up" then
        gfx.fill_rect(cx - 3, cy - 9, 7, 8, COLOR_BOARD)
    elseif dir_name == "down" then
        gfx.fill_rect(cx - 3, cy + 2, 7, 8, COLOR_BOARD)
    end
end

local function draw_ghost(ghost)
    local px, py = cell_pixel(ghost.x, ghost.y)
    local cx = px + math.floor(TILE / 2)
    local cy = py + math.floor(TILE / 2)
    local color = power_ticks > 0 and COLOR_FRIGHTENED or ghost.color

    draw_disc(cx, cy - 3, 8, color)
    gfx.fill_rect(cx - 8, cy - 2, 17, 11, color)

    -- Little feet.
    gfx.fill_rect(cx - 5, cy + 7, 3, 3, COLOR_BOARD)
    gfx.fill_rect(cx + 3, cy + 7, 3, 3, COLOR_BOARD)

    if power_ticks <= 0 then
        draw_disc(cx - 3, cy - 3, 2, COLOR_TEXT)
        draw_disc(cx + 3, cy - 3, 2, COLOR_TEXT)
        gfx.fill_rect(cx - 3, cy - 3, 2, 2, COLOR_BLACK)
        gfx.fill_rect(cx + 3, cy - 3, 2, 2, COLOR_BLACK)
    else
        gfx.fill_rect(cx - 5, cy + 2, 11, 2, COLOR_TEXT)
    end
end

local function load_map()
    tiles = {}
    dots_left = 0

    for y = 1, MAP_H do
        tiles[y] = {}

        for x = 1, MAP_W do
            local ch = MAP_TEMPLATE[y]:sub(x, x)

            tiles[y][x] = ch

            if ch == "." or ch == "o" then
                dots_left = dots_left + 1
            end
        end
    end
end

local function clear_spawn_cells()
    local positions = {
        { x = 14, y = 14 },
        { x = 13, y = 10 },
        { x = 14, y = 10 },
        { x = 15, y = 10 },
        { x = 16, y = 10 },
    }

    for i = 1, #positions do
        local p = positions[i]
        local ch = tile_at(p.x, p.y)

        if ch == "." or ch == "o" then
            dots_left = dots_left - 1
        end

        set_tile(p.x, p.y, " ")
    end
end

local function reset_positions()
    player = {
        x = 14,
        y = 14,
        dir = "left",
        want_dir = "left",
    }

    ghosts = {
        { x = 13, y = 10, start_x = 13, start_y = 10, dir = "left", color = COLOR_RED },
        { x = 14, y = 10, start_x = 14, start_y = 10, dir = "right", color = COLOR_PINK },
        { x = 15, y = 10, start_x = 15, start_y = 10, dir = "up", color = COLOR_ORANGE },
        { x = 16, y = 10, start_x = 16, start_y = 10, dir = "down", color = COLOR_CYAN },
    }
end

local function reset_round_positions()
    power_ticks = 0
    reset_positions()
end

local function reset_game()
    paused = false
    game_over = false

    score = 0
    lives = 3
    wave = 1
    tick_count = 0
    tick_time = 0.14
    power_ticks = 0

    load_map()
    clear_spawn_cells()
    reset_positions()

    message = "EAT ALL DOTS"
end

local function next_wave()
    wave = wave + 1
    tick_count = 0
    power_ticks = 0
    tick_time = math.max(0.075, tick_time - 0.012)

    load_map()
    clear_spawn_cells()
    reset_positions()

    message = "WAVE " .. wave
end

local function eat_at_player()
    local ch = tile_at(player.x, player.y)

    if ch == "." then
        set_tile(player.x, player.y, " ")
        score = score + 10
        dots_left = dots_left - 1
    elseif ch == "o" then
        set_tile(player.x, player.y, " ")
        score = score + 50
        dots_left = dots_left - 1
        power_ticks = 70
        message = "GHOSTS FRIGHTENED"
    end

    if dots_left <= 0 then
        next_wave()
    end
end

local function move_player()
    if player.want_dir and can_move_from(player.x, player.y, player.want_dir) then
        player.dir = player.want_dir
    end

    if can_move_from(player.x, player.y, player.dir) then
        player.x, player.y = target_position(player.x, player.y, player.dir)
    end

    eat_at_player()
end

local function reset_ghost(ghost)
    ghost.x = ghost.start_x
    ghost.y = ghost.start_y
    ghost.dir = dir_names[math.random(1, #dir_names)]
end

local function choose_ghost_dir(ghost)
    local possible = {}
    local reverse = opposite_dir(ghost.dir)

    for i = 1, #dir_names do
        local dir_name = dir_names[i]

        if can_move_from(ghost.x, ghost.y, dir_name) then
            possible[#possible + 1] = dir_name
        end
    end

    if #possible == 0 then
        return ghost.dir
    end

    local filtered = {}

    for i = 1, #possible do
        if possible[i] ~= reverse then
            filtered[#filtered + 1] = possible[i]
        end
    end

    if #filtered > 0 then
        possible = filtered
    end

    if power_ticks > 0 or math.random(1, 5) == 1 then
        return possible[math.random(1, #possible)]
    end

    local best_dir = possible[1]
    local best_distance = 9999

    for i = 1, #possible do
        local dir_name = possible[i]
        local nx, ny = target_position(ghost.x, ghost.y, dir_name)
        local distance = distance_to_player(nx, ny)

        if distance < best_distance then
            best_distance = distance
            best_dir = dir_name
        end
    end

    return best_dir
end

local function move_ghosts()
    for i = 1, #ghosts do
        local ghost = ghosts[i]

        ghost.dir = choose_ghost_dir(ghost)

        if can_move_from(ghost.x, ghost.y, ghost.dir) then
            ghost.x, ghost.y = target_position(ghost.x, ghost.y, ghost.dir)
        end
    end
end

local function handle_collisions()
    for i = 1, #ghosts do
        local ghost = ghosts[i]

        if ghost.x == player.x and ghost.y == player.y then
            if power_ticks > 0 then
                score = score + 200
                reset_ghost(ghost)
                message = "GHOST EATEN"
            else
                lives = lives - 1
                message = "CAUGHT BY GHOST"

                if lives <= 0 then
                    game_over = true
                    paused = false
                    message = "GAME OVER"
                else
                    reset_round_positions()
                end

                return
            end
        end
    end
end

local function step_game()
    if paused or game_over then
        return
    end

    tick_count = tick_count + 1

    if power_ticks > 0 then
        power_ticks = power_ticks - 1
    end

    move_player()
    handle_collisions()

    if game_over then
        return
    end

    local ghost_delay = 2

    if wave >= 3 and power_ticks <= 0 then
        ghost_delay = 1
    elseif power_ticks > 0 then
        ghost_delay = 3
    end

    if tick_count % ghost_delay == 0 then
        move_ghosts()
        handle_collisions()
    end
end

local function draw_tile(x, y, ch)
    local px, py = cell_pixel(x, y)

    if ch == "#" then
        gfx.fill_rect(px + 2, py + 2, TILE - 4, TILE - 4, COLOR_WALL)
        gfx.rect(px + 3, py + 3, TILE - 6, TILE - 6, COLOR_WALL_EDGE)
    elseif ch == "." then
        gfx.fill_rect(px + 9, py + 9, 4, 4, COLOR_DOT)
    elseif ch == "o" then
        draw_disc(px + 11, py + 11, 5, COLOR_POWER)
    end
end

local function draw_game()
    gfx.clear(COLOR_BG)

    gfx.text(30, 24, "PAC-MAN", COLOR_YELLOW, 3)
    gfx.text(
        30,
        62,
        "SCORE " .. score .. "    LIVES " .. lives .. "    WAVE " .. wave,
        COLOR_TEXT,
        2
    )

    if game_over then
        gfx.text(30, 102, "GAME OVER - R OR ENTER TO RESTART - Q TO QUIT", COLOR_RED, 1)
    elseif paused then
        gfx.text(30, 102, "PAUSED - P TO RESUME - Q TO QUIT", COLOR_ORANGE, 1)
    elseif power_ticks > 0 then
        gfx.text(30, 102, "POWER " .. power_ticks .. " - GHOSTS FRIGHTENED", COLOR_FRIGHTENED, 1)
    else
        gfx.text(30, 102, "ARROWS/WASD MOVE - P PAUSE - Q QUIT", COLOR_MUTED, 1)
    end

    gfx.fill_rect(BOARD_X - 6, BOARD_Y - 6, BOARD_W + 12, BOARD_H + 12, COLOR_BORDER)
    gfx.fill_rect(BOARD_X - 2, BOARD_Y - 2, BOARD_W + 4, BOARD_H + 4, COLOR_BLACK)
    gfx.fill_rect(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, COLOR_BOARD)

    for y = 1, MAP_H do
        for x = 1, MAP_W do
            draw_tile(x, y, tile_at(x, y))
        end
    end

    for i = 1, #ghosts do
        draw_ghost(ghosts[i])
    end

    draw_pacman(player.x, player.y, player.dir)

    gfx.text(30, SCREEN_H - 28, message, COLOR_MUTED, 1)
end

local function set_direction(dir_name)
    if not paused and not game_over then
        player.want_dir = dir_name
    end
end

local function handle_key(key)
    if key == nil then
        return
    end

    if key == input.Q then
        running = false
        return
    end

    if key == input.P then
        if not game_over then
            paused = not paused
        end

        return
    end

    if game_over and (key == input.R or key == input.ENTER) then
        reset_game()
        return
    end

    if key == input.UP or key == input.W then
        set_direction("up")
    elseif key == input.DOWN or key == input.S then
        set_direction("down")
    elseif key == input.LEFT or key == input.A then
        set_direction("left")
    elseif key == input.RIGHT or key == input.D then
        set_direction("right")
    end
end

math.randomseed(os.time())
math.random()
math.random()
math.random()

reset_game()

local last_tick = now()

while running do
    local key = input.poll()
    handle_key(key)

    local current = now()

    if not paused and not game_over and current - last_tick >= tick_time then
        step_game()
        last_tick = current
    elseif paused or game_over then
        last_tick = current
    end

    draw_game()
    gfx.present()
end
