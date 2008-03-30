/*
    SLB - Simple Lua Binder
    Copyright (C) 2007 Jose L. Hidalgo Valiño (PpluX)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	Jose L. Hidalgo (www.pplux.com)
	pplux@pplux.com
*/

#include <SLB/Table.hpp>
#include <SLB/Debug.hpp>

namespace SLB {

	Table::Table(const std::string &sep, bool c) : _cacheable(c), _separator(sep) {}
	Table::~Table() {}
		
	Object* Table::rawGet(const std::string &name)
	{
		Elements::iterator i = _elements.find(name);
		if (i == _elements.end())
		{
			SLB_DEBUG(10, "Access Table(%p) [%s] (FAILED!)", this, name.c_str());
			return 0;
		}
		SLB_DEBUG(10, "Access Table(%p) [%s] (OK!)", this, name.c_str());
		return i->second.get();
	}

	inline void Table::rawSet(const std::string &name, Object *obj)
	{
		if (obj == 0)
		{
			SLB_DEBUG(6, "Table (%p) remove '%s'", this, name.c_str());
			_elements.erase(name);
		}
		else
		{
			SLB_DEBUG(6, "Table (%p) [%s] = %p", this, name.c_str(), obj);
			_elements[name] = obj;		
		}
	}

	Object* Table::get(const std::string &name)
	{
		TableFind t = getTable(name, false);
		if (t.first != 0) return t.first->rawGet(t.second);
		return 0;
	}

	void Table::erase(const std::string &name)
	{
		set(name, 0);
	}

