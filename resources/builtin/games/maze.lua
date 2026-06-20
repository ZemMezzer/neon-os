-- STONE MAZE 2: FAST PORTAL VERSION
-- Optimized version: the exit is rendered as a simple live portal
-- with blue sky, green ground and a couple of clouds.

local gfx = require("gfx")
local input = require("input")

local screen_w = gfx.width()
local screen_h = gfx.height()

local maze = {
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
    { 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1 },
    { 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1 },
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1 },
    { 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
    { 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1 },
    { 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
}

local maze_h = #maze
local maze_w = #maze[1]

local FOV = math.pi / 3
local HALF_FOV = FOV / 2
local COLUMN_WIDTH = 4
local MAX_RAY_STEPS = 48
local MOVE_STEP = 0.22
local TURN_STEP = math.pi / 16
local PLAYER_RADIUS = 0.18
local TWO_PI = math.pi * 2
local RAY_COUNT = math.floor((screen_w + COLUMN_WIDTH - 1) / COLUMN_WIDTH)

local SKY_COLOR = 0x00142638
local FLOOR_COLOR = 0x00120F16
local STONE_LIGHT = 0x008AA1AB
local STONE_MID = 0x00677983
local STONE_DARK = 0x0047545D
local MORTAR_COLOR = 0x00232C32
local HUD_COLOR = 0x00FFFFFF
local DIM_HUD_COLOR = 0x00B4C6D9
local PLAYER_COLOR = 0x00FFD45A
local MAP_EMPTY = 0x001C2633
local MAP_WALL = 0x006A7D92
local MAP_EXIT = 0x0059DA71

local PORTAL_SKY = 0x006DBDFF
local PORTAL_SKY_LOW = 0x009BD9FF
local PORTAL_GROUND = 0x004AAA4A
local PORTAL_GROUND_DARK = 0x00398939
local PORTAL_CLOUD = 0x00F4FBFF

local numeric_input = type(input.W) == "number"
local KEY_W = numeric_input and input.W or "w"
local KEY_S = numeric_input and input.S or "s"
local KEY_A = numeric_input and input.A or "a"
local KEY_D = numeric_input and input.D or "d"
local KEY_Q = numeric_input and input.Q or "q"
local KEY_R = numeric_input and input.R or "r"
local KEY_M = numeric_input and input.M or "m"
local KEY_UP = numeric_input and input.UP or "up"
local KEY_DOWN = numeric_input and input.DOWN or "down"
local KEY_LEFT = numeric_input and input.LEFT or "left"
local KEY_RIGHT = numeric_input and input.RIGHT or "right"

-- poll_realtime() asks the driver for new keys even when we do not
-- call gfx.present() every iteration.  poll_latest() alone only refreshes
-- after a presentation, which made the redraw-on-input loop stop on its
-- first frame.
local has_realtime_poll = type(input.poll_realtime) == "function"
local poll_game_input

if has_realtime_poll then
    poll_game_input = input.poll_realtime
else
    poll_game_input = input.poll_latest or input.poll
end

local player_x
local player_y
local player_angle
local won
local show_map
local needs_redraw
local ray_angle_offsets = {}

for i = 0, RAY_COUNT - 1 do
    local screen_x = i * COLUMN_WIDTH
    ray_angle_offsets[i + 1] = -HALF_FOV + ((screen_x + COLUMN_WIDTH / 2) / screen_w) * FOV
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    elseif value > maximum then
        return maximum
    end
    return value
end

local function map_at(x, y)
    if x < 0 or y < 0 or x >= maze_w or y >= maze_h then
        return 1
    end
    return maze[y + 1][x + 1]
end

local function shade_color(color, factor)
    factor = clamp(factor, 0, 1)
    local red = math.floor(color / 0x10000) % 0x100
    local green = math.floor(color / 0x100) % 0x100
    local blue = color % 0x100
    red = math.floor(red * factor)
    green = math.floor(green * factor)
    blue = math.floor(blue * factor)
    return red * 0x10000 + green * 0x100 + blue
end

local function wrap_angle(angle)
    while angle < 0 do
        angle = angle + TWO_PI
    end
    while angle >= TWO_PI do
        angle = angle - TWO_PI
    end
    return angle
end

local function is_walkable(x, y)
    local tile = map_at(math.floor(x), math.floor(y))
    return tile == 0 or tile == 2
end

local function can_stand_at(x, y)
    return is_walkable(x - PLAYER_RADIUS, y - PLAYER_RADIUS)
        and is_walkable(x + PLAYER_RADIUS, y - PLAYER_RADIUS)
        and is_walkable(x - PLAYER_RADIUS, y + PLAYER_RADIUS)
        and is_walkable(x + PLAYER_RADIUS, y + PLAYER_RADIUS)
end

local function check_exit()
    if map_at(math.floor(player_x), math.floor(player_y)) == 2 then
        won = true
    end
end

local function try_move(amount)
    local next_x = player_x + math.cos(player_angle) * amount
    local next_y = player_y + math.sin(player_angle) * amount

    if can_stand_at(next_x, player_y) then
        player_x = next_x
    end
    if can_stand_at(player_x, next_y) then
        player_y = next_y
    end

    check_exit()
end

local function cast_ray(ray_angle)
    local ray_x = math.cos(ray_angle)
    local ray_y = math.sin(ray_angle)
    local cell_x = math.floor(player_x)
    local cell_y = math.floor(player_y)
    local delta_x = math.abs(ray_x) < 0.000001 and 1000000 or math.abs(1 / ray_x)
    local delta_y = math.abs(ray_y) < 0.000001 and 1000000 or math.abs(1 / ray_y)
    local step_x
    local step_y
    local side_x
    local side_y
    local side = 0
    local tile = 0
    local distance
    local wall_u

    if ray_x < 0 then
        step_x = -1
        side_x = (player_x - cell_x) * delta_x
    else
        step_x = 1
        side_x = (cell_x + 1 - player_x) * delta_x
    end

    if ray_y < 0 then
        step_y = -1
        side_y = (player_y - cell_y) * delta_y
    else
        step_y = 1
        side_y = (cell_y + 1 - player_y) * delta_y
    end

    for _ = 1, MAX_RAY_STEPS do
        if side_x < side_y then
            side_x = side_x + delta_x
            cell_x = cell_x + step_x
            side = 0
        else
            side_y = side_y + delta_y
            cell_y = cell_y + step_y
            side = 1
        end

        tile = map_at(cell_x, cell_y)
        if tile ~= 0 then
            break
        end
    end

    if side == 0 then
        distance = side_x - delta_x
        wall_u = player_y + distance * ray_y
        if ray_x > 0 then
            wall_u = 1 - wall_u
        end
    else
        distance = side_y - delta_y
        wall_u = player_x + distance * ray_x
        if ray_y < 0 then
            wall_u = 1 - wall_u
        end
    end

    if distance < 0.001 then
        distance = 0.001
    end

    wall_u = wall_u - math.floor(wall_u)
    return distance, tile, side, wall_u
end

local function draw_stone_column(screen_x, clipped_top, visible_height, full_top, full_height, wall_u, brightness)
    if full_height < 18 then
        gfx.fill_rect(screen_x, clipped_top, COLUMN_WIDTH, visible_height, shade_color(STONE_MID, brightness))
        return
    end

    local clipped_bottom = clipped_top + visible_height - 1
    local brick_rows = 4
    local mortar_height = 1

    for course = 0, brick_rows - 1 do
        local course_top = math.floor(full_top + course * full_height / brick_rows)
        local course_bottom = math.floor(full_top + (course + 1) * full_height / brick_rows) - 1
        local draw_top = clamp(course_top, clipped_top, clipped_bottom)
        local draw_bottom = clamp(course_bottom, clipped_top, clipped_bottom)
        local shifted_u = (wall_u + (course % 2) * 0.2) * 3
        local inside_brick = shifted_u - math.floor(shifted_u)
        local variant = (math.floor(wall_u * 9) + course * 3) % 3
        local base_color = variant == 0 and STONE_LIGHT or (variant == 1 and STONE_MID or STONE_DARK)
        local color

        if inside_brick < 0.08 or inside_brick > 0.92 then
            color = shade_color(MORTAR_COLOR, brightness)
        else
            color = shade_color(base_color, brightness)
        end

        if draw_top <= draw_bottom then
            gfx.fill_rect(screen_x, draw_top, COLUMN_WIDTH, draw_bottom - draw_top + 1, color)
            if course > 0 and course_top >= clipped_top and course_top <= clipped_bottom then
                gfx.fill_rect(screen_x, course_top, COLUMN_WIDTH, mortar_height, shade_color(MORTAR_COLOR, brightness * 0.9))
            end
        end
    end
end

local function draw_portal_column(screen_x, clipped_top, visible_height, full_top, full_height, wall_u, brightness)
    -- The exit cell is intentionally drawn without a stone frame.
    -- The neighbouring maze cells remain the natural edges of the doorway,
    -- but the centre is fully open to the sky and meadow.
    local clipped_bottom = clipped_top + visible_height - 1
    local portal_horizon = full_top + math.floor(full_height * 0.50)
    local draw_horizon = clamp(portal_horizon, clipped_top, clipped_bottom + 1)

    -- Blue sky, with a lighter strip close to the horizon.
    if draw_horizon > clipped_top then
        local sky_mid = clipped_top + math.floor((draw_horizon - clipped_top) * 0.58)

        if sky_mid > clipped_top then
            gfx.fill_rect(
                screen_x,
                clipped_top,
                COLUMN_WIDTH,
                sky_mid - clipped_top,
                shade_color(PORTAL_SKY, brightness)
            )
        end

        if draw_horizon > sky_mid then
            gfx.fill_rect(
                screen_x,
                sky_mid,
                COLUMN_WIDTH,
                draw_horizon - sky_mid,
                shade_color(PORTAL_SKY_LOW, brightness)
            )
        end
    end

    -- Simple living ground beyond the exit.
    if clipped_bottom + 1 > draw_horizon then
        gfx.fill_rect(
            screen_x,
            draw_horizon,
            COLUMN_WIDTH,
            clipped_bottom - draw_horizon + 1,
            shade_color(PORTAL_GROUND, brightness)
        )

        local grass_band_y = draw_horizon
            + math.floor((clipped_bottom - draw_horizon) * 0.48)

        if grass_band_y >= draw_horizon and grass_band_y <= clipped_bottom then
            gfx.fill_rect(
                screen_x,
                grass_band_y,
                COLUMN_WIDTH,
                1,
                shade_color(PORTAL_GROUND_DARK, brightness * 0.95)
            )
        end
    end

    -- Two lightweight clouds. Their position is based on the horizontal
    -- point of the exit tile, so they stay inside the opening in perspective.
    if wall_u > 0.15 and wall_u < 0.31 then
        local cloud_y = full_top + math.floor(full_height * 0.20)
        local cloud_h = math.max(1, math.floor(full_height * 0.05))
        local draw_y = clamp(cloud_y, clipped_top, clipped_bottom)
        local max_h = clipped_bottom - draw_y + 1

        if max_h > 0 then
            gfx.fill_rect(
                screen_x,
                draw_y,
                COLUMN_WIDTH,
                math.min(cloud_h, max_h),
                shade_color(PORTAL_CLOUD, brightness)
            )
        end
    end

    if wall_u > 0.61 and wall_u < 0.83 then
        local cloud_y = full_top + math.floor(full_height * 0.28)
        local cloud_h = math.max(1, math.floor(full_height * 0.04))
        local draw_y = clamp(cloud_y, clipped_top, clipped_bottom)
        local max_h = clipped_bottom - draw_y + 1

        if max_h > 0 then
            gfx.fill_rect(
                screen_x,
                draw_y,
                COLUMN_WIDTH,
                math.min(cloud_h, max_h),
                shade_color(PORTAL_CLOUD, brightness * 0.96)
            )
        end
    end
end

local function draw_minimap()
    local cell_size = 7
    local origin_x = 12
    local origin_y = 42

    gfx.rect(origin_x - 2, origin_y - 2, maze_w * cell_size + 3, maze_h * cell_size + 3, DIM_HUD_COLOR)

    for row = 1, maze_h do
        for column = 1, maze_w do
            local tile = maze[row][column]
            local color = tile == 1 and MAP_WALL or (tile == 2 and MAP_EXIT or MAP_EMPTY)
            gfx.fill_rect(origin_x + (column - 1) * cell_size, origin_y + (row - 1) * cell_size, cell_size - 1, cell_size - 1, color)
        end
    end

    local marker_x = origin_x + math.floor(player_x * cell_size)
    local marker_y = origin_y + math.floor(player_y * cell_size)
    local look_x = marker_x + math.floor(math.cos(player_angle) * 9)
    local look_y = marker_y + math.floor(math.sin(player_angle) * 9)

    gfx.line(marker_x, marker_y, look_x, look_y, PLAYER_COLOR)
    gfx.fill_rect(marker_x - 2, marker_y - 2, 5, 5, PLAYER_COLOR)
end

local function draw_world()
    local horizon = math.floor(screen_h / 2)
    gfx.fill_rect(0, 0, screen_w, horizon, SKY_COLOR)
    gfx.fill_rect(0, horizon, screen_w, screen_h - horizon, FLOOR_COLOR)

    for i = 0, RAY_COUNT - 1 do
        local screen_x = i * COLUMN_WIDTH
        local offset = ray_angle_offsets[i + 1]
        local ray_angle = player_angle + offset
        local ray_distance, tile, side, wall_u = cast_ray(ray_angle)
        local corrected_distance = ray_distance * math.cos(offset)

        if corrected_distance < 0.05 then
            corrected_distance = 0.05
        end

        local wall_height = math.floor(screen_h / corrected_distance)
        local full_wall_top = math.floor(horizon - wall_height / 2)
        local wall_top = full_wall_top
        local visible_height = wall_height

        if wall_top < 0 then
            visible_height = visible_height + wall_top
            wall_top = 0
        end
        if wall_top + visible_height > screen_h then
            visible_height = screen_h - wall_top
        end

        local brightness = 1 / (1 + corrected_distance * 0.17)
        if side == 1 then
            brightness = brightness * 0.72
        end

        if visible_height > 0 then
            if tile == 2 then
                draw_portal_column(screen_x, wall_top, visible_height, full_wall_top, wall_height, wall_u, brightness)
            else
                draw_stone_column(screen_x, wall_top, visible_height, full_wall_top, wall_height, wall_u, brightness)
            end
        end
    end
end

local function draw_meadow_simple()
    local horizon = math.floor(screen_h * 0.47)
    gfx.fill_rect(0, 0, screen_w, horizon, PORTAL_SKY)
    gfx.fill_rect(0, horizon, screen_w, 40, PORTAL_SKY_LOW)
    gfx.fill_rect(0, horizon + 40, screen_w, screen_h - horizon - 40, PORTAL_GROUND)

    gfx.fill_rect(90, 70, 80, 16, PORTAL_CLOUD)
    gfx.fill_rect(108, 60, 38, 12, PORTAL_CLOUD)
    gfx.fill_rect(530, 92, 88, 16, PORTAL_CLOUD)
    gfx.fill_rect(555, 81, 42, 12, PORTAL_CLOUD)

    for y = horizon + 55, screen_h - 1, 28 do
        gfx.fill_rect(0, y, screen_w, 1, PORTAL_GROUND_DARK)
    end

    gfx.fill_rect(0, 0, screen_w, 32, 0x003173A9)
    gfx.text(16, 9, "THE MEADOW", HUD_COLOR, 2)
    gfx.text(18, screen_h - 26, "R: RETURN TO MAZE   Q: EXIT", HUD_COLOR, 1)
end

local function draw_hud()
    if won then
        return
    end

    local panel_width = show_map and 320 or 286
    local panel_x = math.floor((screen_w - panel_width) / 2)

    gfx.text(12, 12, "MAZE", HUD_COLOR, 2)
    gfx.text(12, 28, "FIND THE MEADOW GATE", DIM_HUD_COLOR, 1)

    if show_map then
        draw_minimap()
    end

    gfx.fill_rect(panel_x, screen_h - 28, panel_width, 18, 0x00000000)
    gfx.rect(panel_x, screen_h - 28, panel_width, 18, DIM_HUD_COLOR)
    gfx.text(panel_x + 8, screen_h - 24, "W/S MOVE  A/D TURN  M MAP  Q EXIT", HUD_COLOR, 1)
end

local function reset_game()
    player_x = 1.5
    player_y = 1.5
    player_angle = 0
    won = false
    needs_redraw = true
end

local function handle_key(key)
    if key == KEY_Q then
        return false
    end

    if key == KEY_R then
        reset_game()
        return true
    end

    if key == KEY_M then
        show_map = not show_map
        needs_redraw = true
        return true
    end

    if won then
        return true
    end

    if key == KEY_W or key == KEY_UP then
        try_move(MOVE_STEP)
        needs_redraw = true
    elseif key == KEY_S or key == KEY_DOWN then
        try_move(-MOVE_STEP)
        needs_redraw = true
    elseif key == KEY_A or key == KEY_LEFT then
        player_angle = wrap_angle(player_angle - TURN_STEP)
        needs_redraw = true
    elseif key == KEY_D or key == KEY_RIGHT then
        player_angle = wrap_angle(player_angle + TURN_STEP)
        needs_redraw = true
    end

    return true
end

show_map = true
reset_game()

local function present_current_scene()
    if won then
        draw_meadow_simple()
    else
        draw_world()
        draw_hud()
    end

    gfx.present()
    needs_redraw = false
end

local running = true
while running do
    -- New input API: redraw only after input/state changes.
    -- Old input API: present every pass so it can receive the next key.
    if has_realtime_poll then
        if needs_redraw then
            present_current_scene()
        end
    else
        present_current_scene()
    end

    local key = poll_game_input()
    if key ~= nil then
        local was_won = won
        running = handle_key(key)

        if won ~= was_won then
            needs_redraw = true
        end
    end
end
