// ==========================================================
// alterIWnet project
// 
// Component: aiw_client
// Sub-component: steam_api
// Purpose: Functionality to interact with the GameScript 
//          runtime.
//
// Initial author: NTAuthority
// Adapted: 2015-07-21
// Started: 2011-12-19
// ==========================================================
#include "StdInc.h"
#include "t4_headers.h"
#include "T4.h"

#include "safetyhook.hpp"

#include "MemoryMgr.h"

dvar_t* developer_funcdump;

// COD5R HUD stuff
dvar_t* cg_drawHealthCount;
dvar_t* cg_drawHealthCountCoop;
dvar_t* cg_drawGamepadHUD;
dvar_t* cg_drawDpadLogos;
dvar_t* cg_SoloScoreColorWhite;
dvar_t* cg_drawTimers;
dvar_t* cg_drawTrapTimers;

dvar_t* cg_lowerGun; // lower gun 1st person
dvar_t* zombiemode_dev; // experimental COD5R features

// these below are all fixable within gsc, but i'll add them in for whoever wants to use them, same names as Pluto but im not sure if this implementation is 1:1 with pluto
dvar_t* g_disable_zombie_grab;

dvar_t* g_fix_tesla_bug;

dvar_t* g_fix_health_sets_max;



// custom functions
typedef struct
{
	const char* functionName;
	scr_function_t functionCall;
	int developerOnly;
} scr_funcdef_t;


namespace T4M
{
	const WeaponDef* __cdecl BG_GetWeaponDef(unsigned int weaponIndex)
	{
		return T4::engine::bg_weaponDefs[weaponIndex];
	}

	bool __cdecl BG_IsOverheatingWeapon(unsigned int weapIndex)
	{
		return BG_GetWeaponDef(weapIndex)->overheatWeapon != 0;
	}

	// Not faithful reconstruction of the original BG_GetWeaponIndexForName
	// Has dead code via ; on the if
	// Return should be a pointer not a int
	// But use some original function
	int BG_GetWeaponIndexForName(const char* name)
	{
		if (*(bool*)T4M::GetAddress("dword_18F6DB8"));
		return ((int(__cdecl*)(const char*, void*))T4M::GetAddress("BG_FindWeaponIndex_Internal"))(name, (void*)T4M::GetAddress("BG_LoadWeaponByIndex"));

		return ((int(__cdecl*)(const char*))T4M::GetAddress("BG_FindWeaponIndex"))(name);
	}

#pragma region setupFunctions
	static std::map<std::string, scr_funcdef_t> scriptFunctions;
	static std::map<std::string, scr_funcdef_t> scriptMethods;

	scr_function_t Scr_GetCustomFunction(const char** name, int* isDeveloper)
	{
		scr_funcdef_t func = scriptFunctions[*name];

		if (func.functionName)
		{
			*name = func.functionName;
			*isDeveloper = func.developerOnly;

			return func.functionCall;
		}
		else
			return NULL;
	}

	scr_function_t Scr_GetCustomMethod(const char** name, int* type)
	{
		scr_funcdef_t method = scriptMethods[*name];

		if (method.functionName)
		{
			*name = method.functionName;
			*type = 0;
			return method.functionCall;
		}
		return NULL;
	}


	void Scr_DeclareFunction(const char* name, scr_function_t func, bool developerOnly = false)
	{
		scr_funcdef_t funcDef;
		funcDef.functionName = name;
		funcDef.functionCall = func;
		funcDef.developerOnly = (developerOnly) ? 1 : 0;

		scriptFunctions[name] = funcDef;
	}

	void Scr_DeclareMethod(const char* name, scr_function_t func)
	{
		scr_funcdef_t methodDef;
		methodDef.functionName = name;
		methodDef.functionCall = func;
		methodDef.developerOnly = 0;
		scriptMethods[name] = methodDef;
	}
#pragma endregion setupFunctions

#pragma region engineFunctions
	int __cdecl Scr_GetNumParam(scriptInstance_t inst)
	{
		DWORD* value = (DWORD*)T4M::GetAddress("value"); // getNumParamArray location
		return value[4298 * inst];
	}

	RefString* __cdecl GetRefString(scriptInstance_t inst, unsigned int stringValue)
	{
		// 0x3702390 holds gScrMemTreePub.mt_buffer (a pointer to the string buffer);
		// must deref it — the old &(&local)[...] read the stack (bug).
		DWORD* mt_buffer = (DWORD*)T4::engine::gScrMemTreePub->mt_buffer;
		return (RefString*)&mt_buffer[3 * stringValue + 1];
	}

