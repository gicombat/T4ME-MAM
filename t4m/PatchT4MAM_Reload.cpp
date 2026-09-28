// Reload extensions — see plans/plan_weapon_reload_ext.md (R&D folder).
//
//   closedBolt             : a non-empty reload may reach clipSize + 1 (chambered round);
//                            the HUD shows the extra slot only while it is loaded.
//   reloadAmmoAddEmpty     : rounds added by an empty, non-segmented reload, in place of
//                            reloadAmmoAdd; 0 / absent -> reloadAmmoAdd. Caps nothing else:
//                            reloadAmmoAdd still drives non-empty, segmented, noPartialReload.
//   reloadStartEmpty*      : segmented reload started on an empty clip gets its own start
//                            anim / time / add / sounds. Latched as vm-anim
//                            T4M::VM_ANIM_RELOAD_START_EMPTY in ps->weapAnim, which is
//                            networked and saved; EV_RELOAD_START carries eventParm 1.
//
// Every function here is a vanilla reconstruction with one @modified branch,
// detoured at the vanilla entry so all callers go through it.

#include "StdInc.h"
#include "T4.h"
#include <cstring>

using namespace T4::engine;
using namespace T4::game;

namespace
{
	// @new
	int ClipCapacity(const WeaponDef* weapDef, int clip)
	{
		const int clipSize = BG_GetClipSize(weapDef);
		if (weapDef->closedBolt && clip > 0)
			return clipSize + 1;
		return clipSize;
	}

	// @new
	int ReloadAmmoAddFor(const WeaponDef* weapDef, int clip)
	{
		if (!clip && !weapDef->bSegmentedReload && weapDef->iReloadAmmoAddEmpty)
			return weapDef->iReloadAmmoAddEmpty;
		return weapDef->iReloadAmmoAdd;
	}

	// @new — closed-bolt weapon holding its chambered round: hand the clip graphic a copy
	// whose capacity covers the loaded count. Gameplay never sees the copy.
	const WeaponDef* HudClipWeaponDef(const cg_s* cgameGlob, const WeaponDef* weapDef)
	{
		if (!weapDef->closedBolt)
			return weapDef;

		const int clip = cgameGlob->predictedPlayerState.ammoclip[weapDef->iClipIndex];
		if (clip <= BG_GetClipSize(weapDef))
			return weapDef;

		static WeaponDef shadow;
		memcpy(&shadow, weapDef, sizeof(WeaponDef));

		// Smallest raw iClipSize whose scaled capacity reaches the loaded count
		// (raw + 1 when the clip-size scale is 1).
		for (int raw = weapDef->iClipSize + 1; raw <= weapDef->iClipSize + 64; ++raw)
		{
			shadow.iClipSize = raw;
			if (BG_GetClipSize(&shadow) >= clip)
				break;
		}
		return &shadow;
	}

	// @new — reloadStartEmptyTime is only non-zero once T4M::Reload_ApplyWeaponDefDefaults
	// found at least one reloadStartEmpty* field authored.
	bool HasEmptyStart(const WeaponDef* weapDef)
	{
		return weapDef->iReloadStartEmptyTime != 0;
	}

	// @new
	bool IsEmptyStart(const playerState_s* ps)
	{
		return (ps->weaponstate == WEAPON_RELOAD_START || ps->weaponstate == WEAPON_RELOAD_START_INTERUPT)
			&& (ps->weapAnim & ~0x200) == T4M::VM_ANIM_RELOAD_START_EMPTY;
	}

	int ReloadStartTime(const playerState_s* ps, const WeaponDef* weapDef)
	{
		return IsEmptyStart(ps) ? weapDef->iReloadStartEmptyTime : weapDef->iReloadStartTime;
	}

	int ReloadStartAddTime(const playerState_s* ps, const WeaponDef* weapDef)
	{
		return IsEmptyStart(ps) ? weapDef->iReloadStartEmptyAddTime : weapDef->iReloadStartAddTime;
	}

	int ReloadStartAdd(const playerState_s* ps, const WeaponDef* weapDef)
	{
		return IsEmptyStart(ps) ? weapDef->iReloadStartEmptyAdd : weapDef->iReloadStartAdd;
	}

	// Inlined by vanilla as BG_AddPredictableEventToPlayerstate(event, eventParm, ps).
	void AddPlayerEvent(playerState_s* ps, int event, unsigned int eventParm)
	{
		ps->events[ps->eventSequence & 3] = event;
		ps->eventParms[ps->eventSequence & 3] = eventParm;
		ps->eventSequence = (ps->eventSequence + 1) & 0xFF;
	}

