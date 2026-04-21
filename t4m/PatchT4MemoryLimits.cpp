// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Increasing memory pool sizes
//
// Initial author: TheApadayo
//
// Started: 2015-07-18
// ==========================================================

#include "StdInc.h"
#include <safetyhook.hpp>

#define NEW_ASSET_ENTRY_POOL_SIZE 65535
#define NEW_IMAGE_SORT_BUFFER_SIZE 8192
#define NEW_MAX_GENTITIES   2048
#define ENTITY_SIZE         0x378        // 888 bytes per gentity_s
#define OLD_ENTITY_BASE     0x0176C6F0U
#define GSPAWN_CMP_IMM      0x54EAC3U   // immediate of "cmp ecx, 3FEh" in G_Spawn
#define ENTITY_BASE_DWORD   0x18F5D8CU  // dword_18F5D8C: runtime entity-base ptr
#define NEW_MAX_LOCALIZED   4096

// =====================================================================
// G_Entity pool expansion
//
// sub_502020 (entity system init) is called via sub_5AA5F0 from
// sub_62B7C0+1A1 every map start. It sets:
//   dword_18F5D8C = 0x176C6F0   (runtime entity-base ptr → old 1024-entry BSS array)
//
// We hook at 0x62B969 (first instruction after "call sub_5AA5F0; add esp, 0Ch"
// in sub_62B7C0). At that point sub_502020 has already initialized the entity
// system and written 0x176C6F0 to dword_18F5D8C.
//
// First invocation: allocate a 2048-entry array, copy the initialized BSS
// state into it, then scan .text with ONE VirtualProtect call and replace
// all three reference patterns:
//   0x0176C6F0  (array base)           → new_base
//   0x0184A6F0  (array end, sub_62B7C0 loop bound) → new_base + 2048*0x378
//   0x0184A780  (init loop bound in sub_502020, 0x90 past end) → same new_end
//
// Every invocation (including subsequent map loads where sub_502020 resets
// dword_18F5D8C to 0x176C6F0): redirect dword_18F5D8C to the new array.
// =====================================================================

static BYTE* g_newEntityPool = nullptr;

static void SetupEntityPool()
{
    // Allocate 2048-entry entity array (2048 * 0x378 = ~1.7 MB)
    g_newEntityPool = (BYTE*)VirtualAlloc(
        NULL,
        (SIZE_T)NEW_MAX_GENTITIES * ENTITY_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (!g_newEntityPool) {
        Com_Printf(0, "^1[T4M] FATAL: entity pool VirtualAlloc failed\n");
        return;
    }

    // Copy initialized state from old 1024-entry BSS array
    memcpy(g_newEntityPool, (void*)OLD_ENTITY_BASE, 1024 * ENTITY_SIZE);

    DWORD newBase = (DWORD)g_newEntityPool;
    // "one past last entity" — upper bound for entity iteration and init loops
    DWORD newEnd  = newBase + (DWORD)NEW_MAX_GENTITIES * ENTITY_SIZE;

    // Patch all .text references in one VirtualProtect call.
    // Three patterns:
    //   0x0176C6F0 (OLD_ENTITY_BASE)  → newBase
    //   0x0184A6F0 (array end)        → newEnd  (sub_62B7C0 loop bound, line 833611)
    //   0x0184A780 (init loop bound)  → newEnd  (sub_502020 init loop, line 376007)
    // Scan is byte-granular to catch any alignment within instructions.
    const DWORD TEXT_START = 0x00401000U;
    const DWORD TEXT_END   = 0x00800000U;
    DWORD oldProt;
    VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
                   PAGE_EXECUTE_READWRITE, &oldProt);

    int patched = 0;
    for (BYTE* bp = (BYTE*)TEXT_START; bp < (BYTE*)(TEXT_END - 3); ++bp) {
        DWORD v = *(DWORD*)bp;
        if (v == OLD_ENTITY_BASE) {
            *(DWORD*)bp = newBase;  ++patched;
        } else if (v == 0x0184A6F0U || v == 0x0184A780U) {
            *(DWORD*)bp = newEnd;   ++patched;
        }
    }

    VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
                   PAGE_EXECUTE_READ, &oldProt);

    Com_Printf(0, "[T4M] Entity pool: base=0x%08X, %d refs patched\n",
               newBase, patched);
}

static SafetyHookMid GEntityPool_hook;

