// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Installation of T4M detours for the asset DB system
//          + debug utilities (override-chain dumpers).
//
// The C++ reconstructions of the DB functions (T4_DB_HashAssetName,
// T4_DB_AllocXAssetEntry, T4_DB_FindXAssetByName, T4_DB_FindDefaultAsset,
// T4_DB_LinkXAssetEntry, T4_DB_LinkXAssetEntryOverrideAware,
// T4_DB_PromoteOverride, T4_DB_UnloadZoneAssets) live in T4.cpp.
//
// Here: naked wrappers for __usercall functions, debug dumpers, and
// PatchT4_Override() (the installer).
//
// Sources: CoDWaW_analysis.md §"Système d'override des assets",
//          CoDWaW LanFixed.exe.asm.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"

// =====================================================================
// Debug utilities — all @new (T4M inventions, no vanilla equivalent)
// =====================================================================

// @new — debug utility
__declspec(noinline)
void T4M_DB_DumpOverrideChain(int type, const char* name)
{
	XAssetEntry* head = T4_DB_FindXAssetByName(type, name);
	if (!head)
	{
		Com_Printf(0, "[T4M] '%s' (type %d) introuvable\n", name, type);
		return;
	}

	Com_Printf(0, "[T4M] Override chain for '%s' (type %s):\n",
		name, T4M_DB_GetXAssetTypeName(type));

	XAssetEntry* cur = head;
	int depth = 0;
	while (cur)
	{
		Com_Printf(0, "  [%d] zone=%u (%s) prio=%d inuse=%d header=%p\n",
			depth,
			cur->zoneIndex,
			g_zoneFileNames[cur->zoneIndex].name,
			T4M_ZonePriority(cur->zoneIndex),
			cur->inuse ? 1 : 0,
			cur->asset.header.data);
		if (cur->nextOverride == 0) break;
		cur = T4M_POOL_ENTRY(cur->nextOverride);
		++depth;
	}
}

// @new — debug utility
__declspec(noinline)
void T4M_DB_DumpHashBucket(unsigned int bucket)
{
	if (bucket >= 0x8000) { Com_Printf(0, "[T4M] bucket out of range\n"); return; }
	unsigned short idx = db_hashTable[bucket];
	Com_Printf(0, "[T4M] Bucket %u:\n", bucket);
	int depth = 0;
	while (idx != 0)
	{
		XAssetEntry* e = T4M_POOL_ENTRY(idx);
		Com_Printf(0, "  [%d] type=%d (%s) name='%s' zone=%u nextHash=%u nextOvr=%u\n",
			depth,
			e->asset.type,
			T4M_DB_GetXAssetTypeName(e->asset.type),
			T4M_GetName(e),
			e->zoneIndex,
			e->nextHash,
			e->nextOverride);
		idx = e->nextHash;
		++depth;
	}
}

// @new — debug utility
__declspec(noinline)
int T4M_DB_CountActiveEntries()
{
	int count = 0;
	for (int b = 0; b < 0x8000; ++b)
	{
		unsigned short idx = db_hashTable[b];
		while (idx != 0)
		{
			++count;
			XAssetEntry* e = T4M_POOL_ENTRY(idx);
			// also count nextOverride nodes
			unsigned short ov = e->nextOverride;
			while (ov != 0)
			{
				++count;
				ov = T4M_POOL_ENTRY(ov)->nextOverride;
			}
			idx = e->nextHash;
		}
	}
	return count;
}

// =====================================================================
// __usercall → __cdecl wrappers
//
//   Vanilla sub_48D7D0 and sub_48D760 use an __usercall convention with
//   an argument in edi (register). Our reconstructions
//   T4_DB_FindDefaultAsset / T4_DB_FindXAssetByName are __cdecl. To
//   detour 0x48D7D0 and 0x48D760 cleanly, we interpose a naked stub
//   that pushes the register args onto the stack.
//
//   Vanilla:                               cdecl reconstruction:
//   ─────────                              ─────────────────────
//   sub_48D7D0(edi=type)                   T4_DB_FindDefaultAsset(int type)
//   sub_48D760(arg_0=type, edi=name)       T4_DB_FindXAssetByName(int type, const char* name)
//
//   Return: eax = result (same convention vanilla / cdecl).
// =====================================================================

