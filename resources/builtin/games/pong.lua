-- NeonOS Pong
--
-- Requires:
--   local gfx = require("gfx")
--   local input = require("input")
--
-- Controls:
--   W/S or Up/Down  move paddle
--   P or Space      pause
--   R or Enter      restart after game over
--   Q               quit
--
-- The game uses standard Lua os.clock(), so it needs the ARM64 generic-timer
-- time.c fix already added to NeonOS.

local gfx = require("gfx")
local input = require("input")

local WIN_SCORE = 5
local FIXED_STEP = 1 / 120
local MAX_FRAME_TIME = 0.10

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR_BG         = 0x000B101A
local COLOR_FIELD       = 0x00050C15
local COLOR_BORDER      = 0x002B4C74
local COLOR_NET         = 0x00384B65
local COLOR_TEXT        = 0x00EFF5FF
local COLOR_MUTED       = 0x0095A9C2
local COLOR_PLAYER      = 0x005CF2A8
local COLOR_CPU         = 0x00FF6375
local COLOR_BALL        = 0x00FFF4BB
local COLOR_YELLOW      = 0x00FFE65C
local COLOR_ORANGE      = 0x00FFB65C
local COLOR_RED         = 0x00FF5C70
local COLOR_BLACK       = 0x00000000

local running = true
local paused = false
local game_over = false

local player_score = 0
local cpu_score = 0
local message = "FIRST TO " .. WIN_SCORE

local board = {}
local player = {}
local cpu = {}
local ball = {}

local accumulator = 0
local last_time = 0

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
    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))
        gfx.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)
    end
end

local function setup_layout()
    board.x = 46
    board.y = 138
    board.w = SCREEN_W - 92
    board.h = SCREEN_H - board.y - 54

    if board.h < 260 then
        board.y = 112
        board.h = SCREEN_H - board.y - 32
    end

    player.w = 14
    player.h = math.min(108, math.max(72, math.floor(board.h * 0.24)))
    player.x = board.x + 30

    cpu.w = player.w
    cpu.h = player.h
    cpu.x = board.x + board.w - 30 - cpu.w

    ball.r = 8
end