	char* __cdecl SL_ConvertToString(unsigned int stringValue, scriptInstance_t inst)
	{
		char* v3;
		if (stringValue)
			v3 = T4M::GetRefString(inst, stringValue)->str;
		else
			v3 = 0;
		return v3;
	}

	void Scr_ClearOutParams(scriptInstance_t v1)
	{
		static DWORD dwCall = T4M::GetAddress("Scr_ClearOutParams");

		__asm
		{
			mov edi, v1
			call[dwCall]
		}
	}

	void __cdecl IncInParam(scriptInstance_t inst)
	{
		T4M::Scr_ClearOutParams(inst);

		// TO-DO: define sys_error
		//if (dword_A05AC98[4298 * inst] == dword_A05AC8C[4298 * inst])
		//	Sys_Error("Internal script stack overflow");
		((DWORD*)T4M::GetAddress("g_scrOutParamStackPtr"))[4296 * inst] += 8;
		++((DWORD*)T4M::GetAddress("g_scrOutParamCount"))[4296 * inst];
	}

	void __cdecl Scr_AddInt(int value, scriptInstance_t inst)
	{
		T4M::IncInParam(inst);
		*(DWORD*)((((DWORD*)T4M::GetAddress("g_scrVarStack"))[4296 * value]) + 4) = 6;
		*(DWORD*)(((DWORD*)T4M::GetAddress("g_scrVarStack"))[4296 * value]) = value;
	}
#pragma endregion engineFunctions

#pragma region engineHKFunctions
	void(__cdecl* __cdecl Scr_GetFunction_Hook(const char** pName, int* type))()
	{
		// this is aids and I don't care
		// also if running debugger and a customf func is executed the debugger will instadie
		void(__cdecl * function)();
		// check if the function passed is part of our custom funcs
		if (!(scriptFunctions.find(std::string(*pName)) != scriptFunctions.end()))
			function = (void(__cdecl*)())T4::engine::Scr_GetFunction(pName, type);
		else
			function = (void(__cdecl*)())Scr_GetCustomFunction(pName, type);

		if (developer_funcdump->current.boolean && function != 0)
			T4::engine::Com_Printf(0, "[GSC] Function: %s\nType: %i\nAddress: 0x%X\n\n", *pName, *type, function);

		return function;
	}

	extern "C" int Scr_GetMethod(int* type, const char** pName)
	{
		// also aids and again do not care. #3
		int function;

		*type = 0;
		function = T4::game::Player_GetMethod(pName);

		if (!function)
		{
			function = T4::game::ScriptEnt_GetMethod(pName);
			if (!function)
			{
				function = T4::game::ScriptVehicle_GetMethod(pName);
				if (!function)
				{
					function = T4::game::HudElem_GetMethod(pName);
					if (!function)
					{
						function = T4::game::Helicopter_GetMethod(pName);
						if (!function)
						{
							function = T4::game::Actor_GetMethod(pName);
							if (!function)
							{
								function = T4::game::BuiltIn_GetMethod(pName, type);
								if (!function)
									function = (int)Scr_GetCustomMethod(pName, type);
							}
						}
					}
				}
			}
		}

		if (developer_funcdump->current.boolean && function != 0)
			T4::engine::Com_Printf(0, "[GSC] Method: %s\nType: %i\nAddress: 0x%X\n\n", *pName, *type, function);

		return function;
	}



	void PlayerWeaponOverheatUpdate(gentity_s* ent, uint32_t weapon_index, float amount)
	{
		if (!T4M::BG_IsOverheatingWeapon(weapon_index))
			return;
		auto weapon = T4M::BG_GetWeaponDef(weapon_index);
		float& current_heat = ent->client->ps.heatpercent[weapon->iHeatIndex];
		bool& current_overheat = ent->client->ps.overheating[weapon->iHeatIndex];
		current_heat = amount;
		if (amount == 0.f)
			current_overheat = false;
	}


	int __cdecl Scr_GetInt(scriptInstance_t inst, unsigned int index);
	void SetLowReadyIntent(playerState_s* ps, bool enable);

	void __declspec(naked) Scr_GetMethod_Hook(int* type, const char** pName)
	{
		__asm
		{
			push esi // pName
			push edi // type
			call T4::game::Scr_GetMethod
			add esp, 8
			retn
		}
	}

