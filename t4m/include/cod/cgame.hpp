#pragma once

// Client-side gameplay functions (HUD, viewmodel). T4::game per the namespace bucketing rule.

namespace T4
{
	namespace game
	{
		// WaW sub_42B710 / sub_42B850 / sub_42B990 / sub_42BB00 — usercall(weapIdx@eax, base@ecx,
		// cgameGlob@stack, weapDef@stack, color@stack) ; caller cleans. The clip count is read from
		// bg_weaponDefs[weapIdx], the capacity from the weapDef argument.
		inline void DrawClipAmmo_Usercall(void* fn, engine::cg_s* cgameGlob, const float* base,
			unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			__asm
			{
				push color
				push weapDef
				push cgameGlob
				mov  eax, weapIdx
				mov  ecx, base
				call fn
				add  esp, 0Ch
			}
		}

		inline void DrawClipAmmoMagazine(engine::cg_s* cgameGlob, const float* base, unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DrawClipAmmoMagazine"));
			DrawClipAmmo_Usercall(fn, cgameGlob, base, weapIdx, weapDef, color);
		}

		inline void DrawClipAmmoShortMagazine(engine::cg_s* cgameGlob, const float* base, unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DrawClipAmmoShortMagazine"));
			DrawClipAmmo_Usercall(fn, cgameGlob, base, weapIdx, weapDef, color);
		}

		inline void DrawClipAmmoShotgunShells(engine::cg_s* cgameGlob, const float* base, unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DrawClipAmmoShotgunShells"));
			DrawClipAmmo_Usercall(fn, cgameGlob, base, weapIdx, weapDef, color);
		}

		inline void DrawClipAmmoRockets(engine::cg_s* cgameGlob, const float* base, unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DrawClipAmmoRockets"));
			DrawClipAmmo_Usercall(fn, cgameGlob, base, weapIdx, weapDef, color);
		}

		// WaW sub_42BC40 — usercall(base@eax, cgameGlob@ecx, weapIdx@stack, weapDef@stack, color@stack) ; caller cleans
		inline void DrawClipAmmoBeltfed(engine::cg_s* cgameGlob, const float* base, unsigned int weapIdx, const engine::WeaponDef* weapDef, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DrawClipAmmoBeltfed"));
			__asm
			{
				push color
				push weapDef
				push weapIdx
				mov  eax, base
				mov  ecx, cgameGlob
				call fn
				add  esp, 0Ch
			}
		}

		// WaW sub_464200 — usercall(tree@eax, frac@stack, slot@stack) ; caller cleans
		inline void CG_ViewmodelAnim_AdsBlend(void* tree, float frac, int slot)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CG_ViewmodelAnim_AdsBlend"));
			__asm
			{
				push slot
				push frac
				mov  eax, tree
				call fn
				add  esp, 8
			}
		}

		// WaW sub_464080 — usercall(slot@eax, weapIdx@ecx, tree@stack, blend@stack) ; caller cleans
		inline void CG_ViewmodelAnim_SetSlotBlend(int slot, unsigned int weapIdx, void* tree, float blend)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CG_ViewmodelAnim_SetSlotBlend"));
			__asm
			{
				push blend
				push tree
				mov  eax, slot
				mov  ecx, weapIdx
				call fn
				add  esp, 8
			}
		}

		// WaW sub_662FF0 — usercall(localClientNum@edx, entnum@ecx, aliasList@stack) ; caller cleans
		inline void CG_PlayEntitySoundAlias(int localClientNum, int entnum, engine::snd_alias_list_t* aliasList)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CG_PlayEntitySoundAlias"));
			__asm
			{
				push aliasList
				mov  edx, localClientNum
				mov  ecx, entnum
				call fn
				add  esp, 4
			}
		}

		// WaW sub_663090 — usercall(entnum@ecx, aliasList@edx, localClientNum@stack) ; caller cleans
		inline void CG_StopEntitySoundAlias(int localClientNum, int entnum, engine::snd_alias_list_t* aliasList)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CG_StopEntitySoundAlias"));
			__asm
			{
				push localClientNum
				mov  ecx, entnum
				mov  edx, aliasList
				call fn
				add  esp, 4
			}
		}

		// WaW sub_42B580 — usercall(weapDef@edx) -> eax ; 0 when the alt weapon also uses AltWeapon
		inline unsigned int GetWeaponAltIndex(const engine::WeaponDef* weapDef)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetWeaponAltIndex"));
			unsigned int result;
			__asm
			{
				mov  edx, weapDef
				call fn
				mov  result, eax
			}
			return result;
		}
	}
}
