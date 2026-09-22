// Refer to https://felix86.com/docs/devs/lua/
#pragma once

#ifdef FELIX86_BUILD_LUA_SCRIPTING
#include <filesystem>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include "Zydis/Mnemonic.h"
#include "felix86/common/types.hpp"

static_assert(LUA_VERSION_NUM >= 503, "Lua 5.3 or newer required");

enum class LuaHookType {
    Null,
    Compile,  // Hook on specific address, or address == 0 to run on every compile
    Run,      // Hook when a block compiled from a specific address is run
    Mnemonic, // Hook when a specific instruction is compiled
    Thunk,    // Hook when a specific thunk is run
};

union LuaHookData {
    struct {
        u64 start;
        u64 end;
    } compile_or_run; // [start, end)

    struct {
        ZydisMnemonic mnemonic;
    } mnemonic;

    struct {
        std::string func_name;
    } thunk;
};

struct LuaHook {
    LuaHookType type = LuaHookType::Null;
    LuaHookData data = {};
    int func_ref = LUA_REFNIL;
};

struct Lua {
    static bool loadScript(const std::filesystem::path& path);
    static void callHook(const LuaHook& hook, u64 rip);
    static void callHookDirect(int ref, u64 rip);
};
#endif
