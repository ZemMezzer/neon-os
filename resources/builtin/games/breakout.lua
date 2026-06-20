-- NeonOS Breakout
--
-- Controls:
--   Left/Right or A/D  move paddle
--   Space / Enter     launch ball / next level
--   P                 pause
--   R                 restart
--   Q                 quit
--
-- Requires:
--   local gfx = require("gfx")
--   local input = require("input")
--
-- Timing uses standard Lua os.clock(), so the generic ARM64 timer time.c
-- implementation must be enabled in NeonOS.

local gfx = require("gfx")
local input = require("input")

-- Games must ignore queued keyboard auto-repeat events.  Newer NeonOS input
-- builds expose poll_latest() for that purpose; older builds still work with
-- ordinary poll(), only without the repeat-backlog protection.
local poll_game_input = input.poll_latest or input.poll

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR_BG          = 0x000A101A
local COLOR_FIELD       = 0x00070E17
local COLOR_BORDER      = 0x00364F73
local COLOR_TEXT        = 0x00EDF5FF
local COLOR_MUTED       = 0x0093A9C4
local COLOR_YELLOW      = 0x00FFE45C
local COLOR_GREEN       = 0x005BE89C
local COLOR_CYAN        = 0x0067DDFD
local COLOR_BLUE        = 0x005F9CFF
local COLOR_PURPLE      = 0x00B888FF
local COLOR_ORANGE      = 0x00FFB75C
local COLOR_RED         = 0x00FF5C70
local COLOR_PINK        = 0x00FF83BD
local COLOR_HARD        = 0x00F9C74F
local COLOR_BALL        = 0x00FFFFFF
local COLOR_BLACK       = 0x00000000

local PHYSICS_STEP = 1 / 120
local DRAW_INTERVAL = 1 / 60
local MAX_STEPS_PER_FRAME = 8

local FIELD_X = 42
local FIELD_Y = 144
local FIELD_W = SCREEN_W - 84
local FIELD_H = SCREEN_H - FIELD_Y - 42

if FIELD_H < 260 then
    FIELD_Y = 116
    FIELD_H = SCREEN_H - FIELD_Y - 28
end

local running = true
local paused = false
local game_over = false
local won = false
local ball_launched = false

local score = 0
local lives = 3
local level = 1
local message = "PRESS SPACE"

local paddle = {}
local ball = {}
local bricks = {}
local bricks_left = 0

local last_time = 0
local last_draw = 0
local accumulator = 0
local dirty = true

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

local function sign(value)
    if value < 0 then
        return -1
    end

    return 1
end

local function draw_disc(cx, cy, radius, color)
    cx = math.floor(cx)
    cy = math.floor(cy)

    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))
        gfx.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)
    end
end

