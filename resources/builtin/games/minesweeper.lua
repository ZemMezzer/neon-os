-- NeonOS Minesweeper
--
-- Controls:
--   Menu: Up/Down or W/S, Enter/Space start, Q quit
--   Game: Arrows/WASD move, Enter/Space open, F flag,
--         R restart, M menu, Q quit
--
-- Requires:
--   local gfx = require("gfx")
--   local input = require("input")

local gfx = require("gfx")
local input = require("input")

local SCREEN_W = gfx.width()
local SCREEN_H = gfx.height()

local COLOR_BG          = 0x000A101A
local COLOR_PANEL       = 0x000F1B2B
local COLOR_BOARD       = 0x00060D16
local COLOR_BORDER      = 0x00344F73
local COLOR_TEXT        = 0x00EDF5FF
local COLOR_MUTED       = 0x0093A9C4
local COLOR_YELLOW      = 0x00FFE45C
local COLOR_GREEN       = 0x005BE89C
local COLOR_RED         = 0x00FF5C70
local COLOR_ORANGE      = 0x00FFB75C
local COLOR_CYAN        = 0x0067DDFD
local COLOR_BLUE        = 0x0067A4FF
local COLOR_PURPLE      = 0x00BD8CFF
local COLOR_WHITE       = 0x00FFFFFF
local COLOR_BLACK       = 0x00000000
local COLOR_CELL_CLOSED = 0x00263C59
local COLOR_CELL_EDGE   = 0x004B6B91
local COLOR_CELL_OPEN   = 0x00111E2E
local COLOR_MINE        = 0x00D63E51
local COLOR_FLAG         = 0x00FFD45D

local difficulties = {
    { name = "EASY",   description = "9 X 9 / 10 mines",  width = 9,  height = 9,  mines = 10 },
    { name = "MEDIUM", description = "16 X 12 / 30 mines", width = 16, height = 12, mines = 30 },
    { name = "HARD",   description = "24 X 15 / 70 mines", width = 24, height = 15, mines = 70 },
}

local number_colors = {
    [1] = COLOR_BLUE,
    [2] = COLOR_GREEN,
    [3] = COLOR_RED,
    [4] = COLOR_PURPLE,
    [5] = COLOR_ORANGE,
    [6] = COLOR_CYAN,
    [7] = COLOR_WHITE,
    [8] = COLOR_MUTED,
}

local running = true
local screen = "MENU"

local selected_difficulty = 1
local board_w = 9
local board_h = 9
local mine_count = 10
local flags_left = 10

local cell_size = 20
local board_x = 0
local board_y = 0
local board_px_w = 0
local board_px_h = 0

local grid = {}
local cursor_x = 1
local cursor_y = 1
local first_move = true
local game_over = false
local won = false
local revealed_count = 0
local message = "SELECT DIFFICULTY"

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

local function now()
    local ok, value = pcall(os.clock)

    if ok and type(value) == "number" then
        return value
    end

    return 0
end

local function configure_layout()
    local usable_w = SCREEN_W - 100
    local usable_h = SCREEN_H - 210
    local from_width = math.floor(usable_w / board_w)
    local from_height = math.floor(usable_h / board_h)

    cell_size = math.min(from_width, from_height)
    cell_size = clamp(cell_size, 10, 30)

    board_px_w = board_w * cell_size
    board_px_h = board_h * cell_size
    board_x = math.floor((SCREEN_W - board_px_w) / 2)
    board_y = 152

    if board_y + board_px_h > SCREEN_H - 34 then
        board_y = SCREEN_H - board_px_h - 34
    end

    if board_y < 130 then
        board_y = 130
    end
end

local function cell_key(x, y)
    return tostring(x) .. ":" .. tostring(y)
end

local function inside(x, y)
    return x >= 1 and x <= board_w and y >= 1 and y <= board_h
end

local function grid_cell(x, y)
    if not inside(x, y) then
        return nil
    end

    return grid[y][x]
end

local function for_each_neighbor(x, y, callback)
    for dy = -1, 1 do
        for dx = -1, 1 do
            if dx ~= 0 or dy ~= 0 then
                local nx = x + dx
                local ny = y + dy

                if inside(nx, ny) then
                    callback(nx, ny)
                end
            end
        end
    end
end

