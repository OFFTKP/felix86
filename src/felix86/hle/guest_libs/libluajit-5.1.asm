bits 64

section .text

global luaL_typerror:function
align 16
luaL_typerror:
invlpg [rax]
db "luaL_typerror", 0
ret

global luaL_register:function
align 16
luaL_register:
invlpg [rax]
db "luaL_register", 0
ret

global lua_getfield:function
align 16
lua_getfield:
invlpg [rax]
db "lua_getfield", 0
ret

global lua_pushvfstring:function
align 16
lua_pushvfstring:
invlpg [rax]
db "lua_pushvfstring", 0
ret

global luaL_pushmodule:function
align 16
luaL_pushmodule:
invlpg [rax]
db "luaL_pushmodule", 0
ret

global luaL_pushresult:function
align 16
luaL_pushresult:
invlpg [rax]
db "luaL_pushresult", 0
ret

global lua_xmove:function
align 16
lua_xmove:
invlpg [rax]
db "lua_xmove", 0
ret

global lua_pushthread:function
align 16
lua_pushthread:
invlpg [rax]
db "lua_pushthread", 0
ret

global luaL_openlib:function
align 16
luaL_openlib:
invlpg [rax]
db "luaL_openlib", 0
ret

global luaL_addlstring:function
align 16
luaL_addlstring:
invlpg [rax]
db "luaL_addlstring", 0
ret

global lua_resume:function
align 16
lua_resume:
invlpg [rax]
db "lua_resume", 0
ret

global luaL_optnumber:function
align 16
luaL_optnumber:
invlpg [rax]
db "luaL_optnumber", 0
ret

global lua_setfield:function
align 16
lua_setfield:
invlpg [rax]
db "lua_setfield", 0
ret

global lua_newuserdata:function
align 16
lua_newuserdata:
invlpg [rax]
db "lua_newuserdata", 0
ret

global luaJIT_profile_stop:function
align 16
luaJIT_profile_stop:
invlpg [rax]
db "luaJIT_profile_stop", 0
ret

global lua_gettable:function
align 16
lua_gettable:
invlpg [rax]
db "lua_gettable", 0
ret

global lua_gethookcount:function
align 16
lua_gethookcount:
invlpg [rax]
db "lua_gethookcount", 0
ret

global lua_settop:function
align 16
lua_settop:
invlpg [rax]
db "lua_settop", 0
ret

global lua_rawequal:function
align 16
lua_rawequal:
invlpg [rax]
db "lua_rawequal", 0
ret

global lua_pushstring:function
align 16
lua_pushstring:
invlpg [rax]
db "lua_pushstring", 0
ret

global luaL_execresult:function
align 16
luaL_execresult:
invlpg [rax]
db "luaL_execresult", 0
ret

global lua_dump:function
align 16
lua_dump:
invlpg [rax]
db "lua_dump", 0
ret

global luaL_where:function
align 16
luaL_where:
invlpg [rax]
db "luaL_where", 0
ret

global lua_error:function
align 16
lua_error:
invlpg [rax]
db "lua_error", 0
ret

global lua_settable:function
align 16
lua_settable:
invlpg [rax]
db "lua_settable", 0
ret

global luaL_loadfilex:function
align 16
luaL_loadfilex:
invlpg [rax]
db "luaL_loadfilex", 0
ret

global lua_getinfo:function
align 16
lua_getinfo:
invlpg [rax]
db "lua_getinfo", 0
ret

global lua_isnumber:function
align 16
lua_isnumber:
invlpg [rax]
db "lua_isnumber", 0
ret

global lua_equal:function
align 16
lua_equal:
invlpg [rax]
db "lua_equal", 0
ret

global lua_upvalueid:function
align 16
lua_upvalueid:
invlpg [rax]
db "lua_upvalueid", 0
ret

global lua_next:function
align 16
lua_next:
invlpg [rax]
db "lua_next", 0
ret

global luaL_checklstring:function
align 16
luaL_checklstring:
invlpg [rax]
db "luaL_checklstring", 0
ret

global lua_topointer:function
align 16
lua_topointer:
invlpg [rax]
db "lua_topointer", 0
ret

global lua_tothread:function
align 16
lua_tothread:
invlpg [rax]
db "lua_tothread", 0
ret

global lua_gc:function
align 16
lua_gc:
invlpg [rax]
db "lua_gc", 0
ret

global lua_createtable:function
align 16
lua_createtable:
invlpg [rax]
db "lua_createtable", 0
ret