// @wrapper — __usercall→__cdecl convention bridge to T4_DB_FindDefaultAsset (0x48D7D0)
// sub_48D7D0: edi = type
__declspec(naked) void T4M_DB_FindDefaultAsset_Wrapper()
{
	__asm {
		push    edi                           ; arg_0 = type
		call    T4_DB_FindDefaultAsset            ; cdecl, result in eax
		add     esp, 4
		ret
	}
}

// @wrapper — __usercall→__cdecl convention bridge to T4_DB_FindXAssetByName (0x48D760)
// sub_48D760: arg_0 (stack) = type, edi = name
// On entry: [esp+0] = return addr, [esp+4] = type, edi = name
__declspec(naked) void T4M_DB_FindXAssetByName_Wrapper()
{
	__asm {
		push    edi                           ; arg_1 = name (edi)
		push    dword ptr [esp+8]             ; arg_0 = type (read from original stack, +8 after push edi)
		call    T4_DB_FindXAssetByName            ; cdecl, result in eax
		add     esp, 8                         ; clean our 2 pushes
		ret                                     ; caller cleans its arg (cdecl)
	}
}

// @wrapper — __usercall→__cdecl convention bridge to T4_DB_LinkXAssetEntry (0x48D860)
// sub_48D860: eax = type (register), arg_0 (stack) = name
// On entry: [esp+0] = return addr, [esp+4] = name, eax = type
// Vanilla exits with `retn` (NO arg cleanup) — callers (sub_48DA30 loc_48DDF2,
// sub_48DFF0) clean with `add esp, 4`. `retn 4` would double-clean and shift
// caller ESP by +4, corrupting every frame up the chain.
__declspec(naked) void T4M_DB_LinkXAssetEntry_Wrapper()
{
	__asm {
		push    dword ptr [esp+4]             ; arg_1 = name (original arg_0)
		push    eax                           ; arg_0 = type (from register)
		call    T4_DB_LinkXAssetEntry             ; cdecl, result in eax
		add     esp, 8                         ; clean our 2 pushes
		retn                                   ; caller cleans its name arg (vanilla)
	}
}

// =====================================================================
// PatchT4_Override — installation
// =====================================================================
void PatchT4_Override()
{
	// Active detours: redirect the vanilla addresses to our C++
	// reconstructions (in T4.cpp).
	Detours::X86::DetourFunction((uintptr_t)0x0048F340, (uintptr_t)&T4_DB_UnloadZoneAssets,                 Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction((uintptr_t)0x0048D7D0, (uintptr_t)&T4M_DB_FindDefaultAsset_Wrapper,       Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction((uintptr_t)0x0048D760, (uintptr_t)&T4M_DB_FindXAssetByName_Wrapper,       Detours::X86Option::USE_JUMP);

	// TEMPORAIRE — ce détour sera retiré une fois que tous les appelants
	// de sub_48D860 (sub_48DA30, sub_48DFF0) seront remplacés par des fonctions
	// T4M full-C++ qui appellent directement T4_DB_LinkXAssetEntry en __cdecl.
	// À terme le wrapper et ce détour ne seront plus nécessaires.
	Detours::X86::DetourFunction((uintptr_t)0x0048D860, (uintptr_t)&T4M_DB_LinkXAssetEntry_Wrapper,        Detours::X86Option::USE_JUMP);

	// Re-enabled after adding the loc_48E1FB path (zoneIndex==0 case) which
	// properly frees newEntry instead of chaining it as a ghost override.
	// Detours::X86::DetourFunction((uintptr_t)0x0048DFF0, (uintptr_t)&T4_DB_LinkXAssetEntryOverrideAware, Detours::X86Option::USE_JUMP);

	// Full vanilla-faithful reconstruction of DB_FindXAssetHeader (sub_48DA30).
	//
	// DISABLED — startup OK, but hangs during first map load inside vanilla
	// sub_48E370's hash-chain scan (asset type 9 "ing_small" not found in
	// chain → infinite loop, no termination on idx==0 in vanilla).
	//
	// Confirmed by bisection: with this detour OFF the game loads maps fine.
	// Something our reconstruction does differs from vanilla in a way that
	// leaves an asset's hash-chain linkage broken. Needs further investigation
	// (probable suspects: T4_DB_LinkXAssetEntry fallback ordering, or pump
	// sequence causing DB worker to promote entries in unexpected order).
	// Detours::X86::DetourFunction((uintptr_t)0x0048DA30, (uintptr_t)&T4_DB_FindXAssetHeader,          Detours::X86Option::USE_JUMP);

}