	bool IsBitSet(const unsigned int* bits, unsigned int index)
	{
		return (bits[(int)index >> 5] & (1u << (index & 0x1F))) != 0;
	}
}

namespace T4M
{
	// @modified — sub_41E350 (PM_ReloadClip). Capacity +1 for closedBolt,
	// reloadAmmoAddEmpty, reloadStartEmptyAdd. DETOURED — do not call directly.
	extern "C" void __cdecl PM_ReloadClip(playerState_s* ps)
	{
		const WeaponDef* weapDef = bg_weaponDefs[ps->weapon];
		const bool startState = ps->weaponstate == WEAPON_RELOAD_START
			|| ps->weaponstate == WEAPON_RELOAD_START_INTERUPT;

		const int reloadStartAdd = ReloadStartAdd(ps, weapDef);
		if (startState && !reloadStartAdd)
			return;

		const int ammoIndex = weapDef->iAmmoIndex;
		const int clipIndex = weapDef->iClipIndex;
		const int clip = ps->ammoclip[clipIndex];

		int ammoAdd = ClipCapacity(weapDef, clip) - clip;
		if (ammoAdd > ps->ammo[ammoIndex])
			ammoAdd = ps->ammo[ammoIndex];

		if (startState)
		{
			if (reloadStartAdd < BG_GetClipSize(weapDef) && ammoAdd > reloadStartAdd)
				ammoAdd = reloadStartAdd;
		}
		else
		{
			const int reloadAmmoAdd = ReloadAmmoAddFor(weapDef, clip);
			if (reloadAmmoAdd && reloadAmmoAdd < BG_GetClipSize(weapDef) && ammoAdd > reloadAmmoAdd)
				ammoAdd = reloadAmmoAdd;
		}

		if (!ammoAdd)
			return;

		ps->ammo[ammoIndex] -= ammoAdd;
		ps->ammoclip[clipIndex] += ammoAdd;

		AddPlayerEvent(ps, EV_RELOAD_ADDAMMO, 0);
	}

	// @wrapper — usercall(ps@esi). Only caller: sub_41F4E0.
	__declspec(naked) void PM_ReloadClip_Wrapper()
	{
		__asm
		{
			push esi
			call PM_ReloadClip
			add  esp, 4
			retn
		}
	}

	// @modified — sub_41F410 (PM_Weapon_AllowReload; CSV key PM_Weapon_CheckFastReload).
	// closedBolt capacity; noPartialReload stays on vanilla reloadAmmoAdd. DETOURED — do not call directly.
	extern "C" int __cdecl PM_Weapon_AllowReload(playerState_s* ps)
	{
		const WeaponDef* weapDef = bg_weaponDefs[ps->weapon];
		const int clipIndex = weapDef->iClipIndex;

		if (!ps->ammo[weapDef->iAmmoIndex])
			return 0;

		const int clip = ps->ammoclip[clipIndex];
		const int capacity = ClipCapacity(weapDef, clip);
		if (clip >= capacity)
			return 0;

		if (!weapDef->bNoPartialReload)
			return 1;

		// Vanilla reloadAmmoAdd: reloadAmmoAddEmpty only changes what an empty reload adds.
		if (weapDef->iReloadAmmoAdd && weapDef->iReloadAmmoAdd < BG_GetClipSize(weapDef))
			return capacity - clip >= weapDef->iReloadAmmoAdd;

		return clip == 0;
	}

	// @wrapper — usercall(ps@edi) -> eax
	__declspec(naked) void PM_Weapon_AllowReload_Wrapper()
	{
		__asm
		{
			push edi
			call PM_Weapon_AllowReload
			add  esp, 4
			retn
		}
	}

	// @modified — sub_4FC510 (Fill_Clip). Never pulls a chambered round back into
	// the reserve. DETOURED — do not call directly.
	extern "C" void __cdecl Fill_Clip(playerState_s* ps, unsigned int weapon)
	{
		const WeaponDef* weapDef = bg_weaponDefs[weapon];
		const int ammoIndex = weapDef->iAmmoIndex;
		const int clipIndex = weapDef->iClipIndex;

		if (!weapon || weapon >= *bg_numWeapons.get() + 1)
			return;

		const int inclip = ps->ammoclip[clipIndex];
		int ammomove = BG_GetClipSize(weapDef) - inclip;
		if (ammomove > ps->ammo[ammoIndex])
			ammomove = ps->ammo[ammoIndex];

		if (weapDef->closedBolt && ammomove < 0)
			ammomove = 0;

		if (ammomove)
		{
			ps->ammo[ammoIndex] -= ammomove;
			ps->ammoclip[clipIndex] += ammomove;
		}
	}

