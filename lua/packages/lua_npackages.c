#include "lua_npackages.h"

#include <stddef.h>

#include "lauxlib.h"
#include "package_manager.h"

typedef struct NpackagesListContext {
    lua_State* state;
    int table_index;
    int next_index;
} NpackagesListContext;

static int npackages_has_path_syntax(const char* value) {
    int index = 0;

    if (value == NULL) {
        return 0;
    }

    while (value[index] != '\0') {
        if (
            value[index] == '/' ||
            value[index] == '\\' ||
            value[index] == ':'
        ) {
            return 1;
        }

        index++;
    }

    return 0;
}

static int npackages_return_error(
    lua_State* state,
    const char* message
) {
    lua_pushnil(state);
    lua_pushstring(
        state,
        message != NULL ? message : "unknown package error"
    );
    return 2;
}

static void npackages_set_string_field(
    lua_State* state,
    const char* name,
    const char* value
) {
    lua_pushstring(state, value != NULL ? value : "");
    lua_setfield(state, -2, name);
}

static void npackages_set_optional_string_field(
    lua_State* state,
    const char* name,
    const char* value
) {
    if (value == NULL || value[0] == '\0') {
        lua_pushnil(state);
    } else {
        lua_pushstring(state, value);
    }

    lua_setfield(state, -2, name);
}

static void npackages_push_info(
    lua_State* state,
    const PackageInfo* info
) {
    lua_createtable(state, 0, 7);

    npackages_set_string_field(state, "id", info->id);
    npackages_set_string_field(state, "path", info->path);
    npackages_set_string_field(state, "name", info->name);
    npackages_set_optional_string_field(
        state,
        "version",
        info->version
    );
    npackages_set_optional_string_field(
        state,
        "description",
        info->description
    );
    npackages_set_optional_string_field(
        state,
        "icon_path",
        info->icon_path
    );

    lua_pushboolean(state, info->icon_exists != 0);
    lua_setfield(state, -2, "icon_exists");
}

static int npackages_info(lua_State* state) {
    const char* target;
    PackageInfo info;
    PackageStatus status;

    target = luaL_checkstring(state, 1);

    if (npackages_has_path_syntax(target)) {
        status = package_get_info(target, &info);
    } else {
        status = package_get_info_by_name(target, &info);
    }

    if (status != PACKAGE_OK) {
        return npackages_return_error(
            state,
            package_status_string(status)
        );
    }

    npackages_push_info(state, &info);
    return 1;
}

static int npackages_path(lua_State* state) {
    const char* target;
    char path[NEON_FS_PATH_MAX];
    PackageInfo info;
    PackageStatus status;

    target = luaL_checkstring(state, 1);

    if (npackages_has_path_syntax(target)) {
        status = package_get_info(target, &info);

        if (status == PACKAGE_OK) {
            lua_pushstring(state, info.path);
            return 1;
        }
    } else {
        status = package_get_path(
            target,
            path,
            sizeof(path)
        );

        if (status == PACKAGE_OK) {
            lua_pushstring(state, path);
            return 1;
        }
    }

    return npackages_return_error(
        state,
        package_status_string(status)
    );
}

static void npackages_list_item(
    const PackageInfo* info,
    void* user_data
) {
    NpackagesListContext* context = (NpackagesListContext*)user_data;

    if (context == NULL || context->state == NULL || info == NULL) {
        return;
    }

    npackages_push_info(context->state, info);
    lua_rawseti(
        context->state,
        context->table_index,
        context->next_index
    );
    context->next_index++;
}

static int npackages_list(lua_State* state) {
    NpackagesListContext context;
    PackageStatus status;

    lua_newtable(state);

    context.state = state;
    context.table_index = lua_gettop(state);
    context.next_index = 1;

    status = package_list(npackages_list_item, &context);

    if (status != PACKAGE_OK) {
        lua_pop(state, 1);
        return npackages_return_error(
            state,
            package_status_string(status)
        );
    }

    return 1;
}

static const luaL_Reg npackages_library[] = {
    { "info", npackages_info },
    { "path", npackages_path },
    { "list", npackages_list },
    { NULL, NULL }
};

int luaopen_npackages(lua_State* state) {
    luaL_newlib(state, npackages_library);
    return 1;
}
