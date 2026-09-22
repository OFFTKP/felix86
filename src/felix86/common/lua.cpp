#ifdef FELIX86_BUILD_LUA_SCRIPTING
#include "felix86/common/lua.hpp"
#include "felix86/common/state.hpp"

void Lua::callHookDirect(int ref, u64 rip) {
    ThreadState* state = ThreadState::Get();
    lua_State* L = state->lua_state;
    ASSERT(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(L, -1)) {
        WARN("Hook %d is not a function", ref);
        lua_pop(L, 1);
        return;
    }
    int base = lua_gettop(L);
    lua_pushlightuserdata(L, (void*)&state->ctx);
    lua_pushinteger(L, (lua_Integer)rip);

    int return_count = 0;
    int arg_count = lua_gettop(L) - base;
    if (lua_pcall(L, arg_count, return_count, 0) != LUA_OK) {
        WARN("Lua function call returned error: %s", luaL_tolstring(L, -1, nullptr));
        lua_pop(L, 2);
    }
}

void Lua::callHook(const LuaHook& hook, u64 rip) {
    ThreadState* state = ThreadState::Get();
    lua_State* L = state->lua_state;
    ASSERT(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, hook.func_ref);
    if (!lua_isfunction(L, -1)) {
        WARN("Hook %d is not a function", hook.func_ref);
        lua_pop(L, 1);
        return;
    }
    int base = lua_gettop(L);
    lua_pushlightuserdata(L, (void*)state);

    switch (hook.type) {
    case LuaHookType::Null: {
        ERROR("Bad hook type");
        break;
    }
    case LuaHookType::Compile:
    case LuaHookType::Run: {
        lua_pushinteger(L, (lua_Integer)rip);
        break;
    }
    case LuaHookType::Thunk: {
        lua_pushstring(L, hook.data.thunk.func_name.c_str());
        break;
    }
    case LuaHookType::Mnemonic: {
        lua_pushinteger(L, (lua_Integer)rip);
        lua_pushinteger(L, (lua_Integer)hook.data.mnemonic.mnemonic);
        break;
    }
    }

    int return_count = 0;
    int arg_count = lua_gettop(L) - base;
    if (lua_pcall(L, arg_count, return_count, 0) != LUA_OK) {
        WARN("Lua function call returned error: %s", luaL_tolstring(L, -1, nullptr));
        lua_pop(L, 2);
    }
}
#endif