local function make_empty_grid()
    grid = {}

    for y = 1, board_h do
        grid[y] = {}

        for x = 1, board_w do
            grid[y][x] = {
                mine = false,
                revealed = false,
                flag = false,
                nearby = 0,
            }
        end
    end
end

local function protected_start(x, y, safe_x, safe_y)
    return math.abs(x - safe_x) <= 1 and math.abs(y - safe_y) <= 1
end

local function count_available_cells(safe_x, safe_y)
    local count = 0

    for y = 1, board_h do
        for x = 1, board_w do
            if not protected_start(x, y, safe_x, safe_y) then
                count = count + 1
            end
        end
    end

    return count
end

local function place_mines(safe_x, safe_y)
    local available = count_available_cells(safe_x, safe_y)
    local target = math.min(mine_count, available)
    local placed = 0

    while placed < target do
        local x = math.random(1, board_w)
        local y = math.random(1, board_h)
        local cell = grid[y][x]

        if not cell.mine and not protected_start(x, y, safe_x, safe_y) then
            cell.mine = true
            placed = placed + 1
        end
    end

    for y = 1, board_h do
        for x = 1, board_w do
            local count = 0

            for_each_neighbor(x, y, function(nx, ny)
                if grid[ny][nx].mine then
                    count = count + 1
                end
            end)

            grid[y][x].nearby = count
        end
    end

    mine_count = target
    flags_left = target
end

local function choose_difficulty(index)
    local diff = difficulties[index]

    selected_difficulty = index
    board_w = diff.width
    board_h = diff.height
    mine_count = diff.mines
    flags_left = mine_count
    configure_layout()
end

local function reset_game()
    choose_difficulty(selected_difficulty)

    first_move = true
    game_over = false
    won = false
    revealed_count = 0
    flags_left = mine_count
    cursor_x = math.floor((board_w + 1) / 2)
    cursor_y = math.floor((board_h + 1) / 2)
    message = "CHOOSE A CELL"

    make_empty_grid()
end

local function reveal_all_mines()
    for y = 1, board_h do
        for x = 1, board_w do
            if grid[y][x].mine then
                grid[y][x].revealed = true
            end
        end
    end
end

local function check_win()
    local safe_cells = board_w * board_h - mine_count

    if revealed_count >= safe_cells then
        won = true
        game_over = true
        message = "YOU WIN"
    end
end

local function reveal_cell(x, y)
    if game_over or not inside(x, y) then
        return
    end

    local first_cell = grid[y][x]

    if first_cell.revealed or first_cell.flag then
        return
    end

    if first_move then
        first_move = false
        place_mines(x, y)
    end

    if first_cell.mine then
        first_cell.revealed = true
        reveal_all_mines()
        game_over = true
        won = false
        message = "BOOM"
        return
    end

    local queue = {
        { x = x, y = y },
    }
    local queue_head = 1

    while queue_head <= #queue do
        local item = queue[queue_head]
        queue_head = queue_head + 1

        local cell = grid[item.y][item.x]

        if not cell.revealed and not cell.flag and not cell.mine then
            cell.revealed = true
            revealed_count = revealed_count + 1

            if cell.nearby == 0 then
                for_each_neighbor(item.x, item.y, function(nx, ny)
                    local neighbor = grid[ny][nx]

                    if not neighbor.revealed and not neighbor.flag and not neighbor.mine then
                        queue[#queue + 1] = { x = nx, y = ny }
                    end
                end)
            end
        end
    end

    message = "SAFE"
    check_win()
end

local function toggle_flag(x, y)
    if game_over or not inside(x, y) then
        return
    end

    local cell = grid[y][x]

    if cell.revealed then
        return
    end

    if cell.flag then
        cell.flag = false
        flags_left = flags_left + 1
        message = "FLAG REMOVED"
    elseif flags_left > 0 then
        cell.flag = true
        flags_left = flags_left - 1
        message = "FLAG PLACED"
    else
        message = "NO FLAGS LEFT"
    end
end

local function move_cursor(dx, dy)
    cursor_x = clamp(cursor_x + dx, 1, board_w)
    cursor_y = clamp(cursor_y + dy, 1, board_h)
end

local function cell_pixel(x, y)
    return board_x + (x - 1) * cell_size, board_y + (y - 1) * cell_size
end

local function draw_disc(cx, cy, radius, color)
    for dy = -radius, radius do
        local dx = math.floor(math.sqrt(radius * radius - dy * dy))
        gfx.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)
    end
