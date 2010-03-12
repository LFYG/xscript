#ifndef _XSCRIPT_LUA_XSCRIPT_METHODS_H_
#define _XSCRIPT_LUA_XSCRIPT_METHODS_H_

#include <lua.hpp>

namespace xscript {

class Block;

void setupXScript(lua_State * lua, std::string * buf, Block *block);

} // namespace xscript

#endif // _XSCRIPT_LUA_XSCRIPT_METHODS_H_

