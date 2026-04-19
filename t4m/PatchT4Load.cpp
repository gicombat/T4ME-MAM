// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: various loading changes for T4M
//
// Initial author: DidUknowiPwn
// Started: 2015-10-06
// ==========================================================

#include "StdInc.h"

#include "T4.h"
#include <sstream>


void makeFFLoadStruct(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	static XZoneInfo *newZoneInfo;

	newZoneInfo = new XZoneInfo[zoneCount];
	memset(newZoneInfo, 0, sizeof(XZoneInfo) * zoneCount);

	// continue this when better understood
}
// ── Level 3: PMem slot release ───────────────────────────────────────────────
// @modified — sub_5F5540 reimpl with T4M log + strict LIFO check
__declspec(noinline)
void T4M_DB_FreeXZoneMemory(int poolIndex, const char* zoneName)
{
	Com_PrintfChannel(0, "[T4M] - PMem_Free( %s, %d )\n", zoneName, poolIndex);
	PMem_Pool* pool = &g_pmem_pools[poolIndex];
	int count = pool->count;
	int found = -1;

	for (int i = 0; i < count; i++) 
	{
		if (pool->entries[i].name == zoneName) 
		{ 
			found = i; 
			break; 
		}
	}

	if (found == -1) 
		return;

	pool->entries[found].name = NULL;
	int newCount = count - 1;

	if (found != newCount) 
	{
		Com_Error(0, "[T4M] - free does not match allocation");
		return;
	}

	pool->freePtr = pool->entries[found].next;
	pool->count = newCount;

	for (;;) 
	{
		int top = pool->count;
		if (top == 0 || pool->entries[top - 1].name != NULL) break;
		pool->freePtr = pool->entries[top - 1].next;
		pool->count--;
	}
}

// ── Level 2: entry cleanup + log ─────────────────────────────────────────────
// @modified — sub_48F600 reimpl with T4M log
__declspec(noinline)
void T4M_DB_ZoneEntryCleanup(XZoneLoadedEntry* entry)
{
	memset(entry->runtimeData, 0, sizeof(entry->runtimeData));

	const char* zoneName = g_zoneFileNames[entry->zoneFileIndex].name;
	Com_PrintfChannel(0x10, "Unloaded fastfile %s\n", zoneName);

	T4M_DB_FreeXZoneMemory(entry->memHandle, zoneName);

	g_zoneFileNames[entry->zoneFileIndex].name[0] = '\0';
}

// ── Level 1: removal + g_zoneLoaded compaction ───────────────────────────────
// @modified — sub_48F7D0 reimpl (compaction + cleanup)
__declspec(noinline)
void T4M_DB_RemoveZoneEntry(XZoneLoadedEntry* entry)
{
	int index = (int)(entry - g_zoneLoaded);

	T4M_DB_ZoneEntryCleanup(entry);

	int newCount = *g_zoneCount - 1;
	*g_zoneCount = newCount;

	if (index < newCount)
		memmove(&g_zoneLoaded[index], &g_zoneLoaded[index + 1],
			(newCount - index) * sizeof(XZoneLoadedEntry));
}

// @modified — replaces sub_48F720 to handle T4M custom zones 0x1000
void __cdecl T4M_DB_UnloadAllZones()
{
	Com_Printf(0, "[T4M] - DB_UnloadAllZones Start\n");
	// --- Phase 1: engine sync (identical to T4M_DB_LoadXAssets Phase 0.5) ---
	Sys_SyncDatabase();        // sub_6F6CE0
	DB_PostLoadXZone();        // sub_5A3320
	Sys_WakeDatabase();        // sub_6F6D60
	DB_WaitForPendingLoads();  // sub_5FDBF0
	DB_CheckPendingComplete(); // sub_48E560
	DB_PreUnloadResources();   // sub_48F8D0

	// --- Phase 2: acquire writer lock ---
	T4M_DB_WriterAcquire();

	// --- Phase 3: LIFO asset unload (no PMem free yet) ---
	for (int z = *g_zoneCount - 1; z >= 0; z--)
	{
		T4_DB_UnloadZoneAssets(g_zoneLoaded[z].zoneFileIndex, /*freeMemory=*/false);
	}

	// --- Phase 4: cleanup asset references in the hash table ---
	DB_CleanupAssetRefs();  // sub_48F670 — iterates db_hashTable, returns entries to the free list
	DB_PostUnloadCleanup(); // sub_48F9B0

	// --- Phase 5: LIFO final cleanup (log + PMem_Free + clear zone name) ---
	// All zones are freed in the reverse order of their allocation, which
	// respects the strict LIFO of the PMem pool for all zones (vanilla + T4M).
	for (int z = *g_zoneCount - 1; z >= 0; z--)
	{
		T4M_DB_ZoneEntryCleanup(&g_zoneLoaded[z]);
	}

	// --- Phase 6: reset state flags ---
	*g_zoneCount = 0;
	*g_dbHasLoadedZones = false;  // byte_46DE3B6 — no more zones loaded

	// --- Phase 7: release writer lock ---
	T4M_DB_WriterRelease();
}

