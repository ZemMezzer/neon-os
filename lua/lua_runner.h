#pragma once

/*
    Executes a Lua source file from the mounted FatFs volume.

    Return value:
      0..255  - script completed normally or called os.exit(code)
      < 0     - NeonOS loader/runtime failure
*/
#define LUA_RUNNER_ERR_INVALID_PATH   (-1)
#define LUA_RUNNER_ERR_STATE_CREATE   (-2)
#define LUA_RUNNER_ERR_OPEN_FILE      (-3)
#define LUA_RUNNER_ERR_READ_FILE      (-4)
#define LUA_RUNNER_ERR_LOAD           (-5)
#define LUA_RUNNER_ERR_RUNTIME        (-6)

int lua_run_file(const char* path);
int lua_run_file_args(const char* path, int argc, char** argv);