global lua_setfenv:function
align 16
lua_setfenv:
invlpg [rax]
db "lua_setfenv", 0
ret

global lua_pcall:function
align 16
lua_pcall:
invlpg [rax]
db "lua_pcall", 0
ret

global luaL_unref:function
align 16
luaL_unref:
invlpg [rax]
db "luaL_unref", 0
ret

global lua_call:function
align 16
lua_call:
invlpg [rax]
db "lua_call", 0
ret

global luaL_testudata:function
align 16
luaL_testudata:
invlpg [rax]
db "luaL_testudata", 0
ret

global lua_type:function
align 16
lua_type:
invlpg [rax]
db "lua_type", 0
ret

global luaL_newmetatable:function
align 16
luaL_newmetatable:
invlpg [rax]
db "luaL_newmetatable", 0
ret

global lua_concat:function
align 16
lua_concat:
invlpg [rax]
db "lua_concat", 0
ret

global lua_gethookmask:function
align 16
lua_gethookmask:
invlpg [rax]
db "lua_gethookmask", 0
ret

global lua_lessthan:function
align 16
lua_lessthan:
invlpg [rax]
db "lua_lessthan", 0
ret

global lua_tocfunction:function
align 16
lua_tocfunction:
invlpg [rax]
db "lua_tocfunction", 0
ret

global luaL_loadbuffer:function
align 16
luaL_loadbuffer:
invlpg [rax]
db "luaL_loadbuffer", 0
ret

global lua_getmetatable:function
align 16
lua_getmetatable:
invlpg [rax]
db "lua_getmetatable", 0
ret

global luaL_loadstring:function
align 16
luaL_loadstring:
invlpg [rax]
db "luaL_loadstring", 0
ret

global lua_pushcclosure:function
align 16
lua_pushcclosure:
invlpg [rax]
db "lua_pushcclosure", 0
ret

global luaL_checkudata:function
align 16
luaL_checkudata:
invlpg [rax]
db "luaL_checkudata", 0
ret

global lua_gethook:function
align 16
lua_gethook:
invlpg [rax]
db "lua_gethook", 0
ret

global lua_newstate:function
align 16
lua_newstate:
invlpg [rax]
db "lua_newstate", 0
ret

global lua_getupvalue:function
align 16
lua_getupvalue:
invlpg [rax]
db "lua_getupvalue", 0
ret

global luaL_optlstring:function
align 16
luaL_optlstring:
invlpg [rax]
db "luaL_optlstring", 0
ret

global luaJIT_setmode:function
align 16
luaJIT_setmode:
invlpg [rax]
db "luaJIT_setmode", 0
ret

global luaL_newstate:function
align 16
luaL_newstate:
invlpg [rax]
db "luaL_newstate", 0
ret

global lua_pushnil:function
align 16
lua_pushnil:
invlpg [rax]
db "lua_pushnil", 0
ret

global luaL_gsub:function
align 16
luaL_gsub:
invlpg [rax]
db "luaL_gsub", 0
ret

global lua_getfenv:function
align 16
lua_getfenv:
invlpg [rax]
db "lua_getfenv", 0
ret

global luaL_findtable:function
align 16
luaL_findtable:
invlpg [rax]
db "luaL_findtable", 0
ret

global lua_objlen:function
align 16
lua_objlen:
invlpg [rax]
db "lua_objlen", 0
ret

global lua_remove:function
align 16
lua_remove:
invlpg [rax]
db "lua_remove", 0
ret

global lua_rawseti:function
align 16
lua_rawseti:
invlpg [rax]
db "lua_rawseti", 0
ret

global lua_close:function
align 16
lua_close:
invlpg [rax]
db "lua_close", 0
ret

global lua_pushlightuserdata:function
align 16
lua_pushlightuserdata:
invlpg [rax]
db "lua_pushlightuserdata", 0
ret

global luaL_addstring:function
align 16
luaL_addstring:
invlpg [rax]
db "luaL_addstring", 0
ret

global luaL_checkinteger:function
align 16
luaL_checkinteger:
invlpg [rax]
db "luaL_checkinteger", 0
ret

global lua_rawset:function
align 16
lua_rawset:
invlpg [rax]
db "lua_rawset", 0
ret

global luaL_loadbufferx:function
align 16
luaL_loadbufferx:
invlpg [rax]
db "luaL_loadbufferx", 0
ret

global luaL_checknumber:function
align 16
luaL_checknumber:
invlpg [rax]
db "luaL_checknumber", 0
ret

