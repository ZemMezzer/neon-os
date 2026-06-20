#pragma once

#include "ff.h"

/*
    Installs the Lua files embedded into the kernel image on a freshly
    formatted filesystem. The operation is idempotent: a marker file is
    written only after every bundled file has been stored successfully.
*/
FRESULT builtin_apps_install_if_needed(void);
