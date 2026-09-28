#pragma once

// BG_* / G_* gameplay functions. T4::game per the namespace bucketing rule.

namespace T4
{
	namespace game
	{
		// Resolves a weapon name to its bg_weaponDefs index.
		WEAK engine::symbol<int(const char* name)>BG_FindWeaponIndex{ "BG_FindWeaponIndex" };

		// Same, but takes the loader used when precaching is still open (g_precacheOpen != 0).
		WEAK engine::symbol<int(const char* name, void* loader)>BG_FindWeaponIndex_Internal{ "BG_FindWeaponIndex_Internal" };

		// Passed as the loader callback above; never called directly.
		WEAK engine::symbol<void(int index)>BG_LoadWeaponByIndex{ "BG_LoadWeaponByIndex" };

		// WaW sub_41C930 — usercall(weapDef@eax) -> eax ; iClipSize scaled by the clip-size dvar, min 1
		inline int BG_GetClipSize(const engine::WeaponDef* weapDef)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("BG_GetClipSize"));
			int result;
			__asm
			{
				mov  eax, weapDef
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_422D80 — usercall(weaponIndex@eax, ps@edi) -> al
		inline bool BG_PlayerHasCompatibleWeapon(const engine::playerState_s* ps, unsigned int weaponIndex)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("BG_PlayerHasCompatibleWeapon"));
			unsigned char result;
			__asm
			{
				mov  eax, weaponIndex
				mov  edi, ps
				call fn
				mov  result, al
			}
			return result != 0;
		}

		// WaW sub_41D800 — usercall(weaponIndex@eax, ps@stack, weaponIndexToSkip@stack) -> eax ; caller cleans
		inline int BG_GetAmmoPlayerMax(const engine::playerState_s* ps, unsigned int weaponIndex, unsigned int weaponIndexToSkip)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("BG_GetAmmoPlayerMax"));
			int result;
			__asm
			{
				push weaponIndexToSkip
				push ps
				mov  eax, weaponIndex
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_41D8B0 — usercall(weaponIndex@eax, ps@esi) -> eax
		inline int BG_GetMaxPickupableAmmo(const engine::playerState_s* ps, unsigned int weaponIndex)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("BG_GetMaxPickupableAmmo"));
			int result;
			__asm
			{
				mov  eax, weaponIndex
				mov  esi, ps
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_41D6A0 — usercall(weaponIndex@edi, ps@esi, takeAwayAmmo@stack) -> eax ; caller cleans
		inline int BG_TakePlayerWeapon(engine::playerState_s* ps, unsigned int weaponIndex, int takeAwayAmmo)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("BG_TakePlayerWeapon"));
			int result;
			__asm
			{
				push takeAwayAmmo
				mov  edi, weaponIndex
				mov  esi, ps
				call fn
				add  esp, 4
				mov  result, eax
			}
			return result;
		}

		// WaW sub_412BF0 (CSV key Anim_TriggerEvent) — usercall(event@ecx, ps@eax) ; eventParm = 0
		inline void PM_AddEvent(engine::playerState_s* ps, int event)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Anim_TriggerEvent"));
			__asm
			{
				mov  ecx, event
				mov  eax, ps
				call fn
			}
		}

		// WaW sub_41D420 (CSV key PM_Weapon_SubmitAnimEvent) — usercall(ps@eax, anim@stack) ; caller cleans
		inline void PM_StartWeaponAnim(engine::playerState_s* ps, int anim)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("PM_Weapon_SubmitAnimEvent"));
			__asm
			{
				push anim
				mov  eax, ps
				call fn
				add  esp, 4
			}
		}

		// WaW chunk 0x41E940 (CSV key PM_Weapon_ReloadFinalize) — usercall(ps@eax)
		inline void PM_SetReloadingState(engine::playerState_s* ps)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("PM_Weapon_ReloadFinalize"));
			__asm
			{
				mov  eax, ps
				call fn
			}
		}

		// WaW sub_406D00 (CSV key EventList_GetEntry) — usercall(scriptEvent@eax, clientNum@ecx) -> eax
		inline void* BG_FirstValidItem(unsigned int clientNum, void* scriptEvent)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EventList_GetEntry"));
			void* result;
			__asm
			{
				mov  eax, scriptEvent
				mov  ecx, clientNum
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_406E70 (CSV key WeaponEvent_ApplyToPS) — usercall(arg@eax, ps@stack, scriptCommand@stack, 2 ints@stack) -> eax ; caller cleans
		inline int BG_ExecuteCommand(engine::playerState_s* ps, void* scriptCommand, int eaxArg, int stackArg2, int stackArg3)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("WeaponEvent_ApplyToPS"));
			int result;
			__asm
			{
				push stackArg3
				push stackArg2
				push scriptCommand
				push ps
				mov  eax, eaxArg
				call fn
				add  esp, 10h
				mov  result, eax
			}
			return result;
		}

		// WaW sub_5528D0 — usercall(weaponIndex@eax, ps@ecx, weaponModel@stack) ; caller cleans
		inline void G_GivePlayerWeapon(engine::playerState_s* ps, unsigned int weaponIndex, unsigned char weaponModel)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_GivePlayerWeapon"));
			__asm
			{
				movzx edx, weaponModel
				push  edx
				mov   eax, weaponIndex
				mov   ecx, ps
				call  fn
				add   esp, 4
			}
		}
	}
}
