#include "lua_zip.h"

#include <stddef.h>
#include <string.h>

#include "lauxlib.h"
#include "zip.h"

#define LUA_ZIP_ARCHIVE_METATABLE "neonos.zip.archive"

typedef struct LuaZipArchive {
    zip_t zip;
    int closed;
    int writer;
} LuaZipArchive;


static int lua_zip_has_embedded_nul(const char* value, size_t length) {
    size_t index;

    for (index = 0; index < length; ++index) {
        if (value[index] == '\0') {
            return 1;
        }
    }

    return 0;
}


static const char* lua_zip_check_text(
    lua_State* state,
    int argument_index,
    size_t limit,
    const char* description
) {
    const char* value;
    size_t length;

    value = luaL_checklstring(state, argument_index, &length);

    if (length == 0) {
        luaL_argerror(state, argument_index, "must not be empty");
    }

    if (lua_zip_has_embedded_nul(value, length)) {
        luaL_argerror(state, argument_index, "must not contain a NUL byte");
    }

    if (length >= limit) {
        luaL_argerror(state, argument_index, description);
    }

    return value;
}


static const char* lua_zip_check_path(lua_State* state, int argument_index) {
    return lua_zip_check_text(
        state,
        argument_index,
        ZIP_PATH_MAX,
        "path is too long"
    );
}


static const char* lua_zip_check_entry_name(
    lua_State* state,
    int argument_index
) {
    return lua_zip_check_text(
        state,
        argument_index,
        ZIP_ENTRY_NAME_MAX,
        "archive entry name is too long"
    );
}


static zip_compression lua_zip_check_compression(
    lua_State* state,
    int argument_index
) {
    int type;

    if (lua_isnoneornil(state, argument_index)) {
        return ZIP_DEFLATE;
    }

    type = lua_type(state, argument_index);

    if (type == LUA_TNUMBER) {
        lua_Integer value = luaL_checkinteger(state, argument_index);

        if (value == (lua_Integer)ZIP_STORE) {
            return ZIP_STORE;
        }

        if (value == (lua_Integer)ZIP_DEFLATE) {
            return ZIP_DEFLATE;
        }

        luaL_argerror(
            state,
            argument_index,
            "compression must be zip.STORE or zip.DEFLATE"
        );
    }

    if (type == LUA_TSTRING) {
        const char* value = luaL_checkstring(state, argument_index);

        if (strcmp(value, "store") == 0) {
            return ZIP_STORE;
        }

        if (strcmp(value, "deflate") == 0) {
            return ZIP_DEFLATE;
        }

        luaL_argerror(
            state,
            argument_index,
            "compression must be 'store' or 'deflate'"
        );
    }

    luaL_argerror(
        state,
        argument_index,
        "compression must be zip.STORE, zip.DEFLATE, 'store', or 'deflate'"
    );

    return ZIP_DEFLATE;
}


static int lua_zip_push_status_error(
    lua_State* state,
    const char* operation,
    const zip_t* zip,
    zip_status status
) {
    const char* detail = NULL;

    if (zip != NULL && status == ZIP_ERR_ARCHIVE) {
        detail = zip_last_archive_error(zip);
    }

    lua_pushnil(state);

    if (detail != NULL && detail[0] != '\0') {
        lua_pushfstring(
            state,
            "zip.%s failed: %s (%s)",
            operation,
            zip_status_string(status),
            detail
        );
    } else {
        lua_pushfstring(
            state,
            "zip.%s failed: %s",
            operation,
            zip_status_string(status)
        );
    }

    return 2;
}


static LuaZipArchive* lua_zip_check_archive(
    lua_State* state,
    int argument_index
) {
    return (LuaZipArchive*)luaL_checkudata(
        state,
        argument_index,
        LUA_ZIP_ARCHIVE_METATABLE
    );
}


static LuaZipArchive* lua_zip_check_open_archive(
    lua_State* state,
    int argument_index
) {
    LuaZipArchive* archive = lua_zip_check_archive(state, argument_index);

    if (archive->closed) {
        luaL_error(state, "ZIP archive is already closed");
    }

    return archive;
}


static LuaZipArchive* lua_zip_check_writer(
    lua_State* state,
    int argument_index
) {
    LuaZipArchive* archive = lua_zip_check_open_archive(state, argument_index);

    if (!archive->writer) {
        luaL_error(state, "ZIP archive was opened for reading");
    }

    return archive;
}