global lua_rawgeti:function
align 16
lua_rawgeti:
invlpg [rax]
db "lua_rawgeti", 0
ret

global luaL_loadfile:function
align 16
luaL_loadfile:
invlpg [rax]
db "luaL_loadfile", 0
ret

global luaL_addvalue:function
align 16
luaL_addvalue:
invlpg [rax]
db "luaL_addvalue", 0
ret

global lua_setupvalue:function
align 16
lua_setupvalue:
invlpg [rax]
db "lua_setupvalue", 0
ret

global luaL_setfuncs:function
align 16
luaL_setfuncs:
invlpg [rax]
db "luaL_setfuncs", 0
ret

global luaL_setmetatable:function
align 16
luaL_setmetatable:
invlpg [rax]
db "luaL_setmetatable", 0
ret

global luaL_callmeta:function
align 16
luaL_callmeta:
invlpg [rax]
db "luaL_callmeta", 0
ret

global lua_gettop:function
align 16
lua_gettop:
invlpg [rax]
db "lua_gettop", 0
ret

global lua_replace:function
align 16
lua_replace:
invlpg [rax]
db "lua_replace", 0
ret

global luaL_prepbuffer:function
align 16
luaL_prepbuffer:
invlpg [rax]
db "luaL_prepbuffer", 0
ret

global lua_cpcall:function
align 16
lua_cpcall:
invlpg [rax]
db "lua_cpcall", 0
ret

global lua_getlocal:function
align 16
lua_getlocal:
invlpg [rax]
db "lua_getlocal", 0
ret

global lua_pushvalue:function
align 16
lua_pushvalue:
invlpg [rax]
db "lua_pushvalue", 0
ret

global luaL_traceback:function
align 16
luaL_traceback:
invlpg [rax]
db "luaL_traceback", 0
ret

global lua_tonumberx:function
align 16
lua_tonumberx:
invlpg [rax]
db "lua_tonumberx", 0
ret

global lua_pushnumber:function
align 16
lua_pushnumber:
invlpg [rax]
db "lua_pushnumber", 0
ret

global luaL_ref:function
align 16
luaL_ref:
invlpg [rax]
db "luaL_ref", 0
ret

global lua_newthread:function
align 16
lua_newthread:
invlpg [rax]
db "lua_newthread", 0
ret

global lua_isstring:function
align 16
lua_isstring:
invlpg [rax]
db "lua_isstring", 0
ret

global lua_iscfunction:function
align 16
lua_iscfunction:
invlpg [rax]
db "lua_iscfunction", 0
ret

global lua_touserdata:function
align 16
lua_touserdata:
invlpg [rax]
db "lua_touserdata", 0
ret

global lua_isyieldable:function
align 16
lua_isyieldable:
invlpg [rax]
db "lua_isyieldable", 0
ret

global lua_tolstring:function
align 16
lua_tolstring:
invlpg [rax]
db "lua_tolstring", 0
ret

global lua_setlocal:function
align 16
lua_setlocal:
invlpg [rax]
db "lua_setlocal", 0
ret

global luaL_checkany:function
align 16
luaL_checkany:
invlpg [rax]
db "luaL_checkany", 0
ret

global lua_checkstack:function
align 16
lua_checkstack:
invlpg [rax]
db "lua_checkstack", 0
ret

global lua_getallocf:function
align 16
lua_getallocf:
invlpg [rax]
db "lua_getallocf", 0
ret

global luaL_optinteger:function
align 16
luaL_optinteger:
invlpg [rax]
db "luaL_optinteger", 0
ret

global luaL_checktype:function
align 16
luaL_checktype:
invlpg [rax]
db "luaL_checktype", 0
ret

global lua_version:function
align 16
lua_version:
invlpg [rax]
db "lua_version", 0
ret

global lua_rawget:function
align 16
lua_rawget:
invlpg [rax]
db "lua_rawget", 0
ret

global lua_copy:function
align 16
lua_copy:
invlpg [rax]
db "lua_copy", 0
ret

global lua_load:function
align 16
lua_load:
invlpg [rax]
db "lua_load", 0
ret

global luaL_checkstack:function
align 16
luaL_checkstack:
invlpg [rax]
db "luaL_checkstack", 0
ret

global lua_pushlstring:function
align 16
lua_pushlstring:
invlpg [rax]
db "lua_pushlstring", 0
ret

global lua_toboolean:function
align 16
lua_toboolean:
invlpg [rax]
db "lua_toboolean", 0
ret