// @modified — replaces sub_48E7B0 with T4M Phase 3 for zones 0x1000
void T4M_DB_LoadXAssets(XZoneInfo* zoneInfo, int zoneCount, int sync)
{
	Com_PrintfChannel(0, "[T4M] - DB_LoadXAssets Start for %d zone\n", zoneCount);
	for (size_t i = 0; i < zoneCount; i++)
	{
		Com_PrintfChannel(0, "[T4M] - Zone name : %s \n", zoneInfo[i].name);
	}

	bool anyUnloaded = false;
	// ── PHASE 0: one-shot init ────────────────────────────────────────────
	if (!*g_dbInitialized)
	{
		*g_dbInitialized = true;
		DB_InitAssetEntryPool(); // sub_48D340 — g_assetEntryPool free list + per-type pool clear
	}

	// ── PHASE 0.5: engine synchronization ─────────────────────────────────
	Sys_SyncDatabase();        // sub_6F6CE0 — renderer pre-frame / flush pending
	DB_PostLoadXZone();        // sub_5A3320 — tick sync worker
	Sys_WakeDatabase();        // sub_6F6D60 — renderer post-frame
	DB_WaitForPendingLoads();  // sub_5FDBF0 — PMem frame update
	DB_CheckPendingComplete(); // sub_48E560 — update zone visibility

	if (zoneCount <= 0)
		goto skipToSync;

	// ── PHASE 1: unload assets (freeFlags match, LIFO scan) ───────────────
	for (int i = 0; i < zoneCount; i++)
	{
		int freeFlags = zoneInfo[i].freeFlags;
		if (freeFlags == 0) continue;

		for (int z = *g_zoneCount - 1; z >= 0; z--)
		{
			if (!(g_zoneLoaded[z].allocFlags & freeFlags)) continue;

			if (!anyUnloaded) {
				anyUnloaded = true;
				DB_PreUnloadResources(); // sub_48F8D0 — flush renderer, GPU, D3D buffers

				if (!*g_dbInUse) {
					*g_dbInUse = true;
					// Clear client model weapon slots (renderer scene @ dword_3BF392C)
					R_ClearScene();                          // sub_6B1440
					CL_ClearState();                         // sub_59EA90
					Hunk_ClearTempMemory(*(int*)0x16D7AD0);  // sub_4B2F80
				}

				T4M_DB_WriterAcquire(); // pure C++ reconstruction — replaces the broken register-based call
			}

			// zoneFileIndex = word at [entry+0x00]
			T4_DB_UnloadZoneAssets(g_zoneLoaded[z].zoneFileIndex, /*freeMemory=*/true); // sub_48F340
		}
	}

	if (!anyUnloaded)
		goto skipToSync;

	// ── PHASE 2: post-unload hash table cleanup ──────────────────────────
	DB_PostUnloadCleanup(); // sub_48F9B0 — walks db_hashTable, frees orphan entries

	// ── PHASE 3: remove zones from g_zoneLoaded (hardcoded bits) ─────────
	// Each bit = one independent LIFO pass.
	// NOTE: 0x004 and 0x400 are absent (permanent flags, never unloaded).
	// NOTE: 0x1000 is also absent → manual T4M Phase 3 is mandatory.
	static const int priorityBits[] =
	{
		XZoneFlags::ZONE_T4M_MAP_LOCA, XZoneFlags::ZONE_T4M_PATCH_EX, 
		XZoneFlags::ZONE_MOD, XZoneFlags::ZONE_COMMON, 
		XZoneFlags::ZONE_MAP_PATCH, XZoneFlags::ZONE_RESERVED_80,
		XZoneFlags::ZONE_POST_LOAD, XZoneFlags::ZONE_MAP_LOAD, 
		XZoneFlags::ZONE_UI, XZoneFlags::ZONE_LOC_COMMON,
		XZoneFlags::ZONE_LOCALIZED, XZoneFlags::ZONE_BASE
	};

	for (int i = 0; i < zoneCount; i++)
	{
		int freeFlags = zoneInfo[i].freeFlags;
		for (int b = 0; b < sizeof(priorityBits) / sizeof(int); b++)
		{
			if (!(freeFlags & priorityBits[b])) continue;

			for (int z = *g_zoneCount - 1; z >= 0; z--) 
			{
				if (g_zoneLoaded[z].allocFlags & priorityBits[b])
					T4M_DB_RemoveZoneEntry(&g_zoneLoaded[z]); // sub_48F7D0
			}
		}
	}

	// ── PHASE 4: unload finalization ─────────────────────────────────────
	T4M_DB_WriterRelease(); // release the writer lock taken by T4M_DB_WriterAcquire() in Phase 1
	*g_dbInUse = false;
	*g_assetsDirty = true;
	R_BeginRegistration();    // sub_6B1500
	CL_BeginRegistration();   // sub_59EB00
	Hunk_BeginRegistration(); // sub_4B3090
	DB_SyncAssets();          // sub_41D3A0
	// (updates thread ID in TLS[0x20] if not set)

skipToSync:
	// ── PHASE 5: renderer sync (if sync && not already active) ───────────
	if (sync && !*g_dbInUse)
	{
		*g_dbInUse = true;
		// Clear client model weapon slots (same code as Phase 1)
		R_ClearScene();                          // sub_6B1440
		CL_ClearState();                         // sub_59EA90
		Hunk_ClearTempMemory(*(int*)0x16D7AD0);  // sub_4B2F80
	}

	// ── PHASE 6: enqueue the new zones for loading ──────────────────────
	*g_syncValue = sync;
	DB_AddZonesToQueue(zoneInfo, zoneCount); // sub_48E4C0

	// ── PHASE 7: if sync, wait for the load to finish ───────────────────
	if (sync)
	{
		DB_PostLoadXZone();    // sub_5A3320
		*g_dbInUse = false;
		*g_assetsDirty = true;
		R_BeginRegistration();
		CL_BeginRegistration();
		Hunk_BeginRegistration();
		DB_SyncAssets();       // sub_41D3A0
		// (updates thread ID in TLS[0x20] if not set)
	}
}

