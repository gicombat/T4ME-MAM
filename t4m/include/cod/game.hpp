#pragma once

#include <cstdint>

#define WEAK __declspec(selectany)
#define NAKED __declspec(naked)

// MP/SP selection removed (environment gone): every symbol is name-keyed (AddrMap),
// and all CALL_ADDR sites pass mp=0x0 (MP was never wired). SELECT now yields sp.
#define SELECT(mp, sp) (sp)
#define ASSIGN(type, mp, sp) reinterpret_cast<type>(SELECT(mp, sp))
#define CALL_ADDR(mp, sp) ASSIGN(void*, mp, sp)

// Resolved by AddrMap.cpp; variant-aware address lookup used by symbol<> (name-keyed form).
namespace T4M 
{ 
	std::uintptr_t GetAddress(const char*); 
}

namespace T4
{
	namespace engine
	{
		// Name-keyed symbol: resolved lazily via T4M::GetAddress (variant-aware, see
		// AddrMap.cpp). The name is the key in addr_mapping.csv.
		template <typename T>
		class symbol
		{
		public:
			explicit symbol(const char* name) : name_(name), cached_(nullptr)
			{
			}

			T* get() const
			{
				if (!cached_)
					cached_ = reinterpret_cast<T*>(T4M::GetAddress(name_)); // lazy + cache (runtime only)
				return cached_;
			}

			void set(const size_t ptr)
			{
				this->cached_ = reinterpret_cast<T*>(ptr);
			}

			operator T* () const
			{
				return this->get();
			}

			T* operator->() const
			{
				return this->get();
			}

			// &gX -> base (call sites verbatim)
			T* operator& () const
			{
				return this->get();
			}

			// gX = p (re-point relocated pool)
			symbol& operator=(T* p)
			{
				this->cached_ = p;
				return *this;
			}


		private:
			const char* name_;
			mutable T* cached_;
		};
		

		// Function used in asm, initalized in T4M::InitASMRef
		WEAK void* p_IsWeaponInputBlocked = nullptr;
		WEAK void* p_PM_Weapon_AnimCmdState_Resync = nullptr;
		WEAK void* p_PM_Weapon_HasAnimSync = nullptr;
		WEAK void* p_PM_Weapon_DispatchAnimHandler = nullptr;
		WEAK void* p_PM_Weapon_TickValidation = nullptr;
		WEAK void* p_PM_Weapon_FinalGate = nullptr;
		WEAK void* p_PM_Weapon_TickSprintMachine = nullptr;
		WEAK void* p_PM_Weapon_TickIdleMachine = nullptr;
		WEAK void* p_PM_Weapon_TickOffhandMachine = nullptr;
		WEAK void* p_PM_Weapon_TickPickup = nullptr;
		WEAK void* p_PM_Weapon_TickRecoil = nullptr;
		WEAK void* p_PM_Weapon_ProcessAttackInput = nullptr;
		WEAK void* p_PM_Weapon_FinalizeStateExit = nullptr;
		WEAK void* p_Anim_TriggerEvent = nullptr;
		WEAK void* p_PM_Weapon_IsLocked = nullptr;
		WEAK void* p_PM_Weapon_TickDrop = nullptr;
		WEAK void* p_PM_Weapon_RefreshReload = nullptr;
		WEAK void* p_PM_Weapon_CheckFastReload = nullptr;
		WEAK void* p_PM_Weapon_SubmitAnimEvent = nullptr;
		WEAK void* p_PM_Weapon_ReloadFinalize = nullptr;
		WEAK void* p_PM_Weapon_GetInputMask = nullptr;
		WEAK void* p_PM_Weapon_OffhandSelect = nullptr;
		WEAK void* p_PM_Weapon_SetVmAnimState = nullptr;
		WEAK void* p_PM_Weapon_AdsBlendReset = nullptr;
		WEAK void* p_PM_Weapon_FireSwitchEvent = nullptr;
		WEAK void* p_EventList_GetEntry = nullptr;
		WEAK void* p_rand = nullptr;
		WEAK void* p_WeaponEvent_ApplyToPS = nullptr;
		WEAK void* p_PM_Weapon_Tick_OffhandInit_Stub = nullptr;
		WEAK void* p_PM_Weapon_Tick_Offhand_Stub = nullptr;
		WEAK void* p_FS_AddUserMapDir = nullptr;

		// Should be in console.hpp later
		WEAK void* ConDrawInput_TextLimitChars_asm = nullptr;
		
		// Should be in db.hpp later
		typedef void(__cdecl* DB_PrintError_t)(int, const char*, ...);
		WEAK DB_PrintError_t DB_PrintError = nullptr;

	}
} // namespace T4

// Makes qualified lookup `T4::X` also consider `T4::engine::X`, so the ~138 existing
// `T4::Foo(...)` / `T4::table[type]` call sites keep working while the engine symbols
// live in T4::engine (cod/*.hpp). Members declared directly in T4:: still win, no ambiguity.
namespace T4 { using namespace engine; }

// Pull in all vanilla types here (symbol<>/WEAK already defined above) so engine function
// symbols with real signatures can live in cod/*.hpp. enums BEFORE structs (structs needs
// the enums). T4.h includes this header first, so its later structs include is a no-op.
#include "enums.hpp"
#include "structs.hpp"

