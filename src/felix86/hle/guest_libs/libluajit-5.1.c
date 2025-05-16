#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

struct lua_State;

const char* lua_pushvfstring(struct lua_State* L, const char* fmt, va_list argp);

void luaL_where(struct lua_State* L, int level);

void lua_concat(struct lua_State* L, int n);

int lua_error(struct lua_State* L);

const char* lua_pushfstring(struct lua_State* L, const char* fmt, ...) {
    const char* ret;
    va_list argp;
    va_start(argp, fmt);
    ret = lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    return ret;
}

int luaL_error(struct lua_State* L, const char* fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    luaL_where(L, 1);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    lua_concat(L, 2);
    return lua_error(L);
}

void lua_sethook(struct lua_State* L, void* f, int mask, int count) {
    printf("This application uses lua_sethook!\n");
}

#define LUAJIT_VERSION(num)                                                                                                                          \
    __attribute__((used)) __attribute__((visibility("default"))) void luaJIT_version_2_1_##num() {                                                   \
        return;                                                                                                                                      \
    }

// Some binaries care that this symbol is defined -- ie. luajit binary
// Unsure why you'd do versioning this way, but we'll define every single one we care about
LUAJIT_VERSION(1736781742)
LUAJIT_VERSION(1700008891)