end

local function draw_text_center(y, text, color, scale)
    local estimated_width = #text * 8 * scale
    local x = math.floor((SCREEN_W - estimated_width) / 2)

    gfx.text(x, y, text, color, scale)
end

local function draw_cell(x, y)
    local px, py = cell_pixel(x, y)
    local cell = grid[y][x]
    local is_cursor = x == cursor_x and y == cursor_y
    local inset = 2
    local box_x = px + inset
    local box_y = py + inset
    local box_w = cell_size - inset * 2
    local box_h = cell_size - inset * 2

    if cell.revealed then
        gfx.fill_rect(box_x, box_y, box_w, box_h, COLOR_CELL_OPEN)
        gfx.rect(box_x, box_y, box_w, box_h, COLOR_BORDER)

        if cell.mine then
            local radius = math.max(3, math.floor(cell_size * 0.22))
            local cx = px + math.floor(cell_size / 2)
            local cy = py + math.floor(cell_size / 2)

            gfx.fill_rect(box_x, box_y, box_w, box_h, COLOR_MINE)
            draw_disc(cx, cy, radius, COLOR_BLACK)
            gfx.line(cx - radius - 2, cy, cx + radius + 2, cy, COLOR_BLACK)
            gfx.line(cx, cy - radius - 2, cx, cy + radius + 2, COLOR_BLACK)
        elseif cell.nearby > 0 then
            local number_text = tostring(cell.nearby)
            local color = number_colors[cell.nearby] or COLOR_TEXT
            local text_x = px + math.floor(cell_size / 2) - 4
            local text_y = py + math.floor(cell_size / 2) - 4

            gfx.text(text_x, text_y, number_text, color, 1)
        end
    else
        local closed_color = is_cursor and COLOR_CYAN or COLOR_CELL_CLOSED
        local edge_color = is_cursor and COLOR_WHITE or COLOR_CELL_EDGE

        gfx.fill_rect(box_x, box_y, box_w, box_h, closed_color)
        gfx.rect(box_x, box_y, box_w, box_h, edge_color)

        if cell.flag then
            local flag_x = px + math.floor(cell_size * 0.34)
            local flag_y = py + math.floor(cell_size * 0.24)
            local pole_h = math.max(7, math.floor(cell_size * 0.50))
            local flag_w = math.max(5, math.floor(cell_size * 0.34))

            gfx.fill_rect(flag_x, flag_y, 2, pole_h, COLOR_FLAG)
            gfx.fill_rect(flag_x + 2, flag_y, flag_w, math.max(3, math.floor(cell_size * 0.22)), COLOR_FLAG)
            gfx.fill_rect(flag_x - 2, flag_y + pole_h - 2, 7, 2, COLOR_FLAG)
        end
    end

    if is_cursor and cell.revealed then
        gfx.rect(px + 1, py + 1, cell_size - 2, cell_size - 2, COLOR_CYAN)
    end
end

local function draw_board()
    gfx.fill_rect(board_x - 6, board_y - 6, board_px_w + 12, board_px_h + 12, COLOR_BORDER)
    gfx.fill_rect(board_x - 2, board_y - 2, board_px_w + 4, board_px_h + 4, COLOR_BLACK)
    gfx.fill_rect(board_x, board_y, board_px_w, board_px_h, COLOR_BOARD)

    for y = 1, board_h do
        for x = 1, board_w do
            draw_cell(x, y)
        end
    end
end

