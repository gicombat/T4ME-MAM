// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Friendly-name overlay. Reconstruction of
//          G_UpdateFriendlyOverlay (sub_561D30) + health-based
//          coloring of the displayed name.
//
// RE   : "Claude R&D/T4M CoD WaW/analysis/friendly_name_overlay_RE.md"
// Plan : "Claude R&D/T4M CoD WaW/plans/plan_friendly_name_health_color.md"
//
// Server side writes CS[2*clientNum + 0x10D] (name) and +0x10E (weapon /
// subtitle); CG_DrawFriendOverlay (0x437F40) renders them with the color held
// in friendlyNameFontColor. Coloring writes that dvar's vector, so no
// client-side code is touched.
//
// The ramp has five stops — 100% is friendlyNameFontColor itself (snapshotted,
// so full health is bit-identical to vanilla), then friendlyNameHealthColor75 /
// 50 / 25 / 0, registered as DVAR_TYPE_VEC4 in PatchT4_Console.
//
// Vanilla symbols come from the cod/ headers, the T4M dvars from T4.h
// (registered in PatchT4_Console). Nothing is resolved with T4M::GetAddress
// here except the detour target itself.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <math.h>

using namespace T4::engine;
using namespace T4::game;

namespace
{
	// GSC field opting a single entity in. Lowercase mandatory: the compiler
	// lowercases field names before canonicalizing them.
	const char* const FIELD_HEALTH_OVERLAY = "color_health_overlay";

	// Format vanilla uses for the *name* line only: bytes 15 25 73 00 at
	// 0x008183F8 (SP) / 0x00818408 (ger) -> "\x15%s". Other lines use plain "%s".
	const char* const NAME_FORMAT = "\x15%s";

	bool  colorOverridden = false;
	float savedColor[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };

	// SL_ConvertToString, inlined by vanilla exactly like this (same arithmetic as T4.cpp).
	const char* SL_Str(unsigned short id)
	{
		if (!id)
			return nullptr;
		return (const char*)gScrMemTreePub->mt_buffer + id * 12 + 4;
	}

	// @new — per-entity opt-in read from the GSC variable table.
	//
	// Same path the VM takes for OP_EvalFieldVariable: FindVariable() then
	// Scr_EvalVariable(). Scr_FindVariableField() is deliberately NOT used — on a
	// missing field of a VAR_ENTITY it falls through to the built-in field getters,
	// which touch the VM stack and can raise Scr_Error.
	bool EntityWantsHealthColor(int entnum)
	{
		// Canonical ids are wiped by SL_BeginLoadScripts on every script load, so
		// this is resolved per call (a hash lookup once the string exists).
		const unsigned int name = SL_GetCanonicalString(FIELD_HEALTH_OVERLAY, SCRIPTINSTANCE_SERVER);
		if (!name)
			return false;

		const unsigned int entId = Scr_GetEntityId(entnum, SCRIPTINSTANCE_SERVER, CLASS_NUM_ENTITY, 0);
		if (!entId)
			return false;

		const unsigned int id = FindVariable(name, entId, SCRIPTINSTANCE_SERVER);
		if (!id)
			return false;

		VariableValue value = Scr_EvalVariable(SCRIPTINSTANCE_SERVER, id);

		bool enabled = false;
		if (value.type == VAR_INTEGER)
			enabled = (value.u.intValue != 0);
		else if (value.type == VAR_FLOAT)
			enabled = (value.u.floatValue != 0.0f);

		// Scr_EvalVariable AddRefToValue's its result — release whatever it took.
		RemoveRefToValue(SCRIPTINSTANCE_SERVER, &value);

		return enabled;
	}

	void RestoreColor()
	{
		if (!colorOverridden)
			return;

		if (dvar_t* full = *friendlyNameFontColor)
		{
			for (int i = 0; i < 4; ++i)
				full->current.vec4[i] = savedColor[i];
		}

		colorOverridden = false;
	}

