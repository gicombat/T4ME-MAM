// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 9 - the client compass keeps its own per-actor array,
//          s_compassActors[localClient][36]: entries 0..31 are indexed by actor index,
//          32..35 by client number + 32. Past 32 actors the pings overwrote the player
//          slots, and past 36 they ran into the next compass struct (0x907E98), while the
//          draw loop only ever showed 36. The array moves to a T4M buffer of
//          NEW_MAX_ACTORS + SP_MAX_CLIENTS entries, players at NEW_MAX_ACTORS.
//          The two small writers are reconstructed and detoured; the draw, clear and
//          savegame-restore functions are large and only get their constants rewritten.
//
//          R&D analysis/actor_sentientinfo_sidetable.md (phase 9)
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"
#include "MemoryMgr.h"

using T4::engine::centity_s;
using T4::engine::CompassActor;

#define VANILLA_COMPASS_ENTRIES (VANILLA_MAX_ACTORS + SP_MAX_CLIENTS)
#define COMPASS_ENTRIES         (NEW_MAX_ACTORS + SP_MAX_CLIENTS)

// The draw loop bound is "cmp eax, imm8" + signed jl.
static_assert(COMPASS_ENTRIES <= 127, "compass count is an imm8 in sub_434D80");
static_assert(VANILLA_COMPASS_ENTRIES == 36, "vanilla s_compassActors[1][36]");
static_assert(sizeof(CompassActor) == 0x18, "CompassActor layout");
static_assert(offsetof(centity_s, pose) + offsetof(T4::engine::cpose_t, origin) == 0x24, "centity_s::pose.origin");
static_assert(offsetof(centity_s, nextState) == 0xD0, "centity_s::nextState");
static_assert(offsetof(T4::engine::actorInfo_t, team) == 0x30, "actorInfo_t::team");
static_assert(offsetof(T4::engine::clientInfo_t, team) == 0x2C, "clientInfo_t::team");

namespace
{
	// SP has a single local client; the vanilla per-client stride is kept all the same.
	CompassActor g_compassActors[COMPASS_ENTRIES];

	// Offsets read on both centity_s (0x2D4) and the high-entity element (0x31C).
	constexpr int CENT_NUMBER       = 0xD0;    // nextState.number
	constexpr int CENT_ETYPE        = 0xD4;    // nextState.eType
	constexpr int CENT_ACTOR_INDEX  = 0x124;   // nextState.lerp.u.actor.index
	constexpr int CENT_ORIGIN_X     = 0x24;    // pose.origin[0]
	constexpr int CENT_ORIGIN_Y     = 0x28;    // pose.origin[1]
	constexpr int CENT_YAW          = 0x34;    // pose.angles[1]

	constexpr int ET_PLAYER_COMPASS = 1;
	constexpr int ET_ACTOR_COMPASS  = 0x10;
	constexpr int ET_SKIP_PING      = 0x12;

	template <typename T> T& At(void* base, int ofs) { return *reinterpret_cast<T*>(static_cast<BYTE*>(base) + ofs); }

	// Own entry in cg.bgs.clientInfo[] - vanilla skips its own player.
	T4::engine::clientInfo_t* LocalClientInfo()
	{
		T4::engine::cg_s* cg = T4::engine::cgArray;
		return &cg->bgs.clientInfo[cg->nextSnap->ps.clientNum];
	}

	struct ImmPatch
	{
		const char* key;
		BYTE        immOfs;
		BYTE        immSize;
		DWORD       expect;
		DWORD       value;
	};
}

// @modified — sub_434A80 / CoD4 cgame/cg_compassfriendlies.cpp CG_CompassAddWeaponPingInfo.
//   1:1 with vanilla except the entry comes from the grown T4M array (COMPASS_ENTRIES per
//   local client). Players keep vanilla's index (clientNum, without the +32 used elsewhere).
//   DETOURED — do not call directly.
void T4_Reconstructed::CG_CompassAddWeaponPingInfo(int localClientNum, centity_s* cent, const float* origin, int msec)
{
	const int eType = At<int>(cent, CENT_ETYPE);
	if (eType == ET_SKIP_PING)
		return;

	int index;
	int team;
	if (eType == ET_ACTOR_COMPASS)
	{
		index = At<int>(cent, CENT_ACTOR_INDEX);
		team  = T4M::CgActorInfo(index)->team;
	}
	else if (eType == ET_PLAYER_COMPASS)
	{
		index = At<int>(cent, CENT_NUMBER);
		T4::engine::clientInfo_t* ci = &T4::engine::cgArray->bgs.clientInfo[index];
		if (LocalClientInfo() == ci)
			return;
		team = ci->team;
	}
	else
	{
		return;
	}

	if (team == 3 || team == 4)
		return;

	CompassActor* entry = &g_compassActors[localClientNum * COMPASS_ENTRIES + index];
	entry->beginFadeTime = T4::engine::cgArray->time + msec;
	entry->enemy = (team == 1);
	if (team == 1)
	{
		entry->lastPos[0] = origin[0];
		entry->lastPos[1] = origin[1];
	}
}

