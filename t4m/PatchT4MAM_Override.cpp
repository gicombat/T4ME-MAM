#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>

// =====================================================================
// Debug utilities — all @new (T4M inventions, no vanilla equivalent)
// =====================================================================

// @new — debug utility
__declspec(noinline)
void T4M::DB_DumpOverrideChain(int type, const char* name)
{
	XAssetEntry* head = T4_Reconstructed::DB_FindXAssetByName(type, name);
	if (!head)
	{
		T4::engine::Com_Printf(0, "[T4M] '%s' (type %d) introuvable\n", name, type);
		return;
	}

	T4::engine::Com_Printf(0, "[T4M] Override chain for '%s' (type %s):\n",	name, T4M::DB_GetXAssetTypeName(type));

	XAssetEntry* cur = head;
	int depth = 0;
	while (cur)
	{
		T4::engine::Com_Printf(0, "  [%d] zone=%u (%s) prio=%d inuse=%d header=%p\n",
			depth,
			cur->zoneIndex,
			T4::engine::g_zoneFileNames[cur->zoneIndex].name,
			T4M::ZonePriority(cur->zoneIndex),
			cur->inuse ? 1 : 0,
			cur->asset.header.data);

		if (cur->nextOverride == 0) 
			break;

		cur = T4M::POOL_ENTRY(cur->nextOverride);
		++depth;
	}
}

// @new — debug utility
__declspec(noinline)
void T4M::DB_DumpHashBucket(unsigned int bucket)
{
	if (bucket >= 0x8000) 
	{ 
		T4::engine::Com_Printf(0, "[T4M] bucket out of range\n");
		return; 
	}

	unsigned short idx = T4::engine::db_hashTable[bucket];
	T4::engine::Com_Printf(0, "[T4M] Bucket %u:\n", bucket);
	int depth = 0;
	while (idx != 0)
	{
		XAssetEntry* e = T4M::POOL_ENTRY(idx);
		T4::engine::Com_Printf(0, "  [%d] type=%d (%s) name='%s' zone=%u nextHash=%u nextOvr=%u\n",
			depth,
			e->asset.type,
			T4M::DB_GetXAssetTypeName(e->asset.type),
			T4M::GetName(e),
			e->zoneIndex,
			e->nextHash,
			e->nextOverride);

		idx = e->nextHash;
		++depth;
	}
}

// @new — debug utility
__declspec(noinline)
int T4M::DB_CountActiveEntries()
{
	int count = 0;
	for (int b = 0; b < 0x8000; ++b)
	{
		unsigned short idx = T4::engine::db_hashTable[b];
		while (idx != 0)
		{
			++count;
			XAssetEntry* e = T4M::POOL_ENTRY(idx);
			// also count nextOverride nodes
			unsigned short ov = e->nextOverride;
			while (ov != 0)
			{
				++count;
				ov = T4M::POOL_ENTRY(ov)->nextOverride;
			}
			idx = e->nextHash;
		}
	}
	return count;
}

bool T4M::resetFakeIntroSecondValue = false;
int	T4M::timeAtMapStart = 0;

// =====================================================================
// @modified — sub_44B7E0 (FAKE_INTRO_SECONDS argument formatter, 0x0044B7E0)
// __usercall: eax=argStr, [esp+4]=outBuf (4-byte dest). Hooked via wrapper.
// =====================================================================
void T4M::Key_FormatIntroSeconds(const char* argStr, char* outBuf)
{
	int value = 0;
	T4::engine::Com_sscanf(argStr, "%d", &value);

	if (T4M::resetFakeIntroSecondValue)
	{
		T4M::timeAtMapStart = *T4::engine::com_frameTime;
		T4M::resetFakeIntroSecondValue = false;
	}

	if (value < 0 || value > 40)
	{
		T4::engine::Com_PrintError(1,
			"Argument \"%s\" given for FAKE_INTRO_SECONDS is outside the acceptable range of (%d,%d).",
			argStr, 0, 40);
		value = 0;
	}

	int frameTimeAtMapStart = *T4::engine::com_frameTime - T4M::timeAtMapStart;

	value += (int)(frameTimeAtMapStart / 1000u);   // 0x00351DF34 — vanilla path

	T4::engine::Com_sprintf(outBuf, 4, "%02d", (DWORD)value);
}

