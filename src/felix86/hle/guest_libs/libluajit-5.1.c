#include <alloca.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct lua_State;

const char* lua_pushstring(struct lua_State* L, const char* str);

void luaL_where(struct lua_State* L, int level);

void lua_concat(struct lua_State* L, int n);

int lua_error(struct lua_State* L);

const char* lua_pushfstring(struct lua_State* L, const char* fmt, ...) {
    // va_args is arch-specific. To save us headache, we will use sprintf
    // to construct the final string, then use lua_pushstring. Lua will then
    // make a copy of the string, so we don't need to keep it allocated.
    // If the buffer is small enough, multiplying might not be enough, so
    // add a page of size.
    int size = 4096 + strlen(fmt);
    size *= 2;
    char* buffer = (char*)alloca(size);
    va_list argp;
    va_start(argp, fmt);
    int new_size = vsnprintf(buffer, size - 1, fmt, argp);
    va_end(argp);
    if (new_size >= size) {
        printf("Buffer not big enough during lua_pushfstring?\n");
    }
    return lua_pushstring(L, buffer);
}

int luaL_error(struct lua_State* L, const char* fmt, ...) {
    int size = 4096 + strlen(fmt);
    size *= 2;
    char* buffer = (char*)alloca(size);
    va_list argp;
    va_start(argp, fmt);
    int new_size = vsnprintf(buffer, size - 1, fmt, argp);
    va_end(argp);
    if (new_size >= size) {
        printf("Buffer not big enough during luaL_error?\n");
    }
    luaL_where(L, 1);
    lua_pushstring(L, buffer);
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