// @new — call-site hook on 0x6D5672 (mod_ex / mod_patch / localized_mod, no vanilla equivalent)
void __cdecl T4M_ModFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	static XZoneInfo* modZoneInfo;
	bool load_modEx = false;
	bool load_modPatch = false;
	bool load_modEx_Patch = false;
	bool load_localized_mod = false;
	int totalZoneCount = zoneCount;

	// Default local is english
	const char* selectedLocal = "english";
	if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string, *language_system)))
	{
		load_localized_mod = true;
		totalZoneCount = totalZoneCount + 1;
		selectedLocal = *language_system;
	}

	// try to fallback to localized_english_mod.ff if we haven't didn't succeed reading localization.txt or we haven't found a corresponding localized_mod.ff file
	if (load_localized_mod == false)
	{
		if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string, selectedLocal)))
		{
			load_localized_mod = true;
			totalZoneCount = totalZoneCount + 1;
		}
	}
 
	// in cod waw mods are loaded from appdata not base game
	if (FileExists(va("%s\\%s\\mod_ex.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modEx = true;
		totalZoneCount = totalZoneCount + 1;
	}
	if (FileExists(va("%s\\%s\\mod_patch.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modPatch = true;
		totalZoneCount = totalZoneCount + 1;

	}
	if (FileExists(va("%s\\%s\\mod_ex_patch.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modEx_Patch = true;
		totalZoneCount = totalZoneCount + 1;
	}


	if (!load_modPatch && !load_modEx && !load_modEx_Patch && !load_localized_mod)
	{
		T4M_DB_LoadXAssets(zoneInfo, zoneCount, sync);
	}
	else
	{
		modZoneInfo = new XZoneInfo[totalZoneCount];
		memset(modZoneInfo, 0, sizeof(XZoneInfo) * (totalZoneCount)); // is needed, causes mod_ex to freeze w/o it 
		for (int i = 0; i < zoneCount; ++i)
		{
			modZoneInfo[i].name = zoneInfo[i].name;
			modZoneInfo[i].allocFlags = zoneInfo[i].allocFlags;
			modZoneInfo[i].freeFlags = zoneInfo[i].freeFlags;
		}
		int currentIndex = zoneCount;
		if (load_modEx)
		{
			modZoneInfo[currentIndex].name = "mod_ex";
			modZoneInfo[currentIndex].allocFlags = XZoneFlags::ZONE_MOD; 
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_modPatch)
		{
			modZoneInfo[currentIndex].name = "mod_patch";
			modZoneInfo[currentIndex].allocFlags = XZoneFlags::ZONE_MOD; 
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_modEx_Patch)
		{
			modZoneInfo[currentIndex].name = "mod_ex_patch";
			modZoneInfo[currentIndex].allocFlags = XZoneFlags::ZONE_MOD; 
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_localized_mod)
		{
			modZoneInfo[currentIndex].name = va("localized_%s_mod", selectedLocal);
			modZoneInfo[currentIndex].allocFlags = XZoneFlags::ZONE_MOD; 
			modZoneInfo[currentIndex].freeFlags = 0;
		}
		T4M_DB_LoadXAssets(modZoneInfo, totalZoneCount, sync);
	}
}


// @new — call-site hook on 0x6D5728 (vulkan dvar init + DB_LoadXAssets call)
void __cdecl T4M_CodePostGFXFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	// Code Post gfx is loaded after reading player var, so we need to update them with the value of the .conf, we can do this only here, so let's hack this shit
	UINT enableVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	vulkan = Dvar_RegisterBool(0, "vulkan", DVAR_FLAG_ARCHIVE, "Use vulkan instead of DirectX 9.0c (only used for UI, if you want to change please use the change in the options or in the .conf file");
	vulkan->current.boolean = enableVulkan;

	T4M_DB_LoadXAssets(zoneInfo, zoneCount, sync);
}

void DB_LoadLocalizedZone(const char* mapName)
{
	char localBuf[0x40];
	_snprintf(localBuf, sizeof(localBuf) - 1, "%s%s", "localized_", mapName);
	localBuf[sizeof(localBuf) - 1] = '\0';

	XZoneInfo zi;
	zi.name = localBuf;
	zi.allocFlags = XZoneFlags::ZONE_LOCALIZED;
	zi.freeFlags = XZoneFlags::ZONE_MAP_PATCH | XZoneFlags::ZONE_UI | XZoneFlags::ZONE_LOCALIZED;
	T4M_DB_LoadXAssets(&zi, 1, 0);  // sub_48E7B0
}

// @modified — replaces sub_59E050 (FS_AddUserMapDir + T4M_PATCH_EX zones)
void __cdecl T4M_DB_LoadMapZones(const char* mapName)
{
	Com_PrintfChannel(0x10, "[T4M] - T4M_DB_LoadMapZones Start for map %s\n", mapName);
	// Reset the fastfile streaming progress counters
	db_streamReadBlocksTotal = 0;  // 0x957400
	db_streamReadBlocksDone = 0;  // 0x957408
	db_streamDecompBytesTotal = 0;  // 0x95740C
	db_streamDecompBytesDone = 0;  // 0x9571A4
	db_streamEnabled = 0;  // 0x957404

	if ((*fs_game)->current.string[0] != '\0' && T4M_FS_ZoneFileExists(mapName, 2))
	{
		const char* usermapPath = va("%s/%s", "usermaps", mapName); // sub_5F6D80
		FS_AddUserMapDir(usermapPath);                               // sub_5DD8A0
	}

	// 1. Original localized Zone alloc=0x002, free=0x112
	// DB_LoadLocalizedZone(mapName);
	// Extracted DB_LoadLocalizedZone --sub_48FB50
	char localBuf[0x40];
	_snprintf(localBuf, sizeof(localBuf) - 1, "%s%s", "localized_", mapName);
	localBuf[sizeof(localBuf) - 1] = '\0';

	XZoneInfo zi_localized;
	zi_localized.name = localBuf;
	zi_localized.allocFlags = XZoneFlags::ZONE_LOCALIZED;
	zi_localized.freeFlags = XZoneFlags::ZONE_MAP_PATCH | XZoneFlags::ZONE_UI | XZoneFlags::ZONE_LOCALIZED;
	T4M_DB_LoadXAssets(&zi_localized, 1, 0);  // sub_48E7B0

	// 2. Original _patch — "<mapName>_patch", alloc=0x100, free=0x150
	char patchNameBuf[0x40];
	_snprintf(patchNameBuf, sizeof(patchNameBuf) - 1, "%s_patch", mapName);
	patchNameBuf[sizeof(patchNameBuf) - 1] = '\0';

	XZoneInfo zi_patch;
	zi_patch.name = patchNameBuf;
	zi_patch.allocFlags = XZoneFlags::ZONE_MAP_PATCH;
	zi_patch.freeFlags = XZoneFlags::ZONE_MAP_PATCH | XZoneFlags::ZONE_POST_LOAD | XZoneFlags::ZONE_UI;
	T4M_DB_LoadXAssets(&zi_patch, 1, 0); // sub_48E7B0
	
	char patchNameExBuf[0x40];
	_snprintf(patchNameExBuf, sizeof(patchNameExBuf) - 1, "%s_patch_ex", mapName);
	patchNameExBuf[sizeof(patchNameExBuf) - 1] = '\0';
	/*
	XZoneInfo zi_patch_ex;
	zi_patch_ex.name = patchNameExBuf;
	zi_patch_ex.allocFlags = XZoneFlags::ZONE_T4M_PATCH_EX;
	zi_patch_ex.freeFlags = XZoneFlags::ZONE_T4M_PATCH_EX;
	T4M_DB_LoadXAssets(&zi_patch_ex, 1, 0); // sub_48E7B0
	*/
	// 3. Map — "<mapName>", alloc=0x010, free=0x050
	XZoneInfo zi_map;
	zi_map.name = mapName;
	zi_map.allocFlags = XZoneFlags::ZONE_UI;
	zi_map.freeFlags = XZoneFlags::ZONE_POST_LOAD | XZoneFlags::ZONE_UI;
	T4M_DB_LoadXAssets(&zi_map, 1, 0); // sub_48E7B0
}

void PatchT4_Load()
{
	// Working DO NOT TOUCH
	Detours::X86::DetourFunction((uintptr_t)0x006D5728, (uintptr_t)&T4M_CodePostGFXFFLoadHook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction((uintptr_t)0x006D5672, (uintptr_t)&T4M_ModFFLoadHook, Detours::X86Option::USE_CALL);

	// Original address of T4M_DB_LoadMapZones
	Detours::X86::DetourFunction((uintptr_t)0x0059E050,(uintptr_t)&T4M_DB_LoadMapZones,Detours::X86Option::USE_JUMP);

	// Fully replace sub_48F720 (DB_UnloadAllZones) with our reconstruction
	Detours::X86::DetourFunction((uintptr_t)0x0048F720,(uintptr_t)&T4M_DB_UnloadAllZones,Detours::X86Option::USE_JUMP);

	Detours::X86::DetourFunction((uintptr_t)0x0048E7B0, (uintptr_t)&T4M_DB_LoadXAssets, Detours::X86Option::USE_JUMP);
	

	// Side Note
	// 0x0059E0EA corresponds to loading the map's _patch fastfile ---- hook here to add a patch
	// 0x0048FB90 corresponds to loading the map's localized_ fastfile ---- hook here for new localizations
	//
	// 0x0059E10E corresponds to loading the map fastfile itself ---- not used
	// 0x0059E0B3 corresponds to the "DB_LoadLocalizedZoneFile" call ---- not used

	//00644C5D, r_init
	//if ((*dedicated)->current.integer > 0)
	//nop(0x00644C5D, 5);
	// 
	// TESTING SHIT
	//Detours::X86::DetourFunction((uintptr_t)0x0059E0EA, (uintptr_t)&LoadMapPatchZoneHook, Detours::X86Option::USE_CALL);
	//Detours::X86::DetourFunction((uintptr_t)0x0048FB90, (uintptr_t)&LoadLocalizedMapZoneHook, Detours::X86Option::USE_CALL);

}