static LuaZipArchive* lua_zip_check_reader(
    lua_State* state,
    int argument_index
) {
    LuaZipArchive* archive = lua_zip_check_open_archive(state, argument_index);

    if (archive->writer) {
        luaL_error(state, "ZIP archive was created for writing");
    }

    return archive;
}


static void lua_zip_push_entry(lua_State* state, const zip_entry* entry) {
    lua_newtable(state);

    lua_pushstring(state, entry->name);
    lua_setfield(state, -2, "name");

    lua_pushinteger(state, (lua_Integer)entry->size);
    lua_setfield(state, -2, "size");

    lua_pushinteger(state, (lua_Integer)entry->compressed_size);
    lua_setfield(state, -2, "compressedSize");

    lua_pushboolean(state, entry->is_directory);
    lua_setfield(state, -2, "isDirectory");

    lua_pushboolean(state, entry->is_supported);
    lua_setfield(state, -2, "isSupported");
}


static int lua_zip_create(lua_State* state) {
    const char* path = lua_zip_check_path(state, 1);
    LuaZipArchive* archive;
    zip_status status;

    archive = (LuaZipArchive*)lua_newuserdatauv(
        state,
        sizeof(*archive),
        0
    );

    zip_init(&archive->zip);
    archive->closed = 0;
    archive->writer = 1;

    status = zip_create(&archive->zip, path);
    if (status != ZIP_OK) {
        lua_pop(state, 1);
        return lua_zip_push_status_error(state, "create", &archive->zip, status);
    }

    luaL_setmetatable(state, LUA_ZIP_ARCHIVE_METATABLE);
    return 1;
}


static int lua_zip_open(lua_State* state) {
    const char* path = lua_zip_check_path(state, 1);
    LuaZipArchive* archive;
    zip_status status;

    archive = (LuaZipArchive*)lua_newuserdatauv(
        state,
        sizeof(*archive),
        0
    );

    zip_init(&archive->zip);
    archive->closed = 0;
    archive->writer = 0;

    status = zip_open(&archive->zip, path);
    if (status != ZIP_OK) {
        lua_pop(state, 1);
        return lua_zip_push_status_error(state, "open", &archive->zip, status);
    }

    luaL_setmetatable(state, LUA_ZIP_ARCHIVE_METATABLE);
    return 1;
}