	// @wrapper — usercall(weapon@ecx, ps@esi)
	__declspec(naked) void Fill_Clip_Wrapper()
	{
		__asm
		{
			push ecx
			push esi
			call Fill_Clip
			add  esp, 8
			retn
		}
	}

	// @modified — sub_4FC570 (Add_Ammo). The clip clamp keeps a closed-bolt
	// weapon's chambered round. DETOURED — do not call directly.
	extern "C" int __cdecl Add_Ammo(gentity_s* ent, unsigned int weaponIndex, unsigned char weaponModel, int count, int fillClip)
	{
		const WeaponDef* weapDef = bg_weaponDefs[weaponIndex];
		playerState_s* ps = &ent->client->ps;

		if (!(ps->weapons[(int)weaponIndex >> 5] & (1 << (weaponIndex & 0x1F)))
			&& !BG_PlayerHasCompatibleWeapon(ps, weaponIndex)
			&& weapDef->offhandClass != OFFHAND_CLASS_FRAG_GRENADE)
		{
			return 0;
		}

		const int ammoIndex = weapDef->iAmmoIndex;
		const int clipIndex = weapDef->iClipIndex;
		const int oldClip = ps->ammoclip[clipIndex];
		const int oldAmmo = ps->ammo[ammoIndex];
		bool clipOnly = false;

		const int maxWeaponAmmo = BG_GetAmmoPlayerMax(ps, weaponIndex, 0);
		ps->ammo[ammoIndex] += count;

		if (bg_weaponDefs[weaponIndex]->bClipOnly)
		{
			G_GivePlayerWeapon(ps, weaponIndex, weaponModel);
			clipOnly = true;
		}

		if (fillClip || clipOnly)
			Fill_Clip(ps, weaponIndex);

		if (clipOnly)
			ps->ammo[ammoIndex] = 0;
		else if (ps->ammo[ammoIndex] > maxWeaponAmmo)
			ps->ammo[ammoIndex] = maxWeaponAmmo;

		const int clipSize = BG_GetClipSize(weapDef);
		const int clipLimit = weapDef->closedBolt ? clipSize + 1 : clipSize;
		if (ps->ammoclip[clipIndex] > clipLimit)
			ps->ammoclip[clipIndex] = clipLimit;

		if (bg_weaponDefs[weaponIndex]->iSharedAmmoCapIndex >= 0)
		{
			const int pickup = BG_GetMaxPickupableAmmo(ps, weaponIndex);
			if (pickup < 0)
			{
				if (bg_weaponDefs[weaponIndex]->bClipOnly)
				{
					ps->ammoclip[clipIndex] += pickup;
					if (ps->ammoclip[clipIndex] <= 0)
					{
						BG_TakePlayerWeapon(ps, weaponIndex, 1);
						return 0;
					}
				}
				else
				{
					ps->ammo[ammoIndex] += pickup;
					if (ps->ammo[ammoIndex] < 0)
						ps->ammo[ammoIndex] = 0;
				}
			}
		}

		return ps->ammoclip[clipIndex] - oldClip - oldAmmo + ps->ammo[ammoIndex];
	}

	// @wrapper — usercall(ent@eax, weaponIndex, weaponModel, count, fillClip @stack) -> eax ; caller cleans
	__declspec(naked) void Add_Ammo_Wrapper()
	{
		__asm
		{
			push [esp+10h]          ; fillClip
			push [esp+10h]          ; count
			push [esp+10h]          ; weaponModel
			push [esp+10h]          ; weaponIndex
			push eax                ; ent
			call Add_Ammo
			add  esp, 14h
			retn
		}
	}

	// @modified — sub_42BE50 (DrawClipAmmo). Passes the closed-bolt HUD copy to the
	// clip graphics. DETOURED — do not call directly.
	extern "C" void __cdecl DrawClipAmmo(cg_s* cgameGlob, const float* base, unsigned int weapIdx, const WeaponDef* weapDef, const float* color)
	{
		const WeaponDef* drawDef = HudClipWeaponDef(cgameGlob, weapDef);

		switch (weapDef->ammoCounterClip)
		{
		case AMMO_COUNTER_CLIP_MAGAZINE:
			DrawClipAmmoMagazine(cgameGlob, base, weapIdx, drawDef, color);
			break;
		case AMMO_COUNTER_CLIP_SHORTMAGAZINE:
			DrawClipAmmoShortMagazine(cgameGlob, base, weapIdx, drawDef, color);
			break;
		case AMMO_COUNTER_CLIP_SHOTGUN:
			DrawClipAmmoShotgunShells(cgameGlob, base, weapIdx, drawDef, color);
			break;
		case AMMO_COUNTER_CLIP_ROCKET:
			DrawClipAmmoRockets(cgameGlob, base, weapIdx, drawDef, color);
			break;
		case AMMO_COUNTER_CLIP_BELTFED:
			DrawClipAmmoBeltfed(cgameGlob, base, weapIdx, drawDef, color);
			break;
		case AMMO_COUNTER_CLIP_ALTWEAPON:
			if (!weapDef->altWeaponIndex)
			{
				Com_PrintWarning(17, "Weapon \"%s\" ammoCounterClip property is set to \"AltWeapon\", but it has no alternate weapon.\n",
					weapDef->szInternalName);
				break;
			}
			{
				const unsigned int weapIdxAlt = GetWeaponAltIndex(weapDef);
				if (weapIdxAlt)
					DrawClipAmmo(cgameGlob, base, weapIdxAlt, bg_weaponDefs[weapIdxAlt], color);
			}
			break;
		default:
			break;
		}
	}