	void Table::setCache(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top < 2 ) luaL_error(L, "Not enough elements to perform Table::setCache");
		push(L); // push ::Table
		if (luaL_getmetafield(L,-1, "__indexCache"))
		{
			lua_insert(L, top - 1); // move the metatable above key,value
			lua_settop(L, top + 1); // remove everything else
			SLB_DEBUG_STACK(4, L, "Table(%p) :: setCache (before raw_set) ", this);
			lua_rawset(L,-3);
		}
		else
		{
			luaL_error(L, "Invalid setCache;  %s:%d", __FILE__, __LINE__ );
		}
		lua_settop(L, top - 2);
	}

	void Table::getCache(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top < 1 ) luaL_error(L, "Not enough elements to perform Table::setCache");
		push(L); // push ::Table
		if (luaL_getmetafield(L,-1, "__indexCache"))
		{
			lua_pushvalue(L, top); // copy value
			SLB_DEBUG_STACK(4, L, "Table(%p) :: getCache (before raw_get) ", this);
			lua_rawget(L, -2);
			lua_insert(L, top + 1); // as result
		}
		else
		{
			luaL_error(L, "Invalid setCache;  %s:%d", __FILE__, __LINE__ );
		}
		lua_settop(L, top + 1);
	}


	void Table::set(const std::string &name, Object *obj)
	{
		TableFind t = getTable(name, true);
		t.first->rawSet(t.second, obj);
	}

	Table::TableFind Table::getTable(const std::string &key, bool create)
	{
		if (_separator.empty()) return TableFind(this,key);

		std::string::size_type pos = key.find(_separator);
		if (pos != std::string::npos)
		{
			const std::string &base = key.substr(0, pos);
			const std::string &next = key.substr(pos+_separator.length());

			Table* subtable = dynamic_cast<Table*>(rawGet(base));
			
			if (subtable == 0)
			{
				if (create)
				{
					SLB_DEBUG(6, "Table (%p) create Subtable %s -> %s", this, 
						base.c_str(), next.c_str());

					subtable = new Table(_separator, _cacheable);
					rawSet(base,subtable);
				}
				else
				{
					return TableFind(0,key); // not found
				}
			}

			return subtable->getTable(next, create); //< TODO: recursivity... replace with iterative version.
		}
		// else :
		return TableFind(this,key);
	}
	
	int Table::__index(lua_State *L)
	{
		SLB_DEBUG_STACK(10,L,"Table::__index (%p)",this);
		int result = -1;
		
		if (_cacheable)
		{
			lua_pushvalue(L,2);
			lua_rawget(L, cacheTableIndex());
			if (lua_isnil(L,-1)) lua_pop(L,1); // remove nil
			else
			{
				//TODO tostring modifies the object, use a ifdef endif block here
				SLB_DEBUG(10, "Access Table(%p) [%s] (In CACHE)", this, lua_tostring(L,2));
				result = 1; // we found it
			}
		}

		if (result < 0)
		{
			if (lua_type(L, 2) == LUA_TSTRING)
			{
				Object *obj = get(lua_tostring(L,2));
				if (obj)
				{
					result = 1;
					obj->push(L);
					if (_cacheable)
					{
						// store in the cache...
						SLB_DEBUG(10, "L(%p) table(%p) key %s (->to cache)", L, this, lua_tostring(L,2));
						lua_pushvalue(L,2); // key
						lua_pushvalue(L,-2); // returned value
						lua_rawset(L, cacheTableIndex() ); // table
					}
				}
			}
		}

		return result;
	}
	
	int Table::__newindex(lua_State *L)
	{
		SLB_DEBUG_STACK(10,L,"Table::__newindex (%p)",this);
		luaL_error(L, "(%p)__newindex metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__call(lua_State *L)
	{
		SLB_DEBUG_STACK(10,L,"Table::__call (%p)",this);
		luaL_error(L, "(%p)__call metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__garbageCollector(lua_State *L)
	{
		SLB_DEBUG_STACK(10,L,"Table::__GC (%p)",this);
		luaL_error(L, "(%p) __gc metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__tostring(lua_State *L)
	{
		SLB_DEBUG_STACK(10,L,"Table::__tostring (%p)",this);
		int top = lua_gettop(L);
		lua_pushfstring(L, "Table(%p) [%s] with keys:", this, typeid(*this).name());
		for(Elements::iterator i = _elements.begin(); i != _elements.end(); ++i)
		{
			lua_pushfstring(L, "\n\t%s -> %p [%s]",i->first.c_str(), i->second.get(), typeid(*(i->second.get())).name());
		}
		lua_concat(L, lua_gettop(L) - top);
		return 1;
	}

	void Table::pushImplementation(lua_State *L)
	{
		lua_newtable(L); // an NEW empty table

		lua_newtable(L); // and its metatable:

		lua_newtable(L); // cache

		lua_pushvalue(L, -1); 
		lua_setfield(L, -3, "__indexCache");

		pushMeta(L, &Table::__indexProxy);
		lua_setfield(L,-3, "__index");
		pushMeta(L, &Table::__newindex);
		lua_setfield(L, -3, "__newindex");
		pushMeta(L, &Table::__tostring);
		lua_setfield(L, -3, "__tostring");
		pushMeta(L, &Table::__call);
		lua_setfield(L, -3, "__call");
		pushMeta(L, &Table::__garbageCollector);
		lua_setfield(L, -3, "__gc");

		lua_pop(L,1); // remove the cache table

		lua_setmetatable(L,-2);
	}

	int Table::__indexProxy(lua_State *L)
	{
		SLB_DEBUG(9, "---> __index search");
		SLB_DEBUG_STACK(10,L,"Table::__indexProxy (%p)",this);
		int result = __index(L);
		
		if (result < 0)
			luaL_error(L, "Table (%p) can not use '%s' as key", (void*)this, lua_typename(L, lua_type(L,2)));
		SLB_DEBUG(9, "<--- __index result = %d", result);
		return result;
	}

	
	int Table::__meta(lua_State *L)
	{
		// upvalue(1) is the cache table...
		Table *table = reinterpret_cast<Table*>(lua_touserdata(L, lua_upvalueindex(2)));
		TableMember member = *reinterpret_cast<TableMember*>(lua_touserdata(L, lua_upvalueindex(3)));
		return (table->*member)(L);
	}
	
	/** Pushmeta exepects a cache-table at the top, and creates a metamethod with 3 upvalues,
	 * first the cache table, second the current object, and last the member to call */
	void Table::pushMeta(lua_State *L, Table::TableMember member) const
	{
		assert("Invalid pushMeta, expected a table at the top (the cache-table)" &&
			lua_type(L,-1) == LUA_TTABLE);
		lua_pushvalue(L,-1); // copy of cache table
		lua_pushlightuserdata(L, (void*) this);
		void *p = lua_newuserdata(L, sizeof(TableMember)); 
		memcpy(p,&member,sizeof(TableMember));

		lua_pushcclosure(L, __meta, 3 );
	}
	


}