	// Ramp stops below 100%, in descending health order. The 100% stop is the
	// snapshot of friendlyNameFontColor itself, so full health stays bit-identical
	// to vanilla and honours a player-side "set friendlyNameFontColor".
	const int RAMP_SEGMENTS = 4;   // 100->75->50->25->0, evenly spaced by 0.25

	// Piecewise-linear ramp: snapshot -> 75% -> 50% -> 25% -> 0%.
	void ApplyColor(float frac)
	{
		dvar_t* full = *friendlyNameFontColor;
		if (!full)
			return;

		dvar_t* const stops[RAMP_SEGMENTS] = {
			friendlyNameHealthColor75,
			friendlyNameHealthColor50,
			friendlyNameHealthColor25,
			friendlyNameHealthColor0,
		};
		for (int i = 0; i < RAMP_SEGMENTS; ++i)
			if (!stops[i])
				return;

		// Snapshot before the first write so the player's own setting survives.
		if (!colorOverridden)
		{
			for (int i = 0; i < 4; ++i)
				savedColor[i] = full->current.vec4[i];
			colorOverridden = true;
		}

		// t = 0 at full health, 1 at dead. The curve warps the ramp position only.
		float t = 1.0f - frac;
		const float curve = friendlyNameHealthColorPow ? friendlyNameHealthColorPow->current.value : 1.0f;
		if (curve != 1.0f && t > 0.0f)
			t = powf(t, curve);

		const float scaled = t * static_cast<float>(RAMP_SEGMENTS);
		int seg = static_cast<int>(scaled);
		if (seg < 0)
			seg = 0;
		else if (seg > RAMP_SEGMENTS - 1)
			seg = RAMP_SEGMENTS - 1;
		const float k = scaled - static_cast<float>(seg);

		// Segment 0 starts on the snapshot, every later one on the previous stop.
		const float* const from = seg ? stops[seg - 1]->current.vec4 : savedColor;
		const float* const to   = stops[seg]->current.vec4;

		for (int i = 0; i < 4; ++i)
			full->current.vec4[i] = from[i] + (to[i] - from[i]) * k;
	}

	// @new — health -> name color, driven by the entity the player is looking at.
	void UpdateHealthColor(unsigned short lookNum, int clientNum)
	{
		// CG_DrawFriendOverlay only ever renders the CS pair of cgArray[0].clientNum,
		// so coloring for any other client would just fight over the same dvar.
		if (clientNum != cgArray->clientNum)
			return;

		if (!lookNum)
		{
			RestoreColor();
			return;
		}

		gentity_s* target = &g_entities[lookNum - 1];

		// Vehicles and script entities carry no meaningful health.
		if (target->maxhealth <= 0)
		{
			RestoreColor();
			return;
		}

		const bool all = friendlyNameHealthColorAll && friendlyNameHealthColorAll->isEnabled();
		if (!all && !EntityWantsHealthColor(lookNum - 1))
		{
			RestoreColor();
			return;
		}

		int hp = target->health;
		if (hp < 0)
			hp = 0;

		float frac = static_cast<float>(hp) / static_cast<float>(target->maxhealth);
		if (frac > 1.0f)
			frac = 1.0f;

		ApplyColor(frac);
	}
}