static int lua_zip_archive_add_file(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_writer(state, 1);
    const char* archive_name = lua_zip_check_entry_name(state, 2);
    const char* source_path = lua_zip_check_path(state, 3);
    zip_compression compression = lua_zip_check_compression(state, 4);
    zip_status status = zip_add_file(
        &archive->zip,
        archive_name,
        source_path,
        compression
    );

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "addFile", &archive->zip, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_add_string(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_writer(state, 1);
    const char* archive_name = lua_zip_check_entry_name(state, 2);
    size_t size;
    const char* data = luaL_checklstring(state, 3, &size);
    zip_compression compression = lua_zip_check_compression(state, 4);
    zip_status status = zip_add_memory(
        &archive->zip,
        archive_name,
        data,
        size,
        compression
    );

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "addString", &archive->zip, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_add_directory(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_writer(state, 1);
    const char* archive_name = lua_zip_check_entry_name(state, 2);
    zip_status status = zip_add_directory(&archive->zip, archive_name);

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "addDirectory", &archive->zip, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_entries(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_reader(state, 1);
    size_t count = zip_count(&archive->zip);
    size_t index;

    lua_newtable(state);

    for (index = 0; index < count; ++index) {
        zip_entry entry;
        zip_status status = zip_get_entry(&archive->zip, index, &entry);

        if (status != ZIP_OK) {
            lua_pop(state, 1);
            return lua_zip_push_status_error(state, "entries", &archive->zip, status);
        }

        lua_zip_push_entry(state, &entry);
        lua_rawseti(state, -2, (lua_Integer)(index + 1U));
    }

    return 1;
}


static int lua_zip_archive_extract(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_reader(state, 1);
    const char* archive_name = lua_zip_check_entry_name(state, 2);
    const char* destination_path = lua_zip_check_path(state, 3);
    zip_status status = zip_extract_file(
        &archive->zip,
        archive_name,
        destination_path
    );

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "extract", &archive->zip, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_close(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_archive(state, 1);
    zip_status status;

    if (archive->closed) {
        lua_pushboolean(state, 1);
        return 1;
    }

    status = zip_close(&archive->zip);
    archive->closed = 1;

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "close", &archive->zip, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_abort(lua_State* state) {
    LuaZipArchive* archive = lua_zip_check_archive(state, 1);

    if (!archive->closed) {
        zip_abort(&archive->zip);
        archive->closed = 1;
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_archive_gc(lua_State* state) {
    LuaZipArchive* archive = (LuaZipArchive*)luaL_testudata(
        state,
        1,
        LUA_ZIP_ARCHIVE_METATABLE
    );

    if (archive != NULL && !archive->closed) {
        zip_abort(&archive->zip);
        archive->closed = 1;
    }

    return 0;
}


static int lua_zip_archive_close_metamethod(lua_State* state) {
    LuaZipArchive* archive = (LuaZipArchive*)luaL_testudata(
        state,
        1,
        LUA_ZIP_ARCHIVE_METATABLE
    );

    if (archive != NULL && !archive->closed) {
        (void)zip_close(&archive->zip);
        archive->closed = 1;
    }

    return 0;
}


static int lua_zip_unpack(lua_State* state) {
    const char* archive_path = lua_zip_check_path(state, 1);
    const char* destination_path = lua_zip_check_path(state, 2);
    zip_status status = zip_unpack(archive_path, destination_path);

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "unpack", NULL, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_validate(lua_State* state) {
    const char* archive_path = lua_zip_check_path(state, 1);
    zip_status status = zip_validate(archive_path);

    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "validate", NULL, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_zip_list(lua_State* state) {
    const char* archive_path = lua_zip_check_path(state, 1);
    zip_t archive;
    zip_status status;
    size_t count;
    size_t index;

    zip_init(&archive);

    status = zip_open(&archive, archive_path);
    if (status != ZIP_OK) {
        return lua_zip_push_status_error(state, "list", &archive, status);
    }

    lua_newtable(state);
    count = zip_count(&archive);

    for (index = 0; index < count; ++index) {
        zip_entry entry;

        status = zip_get_entry(&archive, index, &entry);
        if (status != ZIP_OK) {
            (void)zip_close(&archive);
            lua_pop(state, 1);
            return lua_zip_push_status_error(state, "list", &archive, status);
        }

        lua_zip_push_entry(state, &entry);
        lua_rawseti(state, -2, (lua_Integer)(index + 1U));
    }

    status = zip_close(&archive);
    if (status != ZIP_OK) {
        lua_pop(state, 1);
        return lua_zip_push_status_error(state, "list", &archive, status);
    }

    return 1;
}


static const luaL_Reg lua_zip_archive_methods[] = {
    { "addFile",      lua_zip_archive_add_file },
    { "addString",    lua_zip_archive_add_string },
    { "addDirectory", lua_zip_archive_add_directory },
    { "entries",      lua_zip_archive_entries },
    { "extract",      lua_zip_archive_extract },
    { "close",        lua_zip_archive_close },
    { "abort",        lua_zip_archive_abort },
    { NULL,            NULL }
};


static const luaL_Reg lua_zip_library[] = {
    { "create",   lua_zip_create },
    { "open",     lua_zip_open },
    { "unpack",   lua_zip_unpack },
    { "validate", lua_zip_validate },
    { "list",     lua_zip_list },
    { NULL,       NULL }
};


int luaopen_zip(lua_State* state) {
    luaL_newmetatable(state, LUA_ZIP_ARCHIVE_METATABLE);

    luaL_setfuncs(state, lua_zip_archive_methods, 0);

    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");

    lua_pushcfunction(state, lua_zip_archive_gc);
    lua_setfield(state, -2, "__gc");

    lua_pushcfunction(state, lua_zip_archive_close_metamethod);
    lua_setfield(state, -2, "__close");

    lua_pop(state, 1);

    luaL_newlib(state, lua_zip_library);

    lua_pushinteger(state, ZIP_STORE);
    lua_setfield(state, -2, "STORE");

    lua_pushinteger(state, ZIP_DEFLATE);
    lua_setfield(state, -2, "DEFLATE");

    return 1;
}