	// @wrapper — usercall(weapIdx@eax, weapDef@edx, base@edi, color@esi, cgameGlob@stack) ; caller cleans
	__declspec(naked) void DrawClipAmmo_Wrapper()
	{
		__asm
		{
			push esi                ; color
			push edx                ; weapDef
			push eax                ; weapIdx
			push edi                ; base
			push [esp+14h]          ; cgameGlob
			call DrawClipAmmo
			add  esp, 14h
			retn
		}
	}

	// @new — called from the BG_RegisterWeapon hook (PatchT4MAM_LowReady.cpp). When any
	// reloadStartEmpty* field is authored, fill the others from reloadStart*; otherwise
	// leave them all zero so the weapon keeps the vanilla reload start.
	extern "C" void __cdecl Reload_ApplyWeaponDefDefaults(WeaponDef* weapDef)
	{
		if (!weapDef)
			return;

		const bool authored = weapDef->iReloadStartEmptyTime
			|| weapDef->iReloadStartEmptyAddTime
			|| weapDef->iReloadStartEmptyAdd
			|| (weapDef->sreloadStartEmptyAnim && *weapDef->sreloadStartEmptyAnim)
			|| weapDef->reloadStartEmptySound
			|| weapDef->reloadStartEmptySoundPlayer;
		if (!authored)
			return;

		if (!weapDef->iReloadStartEmptyTime)
			weapDef->iReloadStartEmptyTime = weapDef->iReloadStartTime;
		if (!weapDef->iReloadStartEmptyAddTime)
			weapDef->iReloadStartEmptyAddTime = weapDef->iReloadStartAddTime;
		if (!weapDef->iReloadStartEmptyAdd)
			weapDef->iReloadStartEmptyAdd = weapDef->iReloadStartAdd;
		if (!weapDef->sreloadStartEmptyAnim || !*weapDef->sreloadStartEmptyAnim)
			weapDef->sreloadStartEmptyAnim = weapDef->sreloadStartAnim;
		if (!weapDef->reloadStartEmptySound)
			weapDef->reloadStartEmptySound = weapDef->reloadStartSound;
		if (!weapDef->reloadStartEmptySoundPlayer)
			weapDef->reloadStartEmptySoundPlayer = weapDef->reloadStartSoundPlayer;
	}

	// @modified — sub_41E860 (PM_SetWeaponReloadAddAmmoDelay), cdecl. Empty-start times.
	// DETOURED — do not call directly.
	extern "C" void __cdecl PM_SetWeaponReloadAddAmmoDelay(playerState_s* ps)
	{
		const unsigned int weapon = ps->weapon;
		const WeaponDef* weapDef = bg_weaponDefs[weapon];
		int reloadTime;

		if (ps->weaponstate == WEAPON_RELOAD_START || ps->weaponstate == WEAPON_RELOAD_START_INTERUPT)
		{
			reloadTime = ReloadStartAddTime(ps, weapDef);
			if (reloadTime)
			{
				const int startTime = ReloadStartTime(ps, weapDef);
				if (reloadTime >= startTime)
					reloadTime = startTime;
			}
		}
		else
		{
			int addTime = weapDef->iReloadAddTime;
			if (ps->ammoclip[weapDef->iClipIndex] || weapDef->weapType)
			{
				reloadTime = weapDef->iReloadTime;
			}
			else
			{
				reloadTime = weapDef->iReloadEmptyTime;
				if (weapDef->reloadEmptyAddTime)
					addTime = weapDef->reloadEmptyAddTime;
			}
			if (addTime && addTime < reloadTime)
				reloadTime = addTime;
		}

		if (weapDef->bBoltAction && IsBitSet(ps->weaponrechamber, weapon))
		{
			if (!reloadTime)
				reloadTime = ps->weaponTime;
			if (weapDef->iRechamberBoltTime < reloadTime)
				reloadTime = weapDef->iRechamberBoltTime;
			ps->weaponDelay = reloadTime ? reloadTime : 1;
			return;
		}

		if (reloadTime)
			ps->weaponDelay = reloadTime;
	}