	static uintptr_t Scr_GetString_addr = NULL;
	uintptr_t __declspec(naked) Scr_GetString_asm(uint32_t index, scriptInstance_t instance)
	{
		__asm
		{
			push ebp
			mov ebp, esp
			sub esp, __LOCAL_SIZE


			push eax
			push esi

			mov eax, index
			mov esi, instance

			call Scr_GetString_addr

			pop esi
			pop eax

			mov esp, ebp
			pop ebp
			ret
		}
	}

	const char* Scr_GetString(uint32_t index, scriptInstance_t instance)
	{
		return T4::engine::Scr_GetString(index, instance);
	}

	void(__cdecl* __cdecl CScr_GetFunction_Hook(const char** pName, int* type))()
	{
		// this is aids and I don't care #2
		// also if running debugger and a customf func is executed the debugger will instadie
		void(__cdecl * function)();

		function = (void(__cdecl*)())T4::engine::CScr_GetFunction(pName, type);

		if (developer_funcdump->current.boolean && function != 0)
			T4::engine::Com_Printf(0, "[CSC] Function: %s\nType: %i\nAddress: 0x%X\n\n", *pName, *type, function);

		return function;
	}

	void(__cdecl* __cdecl CScr_GetMethod_Hook(const char** pName, int* type))()
	{
		// aids #3 woo!
		void(__cdecl * function)();

		function = (void(__cdecl*)())T4::engine::CScr_GetMethod(pName, type);

		if (developer_funcdump->current.boolean && function != 0)
			T4::engine::Com_Printf(0, "[CSC] Method: %s\nType: %i\nAddress: 0x%X\n\n", *pName, *type, function);

		return function;
	}
#pragma endregion engineHKFunctions

	int __cdecl Scr_GetInt(scriptInstance_t inst, unsigned int index)
	{
		static DWORD func = T4M::GetAddress("Scr_GetInt");
		int result;
		__asm
		{
			mov eax, inst
			mov ecx, index
			call func
			mov result, eax
		}
		return result;
	}

	// for zombies this is applied on each zombie spawn in _spawner
	int __stdcall DisablePushPlayer() {

		if ((T4M::isZombieMode() && g_disable_zombie_grab->current.integer == 1) || g_disable_zombie_grab->current.integer >= 2)
			return 0;
		else
			return T4M::Scr_GetInt(SCRIPTINSTANCE_SERVER, 0);

	}


#pragma region customFunctions
	void GScr_PrintLnConsole(scr_entref_t entity)
	{
		// gets amount of parameters
		if (T4M::Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1)
			T4::engine::Com_Printf(0, "^3Have one!\n");
		else
			T4::engine::Com_Printf(0, "^1the cake is a lie\n\n");
		// iz ded af
		//Scr_AddInt(Scr_GetNumParam(SCRIPTINSTANCE_SERVER), SCRIPTINSTANCE_SERVER);
	}

	void GScr_SetLowReady(scr_entref_t entref)
	{
		if (T4M::Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 1)
			return;
		int enable = T4M::Scr_GetInt(SCRIPTINSTANCE_SERVER, 0);

		unsigned int idx = entref.entnum;
		if (idx >= 1024)
			return;
		gentity_s* ent = &T4::engine::g_entities[idx];
		if (!ent->client)
			return;

		T4M::SetLowReadyIntent(&ent->client->ps, enable != 0);
	}
#pragma endregion customFunctions
}