// =====================================================================
// @new - FAKE_INTRO_FULL_TIME, parse an full hour, with value XX_XX_XX and add timer
// In the same manner as FAKE_INTRO_SECONDS but for a bigger value
// =====================================================================
void T4M::Key_FormatIntroHourMinSec(const char* argStr, char* outBuf)
{
	int hour = 0;
	int minutes = 0;
	int second = 0;

	std::vector<std::string> splittedString = split(argStr, "_");

	if (splittedString.size() != 3)
	{
		T4::engine::Com_PrintError(1, "Incorrect number of argument FAKE_INTRO_FULL_TIME was given, it should be 3 value with '_' delemiter");
	}

	if (T4M::resetFakeIntroSecondValue)
	{
		T4M::timeAtMapStart = *T4::engine::com_frameTime;
		T4M::resetFakeIntroSecondValue = false;
	}

	int frameTimeAtMapStart = *T4::engine::com_frameTime - T4M::timeAtMapStart;

	T4::engine::Com_sscanf(splittedString[0].c_str(), "%d", &hour);
	T4::engine::Com_sscanf(splittedString[1].c_str(), "%d", &minutes);
	T4::engine::Com_sscanf(splittedString[2].c_str(), "%d", &second);

	int finalHourWithSecond = second + minutes * 60 + hour * 60 * 60;

	finalHourWithSecond += (int)(frameTimeAtMapStart / 1000u);   

	// TO DO, FINISH HERE
	// probably addition all value to have the time, then add the frameTimeAtMapStart
	// And split again to retrieve hour,minue and second and format at the final value.
	// use std::chrono::
	// Still need to hook to the root function of the Key_FormatIntroSeconds so we can add this things

	//T4::engine::Com_sprintf(outBuf, 4, "%02d:%02d:%02d", (DWORD)hour, (DWORD)minutes, (DWORD)second);
}

// =====================================================================
// __usercall → __cdecl wrappers
//
//   Vanilla sub_48D7D0 and sub_48D760 use an __usercall convention with
//   an argument in edi (register). Our reconstructions
//   T4_Reconstructed::DB_FindDefaultAsset / T4_Reconstructed::DB_FindXAssetByName
//   are __cdecl. To detour 0x48D7D0 and 0x48D760 cleanly, we interpose a
//   naked stub that pushes the register args onto the stack.
//
//   NOTE on inline asm: __asm `call SYMBOL` resolves via flat linker symbol.
//   Thanks to extern "C" on the reconstructions, their mangled name is just
//   `_DB_FindDefaultAsset` etc. → the bare identifier in `call` works.
//
//   Vanilla:                               cdecl reconstruction:
//   ─────────                              ─────────────────────
//   sub_48D7D0(edi=type)                   DB_FindDefaultAsset(int type)
//   sub_48D760(arg_0=type, edi=name)       DB_FindXAssetByName(int type, const char* name)
//
//   Return: eax = result (same convention vanilla / cdecl).
// =====================================================================

// @wrapper — __usercall→__cdecl bridge to T4_Reconstructed::DB_FindDefaultAsset (0x48D7D0)
// sub_48D7D0: edi = type
__declspec(naked) void T4M::DB_FindDefaultAsset_Wrapper()
{
	__asm {
		push    edi                           ; arg_0 = type
		call    T4_Reconstructed::DB_FindDefaultAsset           ; cdecl (flat extern "C" symbol), result in eax
		add     esp, 4
		ret
	}
}

// @wrapper — __usercall→__cdecl bridge to T4_Reconstructed::DB_FindXAssetByName (0x48D760)
// sub_48D760: arg_0 (stack) = type, edi = name
// On entry: [esp+0] = return addr, [esp+4] = type, edi = name
__declspec(naked) void T4M::DB_FindXAssetByName_Wrapper()
{
	__asm {
		push    edi                           ; arg_1 = name (edi)
		push    dword ptr [esp+8]             ; arg_0 = type (read from original stack, +8 after push edi)
		call    T4_Reconstructed::DB_FindXAssetByName           ; cdecl (flat extern "C" symbol), result in eax
		add     esp, 8                         ; clean our 2 pushes
		ret                                     ; caller cleans its arg (cdecl)
	}
}