	// @modified — sub_41F4E0 (PM_Weapon_ReloadDelayedAction; CSV key PM_Weapon_RefreshReload).
	// Empty-start times. DETOURED — do not call directly.
	extern "C" void __cdecl PM_Weapon_ReloadDelayedAction(playerState_s* ps)
	{
		const unsigned int weapon = ps->weapon;
		const WeaponDef* weapDef = bg_weaponDefs[weapon];

		if (!weapDef->bBoltAction || !IsBitSet(ps->weaponrechamber, weapon))
		{
			PM_ReloadClip(ps);
			return;
		}

		ps->weaponrechamber[(int)weapon >> 5] &= ~(1u << (weapon & 0x1F));
		AddPlayerEvent(ps, EV_EJECT_BRASS, 0);

		const bool startState = ps->weaponstate == WEAPON_RELOAD_START
			|| ps->weaponstate == WEAPON_RELOAD_START_INTERUPT;
		if (startState && !ReloadStartAddTime(ps, weapDef))
			return;

		if (!ps->weaponTime)
		{
			PM_ReloadClip(ps);
			return;
		}

		int reloadTime;
		if (startState)
		{
			reloadTime = ReloadStartAddTime(ps, weapDef);
			const int startTime = ReloadStartTime(ps, weapDef);
			if (reloadTime >= startTime)
				reloadTime = startTime;
		}
		else
		{
			const WeaponDef* current = bg_weaponDefs[ps->weapon];
			int addTime = weapDef->iReloadAddTime;
			if (ps->ammoclip[current->iClipIndex] || weapDef->weapType)
			{
				reloadTime = weapDef->iReloadTime;
			}
			else
			{
				reloadTime = weapDef->iReloadEmptyTime;
				if (weapDef->reloadEmptyAddTime)
					addTime = weapDef->reloadEmptyAddTime;
			}
			if (addTime && addTime < reloadTime)
				reloadTime = addTime;
		}

		int rechamberTime = weapDef->iRechamberBoltTime;
		if (rechamberTime >= reloadTime)
			rechamberTime = 1;
		reloadTime -= rechamberTime;

		if (reloadTime < 1)
		{
			PM_ReloadClip(ps);
			return;
		}
		ps->weaponDelay = reloadTime;
	}

	// @wrapper — usercall(ps@eax)
	__declspec(naked) void PM_Weapon_ReloadDelayedAction_Wrapper()
	{
		__asm
		{
			push eax
			call PM_Weapon_ReloadDelayedAction
			add  esp, 4
			retn
		}
	}

	// @modified — sub_41EA30 (PM_BeginWeaponReload). Empty segmented start, and the
	// lowReady gate formerly installed as a midhook on this entry. DETOURED — do not call directly.
	extern "C" void __cdecl PM_BeginWeaponReload(playerState_s* ps)
	{
		if ((ps->eFlags & 0x400)
			|| ps->weaponstate == WEAPON_LOWREADY_START
			|| ps->weaponstate == WEAPON_LOWREADY_LOOP
			|| ps->weaponstate == WEAPON_LOWREADY_END)
		{
			return;
		}

		const int ws = ps->weaponstate;
		const unsigned int weapon = ps->weapon;
		const WeaponDef* weapDef = bg_weaponDefs[weapon];

		if (ws != WEAPON_READY && ws != WEAPON_FIRING && ws != WEAPON_RECHAMBERING
			&& (ws < WEAPON_SPRINT_RAISE || ws > WEAPON_SPRINT_DROP))
		{
			return;
		}
		if (!weapon || weapon >= *bg_numWeapons.get() + 1)
			return;

		// Inlined by vanilla: BG_AnimScriptEvent(ps, ANIM_ET_RELOAD, 0, 1)
		if (!weapDef->bClipOnly && ps->pm_type < 8)
		{
			char* scriptEvent = *globalScriptData.get() + 0x19B98;
			if (*(int*)scriptEvent)
			{
				char* item = (char*)BG_FirstValidItem(ps->clientNum, scriptEvent);
				if (item && *(int*)(item + 0x34))
				{
					const int r = ((int(__cdecl*)())T4::engine::p_rand)() % *(int*)(item + 0x34);
					char* scriptCommand = item + r * 20 + 0x38;
					if (scriptCommand)
						BG_ExecuteCommand(ps, scriptCommand, 1, 0, 1);
				}
			}
		}

		ps->weaponShotCount = 0;
		AddPlayerEvent(ps, EV_RESET_ADS, 0);
		AddPlayerEvent(ps, EV_RELOAD_START_NOTIFY, 0);

		const bool emptyStart = HasEmptyStart(weapDef) && !ps->ammoclip[weapDef->iClipIndex];
		const int startTime = emptyStart ? weapDef->iReloadStartEmptyTime : weapDef->iReloadStartTime;

		if (!weapDef->bSegmentedReload || !startTime)
		{
			PM_SetReloadingState(ps);
			return;
		}

		PM_StartWeaponAnim(ps, emptyStart ? VM_ANIM_RELOAD_START_EMPTY : 0xF);
		ps->weaponTime = startTime;
		ps->weaponstate = WEAPON_RELOAD_START;
		if (emptyStart)
			AddPlayerEvent(ps, EV_RELOAD_START, 1);
		else
			PM_AddEvent(ps, EV_RELOAD_START);
		PM_SetWeaponReloadAddAmmoDelay(ps);
	}