void PatchT4_Script()
{
	developer_funcdump = T4::dvar::Dvar_RegisterBool(0, "developer_funcdump", 0, "Dump script function information (engine)");

	cg_drawHealthCount = T4::dvar::Dvar_RegisterBool(0, "cg_drawHealthCount", 0, "Draw developer health counter in solo (requires map restart)"); // requires NZ remastererd mod
	cg_drawHealthCountCoop = T4::dvar::Dvar_RegisterBool(0, "cg_drawHealthCountCoop", 0, "Draw developer health counter in co-op (requires map restart)"); // requires NZ remastererd mod 
	cg_drawGamepadHUD = T4::dvar::Dvar_RegisterBool(0, "cg_drawGamepadHUD", 0, "Draw gamepad style HUD optimized for controller use"); // requires NZ remastererd mod
	cg_drawDpadLogos = T4::dvar::Dvar_RegisterBool(1, "cg_drawDpadLogos", 0, "Draw D-pad background textures"); // requires NZ remastererd mod
	cg_lowerGun = T4::dvar::Dvar_RegisterBool(0, "cg_lowerGun", 0, "Enable weapon lowering while moving in solo (requires map restart)"); // requires NZ remastererd mod
	cg_SoloScoreColorWhite = T4::dvar::Dvar_RegisterBool(0, "cg_SoloScoreColorWhite", 0, "Force white score color in solo (requires map restart)"); // requires NZ remastererd mod
	cg_drawTimers = T4::dvar::Dvar_RegisterBool(0, "cg_drawTimers", 0, "Draw game and round timers (requires map restart)"); // requires NZ remastererd mod
	cg_drawTrapTimers = T4::dvar::Dvar_RegisterBool(0, "cg_drawTrapTimers", 0, "Draw trap timers (requires map restart)"); // requires NZ remastererd mod
	zombiemode_dev = T4::dvar::Dvar_RegisterBool(0, "zombiemode_dev", 0, "Enable experimental developer features for Nazi Zombies remastered mod (requires map restart)"); // requires NZ remastererd mod

	g_disable_zombie_grab = T4::dvar::Dvar_RegisterInt(0, "g_disable_zombie_grab", 0, 2, DVAR_FLAG_CHEAT, "Disables pushPlayer() from executing\n1 = Disable when playing zombies\n2 = always disabled");

	g_fix_tesla_bug = T4::dvar::Dvar_RegisterBool(0, "g_fix_tesla_bug", DVAR_FLAG_CHEAT, "Applies same wunderwaffe's 'fix' as seen in Black Ops 1");

	Memory::VP::InjectHook(T4M::GetAddress("DisablePushPlayer_hook"), T4M::DisablePushPlayer);

	static auto fix_tesla_bug = safetyhook::create_mid(T4M::GetAddress("fix_tesla_bug_hook"), [](SafetyHookContext& ctx) {
		if (g_fix_tesla_bug->current.boolean)
			ctx.eip = T4M::GetAddress("tesla_bug_resume");
	});



	g_fix_health_sets_max = T4::dvar::Dvar_RegisterBool(0, "g_fix_health_sets_max", DVAR_FLAG_CHEAT, "Stops health also changing maxhealth");

	static auto fix_health_sets_max = safetyhook::create_mid(T4M::GetAddress("fix_health_sets_max_hook"), [](SafetyHookContext& ctx) {
		if (g_fix_health_sets_max->current.boolean)
			ctx.eip = T4M::GetAddress("health_sets_max_resume");
	});

	static dvar_t* gsc_OverheatMaxAmmo = T4::dvar::Dvar_RegisterBool(false, "gsc_OverheatMaxAmmo", 0, "Resets cooldown for 'overheat' weapon types when GiveMaxAmmo is called");

	// [GSC]
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Scr_GetFunction_callsite"), (uintptr_t)&T4M::Scr_GetFunction_Hook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Scr_GetMethod_callsite"), (uintptr_t)&T4M::Scr_GetMethod_Hook, Detours::X86Option::USE_CALL);
	T4M::Scr_DeclareFunction("printlnconsole", T4M::GScr_PrintLnConsole);
	T4M::Scr_DeclareMethod("setlowready", T4M::GScr_SetLowReady);

	// i hate asm and safetyhook midhook ftw -clippy95
	static auto PlayerCmd_GiveMaxAmmo_midhook = safetyhook::create_mid(T4M::GetAddress("PlayerCmd_GiveMaxAmmo_hook"), [](SafetyHookContext& ctx)
	{
		if (gsc_OverheatMaxAmmo && gsc_OverheatMaxAmmo->current.boolean) 
		{
			T4M::PlayerWeaponOverheatUpdate((gentity_s*)ctx.ebx, ctx.eax, 0.f);
		}
	});

	// [CSC]
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("CScr_GetFunction_callsite"), (uintptr_t)&T4M::CScr_GetFunction_Hook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("CScr_GetMethod_callsite"), (uintptr_t)&T4M::CScr_GetMethod_Hook, Detours::X86Option::USE_CALL);

	nop(T4M::GetAddress("tesla_notetrack_strnicmp_nop"), 2); // disable jnz on I_strnicmp for tesla notetrack

	// DON'T USE
	//nop(0x00668EDC, 5);
	//nop(0x00668F86, 5);
	//nop(0x00668E63, 5);
}