static void PatchT4_GEntityPool()
{
    // Hook at 0x62B969: first instruction after "call sub_5AA5F0; add esp, 0Ch"
    // in sub_62B7C0 (sub_62B7C0+1A1 calls sub_5AA5F0, +1A6 is add esp; +1A9 is here).
    GEntityPool_hook = safetyhook::create_mid(0x62B969, [](SafetyHookContext&) {
        if (!g_newEntityPool) {
            SetupEntityPool(); // allocate + patch .text (one-shot)
        }
        // Always restore dword_18F5D8C — sub_502020 resets it to 0x176C6F0 each map load.
        if (g_newEntityPool) {
            *(DWORD*)ENTITY_BASE_DWORD = (DWORD)g_newEntityPool;
        }
    });
}

void PatchT4_MemoryLimits()
{
	// increase pool sizes to similar (or greater) t5 sizes.
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_FX, 2048);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_IMAGE, 8192);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_LOADED_SOUND, 4096);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_MATERIAL, 4096);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_WEAPON, 512);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_XMODEL, 4096);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_RAWFILE, 2048);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_PHYSCONSTRAINTS, 256);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_PHYSPRESET, 256);
	T4M_DB_ReallocXAssetPool(ASSET_TYPE_XMODELPIECES, 256);

	// change the size of g_mem from 0x12C00000 to 0x19600000, UGX-Mod v1.1 is pretty fucking huge
	// had to increase due to it crashing in Com_BeginParseSession
	*(DWORD*)0x5F5492 = 0x40000000; //0x14800000
	*(DWORD*)0x5F54D1 = 0x40000000; //0x14800000
	*(DWORD*)0x5F54DB = 0x40000000; //0x14800000

	//*(DWORD*)0x5F5492 = 0x26100000; //0x14800000
	//*(DWORD*)0x5F54D1 = 0x26100000; //0x14800000
	//*(DWORD*)0x5F54DB = 0x26100000; //0x14800000

	// change the num of entities available to be spawned in G_Spawn from 1022 to 1500
	// still a W.I.P. is missing array and hash table(?) changes
	//PatchMemory(0x0054EAC3, (PBYTE)"\xDC\x05", 2);

	// =====================================================================
	// Increase XASSET_ENTRY_POOL_SIZE from 32767 to 65535 (max uint16)
	//
	// The vanilla g_assetEntryPool at 0xA51C50 is a static 32767-entry pool
	// (0x80000 bytes) shared by ALL asset types. T4M's DB_ReallocXAssetPool
	// increases per-type pools but not this global pool, causing crashes
	// ("Could not allocate asset") when total loaded assets exceed ~32K.
	//
	// Cannot extend in-place: g_usageFrame (dword_AD1C40) sits right at
	// pool_base + 0x7FFF0, and other variables follow immediately after.
	// Must allocate a new pool and patch all 24 code references.
	// =====================================================================


	// Allocate new pool (persists for lifetime of process)
	static XAssetEntryPoolEntry* newPool = (XAssetEntryPoolEntry*)VirtualAlloc(
		NULL,
		NEW_ASSET_ENTRY_POOL_SIZE * sizeof(XAssetEntryPoolEntry),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);

	if (!newPool) {
		Com_Printf(0, "^1ERROR: Failed to allocate expanded asset entry pool\n");
		return;
	}

	DWORD newPoolAddr = (DWORD)newPool;
	DWORD newPoolAddr10 = (DWORD)&newPool[1]; // pool + 0x10 (unk_A51C60 equivalent)

	// Update T4M's C pointer so DB_ListAssetPool and other T4M code use the new pool
	g_assetEntryPool = newPool;

	// Unprotect .text section pages covering all patch addresses (0x48D340 – 0x48FA30)
	DWORD oldProtect;
	VirtualProtect((LPVOID)0x48D340, 0x48FA30 - 0x48D340, PAGE_EXECUTE_READWRITE, &oldProtect);

	// All addresses below verified by scanning the binary for byte patterns
	// 0x00A51C50 (LE: 50 1C A5 00) and 0x00A51C60 (LE: 60 1C A5 00).
	// Encoding rules:
	//   add eax, imm32 = 05 [imm32]       → immediate at instr+1
	//   add esi, imm32 = 81 C6 [imm32]    → immediate at instr+2
	//   add edi, imm32 = 81 C7 [imm32]    → immediate at instr+2
	//   sub edx, imm32 = 81 EA [imm32]    → immediate at instr+2
	//   sub edi, imm32 = 81 EF [imm32]    → immediate at instr+2
	//   lea reg,[reg+imm32] = 8D XX [imm32] → immediate at instr+2
	//   mov [reg+imm32],reg = 89 XX [imm32] → immediate at instr+2
	//   cmp eax, imm32 = 3D [imm32]       → immediate at instr+1
	//   mov [imm32],imm32 = C7 05 [a4][v4] → value at instr+6

	// ---- sub_48D340 (DB_InitAssetEntryPool) ----
	// C7 05 84 78 95 00 [60 1C A5 00] → mov dword_957884, offset unk_A51C60
	*(DWORD*)0x48D371 = newPoolAddr10;
	// 8D 88 [60 1C A5 00] → lea ecx, [eax + unk_A51C60]
	*(DWORD*)0x48D382 = newPoolAddr10;
	// 89 88 [50 1C A5 00] → mov [eax + dword_A51C50], ecx
	*(DWORD*)0x48D388 = newPoolAddr;
	// 3D [F0 FF 07 00] → cmp eax, 7FFF0h  →  change limit to 0xFFFF0
	*(DWORD*)0x48D390 = NEW_ASSET_ENTRY_POOL_SIZE * 0x10; // 65535 * 0x10 = 0xFFFF0

	// ---- sub_48D560 (DB_EnumXAssets) ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48D5B8 = newPoolAddr;
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48D5E5 = newPoolAddr;

	// ---- sub_48D760 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48D784 = newPoolAddr;

	// ---- sub_48D7D0 ----
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48D7F5 = newPoolAddr;
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48D848 = newPoolAddr;

	// ---- sub_48D860 (DB_AddXAsset / link entry) ----
	// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
	*(DWORD*)0x48D90F = newPoolAddr;

	// ---- sub_48DEA0 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48DEF4 = newPoolAddr;

	// ---- sub_48DFB0 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48DFB4 = newPoolAddr;

	// ---- sub_48DFF0 (DB_UnloadXAssets) ----
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48E059 = newPoolAddr;
	// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
	*(DWORD*)0x48E115 = newPoolAddr;
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48E1C2 = newPoolAddr;
	// 81 EF [50 1C A5 00] → sub edi, offset dword_A51C50
	*(DWORD*)0x48E1E8 = newPoolAddr;
	// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
	*(DWORD*)0x48E292 = newPoolAddr;

	// ---- sub_48E370 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48E3A6 = newPoolAddr;

	// ---- sub_48F340 (DB_PostLoadXZone) ----
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48F378 = newPoolAddr;
	// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
	*(DWORD*)0x48F4D4 = newPoolAddr;
	// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
	*(DWORD*)0x48F558 = newPoolAddr;

	// ---- sub_48F670 ----
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48F68C = newPoolAddr;

	// ---- sub_48F6E0 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48F704 = newPoolAddr;

	// ---- sub_48F9B0 ----
	// 05 [50 1C A5 00] → add eax, offset dword_A51C50
	*(DWORD*)0x48F9D4 = newPoolAddr;
	// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
	*(DWORD*)0x48FA28 = newPoolAddr;

	// =====================================================================
	// Fix renderer image sort array overflow
	//
	// R_LoadWorld and sub_719F40 call sub_48DF60 (DB_EnumXAssets_FastFile_Array)
	// to fill dword_3BF1880[] with image asset headers, then sort them.
	// The array is hardcoded for 0x800 (2048) entries, but T4M increases the
	// image pool to 8192. sub_48DF60 has NO bounds check — it writes ALL
	// matching assets, overflowing the buffer and corrupting:
	//   - dword_3BF3884 (image count, at array + 0x2004)
	//   - dword_3BF392C (scene structure pointer, at array + 0x20AC)
	// This causes both observed crashes:
	//   0x719A2E: sort comparator gets garbage → access violation
	//   0x491500: corrupted scene pointer → bitfield access violation
	//
	// Fix: allocate a larger buffer (8192 entries) and patch all 21 references.
	// =====================================================================


	// Allocate new image sort buffer + count variable (contiguous)
	static DWORD* newImageBuffer = (DWORD*)VirtualAlloc(
		NULL,
		NEW_IMAGE_SORT_BUFFER_SIZE * sizeof(DWORD) + sizeof(DWORD), // array + count
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);

	if (!newImageBuffer) {
		Com_Printf(0, "^1ERROR: Failed to allocate expanded image sort buffer\n");
		return;
	}

	DWORD newImgBufAddr = (DWORD)newImageBuffer;
	// Place the count variable right after the array, mirroring original layout
	// Original: array at 0x3BF1880, count at 0x3BF3884 (offset +0x2004 from array, but
	// we just need a separate DWORD for the count — any stable address works)
	DWORD* newImageCount = &newImageBuffer[NEW_IMAGE_SORT_BUFFER_SIZE];
	DWORD newImgCntAddr = (DWORD)newImageCount;

	// Unprotect renderer .text pages covering patch addresses
	// Range: 0x6D69EB to 0x74200E+4
	DWORD oldProtect2;
	VirtualProtect((LPVOID)0x6D69E0, 0x742012 - 0x6D69E0, PAGE_EXECUTE_READWRITE, &oldProtect2);

	// --- Patch 12 references to dword_3BF1880 (image sort array) ---
	// Binary scan found pattern 80 18 BF 03 at the exact VA of the immediate.
	// Patch address = VA directly (no offset needed).

	// 68 [80 18 BF 03] → push offset dword_3BF1880
	*(DWORD*)0x6D69EB = newImgBufAddr;
	// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
	*(DWORD*)0x6DC964 = newImgBufAddr;
	// 8B 14 95 [80 18 BF 03] → mov edx, dword_3BF1880[edx*4]
	*(DWORD*)0x6DCA8C = newImgBufAddr;
	// 89 0C 85 [80 18 BF 03] → mov dword_3BF1880[eax*4], ecx
	*(DWORD*)0x6E993D = newImgBufAddr;
	// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld)
	*(DWORD*)0x705784 = newImgBufAddr;
	// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld sort call)
	*(DWORD*)0x70579F = newImgBufAddr;
	// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40)
	*(DWORD*)0x719F52 = newImgBufAddr;
	// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40 sort call)
	*(DWORD*)0x719F6D = newImgBufAddr;
	// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
	*(DWORD*)0x741C11 = newImgBufAddr;
	// 8B 1C 85 [80 18 BF 03] → mov ebx, dword_3BF1880[eax*4]
	*(DWORD*)0x741C98 = newImgBufAddr;
	// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
	*(DWORD*)0x741EB7 = newImgBufAddr;
	// 8B 2C 85 [80 18 BF 03] → mov ebp, dword_3BF1880[eax*4]
	*(DWORD*)0x74200E = newImgBufAddr;

	// --- Patch 9 references to dword_3BF3884 (image count) ---
	// Binary scan found pattern 84 38 BF 03 at the exact VA of the immediate.

	// A1 [84 38 BF 03] → mov eax, dword_3BF3884
	*(DWORD*)0x6E990F = newImgCntAddr;
	// A1 [84 38 BF 03] → mov eax, dword_3BF3884
	*(DWORD*)0x6E9936 = newImgCntAddr;
	// A1 [84 38 BF 03] → mov eax, dword_3BF3884
	*(DWORD*)0x6E9942 = newImgCntAddr;
	// A3 [84 38 BF 03] → mov dword_3BF3884, eax
	*(DWORD*)0x6E995A = newImgCntAddr;
	// C7 05 [84 38 BF 03] 00000000 → mov dword_3BF3884, 0
	*(DWORD*)0x6E9D3C = newImgCntAddr;
	// A3 [84 38 BF 03] → mov dword_3BF3884, eax  (R_LoadWorld)
	*(DWORD*)0x705793 = newImgCntAddr;
	// 8B 0D [84 38 BF 03] → mov ecx, dword_3BF3884
	*(DWORD*)0x705799 = newImgCntAddr;
	// A3 [84 38 BF 03] → mov dword_3BF3884, eax  (sub_719F40)
	*(DWORD*)0x719F61 = newImgCntAddr;
	// 8B 0D [84 38 BF 03] → mov ecx, dword_3BF3884
	*(DWORD*)0x719F67 = newImgCntAddr;

	// Also patch the max count passed to sub_48DF60 (0x800 → 0x2000)
	// At 0x70577F: push 800h → change immediate to 8192
	// At 0x719F4D: push 800h → change immediate to 8192
	// These are ignored by the function, but patch them for correctness

	// =====================================================================
	// G_Spawn entity limit increase
	//
	// G_Spawn (sub_54EAB0) at loc_54EAF2 allocates new entities as:
	//   entity_ptr = dword_18F5D8C + numGEntities * 0x378
	// The check "cmp ecx, 3FEh; jnz" at 0x54EAC1 errors when numGEntities
	// reaches exactly 1022, capping usable entity indices at 0-1021.
	//
	// This patch raises the limit to NEW_MAX_GENTITIES - 2.
	// The actual entity array relocation (VirtualAlloc + .text scan + dword_18F5D8C
	// redirect) is handled by PatchT4_GEntityPool() / SetupEntityPool() above,
	// which fires on the first map load via a hook at 0x62B969.
	// =====================================================================

	// Mid-hook at 0x54EAC1 replaces the "cmp ecx, 3FEh; jnz loc_54EAF2" pair entirely.
	// At hook point: ecx = numGEntities (set by "mov ecx, dword_18F5D94" just before).
	// Original behaviour: error when ecx == 0x3FE (1022), allocate otherwise.
	// New behaviour: error when ecx >= NEW_MAX_GENTITIES - 2, allocate otherwise.
	//
	// Entity array note: dword_18F5D8C (runtime entity-base ptr) currently points to the
	// 1024-entry BSS array at 0x176C6F0. Entities at index >= 1024 will land past that
	// array until a full pool relocation (VirtualAlloc + ~498 .text patches) is done.
	//
	// Hash table note: dword_18F7910 and dword_18F794C (BSS, 0x18F7910 / 0x18F794C) are
	// set to 0x3FF (1023) at init by sub_502020. They act as sentinel/comparison values
	// in sub_54DDC0 and sub_54EDC0 for entity hash-slot tracking. When entity index 1023
	// is freed those functions compare against 0x3FF and perform a no-op reset — harmless.
	// These do not need updating for the spawn-limit increase alone.
	/*
	static auto GSpawn_limit_hook = safetyhook::create_mid(0x54EAC1, [](SafetyHookContext& ctx) {
		// ctx.ecx = numGEntities. Redirect eip to skip or enter the error path.
		if (ctx.ecx < (DWORD)(NEW_MAX_GENTITIES - 2)) {
			ctx.eip = 0x54EAF2; // allocation path (loc_54EAF2)
		} else {
			ctx.eip = 0x54EAC9; // error path ("G_Spawn: no free entities")
		}
	});
	Com_Printf(0, "[T4M] G_Spawn limit patched to %d (mid-hook)\n", NEW_MAX_GENTITIES - 2);
	PatchT4_GEntityPool();
	*/
	// =====================================================================
	// Localized string hash table limit increase
	//
	// sub_54A1A0 (generic asset lookup) uses a 1023-entry WORD hash table
	// at BSS address 0x2350428. The table address is encoded as a 4-byte
	// immediate in a single lea instruction at 0x54A20C, and the lookup
	// bound 0x3FF (1023) appears as push imm32 immediates at 0x54A2F3 and
	// 0x54A330. Redirecting all three gives the new limit.
	// =====================================================================

	/*
	static WORD* newStringTable = (WORD*)VirtualAlloc(
		NULL,
		(DWORD)NEW_MAX_LOCALIZED * sizeof(WORD),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);

	if (newStringTable) {
		DWORD newStrBase = (DWORD)newStringTable;

		// Patch "lea ecx, ds:2350428h[ecx*2]" → redirect to new table
		// Encoding: 8D 0C 4D [imm32] at 0x54A209; address immediate at 0x54A20C
		*(DWORD*)0x54A20C = newStrBase;

		// Patch first "push 3FFh" (limit arg to sub_54A1A0)
		// Encoding: 68 [imm32]; immediate at 0x54A2F3
		*(DWORD*)0x54A2F3 = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

		// Patch second "push 3FFh" (limit arg to sub_54A1A0)
		// Encoding: 68 [imm32]; immediate at 0x54A330
		*(DWORD*)0x54A330 = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

		Com_Printf(0, "[T4M] Localized string table expanded: new limit=%d\n",
		           NEW_MAX_LOCALIZED - 1);
	} else {
		Com_Printf(0, "^1ERROR: Failed to allocate expanded localized string table\n");
	}
		*/
}