	// @wrapper — usercall(ps@esi)
	__declspec(naked) void PM_BeginWeaponReload_Wrapper()
	{
		__asm
		{
			push esi
			call PM_BeginWeaponReload
			add  esp, 4
			retn
		}
	}

	// @modified — sub_41F8D0 (PM_Weapon_CheckForReload; CSV key PM_Weapon_TickRecoil).
	// Empty-start time in the start-interrupt fraction. DETOURED — do not call directly.
	extern "C" void __cdecl PM_Weapon_CheckForReload(pmove_s* pm)
	{
		playerState_s* ps = pm->ps;
		const unsigned int weapon = ps->weapon;
		const WeaponDef* weapDef = bg_weaponDefs[weapon];
		const int ws = ps->weaponstate;
		bool reloadRequested = false;

		if (ws == WEAPON_MELEE_END
			&& !ps->ammoclip[weapDef->iClipIndex]
			&& !weapDef->unlimitedAmmo
			&& weapDef->bayonet != 1)
		{
			ps->weaponstate = WEAPON_DETONATING;
			ps->weaponTime = weapDef->iEmptyRaiseTime;
			if (ps->pm_type < 8)
				ps->weapAnim = (~ps->weapAnim & 0x200) | 0x16;
			return;
		}

		if (ws >= WEAPON_OFFHAND_INIT && ws <= WEAPON_OFFHAND_END)
			return;
		if (ws == WEAPON_MELEE_INIT || ws == WEAPON_MELEE_FIRE || ws == WEAPON_MELEE_END)
			return;
		if (ps->waterlevel >= 3)
			return;

		bool reloadPressed = ((int)pm->cmd.buttons & 0x10) != 0;
		if (ps->weapFlags & 1)
		{
			ps->weapFlags &= ~1;
			reloadPressed = true;
		}

		if (weapDef->bSegmentedReload
			&& (ws == WEAPON_RELOAD_START || ws == WEAPON_RELOADING)
			&& ((int)pm->cmd.buttons & 1)
			&& !((int)pm->oldcmd.buttons & 1))
		{
			const int startTime = ws == WEAPON_RELOAD_START ? ReloadStartTime(ps, weapDef) : 0;
			if (ws == WEAPON_RELOAD_START && startTime)
			{
				if ((float)(startTime - ps->weaponTime) / (float)startTime > 0.4f)     // dword_83FCF4
					ps->weaponstate = WEAPON_RELOAD_START_INTERUPT;
			}
			else if (ws == WEAPON_RELOADING)
			{
				ps->weaponstate = WEAPON_RELOADING_INTERUPT;
			}
		}

		const int state = ps->weaponstate;
		switch (state)
		{
		case 1: case 2: case 3: case 4:
		case 7: case 8: case 9: case 10: case 11:
		case 29:
			return;
		default:
			break;
		}

		const WeaponDef* current = bg_weaponDefs[weapon];
		const int clipIndex = current->iClipIndex;
		const int ammoIndex = current->iAmmoIndex;

		if (reloadPressed && PM_Weapon_AllowReload(ps))
			reloadRequested = true;

		bool autoReload = false;
		if (!ps->ammoclip[clipIndex]
			&& ps->ammo[ammoIndex]
			&& state != WEAPON_FIRING
			&& (state < WEAPON_SPRINT_RAISE || state > WEAPON_SPRINT_DROP)
			&& state != WEAPON_DEPLOYING && state != WEAPON_DEPLOYED && state != WEAPON_BREAKING_DOWN
			&& !(ps->eFlags & 0x300))
		{
			autoReload = !weapDef->noADSAutoReload || !(ps->fWeaponPosFrac == 1.0f);     // dword_826A4C
		}

		if (autoReload || reloadRequested)
			PM_BeginWeaponReload(ps);
	}

