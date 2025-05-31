#ifdef BUILD_THUNKING

#include <unordered_map>
#include <dlfcn.h>
#include "felix86/common/global.hpp"
#include "felix86/hle/abi.hpp"
#include "felix86/hle/thunks.hpp"

extern void* libluajit;

typedef int (*lua_CFunction)(struct lua_State* L);

typedef void* (*lua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

typedef const char* (*lua_Reader)(lua_State* L, void* ud, size_t* sz);

typedef int (*lua_Writer)(lua_State* L, const void* p, size_t sz, void* ud);

static std::unordered_map<u64, u64> x86_to_riscv_pointers{};
static std::unordered_map<u64, u64> riscv_to_x86_pointers{};

static void register_mapping(u64 x86_pointer, u64 riscv_pointer) {
    auto guard = g_process_globals.states_lock.lock();
    x86_to_riscv_pointers[x86_pointer] = riscv_pointer;
    riscv_to_x86_pointers[riscv_pointer] = x86_pointer;
}

static u64 riscv_to_x86(u64 riscv_pointer) {
    auto guard = g_process_globals.states_lock.lock();
    if (riscv_to_x86_pointers.find(riscv_pointer) != riscv_to_x86_pointers.end()) {
        return riscv_to_x86_pointers[riscv_pointer];
    } else {
        return 0;
    }
}

static u64 x86_to_riscv(u64 x86_pointer) {
    auto guard = g_process_globals.states_lock.lock();
    if (x86_to_riscv_pointers.find(x86_pointer) != x86_to_riscv_pointers.end()) {
        return x86_to_riscv_pointers[x86_pointer];
    } else {
        return 0;
    }
}

void* cached_or_trampoline(const char* signature, void* guest_function) {
    u64 cached = x86_to_riscv((u64)guest_function);
    if (cached) {
        return (void*)cached;
    }

    return ABIMadness::hostToGuestTrampoline(signature, (void*)guest_function);
}

int host_lua_cpcall(lua_State* L, lua_CFunction func, void* ud) {
    static auto lua_cpcall = (int (*)(lua_State* L, lua_CFunction func, void* ud))dlsym(libluajit, "lua_cpcall");
    return lua_cpcall(L, func, ud);
}

lua_CFunction host_lua_atpanic(lua_State* L, lua_CFunction panicf) {
    static auto lua_atpanic = (lua_CFunction(*)(lua_State * L, lua_CFunction panicf)) dlsym(libluajit, "lua_atpanic");
    return lua_atpanic(L, panicf);
}

lua_CFunction host_lua_tocfunction(lua_State* L, int idx) {
    static auto lua_tocfunction = (lua_CFunction(*)(lua_State * L, int idx)) dlsym(libluajit, "lua_tocfunction");
    return lua_tocfunction(L, idx);
}

void host_lua_pushcclosure(lua_State* L, lua_CFunction fn, int n) {
    static auto lua_pushcclosure = (lua_CFunction(*)(lua_State * L, lua_CFunction fn, int n)) dlsym(libluajit, "lua_pushcclosure");
    lua_pushcclosure(L, fn, n);
}

lua_State* host_lua_newstate(lua_Alloc f, void* ud) {
    static auto lua_newstate = (lua_State * (*)(lua_Alloc f, void* ud)) dlsym(libluajit, "lua_newstate");
    return lua_newstate(f, ud);
}

lua_Alloc host_lua_getallocf(lua_State* L, void** ud) {
    static auto lua_getallocf = (lua_Alloc(*)(lua_State * f, void* ud)) dlsym(libluajit, "lua_getallocf");
    return lua_getallocf(L, ud);
}

void host_lua_setallocf(lua_State* L, lua_Alloc f, void* ud) {
    static auto lua_setallocf = (void (*)(lua_State* L, lua_Alloc f, void* ud))dlsym(libluajit, "lua_setallocf");
    lua_setallocf(L, f, ud);
}

int host_lua_load(lua_State* L, lua_Reader reader, void* dt, const char* chunkname) {
    static auto lua_load = (int (*)(lua_State* L, lua_Reader reader, void* dt, const char* chunkname))dlsym(libluajit, "lua_load");
    return lua_load(L, reader, dt, chunkname);
}

int host_lua_loadx(lua_State* L, lua_Reader reader, void* dt, const char* chunkname, const char* mode) {
    static auto lua_loadx =
        (int (*)(lua_State* L, lua_Reader reader, void* dt, const char* chunkname, const char* mode))dlsym(libluajit, "lua_loadx");
    return lua_loadx(L, reader, dt, chunkname, mode);
}

int host_lua_dump(lua_State* L, lua_Writer writer, void* data) {
    static auto lua_dump = (int (*)(lua_State* L, lua_Writer write, void* data))dlsym(libluajit, "lua_dump");
    return lua_dump(L, writer, data);
}

lua_CFunction felix86_thunk_lua_atpanic(lua_State* L, lua_CFunction panicf) {
    // We return the old x86 function, not the trampoline, because this exposes the function back to the guest
    // And we wouldn't want the guest to attempt to call RISC-V code (even though realistically it probably wouldn't call the ret value)
    // Same goes for any other function below
    void* host_callable = nullptr;

    if (panicf) {
        host_callable = cached_or_trampoline("d_q", (void*)panicf);
        register_mapping((u64)panicf, (u64)host_callable);
    }

    u64 old_ptr = (u64)host_lua_atpanic(L, (lua_CFunction)host_callable);

    if (old_ptr) {
        old_ptr = riscv_to_x86(old_ptr);
    }

    return (lua_CFunction)old_ptr;
}

int felix86_thunk_lua_cpcall(lua_State* L, lua_CFunction func, void* ud) {
    if (func) {
        void* host_callable = cached_or_trampoline("d_q", (void*)func);
        register_mapping((u64)func, (u64)host_callable);
        return host_lua_cpcall(L, (lua_CFunction)host_callable, ud);
    } else {
        return host_lua_cpcall(L, nullptr, ud);
    }
}

lua_CFunction felix86_thunk_lua_tocfunction(lua_State* L, int idx) {
    // Must've been previously pushed via lua_pushcclosure
    u64 riscv_pointer = (u64)host_lua_tocfunction(L, idx);
    if (riscv_pointer) {
        return (lua_CFunction)riscv_to_x86(riscv_pointer);
    } else {
        return nullptr;
    }
}

void felix86_thunk_lua_pushcclosure(lua_State* L, lua_CFunction fn, int n) {
    if (fn) {
        void* host_callable = cached_or_trampoline("d_q", (void*)fn);
        register_mapping((u64)fn, (u64)host_callable);
        host_lua_pushcclosure(L, (lua_CFunction)host_callable, n);
    } else {
        host_lua_pushcclosure(L, nullptr, n);
    }
}

lua_State* felix86_thunk_lua_newstate(lua_Alloc f, void* ud) {
    if (f) {
        void* host_callable = cached_or_trampoline("q_qqqq", (void*)f);
        register_mapping((u64)f, (u64)host_callable);
        return host_lua_newstate((lua_Alloc)host_callable, ud);
    } else {
        return host_lua_newstate(nullptr, ud);
    }
}

lua_Alloc felix86_thunk_lua_getallocf(lua_State* L, void** ud) {
    lua_Alloc func = host_lua_getallocf(L, ud);
    if (func) {
        u64 x86_pointer = riscv_to_x86((u64)func);
        return (lua_Alloc)x86_pointer;
    } else {
        return nullptr;
    }
}

void felix86_thunk_lua_setallocf(lua_State* L, lua_Alloc f, void* ud) {
    if (f) {
        void* host_callable = cached_or_trampoline("q_qqqq", (void*)f);
        register_mapping((u64)f, (u64)host_callable);
        host_lua_setallocf(L, f, ud);
    } else {
        host_lua_setallocf(L, nullptr, ud);
    }
}

int felix86_thunk_lua_load(lua_State* L, lua_Reader reader, void* dt, const char* chunkname) {
    if (reader) {
        void* host_callable = cached_or_trampoline("q_qqq", (void*)reader);
        register_mapping((u64)reader, (u64)host_callable);
        return host_lua_load(L, (lua_Reader)host_callable, dt, chunkname);
    } else {
        return host_lua_load(L, nullptr, dt, chunkname);
    }
}

int felix86_thunk_lua_loadx(lua_State* L, lua_Reader reader, void* dt, const char* chunkname, const char* mode) {
    if (reader) {
        void* host_callable = cached_or_trampoline("q_qqq", (void*)reader);
        register_mapping((u64)reader, (u64)host_callable);
        return host_lua_loadx(L, (lua_Reader)host_callable, dt, chunkname, mode);
    } else {
        return host_lua_loadx(L, nullptr, dt, chunkname, mode);
    }
}

int felix86_thunk_lua_dump(lua_State* L, lua_Writer writer, void* data) {
    if (writer) {
        void* host_callable = cached_or_trampoline("d_qqqq", (void*)writer);
        register_mapping((u64)writer, (u64)host_callable);
        return host_lua_dump(L, (lua_Writer)host_callable, data);
    } else {
        return host_lua_dump(L, nullptr, data);
    }
}

void* get_custom_luajit_thunk(const std::string& name) {
    if (name == "lua_atpanic") {
        return (void*)felix86_thunk_lua_atpanic;
    } else if (name == "lua_cpcall") {
        return (void*)felix86_thunk_lua_cpcall;
    } else if (name == "lua_tocfunction") {
        return (void*)felix86_thunk_lua_tocfunction;
    } else if (name == "lua_pushcclosure") {
        return (void*)felix86_thunk_lua_pushcclosure;
    } else if (name == "lua_newstate") {
        return (void*)felix86_thunk_lua_newstate;
    } else if (name == "lua_getallocf") {
        return (void*)felix86_thunk_lua_getallocf;
    } else if (name == "lua_setallocf") {
        return (void*)felix86_thunk_lua_setallocf;
    } else if (name == "lua_load") {
        return (void*)felix86_thunk_lua_load;
    } else if (name == "lua_loadx") {
        return (void*)felix86_thunk_lua_loadx;
    } else if (name == "lua_dump") {
        return (void*)felix86_thunk_lua_dump;
    } else {
        return nullptr;
    }
}

#endif