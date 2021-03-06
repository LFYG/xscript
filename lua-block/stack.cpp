#include "settings.h"

#include "stack.h"
#include "exception.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

void
luaCheckNumber(lua_State *lua, int index) {
    if (!lua_isnumber(lua, index)) {
        throw BadType("number", index);
    }
}

void
luaCheckString(lua_State *lua, int index) {
    if (!lua_isstring(lua, index)) {
        throw BadType("string", index);
    }
}

void
luaCheckBoolean(lua_State *lua, int index) {
    if (!lua_isboolean(lua, index)) {
        throw BadType("boolean", index);
    }
}

void
luaCheckTable(lua_State *lua, int index) {
    if (!lua_istable(lua, index)) {
        throw BadType("table", index);
    }
}

bool
luaIsArrayTable(lua_State *lua, int index) {
    luaCheckTable(lua, index);
    bool res = true;
    lua_pushnil(lua);
    if (lua_next(lua, index)) {
        if (!lua_isnumber(lua, -2)) {
            res = false;
        }
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    return res;
}

bool
luaIsNil(lua_State *lua, int index) {
    return lua_isnil(lua, index);
}

int
luaCheckStackSize(lua_State *lua, int count_min, int count_max) {
    int count = lua_gettop(lua);
    if (count < count_min || count > count_max) {
        throw BadArgCount(count);
    }
    return count;
}

void
luaCheckStackSize(lua_State *lua, int count) {
    luaCheckStackSize(lua, count, count);
}

void*
luaCheckUserData(lua_State *lua, const char *name, int index) {
    void *ptr = luaL_checkudata(lua, index, name);
    if (NULL == ptr) {
        throw BadType(name, index);
    }
    luaL_argcheck(lua, ptr != NULL, index, "`ud' expected");
    return ptr;

}

static bool
isLongInteger(double d) {
    if (d > (double)std::numeric_limits<boost::int64_t>::max() ||
        d < (double)std::numeric_limits<boost::int64_t>::min()) {
        return false;
    }
    if (d - (boost::int64_t)d > std::numeric_limits<double>::epsilon() ||
        d - (boost::int64_t)d < -std::numeric_limits<double>::epsilon()) {
        return false;
    }
    return true;
}

static TypedValue
luaReadTableInternal(lua_State *lua, int index) {
    lua_pushnil(lua);
    TypedValue value = TypedValue::createArrayValue();
    bool is_map  = false;
    bool is_first  = true;
    while (lua_next(lua, index)) {
        if (is_first) {
            is_first = false;
            is_map = !lua_isnumber(lua, -2);
            if (is_map) {
                value = TypedValue::createMapValue();
            }
        }
        std::string key;
        if (is_map) {
            key.assign(lua_tostring(lua, -2));
        }
        switch (lua_type(lua, -1)) {
            case LUA_TNIL:
                value.add(key, TypedValue());
                break;
            case LUA_TBOOLEAN:
                value.add(key, TypedValue(0 != lua_toboolean(lua, -1)));
                break;
            case LUA_TNUMBER: {
                double d = lua_tonumber(lua, -1);
                isLongInteger(d) ? value.add(key, TypedValue((boost::int64_t)d)) :
                    value.add(key, TypedValue(d));
                break;
            }
            case LUA_TSTRING:
                value.add(key, TypedValue(std::string(lua_tostring(lua, -1))));
                break;
            case LUA_TTABLE:
                value.add(key, luaReadTableInternal(lua, lua_gettop(lua)));
                break;
            default:
                throw BadType("nil, bool, number, string or table", -1);
        }
        lua_pop(lua, 1);
    }
    return value;
}

TypedValue
luaReadTable(lua_State *lua, int index) {
    luaCheckTable(lua, index);
    return luaReadTableInternal(lua, index);
}

} // namespace xscript