	// @wrapper — usercall(pm@edx)
	__declspec(naked) void PM_Weapon_CheckForReload_Wrapper()
	{
		__asm
		{
			push edx
			call PM_Weapon_CheckForReload
			add  esp, 4
			retn
		}
	}

	// @modified — branch inserted in front of sub_4643A0 by T4M_ChooserHook_LowReady.
	// Mirrors a vanilla chooser case (pre-switch ADS blend, dedup, slot play, dedup
	// store) for the T4M vm-anim codes: lowReady 0x20-0x22 -> slots 0x25-0x27,
	// VM_ANIM_RELOAD_START_EMPTY -> VM_SLOT_RELOAD_START_EMPTY.
	extern "C" bool __cdecl ViewmodelChooser_CustomSlot(playerState_s* ps, void* weaponHandle)
	{
		const int vmAnim = ps->weapAnim & ~0x200;
		int slot;
		if (vmAnim >= 0x20 && vmAnim <= 0x22)
			slot = 0x25 + (vmAnim - 0x20);
		else if (vmAnim == VM_ANIM_RELOAD_START_EMPTY)
			slot = VM_SLOT_RELOAD_START_EMPTY;
		else
			return false;

		void* tree = *(void**)weaponHandle;
		const unsigned int weapIdx = (ps->weapFlags & 2) ? ps->offHandIndex : ps->weapon;
		const WeaponDef* weapDef = bg_weaponDefs[weapIdx];

		if (weapDef->aimDownSight)
		{
			const bool reloadPastAds = ps->weaponstate == WEAPON_RELOADING
				&& ps->weaponTime - weapDef->iPositionReloadTransTime > 0;
			const bool adsUp = ((int)ps->pm_flags & 0x10) && !(ps->weapFlags & 2);
			CG_ViewmodelAnim_AdsBlend(tree, ps->fWeaponPosFrac, (!reloadPastAds && adsUp) ? 0x21 : 0x22);
		}
		else if (*weapDef->sadsDownAnim)
		{
			CG_ViewmodelAnim_AdsBlend(tree, 0.0f, 0x22);
		}

		int* lastAnim = (int*)((char*)weaponHandle + 0x28);
		if (ps->weapAnim == *lastAnim && weapIdx == *cg_lastViewmodelAnimWeapon.get())
			return true;

		CG_ViewmodelAnim_SetSlotBlend(slot, weapIdx, tree, 0.0f);
		*lastAnim = ps->weapAnim;
		*cg_lastViewmodelAnimWeapon.get() = weapIdx;
		return true;
	}

	// @modified — sub_448640 (CG_StopWeaponReloadSounds). Interrupting a start also stops
	// the empty-start alias. DETOURED — do not call directly.
	extern "C" void __cdecl CG_StopWeaponReloadSounds(int weaponState, int isPlayerView, const WeaponDef* weapDef, int entnum, int localClientNum)
	{
		switch (weaponState)
		{
		case WEAPON_RELOADING:
		case WEAPON_RELOADING_INTERUPT:
			if (isPlayerView)
			{
				if (weapDef->reloadEmptySoundPlayer)
					CG_StopEntitySoundAlias(localClientNum, entnum, weapDef->reloadEmptySoundPlayer);
				if (weapDef->reloadSoundPlayer)
					CG_StopEntitySoundAlias(localClientNum, entnum, weapDef->reloadSoundPlayer);
			}
			else
			{
				if (weapDef->reloadEmptySound)
					CG_StopEntitySoundAlias(localClientNum, entnum, weapDef->reloadEmptySound);
				if (weapDef->reloadSound)
					CG_StopEntitySoundAlias(localClientNum, entnum, weapDef->reloadSound);
			}
			break;

		case WEAPON_RELOAD_START:
		case WEAPON_RELOAD_START_INTERUPT:
		{
			snd_alias_list_t* start = isPlayerView ? weapDef->reloadStartSoundPlayer : weapDef->reloadStartSound;
			snd_alias_list_t* startEmpty = isPlayerView ? weapDef->reloadStartEmptySoundPlayer : weapDef->reloadStartEmptySound;
			CG_StopEntitySoundAlias(localClientNum, entnum, start);
			if (startEmpty && startEmpty != start)
				CG_StopEntitySoundAlias(localClientNum, entnum, startEmpty);
			break;
		}

		case WEAPON_RELOAD_END:
			CG_StopEntitySoundAlias(localClientNum, entnum, isPlayerView ? weapDef->reloadEndSoundPlayer : weapDef->reloadEndSound);
			break;

		default:
			break;
		}
	}

