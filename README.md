![Preview](.github/preview.png)

# NeonOS

A bare-metal, single-threaded operating system. Written in C and minimal assembly. Lua is the primary programming language exposed to the user.

Boots without UEFI. No Linux. No runtime dependencies.

---

## Features

- Boots from scratch without UEFI or bootloader abstraction
- FAT16 filesystem support via FatFs (up to 1 GB)
- PS/2 keyboard input
- Interactive shell with built-in commands
- Lua scripting environment with native APIs for graphics, input, and filesystem

---

## Shell Commands

| Command | Description |
|---|---|
| `help` | Show all available commands |
| `pwd` | Print current directory |
| `cd <path>` | Change directory |
| `ls [path]` | List directory contents |
| `mkdir <path>` | Create a directory |
| `cat <file>` | Print file contents |
| `write <file> <text>` | Write text to a file (overwrites) |
| `append <file> <text>` | Append text to a file |
| `rm <path>` | Remove a file or empty directory |
| `mv <old> <new>` | Rename or move a file |
| `echo <text>` | Print text to console |
| `lua <script>` | Run a Lua script |
| `path` | Show or edit the program search path (like PATH in classic systems) |
| `alias` | Add or remove file associations |
| `open <file>` | Open a file using its alias association |
| `sh <script>` | Run a shell script |

---

## Lua API

NeonOS exposes three native modules to Lua programs.

### `gfx` — Graphics

```lua
gfx.width()                            -- framebuffer width in pixels
gfx.height()                           -- framebuffer height in pixels
gfx.clear(color)                       -- fill the screen with a color
gfx.pixel(x, y, color)                 -- draw a single pixel
gfx.line(x0, y0, x1, y1, color)        -- draw a line
gfx.rect(x, y, w, h, color)            -- draw a rectangle outline
gfx.fill_rect(x, y, w, h, color)       -- draw a filled rectangle
gfx.text(x, y, text, color [, scale])  -- draw text (scale 1–32, default 1)
gfx.present()                          -- flush framebuffer to screen
```

Colors are 32-bit integers in `0xRRGGBB` or `0xAARRGGBB` format.
Coordinates range from −4096 to 4096.

### `input` — Keyboard

```lua
input.poll()              -- returns key_code, modifiers (or nil if no event)
input.poll_latest()       -- drains the queue, returns only the most recent event
input.pressed(key_code)   -- true if the given key was pressed this frame
input.any_pressed()       -- true if any key was pressed this frame
```

**Key constants:**

```lua
-- Letters: input.A through input.Z
input.LEFT,  input.RIGHT, input.UP,   input.DOWN
input.HOME,  input.END,   input.DELETE
input.PAGE_UP, input.PAGE_DOWN, input.INSERT
input.ENTER, input.BACKSPACE, input.TAB, input.SPACE
input.ESCAPE
input.F1, input.F2, input.F3,  input.F4
input.F5, input.F6, input.F7,  input.F8
input.F9, input.F10, input.F11, input.F12
```

**Modifiers table** (returned by `input.poll()` and `input.poll_latest()`):

```lua
local key, mods = input.poll()
if key and mods.ctrl and key == input.S then
    -- Ctrl+S
end
-- mods.shift, mods.ctrl, mods.alt (booleans)
```

Input is frame-locked: one event is consumed per `gfx.present()` call.
`poll_latest()` is useful for fast-repeat scenarios (e.g. scrolling) where only the last held key matters.

### `shell` — Shell

```lua
shell.exec(command)        -- execute a shell command string, returns exit status
shell.run_script(path)     -- run a shell script file, returns exit status
```

### `fs` — Filesystem

```lua
fs.list(path)                    -- returns array of entry names
fs.listInfo(path)                -- returns array of {name, size, is_dir, ...}
fs.exists(path)                  -- boolean
fs.isDir(path)                   -- boolean
fs.isReadOnly(path)              -- boolean
fs.getSize(path)                 -- file size in bytes
fs.attributes(path)              -- {size, is_dir, readonly, hidden, system, archive}
fs.makeDir(path)                 -- create directory
fs.delete(path)                  -- delete file or directory tree
fs.copy(source, destination)     -- copy file or directory tree
fs.move(source, destination)     -- move/rename
fs.rename(source, destination)   -- alias for fs.move
fs.getName(path)                 -- last path component
fs.getDir(path)                  -- parent directory
fs.combine(...)                  -- join path components
fs.getFreeSpace(path)            -- free bytes on volume
fs.getCapacity(path)             -- total volume capacity in bytes
```

Paths accept both Unix-style (`/folder/file.lua`) and FatFs-style (`0:/folder/file.lua`) notation.

---

## Example Lua Program

```lua
local w = gfx.width()
local h = gfx.height()

while true do
    gfx.clear(0x000000)
    gfx.text(10, 10, "Hello from NeonOS!", 0x00FF00)
    gfx.present()

    local key = input.poll()
    if key == input.ESCAPE then
        break
    end
end
```

---

## Status

R&D / experimental. APIs are subject to change.
