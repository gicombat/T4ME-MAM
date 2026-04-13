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

#define NEW_ASSET_ENTRY_POOL_SIZE 65535
#define NEW_IMAGE_SORT_BUFFER_SIZE 8192

void PatchT4_MemoryLimits()
{
	// increase pool sizes to similar (or greater) t5 sizes.
	DB_ReallocXAssetPool(ASSET_TYPE_FX, 2048);
	DB_ReallocXAssetPool(ASSET_TYPE_IMAGE, NEW_IMAGE_SORT_BUFFER_SIZE);
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
	// For the record what is this things :  
	// Fix renderer image sort array overflow
	// R_LoadWorld and sub_719F40 call sub_48DF60
	// to fill dword_3BF1880[] with image asset headers, then sort them.
	// The array is hardcoded for 0x800 (2048) entries, but since we increases the
	// image pool to 8192. sub_48DF60 has NO bounds check — it writes ALL
	// matching assets, overflowing the buffer and corrupting:
	//   - dword_3BF3884 (image count, at array + 0x2004)
	//   - dword_3BF392C (scene structure pointer, at array + 0x20AC)
	// This causes both observed crashes via xdbg:
	//   0x719A2E: sort comparator gets garbage → access violation
	//   0x491500: corrupted scene pointer → bitfield access violation
	//
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

	DWORD oldProtect2;
	VirtualProtect((LPVOID)0x6D69E0, 0x742012 - 0x6D69E0, PAGE_EXECUTE_READWRITE, &oldProtect2);

	// --- Patch 12 references to dword_3BF1880 (image sort array) ---

	*(DWORD*)0x6D69EB = newImgBufAddr;
	*(DWORD*)0x6DC964 = newImgBufAddr;
	*(DWORD*)0x6DCA8C = newImgBufAddr;
	*(DWORD*)0x6E993D = newImgBufAddr;
	*(DWORD*)0x705784 = newImgBufAddr;
	*(DWORD*)0x70579F = newImgBufAddr;
	*(DWORD*)0x719F52 = newImgBufAddr;
	*(DWORD*)0x719F6D = newImgBufAddr;
	*(DWORD*)0x741C11 = newImgBufAddr;
	*(DWORD*)0x741C98 = newImgBufAddr;
	*(DWORD*)0x741EB7 = newImgBufAddr;
	*(DWORD*)0x74200E = newImgBufAddr;

	// --- Patch 9 references to dword_3BF3884 (image count) ---

	*(DWORD*)0x6E990F = newImgCntAddr;
	*(DWORD*)0x6E9936 = newImgCntAddr;
	*(DWORD*)0x6E9942 = newImgCntAddr;
	*(DWORD*)0x6E995A = newImgCntAddr;
	*(DWORD*)0x6E9D3C = newImgCntAddr;
	*(DWORD*)0x705793 = newImgCntAddr;
	*(DWORD*)0x705799 = newImgCntAddr;
	*(DWORD*)0x719F61 = newImgCntAddr;
	*(DWORD*)0x719F67 = newImgCntAddr;

}