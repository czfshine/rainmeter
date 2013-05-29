/*
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "StdAfx.h"
#include "MeasureScript.h"
#include "lua/LuaManager.h"
#include "Litestep.h"
#include "Rainmeter.h"

const char* g_InitializeFunctionName = "Initialize";
const char* g_UpdateFunctionName = "Update";
const char* g_GetStringFunctionName = "GetStringValue";

/*
** The constructor
**
*/
CMeasureScript::CMeasureScript(CMeterWindow* meterWindow, const WCHAR* name) : CMeasure(meterWindow, name),
	m_HasUpdateFunction(false),
	m_HasGetStringFunction(false),
	m_ValueType(LUA_TNIL)
{
	LuaManager::Initialize();
}

/*
** The destructor
**
*/
CMeasureScript::~CMeasureScript()
{
	UninitializeLuaScript();
	LuaManager::Finalize();
}

void CMeasureScript::UninitializeLuaScript()
{
	m_LuaScript.Uninitialize();

	m_HasUpdateFunction = false;
	m_HasGetStringFunction = false;
}

/*
** Runs the function "Update()" in the script.
**
*/
void CMeasureScript::UpdateValue()
{
	if (m_HasUpdateFunction)
	{
		m_ValueType = m_LuaScript.RunFunctionWithReturn(g_UpdateFunctionName, m_Value, m_StringValue);

		if (m_ValueType == LUA_TNIL && m_HasGetStringFunction)
		{
			// For backwards compatbility
			m_ValueType = m_LuaScript.RunFunctionWithReturn(g_GetStringFunctionName, m_Value, m_StringValue);
		}
	}
}

/*
** Returns the value as a string.
**
*/
const WCHAR* CMeasureScript::GetStringValue()
{
	return (m_ValueType == LUA_TSTRING) ? CheckSubstitute(m_StringValue.c_str()) : NULL;
}

/*
** Read the options specified in the ini file.
**
*/
void CMeasureScript::ReadOptions(CConfigParser& parser, const WCHAR* section)
{
	CMeasure::ReadOptions(parser, section);

	std::wstring scriptFile = parser.ReadString(section, L"ScriptFile", L"");
	if (!scriptFile.empty())
	{
		if (m_MeterWindow)
		{
			m_MeterWindow->MakePathAbsolute(scriptFile);
		}

		if (!m_Initialized ||
			wcscmp(scriptFile.c_str(), m_LuaScript.GetFile().c_str()) != 0)
		{
			UninitializeLuaScript();

			lua_State* L = LuaManager::GetState();
			if (m_LuaScript.Initialize(scriptFile))
			{
				bool hasInitializeFunction = m_LuaScript.IsFunction(g_InitializeFunctionName);
				m_HasUpdateFunction = m_LuaScript.IsFunction(g_UpdateFunctionName);
				m_HasGetStringFunction = m_LuaScript.IsFunction(g_GetStringFunctionName);  // For backwards compatbility

				if (m_HasGetStringFunction)
				{
					CLogger_WarningF(L"Script: Using deprecated GetStringValue() in [%s]", m_Name.c_str());
				}

				lua_rawgeti(L, LUA_GLOBALSINDEX, m_LuaScript.GetRef());

				*(CMeterWindow**)lua_newuserdata(L, sizeof(CMeterWindow*)) = m_MeterWindow;
				lua_getglobal(L, "CMeterWindow");
				lua_setmetatable(L, -2);
				lua_setfield(L, -2, "SKIN");

				*(CMeasure**)lua_newuserdata(L, sizeof(CMeasure*)) = this;
				lua_getglobal(L, "CMeasure");
				lua_setmetatable(L, -2);
				lua_setfield(L, -2, "SELF");

				// For backwards compatibility
				lua_getfield(L, -1, "PROPERTIES");
				if (lua_isnil(L, -1) == 0)
				{
					lua_pushnil(L);
					
					// Look in the table for values to read from the section
					while (lua_next(L, -2))
					{
						lua_pop(L, 1);
						const char* strKey = lua_tostring(L, -1);

						std::wstring wstrKey = StringUtil::Widen(strKey);
						const std::wstring& wstrValue = parser.ReadString(section, wstrKey.c_str(), L"");

						if (!wstrValue.empty())
						{
							std::string strStrVal = StringUtil::Narrow(wstrValue);
							const char* strValue = strStrVal.c_str();

							lua_pushstring(L, strValue);
							lua_setfield(L, -3, strKey);
						}
					}
				}

				// Pop PROPERTIES table and our table
				lua_pop(L, 2);

				if (hasInitializeFunction)
				{
					m_LuaScript.RunFunction(g_InitializeFunctionName);
				}

				// Valid script.
				return;
			}
		}
		else if (m_LuaScript.IsInitialized())
		{
			// Already initialized.
			return;
		}
	}

	CLogger_ErrorF(L"Script: File not valid in [%s]", m_Name.c_str());
	UninitializeLuaScript();
}

/*
** Executes a custom bang.
**
*/
void CMeasureScript::Command(const std::wstring& command)
{
	std::string str = StringUtil::Narrow(command);
	m_LuaScript.RunString(str.c_str());
}

//static void stackDump(lua_State *L)
//{
//	LuaManager::LuaLogger::Debug(" ----------------  Stack Dump ----------------" );
//	for (int i = lua_gettop(L); i > 0; --i)
//	{
//		int t = lua_type(L, i);
//		switch (t)
//		{
//		case LUA_TSTRING:
//			LuaManager::LuaLogger::Debug("%d:'%s'", i, lua_tostring(L, i));
//			break;
//
//		case LUA_TBOOLEAN:
//			LuaManager::LuaLogger::Debug("%d: %s", i, lua_toboolean(L, i) ? "true" : "false");
//			break;
//
//		case LUA_TNUMBER:
//			LuaManager::LuaLogger::Debug("%d: %g", i, lua_tonumber(L, i));
//			break;
//
//		default:
//			LuaManager::LuaLogger::Debug("%d: %s", i, lua_typename(L, t));
//			break;
//		}
//	}
//	LuaManager::LuaLogger::Debug("--------------- Stack Dump Finished ---------------" );
//}
