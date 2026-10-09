// ==========================================================
// T4M project
//
// Purpose: load MP map fastfiles (mp_*.ff) in the SP executable.
//
//   SP fetches world assets as "maps/<mapname>.d3dbsp"; MP zones name them
//   "maps/mp/<mapname>.d3dbsp" and ship game_map_mp instead of game_map_sp
//   (same 0x2C layout, sub_47AEA0 sizes both). DB_AddXAsset is the single
//   entry point of every zone asset, so both are fixed there before linking.
// ==========================================================

#include "StdInc.h"
#include "T4.h"

namespace
{
	// @new - "maps/mp/X.d3dbsp" -> "maps/X.d3dbsp", in place (zone memory). Idempotent.
	void StripMpBspPrefix(char* name)
	{
		static const char kPrefix[] = "maps/mp/";
		static const char kExt[] = ".d3dbsp";

		if (_strnicmp(name, kPrefix, sizeof(kPrefix) - 1) != 0)
			return;

		const size_t len = strlen(name);
		if (len < sizeof(kExt) - 1 || _stricmp(name + len - (sizeof(kExt) - 1), kExt) != 0)
			return;

		char* tail = name + sizeof("maps/") - 1;
		memmove(tail, tail + 3, strlen(tail + 3) + 1);
	}
}

// @modified - sub_48E300 (DB_AddXAsset)
void* __cdecl T4M::DB_AddXAsset(int type, void* header)
{
	T4::engine::XAssetHeader h;
	h.data = header;
	// World assets may share one name string, so the strip can already be done here.
	StripMpBspPrefix(const_cast<char*>(T4::engine::DB_XAssetGetNameHandlers[type](&h)));
	// SP has no game_map_mp pool; the layout is game_map_sp's.
	if (type == T4::engine::ASSET_TYPE_GAME_MAP_MP)
		type = T4::engine::ASSET_TYPE_GAME_MAP_SP;

	XAssetEntry newEntry;
	newEntry.asset.type = (decltype(newEntry.asset.type))type;
	newEntry.asset.header.data = header;

	T4M::DB_WriterAcquire();                                   // sub_48D020 (esi=g_dbReaderCount)
	XAssetEntry* existing = T4_Reconstructed::DB_LinkXAssetEntryOverrideAware(&newEntry, 0);
	T4M::DB_WriterRelease();                                   // lock xadd [g_dbWriterCount], -1

	volatile uint8_t* isRecovering = (uint8_t*)T4::engine::g_isRecoveringLostDevice;
	if (*isRecovering)
	{
		*(volatile uint8_t*)(uint8_t*)T4::engine::g_mayRecoverLostAssets = 1;
		do
			Sleep(25);
		while (*isRecovering);
	}

	return existing->asset.header.data;
}

// @wrapper - sub_48E300: eax = type, arg_0 (stack) = header. Vanilla exits with plain
// `retn`; callers clean their arg (`add esp, 4`).
__declspec(naked) void T4M::DB_AddXAsset_Wrapper()
{
	__asm {
		push    dword ptr [esp+4]
		push    eax
		call    T4M::DB_AddXAsset
		add     esp, 8
		retn
	}
}

void PatchT4MAM_MpMapInSp()
{
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_AddXAsset"), (uintptr_t)&T4M::DB_AddXAsset_Wrapper, Detours::X86Option::USE_JUMP);
}
