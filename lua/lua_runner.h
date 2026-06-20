#pragma once

/*
    Runs a text Lua script stored on the mounted FatFs volume.

    Returns:
      0  - script completed successfully
     -1  - invalid path
     -2  - could not create Lua state
     -3  - could not open script file
     -4  - file read error
     -5  - Lua load/parse error
     -6  - Lua runtime error
*/
int lua_run_file(const char* path);
