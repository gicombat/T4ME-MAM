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

void PatchT4_MemoryLimits()
{
	// increase pool sizes to similar (or greater) t5 sizes.
	DB_ReallocXAssetPool(ASSET_TYPE_FX, 2048);
	DB_ReallocXAssetPool(ASSET_TYPE_IMAGE, 8192);
	DB_ReallocXAssetPool(ASSET_TYPE_LOADED_SOUND, 4096);
	DB_ReallocXAssetPool(ASSET_TYPE_MATERIAL, 4096);
	DB_ReallocXAssetPool(ASSET_TYPE_WEAPON, 512);
	DB_ReallocXAssetPool(ASSET_TYPE_XMODEL, 4096);
	DB_ReallocXAssetPool(ASSET_TYPE_RAWFILE, 2048);
	DB_ReallocXAssetPool(ASSET_TYPE_PHYSCONSTRAINTS, 256);
	DB_ReallocXAssetPool(ASSET_TYPE_PHYSPRESET, 256);
	DB_ReallocXAssetPool(ASSET_TYPE_XMODELPIECES, 256);

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

	#define NEW_ASSET_ENTRY_POOL_SIZE 65535

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

	#define NEW_IMAGE_SORT_BUFFER_SIZE 8192

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
}