	// @wrapper — usercall(weaponState@eax, isPlayerView@cl, weapDef@esi, entnum@edi, localClientNum@stack) ; caller cleans
	__declspec(naked) void CG_StopWeaponReloadSounds_Wrapper()
	{
		__asm
		{
			movzx ecx, cl
			push [esp+4]            ; localClientNum
			push edi                ; entnum
			push esi                ; weapDef
			push ecx                ; isPlayerView
			push eax                ; weaponState
			call CG_StopWeaponReloadSounds
			add  esp, 14h
			retn
		}
	}

	// @new — replaces the two `call sub_662FF0` of CG_EntityEvent case EV_RELOAD_START.
	// eventParm 1 = empty segmented start (set by PM_BeginWeaponReload).
	extern "C" void __cdecl CG_PlayReloadStartSound(int localClientNum, int entnum, snd_alias_list_t* aliasList,
		const WeaponDef* weapDef, int eventParm, int isPlayerView)
	{
		if (eventParm == 1 && weapDef && HasEmptyStart(weapDef))
			aliasList = isPlayerView ? weapDef->reloadStartEmptySoundPlayer : weapDef->reloadStartEmptySound;

		CG_PlayEntitySoundAlias(localClientNum, entnum, aliasList);
	}

	// @wrapper — call-site replacement: usercall(localClientNum@edx, entnum@ecx, aliasList@stack),
	// caller cleans. CG_EntityEvent frame (esp at the call = entry + 8): eventParm at +0x5C
	// (var_64), weapDef at +0x50 (var_70).
	__declspec(naked) void CG_PlayReloadStartSoundPlayer_Wrapper()
	{
		__asm
		{
			push 1                  ; isPlayerView
			push [esp+68h]          ; eventParm
			push [esp+60h]          ; weapDef
			push [esp+10h]          ; aliasList
			push ecx                ; entnum
			push edx                ; localClientNum
			call CG_PlayReloadStartSound
			add  esp, 18h
			retn
		}
	}

	__declspec(naked) void CG_PlayReloadStartSound_Wrapper()
	{
		__asm
		{
			push 0                  ; isPlayerView
			push [esp+68h]          ; eventParm
			push [esp+60h]          ; weapDef
			push [esp+10h]          ; aliasList
			push ecx                ; entnum
			push edx                ; localClientNum
			call CG_PlayReloadStartSound
			add  esp, 18h
			retn
		}
	}
}

// Init-time (Sys_RunInit): no engine prints here.
void PatchT4MAM_Reload()
{
	Detours::X86::DetourFunction(T4M::GetAddress("PM_ReloadClip"), (uintptr_t)&T4M::PM_ReloadClip_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("PM_Weapon_CheckFastReload"), (uintptr_t)&T4M::PM_Weapon_AllowReload_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Fill_Clip"), (uintptr_t)&T4M::Fill_Clip_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Add_Ammo"), (uintptr_t)&T4M::Add_Ammo_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("DrawClipAmmo"), (uintptr_t)&T4M::DrawClipAmmo_Wrapper, Detours::X86Option::USE_JUMP);

	// Empty segmented reload start. PM_BeginWeaponReload replaces the lowReady midhook
	// that used to sit on the same entry (CG_ReloadStartCheck_hook).
	Detours::X86::DetourFunction(T4M::GetAddress("PM_BeginWeaponReload"), (uintptr_t)&T4M::PM_BeginWeaponReload_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("PM_SetWeaponReloadAddAmmoDelay"), (uintptr_t)&T4M::PM_SetWeaponReloadAddAmmoDelay, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("PM_Weapon_RefreshReload"), (uintptr_t)&T4M::PM_Weapon_ReloadDelayedAction_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("PM_Weapon_TickRecoil"), (uintptr_t)&T4M::PM_Weapon_CheckForReload_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("CG_StopWeaponReloadSounds"), (uintptr_t)&T4M::CG_StopWeaponReloadSounds_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("CG_EntityEvent_ReloadStartSoundPlayer_callsite"), (uintptr_t)&T4M::CG_PlayReloadStartSoundPlayer_Wrapper, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction(T4M::GetAddress("CG_EntityEvent_ReloadStartSound_callsite"), (uintptr_t)&T4M::CG_PlayReloadStartSound_Wrapper, Detours::X86Option::USE_CALL);
}