local function reset_ball(direction)
    ball.x = board.x + math.floor(board.w / 2)
    ball.y = board.y + math.floor(board.h / 2)

    ball.speed = 320
    ball.vx = direction * ball.speed

    local choices = { -0.50, -0.34, -0.20, 0.20, 0.34, 0.50 }
    ball.vy = choices[math.random(1, #choices)] * ball.speed
end

local function reset_positions()
    player.y = board.y + math.floor((board.h - player.h) / 2)
    cpu.y = player.y
end

local function reset_game()
    setup_layout()

    player_score = 0
    cpu_score = 0
    paused = false
    game_over = false
    message = "FIRST TO " .. WIN_SCORE

    reset_positions()
    reset_ball(math.random(0, 1) == 0 and -1 or 1)

    accumulator = 0
end

local function move_player(direction)
    if paused or game_over then
        return
    end

    local amount = 22
    player.y = clamp(
        player.y + direction * amount,
        board.y,
        board.y + board.h - player.h
    )
end

local function update_cpu(dt)
    local paddle_center = cpu.y + cpu.h / 2
    local target = board.y + board.h / 2

    -- CPU primarily reacts when the ball travels toward it.
    if ball.vx > 0 then
        target = ball.y
    end

    -- It intentionally trails a little, otherwise it would be unbeatable.
    target = target - cpu.h * 0.42

    local speed = 225

    if cpu_score > player_score then
        speed = 250
    end

    if cpu.y < target - 4 then
        cpu.y = cpu.y + speed * dt
    elseif cpu.y > target + 4 then
        cpu.y = cpu.y - speed * dt
    end

    cpu.y = clamp(cpu.y, board.y, board.y + board.h - cpu.h)
end

local function ball_hits_paddle(paddle)
    return (
        ball.x + ball.r >= paddle.x and
        ball.x - ball.r <= paddle.x + paddle.w and
        ball.y + ball.r >= paddle.y and
        ball.y - ball.r <= paddle.y + paddle.h
    )
end

local function bounce_from_paddle(paddle, direction)
    local paddle_center = paddle.y + paddle.h / 2
    local relative = (ball.y - paddle_center) / (paddle.h / 2)

    relative = clamp(relative, -1, 1)

    ball.speed = math.min(ball.speed + 18, 560)
    ball.vx = direction * ball.speed
    ball.vy = relative * ball.speed * 0.86

    -- Keep shallow hits from creating a perfectly horizontal endless rally.
    if math.abs(ball.vy) < 76 then
        ball.vy = 76 * sign(relative == 0 and (math.random(0, 1) == 0 and -1 or 1) or relative)
    end
end

local function add_point(player_scored)
    if player_scored then
        player_score = player_score + 1
        message = "YOU SCORED"
        reset_ball(-1)
    else
        cpu_score = cpu_score + 1
        message = "CPU SCORED"
        reset_ball(1)
    end

    if player_score >= WIN_SCORE then
        game_over = true
        paused = false
        message = "YOU WIN"
    elseif cpu_score >= WIN_SCORE then
        game_over = true
        paused = false
        message = "CPU WINS"
    end
end

local function step_game(dt)
    if paused or game_over then
        return
    end

    update_cpu(dt)

    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    if ball.y - ball.r <= board.y then
        ball.y = board.y + ball.r
        ball.vy = math.abs(ball.vy)
    elseif ball.y + ball.r >= board.y + board.h then
        ball.y = board.y + board.h - ball.r
        ball.vy = -math.abs(ball.vy)
    end

    if ball.vx < 0 and ball_hits_paddle(player) then
        ball.x = player.x + player.w + ball.r
        bounce_from_paddle(player, 1)
    elseif ball.vx > 0 and ball_hits_paddle(cpu) then
        ball.x = cpu.x - ball.r
        bounce_from_paddle(cpu, -1)
    end

    if ball.x + ball.r < board.x then
        add_point(false)
    elseif ball.x - ball.r > board.x + board.w then
        add_point(true)
    end
end

local function draw_center_net()
    local x = board.x + math.floor(board.w / 2)

    for y = board.y + 8, board.y + board.h - 8, 24 do
        gfx.fill_rect(x - 2, y, 4, 13, COLOR_NET)
    end
end

local function draw_paddle(paddle, color)
    -- CPU movement uses fractional pixels for smooth motion, but gfx
    -- primitives accept integer coordinates only.
    local x = math.floor(paddle.x)
    local y = math.floor(paddle.y)
    local w = math.floor(paddle.w)
    local h = math.floor(paddle.h)

    gfx.fill_rect(x, y, w, h, color)
    gfx.rect(x, y, w, h, COLOR_TEXT)
end

local function draw_overlay(text, color)
    local w = 300
    local h = 56
    local x = math.floor(SCREEN_W / 2 - w / 2)
    local y = math.floor(board.y + board.h / 2 - h / 2)

    gfx.fill_rect(x, y, w, h, COLOR_BLACK)
    gfx.rect(x, y, w, h, color)
    gfx.text(x + 34, y + 18, text, color, 2)
end

local function draw_game()
    gfx.clear(COLOR_BG)

    gfx.text(38, 28, "PONG", COLOR_YELLOW, 3)
    gfx.text(
        285,
        42,
        "YOU " .. player_score .. "  :  " .. cpu_score .. " CPU",
        COLOR_TEXT,
        2
    )

    if game_over then
        gfx.text(38, 92, "R / ENTER RESTART     Q QUIT", COLOR_RED, 1)
    elseif paused then
        gfx.text(38, 92, "PAUSED - P / SPACE RESUME     Q QUIT", COLOR_ORANGE, 1)
    else
        gfx.text(38, 92, "W/S OR ARROWS MOVE     P / SPACE PAUSE     Q QUIT", COLOR_MUTED, 1)
    end

    gfx.fill_rect(board.x - 5, board.y - 5, board.w + 10, board.h + 10, COLOR_BORDER)
    gfx.fill_rect(board.x, board.y, board.w, board.h, COLOR_FIELD)
    gfx.rect(board.x, board.y, board.w, board.h, COLOR_TEXT)

    draw_center_net()
    draw_paddle(player, COLOR_PLAYER)
    draw_paddle(cpu, COLOR_CPU)
    draw_disc(math.floor(ball.x), math.floor(ball.y), ball.r, COLOR_BALL)

    gfx.text(38, SCREEN_H - 28, message, COLOR_MUTED, 1)

    if game_over then
        draw_overlay(message, message == "YOU WIN" and COLOR_PLAYER or COLOR_CPU)
    elseif paused then
        draw_overlay("PAUSED", COLOR_ORANGE)
    end
end

local function toggle_pause()
    if not game_over then
        paused = not paused
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

    if key == input.P or key == input.SPACE then
        toggle_pause()
        return
    end

    if game_over and (key == input.R or key == input.ENTER) then
        reset_game()
        return
    end

    if key == input.UP or key == input.W then
        move_player(-1)
    elseif key == input.DOWN or key == input.S then
        move_player(1)
    end
end

math.randomseed(math.floor(now() * 1000000) + 41)
math.random()
math.random()
math.random()

reset_game()
last_time = now()

while running do
    -- Exactly one keyboard read per visual frame: compatible with the
    -- stable NeonOS input.poll() API.
    handle_key(input.poll())

    local current = now()
    local frame_time = current - last_time
    last_time = current

    if frame_time < 0 then
        frame_time = 0
    elseif frame_time > MAX_FRAME_TIME then
        frame_time = MAX_FRAME_TIME
    end

    if paused or game_over then
        accumulator = 0
    else
        accumulator = accumulator + frame_time

        local step_count = 0

        while accumulator >= FIXED_STEP and step_count < 10 do
            step_game(FIXED_STEP)
            accumulator = accumulator - FIXED_STEP
            step_count = step_count + 1
        end
    end

    draw_game()
    gfx.present()
end