// @wrapper — __usercall→__cdecl bridge to T4_Reconstructed::DB_LinkXAssetEntry (0x48D860)
// sub_48D860: eax = type (register), arg_0 (stack) = name
// On entry: [esp+0] = return addr, [esp+4] = name, eax = type
// Vanilla exits with `retn` (NO arg cleanup) — callers (sub_48DA30 loc_48DDF2,
// sub_48DFF0) clean with `add esp, 4`. `retn 4` would double-clean and shift
// caller ESP by +4, corrupting every frame up the chain.
__declspec(naked) void T4M::DB_LinkXAssetEntry_Wrapper()
{
	__asm {
		push    dword ptr [esp+4]             ; arg_1 = name (original arg_0)
		push    eax                           ; arg_0 = type (from register)
		call    T4_Reconstructed::DB_LinkXAssetEntry            ; cdecl (flat extern "C" symbol), result in eax
		add     esp, 8                         ; clean our 2 pushes
		retn                                   ; caller cleans its name arg (vanilla)
	}
}

// @wrapper — __usercall→__cdecl bridge to T4M::Key_FormatIntroSeconds (0x44B7E0)
// sub_44B7E0: eax = argStr (register), arg_0 (stack) = outBuf
// On entry: [esp+0] = return addr, [esp+4] = outBuf, eax = argStr
// Vanilla exits with `retn` (NO arg cleanup) — caller (sub_44B8E0 loc_44B91E)
// cleans with `add esp, 4`. `retn 4` would double-clean → corruption stack.
__declspec(naked) void T4M::Key_FormatIntroSeconds_Wrapper()
{
	__asm {
		push    dword ptr [esp+4]             ; arg_1 = outBuf (original arg_0)
		push    eax                           ; arg_0 = argStr (from register)
		call    T4M::Key_FormatIntroSeconds                     ; cdecl (flat extern "C" symbol)
		add     esp, 8                         ; clean our 2 pushes
		retn                                   ; caller cleans its outBuf arg (vanilla)
	}
}

// =====================================================================
// PatchT4_Override — installation
// =====================================================================
void PatchT4MAM_Override()
{
	// Active detours: redirect the vanilla addresses to our C++
	// reconstructions (in T4.cpp).
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_UnloadZoneAssets"), (uintptr_t)&T4_Reconstructed::DB_UnloadZoneAssets, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_FindDefaultAsset"), (uintptr_t)&T4M::DB_FindDefaultAsset_Wrapper, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_FindXAssetByName"), (uintptr_t)&T4M::DB_FindXAssetByName_Wrapper, Detours::X86Option::USE_JUMP);

	// TEMPORAIRE — ce détour sera retiré une fois que tous les appelants
	// seront remplacés par des fonctions T4M full-C++ qui appellent directement
	// T4_Reconstructed::DB_LinkXAssetEntry en __cdecl. À terme le wrapper et ce détour ne seront plus nécessaires.
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_LinkXAssetEntry"), (uintptr_t)&T4M::DB_LinkXAssetEntry_Wrapper, Detours::X86Option::USE_JUMP);

	// @modified — sub_44B7E0 → reconstruction T4M (cl.serverTime au lieu de
	// com_frameTime, pour reset du compteur entre scènes).
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Key_FormatIntroSeconds"), (uintptr_t)&T4M::Key_FormatIntroSeconds_Wrapper, Detours::X86Option::USE_JUMP);

	// SafetyHook on sub_62BEB0 — fast_restart server cmd handler.
	static auto SV_FastRestart = safetyhook::create_mid(T4M::GetAddress("SV_FastRestart"), [](SafetyHookContext& ctx)
	{
		T4M::resetFakeIntroSecondValue = true;
	});

	// TEMPORAIRE — 0x0048DFF0
	 Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_LinkXAssetEntryOverrideAware"), (uintptr_t)&T4_Reconstructed::DB_LinkXAssetEntryOverrideAware, Detours::X86Option::USE_JUMP);

	// Full vanilla-faithful reconstruction of DB_FindXAssetHeader (sub_48DA30).
	 Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("DB_FindXAssetHeader"), (uintptr_t)&T4_Reconstructed::DB_FindXAssetHeader, Detours::X86Option::USE_JUMP);
}