// @modified — sub_434B30 / CoD4 CG_CompassUpdateActorInfo. 1:1 with vanilla except:
//   the grown T4M array, players at NEW_MAX_ACTORS instead of 32, and the actor-index sign
//   test moved before the team read (vanilla reads actorinfo[-1] first and discards it; the
//   relocated array has no mapped memory in front of it).
//   Other entity types fall through to entry 0 with team 3, exactly like vanilla.
//   DETOURED — do not call directly.
void T4_Reconstructed::CG_CompassUpdateActorInfo(int localClientNum, int entityIndex)
{
	const int slot = (localClientNum << 10) + entityIndex;
	BYTE* cent = (entityIndex < 0x400)
		? reinterpret_cast<BYTE*>(static_cast<centity_s*>(T4::engine::cg_entities) + slot)
		: static_cast<BYTE*>(T4::engine::cg_entitiesHigh) + slot * 0x31C;

	int index = 0;
	int team  = 3;
	const int eType = At<int>(cent, CENT_ETYPE);
	if (eType == ET_ACTOR_COMPASS)
	{
		index = At<int>(cent, CENT_ACTOR_INDEX);
		if (index < 0)
			return;
		team = T4M::CgActorInfo(index)->team;
	}
	else if (eType == ET_PLAYER_COMPASS)
	{
		index = At<int>(cent, CENT_NUMBER);
		T4::engine::clientInfo_t* ci = &T4::engine::cgArray->bgs.clientInfo[index];
		if (LocalClientInfo() == ci)
			return;
		team = ci->team;
		index += NEW_MAX_ACTORS;   // @modified: vanilla adds 0x20
	}

	CompassActor* entry = &g_compassActors[localClientNum * COMPASS_ENTRIES + index];
	entry->lastUpdate = T4::engine::cgArray->time;
	entry->lastPos[0] = At<float>(cent, CENT_ORIGIN_X);
	entry->lastPos[1] = At<float>(cent, CENT_ORIGIN_Y);
	entry->lastYaw    = At<float>(cent, CENT_YAW);
	entry->enemy      = (team == 1);
}

// @wrapper — usercall(localClientNum@eax, cent@ecx, origin@edi, msec@stack) ; plain retn,
//   caller cleans. ecx/edx preserved: vanilla's early-out paths leave them untouched.
__declspec(naked) void T4M::CG_CompassAddWeaponPingInfo_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
		push    dword ptr [esp+0Ch]         ; msec (original arg_0)
		push    edi                         ; origin
		push    ecx                         ; cent
		push    eax                         ; localClientNum
		call    T4_Reconstructed::CG_CompassAddWeaponPingInfo
		add     esp, 10h
		pop     edx
		pop     ecx
		retn
	}
}

// @wrapper — usercall(localClientNum@edi, entityIndex@ecx) ; plain retn, no stack args.
__declspec(naked) void T4M::CG_CompassUpdateActorInfo_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
		push    ecx                         ; entityIndex
		push    edi                         ; localClientNum
		call    T4_Reconstructed::CG_CompassUpdateActorInfo
		add     esp, 8
		pop     edx
		pop     ecx
		retn
	}
}

void PatchT4MAM_CompassActors()
{
	const DWORD vanillaBase = static_cast<DWORD>(T4M::GetAddress("s_compassActors"));
	const DWORD newBase     = reinterpret_cast<DWORD>(g_compassActors);
	const DWORD vanillaSize = VANILLA_COMPASS_ENTRIES * sizeof(CompassActor);
	const DWORD newSize     = COMPASS_ENTRIES * sizeof(CompassActor);

	const ImmPatch patches[] =
	{
		// sub_434D80 (compass draw): per-client stride, base, actor/player split, both loop counts.
		{ "compass_DrawStride_434E88",      2, 4, vanillaSize,             newSize },
		{ "compass_DrawBase_434EC0",        2, 4, vanillaBase,             newBase },
		{ "compass_DrawPlayerSplit_43520E", 4, 1, VANILLA_MAX_ACTORS,      NEW_MAX_ACTORS },
		{ "compass_DrawCount_43537C",       2, 1, VANILLA_COMPASS_ENTRIES, COMPASS_ENTRIES },
		{ "compass_DrawCount2_4353A0",      1, 4, VANILLA_COMPASS_ENTRIES, COMPASS_ENTRIES },
		// sub_63BF10 (client savegame restore): clamps every lastUpdate.
		{ "compass_LoadStride_63C028",      2, 4, vanillaSize,             newSize },
		{ "compass_LoadBase_63C02E",        1, 4, vanillaBase,             newBase },
		{ "compass_LoadCount_63C039",       1, 4, VANILLA_COMPASS_ENTRIES, COMPASS_ENTRIES },
		// sub_664570 / sub_6650F0: memset of the whole array.
		{ "compass_ClearSize_664611",       1, 4, vanillaSize,             newSize },
		{ "compass_ClearBase_664617",       1, 4, vanillaBase,             newBase },
		{ "compass_ClearSize_665402",       1, 4, vanillaSize,             newSize },
		{ "compass_ClearBase_665408",       1, 4, vanillaBase,             newBase },
	};

	// All-or-nothing: a half-moved array is worse than the vanilla one.
	if (!vanillaBase)
		return;
	for (const ImmPatch& p : patches)
	{
		const BYTE* at = reinterpret_cast<const BYTE*>(T4M::GetAddress(p.key));
		if (!at)
			return;
		at += p.immOfs;
		const DWORD cur = (p.immSize == 1) ? *at : *reinterpret_cast<const DWORD*>(at);
		if (cur != p.expect)
			return;
	}

	for (const ImmPatch& p : patches)
	{
		BYTE* at = reinterpret_cast<BYTE*>(T4M::GetAddress(p.key) + p.immOfs);
		if (p.immSize == 1)
			Memory::VP::Patch<BYTE>(at, static_cast<BYTE>(p.value));
		else
			Memory::VP::Patch<DWORD>(at, p.value);
	}

	Detours::X86::DetourFunction(T4M::GetAddress("CG_CompassAddWeaponPingInfo"),
	                             reinterpret_cast<uintptr_t>(&T4M::CG_CompassAddWeaponPingInfo_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("CG_CompassUpdateActorInfo"),
	                             reinterpret_cast<uintptr_t>(&T4M::CG_CompassUpdateActorInfo_Wrapper),
	                             Detours::X86Option::USE_JUMP);
}