global lua_insert:function
align 16
lua_insert:
invlpg [rax]
db "lua_insert", 0
ret

global lua_tointegerx:function
align 16
lua_tointegerx:
invlpg [rax]
db "lua_tointegerx", 0
ret

global luaL_checkoption:function
align 16
luaL_checkoption:
invlpg [rax]
db "luaL_checkoption", 0
ret

global luaL_fileresult:function
align 16
luaL_fileresult:
invlpg [rax]
db "luaL_fileresult", 0
ret

global luaJIT_profile_start:function
align 16
luaJIT_profile_start:
invlpg [rax]
db "luaJIT_profile_start", 0
ret

global lua_tonumber:function
align 16
lua_tonumber:
invlpg [rax]
db "lua_tonumber", 0
ret

global lua_typename:function
align 16
lua_typename:
invlpg [rax]
db "lua_typename", 0
ret

global luaL_buffinit:function
align 16
luaL_buffinit:
invlpg [rax]
db "luaL_buffinit", 0
ret

global luaJIT_profile_dumpstack:function
align 16
luaJIT_profile_dumpstack:
invlpg [rax]
db "luaJIT_profile_dumpstack", 0
ret

global lua_tointeger:function
align 16
lua_tointeger:
invlpg [rax]
db "lua_tointeger", 0
ret

global lua_pushboolean:function
align 16
lua_pushboolean:
invlpg [rax]
db "lua_pushboolean", 0
ret

global lua_status:function
align 16
lua_status:
invlpg [rax]
db "lua_status", 0
ret

global lua_yield:function
align 16
lua_yield:
invlpg [rax]
db "lua_yield", 0
ret

global luaL_openlibs:function
align 16
luaL_openlibs:
invlpg [rax]
db "luaL_openlibs", 0
ret

global luaL_getmetafield:function
align 16
luaL_getmetafield:
invlpg [rax]
db "luaL_getmetafield", 0
ret

global lua_upvaluejoin:function
align 16
lua_upvaluejoin:
invlpg [rax]
db "lua_upvaluejoin", 0
ret

global lua_setallocf:function
align 16
lua_setallocf:
invlpg [rax]
db "lua_setallocf", 0
ret

global lua_setmetatable:function
align 16
lua_setmetatable:
invlpg [rax]
db "lua_setmetatable", 0
ret

global lua_loadx:function
align 16
lua_loadx:
invlpg [rax]
db "lua_loadx", 0
ret

global lua_isuserdata:function
align 16
lua_isuserdata:
invlpg [rax]
db "lua_isuserdata", 0
ret

global lua_pushinteger:function
align 16
lua_pushinteger:
invlpg [rax]
db "lua_pushinteger", 0
ret

global lua_getstack:function
align 16
lua_getstack:
invlpg [rax]
db "lua_getstack", 0
ret

global luaL_argerror:function
align 16
luaL_argerror:
invlpg [rax]
db "luaL_argerror", 0
ret

global lua_atpanic:function
align 16
lua_atpanic:
invlpg [rax]
db "lua_atpanic", 0
ret


global luaopen_io:function
align 16
luaopen_io:
invlpg [rax]
db "luaopen_io", 0
ret

global luaopen_math:function
align 16
luaopen_math:
invlpg [rax]
db "luaopen_math", 0
ret

global luaopen_table:function
align 16
luaopen_table:
invlpg [rax]
db "luaopen_table", 0
ret

global luaopen_string:function
align 16
luaopen_string:
invlpg [rax]
db "luaopen_string", 0
ret

global luaopen_os:function
align 16
luaopen_os:
invlpg [rax]
db "luaopen_os", 0
ret

global luaopen_string_buffer:function
align 16
luaopen_string_buffer:
invlpg [rax]
db "luaopen_string_buffer", 0
ret

global luaopen_ffi:function
align 16
luaopen_ffi:
invlpg [rax]
db "luaopen_ffi", 0
ret

global luaopen_package:function
align 16
luaopen_package:
invlpg [rax]
db "luaopen_package", 0
ret

global luaopen_debug:function
align 16
luaopen_debug:
invlpg [rax]
db "luaopen_debug", 0
ret

global luaopen_bit:function
align 16
luaopen_bit:
invlpg [rax]
db "luaopen_bit", 0
ret

global luaopen_jit:function
align 16
luaopen_jit:
invlpg [rax]
db "luaopen_jit", 0
ret

global luaopen_base:function
align 16
luaopen_base:
invlpg [rax]
db "luaopen_base", 0
ret