local function draw_game()
    local diff = difficulties[selected_difficulty]

    gfx.clear(COLOR_BG)

    gfx.text(34, 26, "MINESWEEPER", COLOR_YELLOW, 3)
    gfx.text(
        34,
        82,
        diff.name .. "   MINES " .. flags_left .. "   OPEN " .. revealed_count,
        COLOR_TEXT,
        2
    )

    if game_over then
        local color = won and COLOR_GREEN or COLOR_RED
        gfx.text(34, 116, "R / ENTER RESTART     M MENU     Q QUIT", color, 1)
    else
        gfx.text(34, 116, "ARROWS/WASD MOVE   ENTER/SPACE OPEN   F FLAG", COLOR_MUTED, 1)
    end

    draw_board()

    gfx.text(34, SCREEN_H - 28, message, COLOR_MUTED, 1)

    if game_over then
        local label = won and "YOU WIN" or "BOOM"
        local color = won and COLOR_GREEN or COLOR_RED
        local panel_w = 270
        local panel_h = 66
        local panel_x = math.floor((SCREEN_W - panel_w) / 2)
        local panel_y = math.floor(board_y + board_px_h / 2 - panel_h / 2)

        gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR_BLACK)
        gfx.rect(panel_x, panel_y, panel_w, panel_h, color)
        gfx.text(panel_x + 66, panel_y + 23, label, color, 2)
    end
end

local function draw_menu()
    gfx.clear(COLOR_BG)

    draw_text_center(34, "MINESWEEPER", COLOR_YELLOW, 3)
    draw_text_center(88, "UP/DOWN SELECT   ENTER START   Q QUIT", COLOR_MUTED, 1)

    local panel_w = math.min(620, SCREEN_W - 96)
    local panel_h = 234
    local panel_x = math.floor((SCREEN_W - panel_w) / 2)
    local panel_y = math.floor((SCREEN_H - panel_h) / 2) - 12

    gfx.fill_rect(panel_x, panel_y, panel_w, panel_h, COLOR_PANEL)
    gfx.rect(panel_x, panel_y, panel_w, panel_h, COLOR_BORDER)

    for i = 1, #difficulties do
        local diff = difficulties[i]
        local y = panel_y + 28 + (i - 1) * 59
        local active = i == selected_difficulty
        local color = active and COLOR_GREEN or COLOR_TEXT

        if active then
            gfx.fill_rect(panel_x + 18, y - 10, panel_w - 36, 40, 0x00192D43)
            gfx.rect(panel_x + 18, y - 10, panel_w - 36, 40, COLOR_GREEN)
            gfx.text(panel_x + 34, y, "> " .. diff.name, color, 2)
        else
            gfx.text(panel_x + 34, y, "  " .. diff.name, color, 2)
        end

        gfx.text(panel_x + 238, y + 4, diff.description, active and COLOR_CYAN or COLOR_MUTED, 1)
    end

    draw_text_center(SCREEN_H - 48, "FIRST CLICK IS ALWAYS SAFE", COLOR_CYAN, 1)
end

local function draw_current_screen()
    if screen == "MENU" then
        draw_menu()
    else
        draw_game()
    end

    gfx.present()
end

local function cycle_selection(delta)
    selected_difficulty = selected_difficulty + delta

    if selected_difficulty < 1 then
        selected_difficulty = #difficulties
    elseif selected_difficulty > #difficulties then
        selected_difficulty = 1
    end
end

local function start_game()
    screen = "GAME"
    reset_game()
end

local function handle_key(key)
    if key == nil then
        return
    end

    if key == input.Q then
        running = false
        return
    end

    if screen == "MENU" then
        if key == input.UP or key == input.W then
            cycle_selection(-1)
        elseif key == input.DOWN or key == input.S then
            cycle_selection(1)
        elseif key == input.ENTER or key == input.SPACE then
            start_game()
        end

        return
    end

    if key == input.M then
        screen = "MENU"
        message = "SELECT DIFFICULTY"
        return
    end

    if key == input.R then
        reset_game()
        return
    end

    if game_over then
        if key == input.ENTER or key == input.SPACE then
            reset_game()
        end

        return
    end

    if key == input.UP or key == input.W then
        move_cursor(0, -1)
    elseif key == input.DOWN or key == input.S then
        move_cursor(0, 1)
    elseif key == input.LEFT or key == input.A then
        move_cursor(-1, 0)
    elseif key == input.RIGHT or key == input.D then
        move_cursor(1, 0)
    elseif key == input.ENTER or key == input.SPACE then
        reveal_cell(cursor_x, cursor_y)
    elseif key == input.F then
        toggle_flag(cursor_x, cursor_y)
    end
end

math.randomseed(math.floor(now() * 1000000) + 9173)
math.random()
math.random()
math.random()

while running do
    -- One input event per visual frame: compatible with NeonOS input.poll().
    handle_key(input.poll())
    draw_current_screen()
end