namespace T4
{
	namespace game   // *_GetMethod are T4::game per the namespace convention (call sites use T4::game::)
	{
		// (AddFunction / EmitMethod already live as symbol<> in cscr_compiler.hpp — do NOT redeclare.)
		WEAK engine::symbol<int(const char**)> Player_GetMethod{ "Player_GetMethod" };
		WEAK engine::symbol<int(const char**)> ScriptEnt_GetMethod{ "ScriptEnt_GetMethod" };
		WEAK engine::symbol<int(const char**)> ScriptVehicle_GetMethod{ "ScriptVehicle_GetMethod" };
		WEAK engine::symbol<int(const char**)> HudElem_GetMethod{ "HudElem_GetMethod" };
		WEAK engine::symbol<int(const char**)> Helicopter_GetMethod{ "Helicopter_GetMethod" };
		WEAK engine::symbol<int(const char**)> Actor_GetMethod{ "Actor_GetMethod" };
		WEAK engine::symbol<int(const char**, int*)> BuiltIn_GetMethod{ "BuiltIn_GetMethod" };
	}
}


namespace T4M
{
	// For all call used in ASM we need to "precache" them to have the correct address depending on the exe
	inline void InitASMRef()
	{
		T4::engine::ConDrawInput_TextLimitChars_asm = (void*)T4M::GetAddress("ConDrawInput_TextLimitChars");

		// DB error fn-pointer (variadic — called through the pointer with varargs)
		T4::engine::DB_PrintError = (T4::engine::DB_PrintError_t)T4M::GetAddress("DB_PrintError");
		// resolve vanilla call targets for the naked shims (variant-aware)
		T4::engine::p_IsWeaponInputBlocked = (void*)T4M::GetAddress("IsWeaponInputBlocked");
		T4::engine::p_PM_Weapon_AnimCmdState_Resync = (void*)T4M::GetAddress("PM_Weapon_AnimCmdState_Resync");
		T4::engine::p_PM_Weapon_HasAnimSync = (void*)T4M::GetAddress("PM_Weapon_HasAnimSync");
		T4::engine::p_PM_Weapon_DispatchAnimHandler = (void*)T4M::GetAddress("PM_Weapon_DispatchAnimHandler");
		T4::engine::p_PM_Weapon_TickValidation = (void*)T4M::GetAddress("PM_Weapon_TickValidation");
		T4::engine::p_PM_Weapon_FinalGate = (void*)T4M::GetAddress("PM_Weapon_FinalGate");
		T4::engine::p_PM_Weapon_TickSprintMachine = (void*)T4M::GetAddress("PM_Weapon_TickSprintMachine");
		T4::engine::p_PM_Weapon_TickIdleMachine = (void*)T4M::GetAddress("PM_Weapon_TickIdleMachine");
		T4::engine::p_PM_Weapon_TickOffhandMachine = (void*)T4M::GetAddress("PM_Weapon_TickOffhandMachine");
		T4::engine::p_PM_Weapon_TickPickup = (void*)T4M::GetAddress("PM_Weapon_TickPickup");
		T4::engine::p_PM_Weapon_TickRecoil = (void*)T4M::GetAddress("PM_Weapon_TickRecoil");
		T4::engine::p_PM_Weapon_ProcessAttackInput = (void*)T4M::GetAddress("PM_Weapon_ProcessAttackInput");
		T4::engine::p_PM_Weapon_FinalizeStateExit = (void*)T4M::GetAddress("PM_Weapon_FinalizeStateExit");
		T4::engine::p_Anim_TriggerEvent = (void*)T4M::GetAddress("Anim_TriggerEvent");
		T4::engine::p_PM_Weapon_IsLocked = (void*)T4M::GetAddress("PM_Weapon_IsLocked");
		T4::engine::p_PM_Weapon_TickDrop = (void*)T4M::GetAddress("PM_Weapon_TickDrop");
		T4::engine::p_PM_Weapon_RefreshReload = (void*)T4M::GetAddress("PM_Weapon_RefreshReload");
		T4::engine::p_PM_Weapon_CheckFastReload = (void*)T4M::GetAddress("PM_Weapon_CheckFastReload");
		T4::engine::p_PM_Weapon_SubmitAnimEvent = (void*)T4M::GetAddress("PM_Weapon_SubmitAnimEvent");
		T4::engine::p_PM_Weapon_ReloadFinalize = (void*)T4M::GetAddress("PM_Weapon_ReloadFinalize");
		T4::engine::p_PM_Weapon_GetInputMask = (void*)T4M::GetAddress("PM_Weapon_GetInputMask");
		T4::engine::p_PM_Weapon_OffhandSelect = (void*)T4M::GetAddress("PM_Weapon_OffhandSelect");
		T4::engine::p_PM_Weapon_SetVmAnimState = (void*)T4M::GetAddress("PM_Weapon_SetVmAnimState");
		T4::engine::p_PM_Weapon_AdsBlendReset = (void*)T4M::GetAddress("PM_Weapon_AdsBlendReset");
		T4::engine::p_PM_Weapon_FireSwitchEvent = (void*)T4M::GetAddress("PM_Weapon_FireSwitchEvent");
		T4::engine::p_EventList_GetEntry = (void*)T4M::GetAddress("EventList_GetEntry");
		T4::engine::p_rand = (void*)T4M::GetAddress("rand");
		T4::engine::p_WeaponEvent_ApplyToPS = (void*)T4M::GetAddress("WeaponEvent_ApplyToPS");
		T4::engine::p_PM_Weapon_Tick_OffhandInit_Stub = (void*)T4M::GetAddress("PM_Weapon_Tick_OffhandInit_Stub");
		T4::engine::p_PM_Weapon_Tick_Offhand_Stub = (void*)T4M::GetAddress("PM_Weapon_Tick_Offhand_Stub");
		T4::engine::p_FS_AddUserMapDir = (void*)T4M::GetAddress("FS_AddUserMapDir");
	}
}