local function brick_color_for_row(row)
    local colors = {
        COLOR_RED,
        COLOR_ORANGE,
        COLOR_YELLOW,
        COLOR_GREEN,
        COLOR_CYAN,
        COLOR_BLUE,
        COLOR_PURPLE,
        COLOR_PINK,
    }

    return colors[((row - 1) % #colors) + 1]
end

local function reset_ball_on_paddle()
    ball_launched = false
    ball.r = math.max(6, math.floor(FIELD_W / 140))
    ball.x = paddle.x + paddle.w / 2
    ball.y = paddle.y - ball.r - 1
    ball.speed = 355 + (level - 1) * 20
    ball.vx = 0
    ball.vy = -ball.speed
    message = "PRESS SPACE"
end

local function build_bricks()
    bricks = {}
    bricks_left = 0

    local margin = 24
    local gap = 5
    local rows = math.min(8, 5 + math.floor((level - 1) / 2))
    local columns = math.floor((FIELD_W - margin * 2 + gap) / 62)

    columns = clamp(columns, 8, 14)

    local brick_w = math.floor((FIELD_W - margin * 2 - (columns - 1) * gap) / columns)
    local brick_h = 20
    local start_x = FIELD_X + margin
    local start_y = FIELD_Y + 28

    for row = 1, rows do
        for col = 1, columns do
            local hp = 1

            if row == 1 then
                hp = 2
            elseif level >= 3 and (row + col + level) % 6 == 0 then
                hp = 2
            end

            local brick = {
                x = start_x + (col - 1) * (brick_w + gap),
                y = start_y + (row - 1) * (brick_h + gap),
                w = brick_w,
                h = brick_h,
                hp = hp,
                color = brick_color_for_row(row),
                alive = true,
            }

            bricks[#bricks + 1] = brick
            bricks_left = bricks_left + 1
        end
    end
end

local function reset_level()
    local paddle_w = math.max(90, math.floor(FIELD_W * 0.18) - (level - 1) * 7)
    local paddle_h = 16

    paddle = {
        x = math.floor(FIELD_X + (FIELD_W - paddle_w) / 2),
        y = FIELD_Y + FIELD_H - 30,
        w = paddle_w,
        h = paddle_h,
    }

    build_bricks()
    reset_ball_on_paddle()
    paused = false
    game_over = false
    won = false
    dirty = true
end

local function reset_game()
    score = 0
    lives = 3
    level = 1
    message = "PRESS SPACE"
    reset_level()
end

local function next_level()
    level = level + 1
    message = "LEVEL " .. level
    reset_level()
end

local function move_paddle(direction)
    if paused or game_over then
        return
    end

    local amount = math.max(15, math.floor(FIELD_W * 0.028))
    paddle.x = clamp(
        paddle.x + direction * amount,
        FIELD_X + 4,
        FIELD_X + FIELD_W - paddle.w - 4
    )

    if not ball_launched then
        ball.x = paddle.x + paddle.w / 2
        ball.y = paddle.y - ball.r - 1
    end

    dirty = true
end

local function launch_ball()
    if paused then
        return
    end

    if game_over then
        if won then
            next_level()
        else
            reset_game()
        end

        return
    end

    if not ball_launched then
        ball_launched = true

        local direction = math.random(0, 1) == 0 and -1 or 1
        ball.speed = 355 + (level - 1) * 20
        ball.vx = direction * ball.speed * 0.34
        ball.vy = -math.sqrt(ball.speed * ball.speed - ball.vx * ball.vx)
        message = "GO"
        dirty = true
    end
end

local function lose_ball()
    lives = lives - 1

    if lives <= 0 then
        game_over = true
        won = false
        paused = false
        message = "GAME OVER"
    else
        reset_ball_on_paddle()
        message = "BALL LOST"
    end

    dirty = true
end

local function destroy_brick(brick)
    brick.hp = brick.hp - 1

    if brick.hp <= 0 then
        brick.alive = false
        bricks_left = bricks_left - 1
        score = score + 50
        message = "BRICK DESTROYED"
    else
        score = score + 10
        message = "HARD BRICK HIT"
    end

    if bricks_left <= 0 then
        game_over = true
        won = true
        paused = false
        ball_launched = false
        message = "LEVEL CLEAR"
    end
end

local function circle_intersects_rect(cx, cy, radius, rect)
    local closest_x = clamp(cx, rect.x, rect.x + rect.w)
    local closest_y = clamp(cy, rect.y, rect.y + rect.h)
    local dx = cx - closest_x
    local dy = cy - closest_y

    return dx * dx + dy * dy <= radius * radius
end

local function resolve_brick_collision(old_x, old_y)
    for i = 1, #bricks do
        local brick = bricks[i]

        if brick.alive and circle_intersects_rect(ball.x, ball.y, ball.r, brick) then
            local hit_left = old_x + ball.r <= brick.x
            local hit_right = old_x - ball.r >= brick.x + brick.w
            local hit_top = old_y + ball.r <= brick.y
            local hit_bottom = old_y - ball.r >= brick.y + brick.h

            destroy_brick(brick)

            if hit_left then
                ball.x = brick.x - ball.r - 1
                ball.vx = -math.abs(ball.vx)
            elseif hit_right then
                ball.x = brick.x + brick.w + ball.r + 1
                ball.vx = math.abs(ball.vx)
            elseif hit_top then
                ball.y = brick.y - ball.r - 1
                ball.vy = -math.abs(ball.vy)
            elseif hit_bottom then
                ball.y = brick.y + brick.h + ball.r + 1
                ball.vy = math.abs(ball.vy)
            else
                -- A corner/ambiguous overlap: reflect on the shallowest axis.
                local overlap_left = ball.x + ball.r - brick.x
                local overlap_right = brick.x + brick.w - (ball.x - ball.r)
                local overlap_top = ball.y + ball.r - brick.y
                local overlap_bottom = brick.y + brick.h - (ball.y - ball.r)

                local horizontal = math.min(overlap_left, overlap_right)
                local vertical = math.min(overlap_top, overlap_bottom)

                if horizontal < vertical then
                    ball.vx = -ball.vx
                else
                    ball.vy = -ball.vy
                end
            end

            return true
        end
    end

    return false
end

local function bounce_from_paddle()
    local center = paddle.x + paddle.w / 2
    local relative = (ball.x - center) / math.max(1, paddle.w / 2)

    relative = clamp(relative, -0.92, 0.92)

    ball.speed = math.min(ball.speed + 7, 590)
    ball.vx = ball.speed * relative
    ball.vy = -math.sqrt(math.max(1, ball.speed * ball.speed - ball.vx * ball.vx))

    if math.abs(ball.vx) < ball.speed * 0.16 then
        ball.vx = ball.speed * 0.16 * sign(relative == 0 and (math.random(0, 1) == 0 and -1 or 1) or relative)
        ball.vy = -math.sqrt(math.max(1, ball.speed * ball.speed - ball.vx * ball.vx))
    end

    score = score + 1
    message = "NICE HIT"
end

local function update_ball(dt)
    if not ball_launched or game_over or paused then
        return
    end

    local old_x = ball.x
    local old_y = ball.y

    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Left / right walls.
    if ball.x - ball.r <= FIELD_X then
        ball.x = FIELD_X + ball.r
        ball.vx = math.abs(ball.vx)
    elseif ball.x + ball.r >= FIELD_X + FIELD_W then
        ball.x = FIELD_X + FIELD_W - ball.r
        ball.vx = -math.abs(ball.vx)
    end

    -- Ceiling.
    if ball.y - ball.r <= FIELD_Y then
        ball.y = FIELD_Y + ball.r
        ball.vy = math.abs(ball.vy)
    end

    -- Floor.
    if ball.y - ball.r > FIELD_Y + FIELD_H then
        lose_ball()
        return
    end

    -- Paddle only catches a descending ball.
    if ball.vy > 0 and
       old_y + ball.r <= paddle.y and
       ball.y + ball.r >= paddle.y and
       ball.x + ball.r >= paddle.x and
       ball.x - ball.r <= paddle.x + paddle.w then
        ball.y = paddle.y - ball.r - 1
        bounce_from_paddle()
        return
    end

    resolve_brick_collision(old_x, old_y)
end

local function draw_brick(brick)
    if not brick.alive then
        return
    end

    local fill = brick.color
    local border = brick.hp > 1 and COLOR_HARD or COLOR_BLACK
    local inset = 2

    gfx.fill_rect(brick.x, brick.y, brick.w, brick.h, fill)
    gfx.rect(brick.x, brick.y, brick.w, brick.h, border)
    gfx.fill_rect(brick.x + inset, brick.y + inset, brick.w - inset * 2, 2, COLOR_TEXT)

    if brick.hp > 1 then
        local cx = brick.x + math.floor(brick.w / 2)
        gfx.fill_rect(cx - 2, brick.y + 6, 4, brick.h - 12, COLOR_HARD)
    end
end

local function draw_paddle()
    gfx.fill_rect(paddle.x, paddle.y, paddle.w, paddle.h, COLOR_GREEN)
    gfx.rect(paddle.x, paddle.y, paddle.w, paddle.h, COLOR_TEXT)
    gfx.fill_rect(
        paddle.x + math.floor(paddle.w * 0.25),
        paddle.y - 3,
        math.floor(paddle.w * 0.50),
        3,
        COLOR_CYAN
    )
end

local function draw_ball()
    draw_disc(ball.x, ball.y, ball.r, COLOR_BALL)
    draw_disc(ball.x - math.max(1, math.floor(ball.r / 3)), ball.y - math.max(1, math.floor(ball.r / 3)), math.max(1, math.floor(ball.r / 4)), COLOR_YELLOW)
end

local function draw_overlay(text, color)
    local box_w = 360
    local box_h = 70
    local box_x = math.floor((SCREEN_W - box_w) / 2)
    local box_y = math.floor(FIELD_Y + FIELD_H / 2 - box_h / 2)

    gfx.fill_rect(box_x, box_y, box_w, box_h, COLOR_BLACK)
    gfx.rect(box_x, box_y, box_w, box_h, color)
    gfx.text(box_x + 58, box_y + 24, text, color, 2)
end

local function draw_game()
    gfx.clear(COLOR_BG)

    gfx.text(34, 26, "BREAKOUT", COLOR_YELLOW, 3)
    gfx.text(
        278,
        40,
        "SCORE " .. score .. "   LIVES " .. lives .. "   LEVEL " .. level,
        COLOR_TEXT,
        2
    )

    if game_over then
        local color = won and COLOR_GREEN or COLOR_RED
        local extra = won and "SPACE NEXT LEVEL" or "R / ENTER RESTART"
        gfx.text(34, 92, extra .. "     Q QUIT", color, 1)
    elseif paused then
        gfx.text(34, 92, "PAUSED - P RESUME     Q QUIT", COLOR_ORANGE, 1)
    elseif not ball_launched then
        gfx.text(34, 92, "A/D OR ARROWS MOVE     SPACE LAUNCH     P PAUSE     Q QUIT", COLOR_MUTED, 1)
    else
        gfx.text(34, 92, "A/D OR ARROWS MOVE     P PAUSE     Q QUIT", COLOR_MUTED, 1)
    end

    gfx.fill_rect(FIELD_X - 6, FIELD_Y - 6, FIELD_W + 12, FIELD_H + 12, COLOR_BORDER)
    gfx.fill_rect(FIELD_X - 2, FIELD_Y - 2, FIELD_W + 4, FIELD_H + 4, COLOR_BLACK)
    gfx.fill_rect(FIELD_X, FIELD_Y, FIELD_W, FIELD_H, COLOR_FIELD)

    for i = 1, #bricks do
        draw_brick(bricks[i])
    end

    draw_paddle()
    draw_ball()

    gfx.text(34, SCREEN_H - 28, message, COLOR_MUTED, 1)

    if game_over then
        draw_overlay(won and "LEVEL CLEAR" or "GAME OVER", won and COLOR_GREEN or COLOR_RED)
    elseif paused then
        draw_overlay("PAUSED", COLOR_ORANGE)
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
            last_time = now()
            accumulator = 0
            dirty = true
        end
        return
    end

    if key == input.R then
        reset_game()
        return
    end

    if key == input.LEFT or key == input.A then
        move_paddle(-1)
    elseif key == input.RIGHT or key == input.D then
        move_paddle(1)
    elseif key == input.SPACE or key == input.ENTER then
        launch_ball()
    end
end

math.randomseed(math.floor(now() * 1000000) + 481)
math.random()
math.random()
math.random()

reset_game()
last_time = now()
last_draw = last_time

while running do
    -- poll_latest() drains queued auto-repeat input, so releasing Left/Right
    -- stops the paddle immediately instead of replaying stale key presses.
    handle_key(poll_game_input())

    local current = now()
    local elapsed = current - last_time
    last_time = current

    if elapsed < 0 then
        elapsed = 0
    elseif elapsed > 0.10 then
        elapsed = 0.10
    end

    if paused or game_over then
        accumulator = 0
    else
        accumulator = accumulator + elapsed

        local steps = 0
        while accumulator >= PHYSICS_STEP and steps < MAX_STEPS_PER_FRAME do
            update_ball(PHYSICS_STEP)
            accumulator = accumulator - PHYSICS_STEP
            steps = steps + 1
        end

        if steps == MAX_STEPS_PER_FRAME then
            accumulator = 0
        end
    end

    if dirty or current - last_draw >= DRAW_INTERVAL then
        draw_game()
        gfx.present()
        last_draw = current
        dirty = false
    end
end

gfx.clear(COLOR_BLACK)
gfx.present()