namespace T4M
{
	// -----------------------------------------------------------------------
	// @modified — vanilla sub_561D30 reconstructed branch for branch, plus the
	// health-coloring block. Enriched behavior, so T4M:: and not
	// T4_Reconstructed::. Vanilla is __usercall(ecx = ent); the wrapper below
	// feeds this cdecl body. extern "C" -> flat linker symbol for that wrapper.
	// DETOURED — do not call directly.
	// -----------------------------------------------------------------------
	extern "C" void __cdecl G_UpdateFriendlyOverlay(gentity_s* ent)
	{
		gclient_s* client = ent->client;

		const int clientNum = client->ps.clientNum;
		const int CS_NAME   = 2 * clientNum + 0x10D;
		const int CS_LAST   = 2 * clientNum + 0x10E;

		// EntHandle.number is 1-based; 0 means "not looking at anything".
		const unsigned short lookNum = client->pLookatEnt.number;

		// @modified — no vanilla equivalent.
		UpdateHealthColor(lookNum, clientNum);

		if (!lookNum)
		{
			configstringBundle pair;
			pair.index = CS_NAME;
			pair.val   = const_cast<char*>(Com_FormatMsg("%s", "none"));
			SV_SetConfigstrings(&pair, 1);
			return;
		}

		gentity_s* target = &g_entities[lookNum - 1];
		actor_s*   actor  = target->actor;

		// Branch 1 — real AI with a proper name. Wins over any setlookattext.
		if (actor && actor->properName)
		{
			configstringBundle name;
			name.index = CS_NAME;
			name.val   = const_cast<char*>(Com_FormatMsg(NAME_FORMAT, SL_Str(static_cast<unsigned short>(actor->properName))));
			SV_SetConfigstrings(&name, 1);

			const char* weapName = SL_Str(static_cast<unsigned short>(target->actor->weaponName));
			const int   weapIdx  = *g_precacheOpen
				? BG_FindWeaponIndex_Internal(weapName, (void*)BG_LoadWeaponByIndex.get())
				: BG_FindWeaponIndex(weapName);

			configstringBundle overlay;
			overlay.index = CS_LAST;
			overlay.val   = const_cast<char*>(Com_FormatMsg("%s", bg_weaponDefs[weapIdx]->szOverlayName));
			SV_SetConfigstrings(&overlay, 1);
			return;
		}

		// Branch 2 — vehicle.
		if (target->s.eType == ET_VEHICLE)
		{
			scr_vehicle_s* veh = target->scr_vehicle;

			if (!veh->lookAtText0)
			{
				SV_SetConfigstring(CS_NAME, "none");
				return;
			}

			SV_SetConfigstring(CS_NAME, Com_FormatMsg(NAME_FORMAT, SL_Str(veh->lookAtText0)));

			const unsigned short second = target->scr_vehicle->lookAtText1;
			if (!second)
			{
				SV_SetConfigstring(CS_LAST, "none");
				return;
			}

			SV_SetConfigstring(CS_LAST, SL_Str(second));
			return;
		}

		// Branch 3 — plain entity carrying setlookattext text.
		if (!target->lookAtText0)
		{
			SV_SetConfigstring(CS_NAME, Com_FormatMsg("%s", "none"));
			return;
		}

		SV_SetConfigstring(CS_NAME, Com_FormatMsg(NAME_FORMAT, SL_Str(static_cast<unsigned short>(target->lookAtText0))));

		const unsigned short second = static_cast<unsigned short>(target->lookAtText1);
		if (!second)
		{
			SV_SetConfigstring(CS_LAST, "none");
			return;
		}

		SV_SetConfigstring(CS_LAST, SL_Str(second));
	}

	// @wrapper — __usercall(ecx = ent) -> cdecl. Detour target for sub_561D30.
	__declspec(naked) void G_UpdateFriendlyOverlay_Wrapper()
	{
		__asm
		{
			push ecx
			call G_UpdateFriendlyOverlay
			add  esp, 4
			retn
		}
	}
}

// ---------------------------------------------------------------------------
// Install. Init-time (Sys_RunInit): NO engine prints here.
// The T4M dvars this reads are registered by PatchT4_Console (T4.h externs).
// ---------------------------------------------------------------------------
void PatchT4MAM_FriendOverlay()
{
	const uintptr_t target = T4M::GetAddress("G_UpdateFriendlyOverlay");
	if (!target)
		return;

	Detours::X86::DetourFunction(target,
	                             (uintptr_t)&T4M::G_UpdateFriendlyOverlay_Wrapper,
	                             Detours::X86Option::USE_JUMP);
}
