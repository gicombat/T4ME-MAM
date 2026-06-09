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

namespace T4M 
{

	// ── Level 3: PMem slot release ───────────────────────────────────────────────
	// @modified — sub_5F5540 reimpl with T4M log + strict LIFO check
	__declspec(noinline)
	void DB_FreeXZoneMemory(int poolIndex, const char* zoneName)
	{
		T4::engine::Com_Printf(0, "[T4M] - PMem_Free( %s, %d )\n", zoneName, poolIndex);
		PMem_Pool* pool = &T4::engine::g_pmem_pools[poolIndex];
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
			T4::engine::Com_Error(0, "[T4M] - free does not match allocation");
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
	void DB_ZoneEntryCleanup(XZoneLoadedEntry* entry)
	{
		memset(entry->runtimeData, 0, sizeof(entry->runtimeData));

		const char* zoneName = T4::engine::g_zoneFileNames[entry->zoneFileIndex].name;
		T4::engine::Com_Printf(0x10, "Unloaded fastfile %s\n", zoneName);

		DB_FreeXZoneMemory(entry->memHandle, zoneName);

		T4::engine::g_zoneFileNames[entry->zoneFileIndex].name[0] = '\0';
	}

	// ── Level 1: removal + g_zoneLoaded compaction ───────────────────────────────
	// @modified — sub_48F7D0 reimpl (compaction + cleanup)
	__declspec(noinline)
	void DB_RemoveZoneEntry(XZoneLoadedEntry* entry)
	{
		int index = (int)(entry - T4::engine::g_zoneLoaded);

		DB_ZoneEntryCleanup(entry);

		int newCount = *T4::engine::g_zoneCount - 1;
		*T4::engine::g_zoneCount = newCount;

		if (index < newCount)
			memmove(&T4::engine::g_zoneLoaded[index], &T4::engine::g_zoneLoaded[index + 1],
				(newCount - index) * sizeof(XZoneLoadedEntry));
	}

	// @modified — replaces sub_48F720 to handle T4M custom zones 0x1000
	void __cdecl DB_UnloadAllZones()
	{
		T4::engine::Com_Printf(0, "[T4M] - DB_UnloadAllZones Start\n");
		// --- Phase 1: engine sync (identical to DB_LoadXAssets Phase 0.5) ---
		T4::engine::Sys_SyncDatabase();        // sub_6F6CE0
		T4::engine::DB_PostLoadXZone();        // sub_5A3320
		T4::engine::Sys_WakeDatabase();        // sub_6F6D60
		T4::engine::DB_WaitForPendingLoads();  // sub_5FDBF0
		T4::engine::DB_CheckPendingComplete(); // sub_48E560
		T4::engine::DB_PreUnloadResources();   // sub_48F8D0

		// --- Phase 2: acquire writer lock ---
		DB_WriterAcquire();

		// --- Phase 3: LIFO asset unload (no PMem free yet) ---
		for (int z = *T4::engine::g_zoneCount - 1; z >= 0; z--)
		{
			T4_Reconstructed::DB_UnloadZoneAssets(T4::engine::g_zoneLoaded[z].zoneFileIndex, /*freeMemory=*/false);
		}

		// --- Phase 4: cleanup asset references in the hash table ---
		T4::engine::DB_CleanupAssetRefs();  // sub_48F670 — iterates db_hashTable, returns entries to the free list
		T4::engine::DB_PostUnloadCleanup(); // sub_48F9B0

		// --- Phase 5: LIFO final cleanup (log + PMem_Free + clear zone name) ---
		// All zones are freed in the reverse order of their allocation, which
		// respects the strict LIFO of the PMem pool for all zones (vanilla + T4M).
		for (int z = *T4::engine::g_zoneCount - 1; z >= 0; z--)
		{
			DB_ZoneEntryCleanup(&T4::engine::g_zoneLoaded[z]);
		}

		// --- Phase 6: reset state flags ---
		*T4::engine::g_zoneCount = 0;
		*T4::engine::g_dbHasLoadedZones = false;  // byte_46DE3B6 — no more zones loaded

		// --- Phase 7: release writer lock ---
		DB_WriterRelease();
	}

	// @modified — replaces sub_48E7B0 with T4M Phase 3 for zones 0x1000
	void DB_LoadXAssets(XZoneInfo* zoneInfo, int zoneCount, int sync)
	{
		T4::engine::Com_Printf(0, "[T4M] - DB_LoadXAssets Start for %d zone\n", zoneCount);
		for (size_t i = 0; i < zoneCount; i++)
		{
			T4::engine::Com_Printf(0, "[T4M] - Zone name : %s \n", zoneInfo[i].name);
		}

		bool anyUnloaded = false;
		// ── PHASE 0: one-shot init ────────────────────────────────────────────
		if (!*T4::engine::g_dbInitialized)
		{
			*T4::engine::g_dbInitialized = true;
			T4::engine::DB_InitAssetEntryPool(); // sub_48D340 — g_assetEntryPool free list + per-type pool clear
		}

		// ── PHASE 0.5: engine synchronization ─────────────────────────────────
		T4::engine::Sys_SyncDatabase();        // sub_6F6CE0 — renderer pre-frame / flush pending
		T4::engine::DB_PostLoadXZone();        // sub_5A3320 — tick sync worker
		T4::engine::Sys_WakeDatabase();        // sub_6F6D60 — renderer post-frame
		T4::engine::DB_WaitForPendingLoads();  // sub_5FDBF0 — PMem frame update
		T4::engine::DB_CheckPendingComplete(); // sub_48E560 — update zone visibility

		if (zoneCount <= 0)
			goto skipToSync;

		// ── PHASE 1: unload assets (freeFlags match, LIFO scan) ───────────────
		for (int i = 0; i < zoneCount; i++)
		{
			int freeFlags = zoneInfo[i].freeFlags;
			if (freeFlags == 0) continue;

			for (int z = *T4::engine::g_zoneCount - 1; z >= 0; z--)
			{
				if (!(T4::engine::g_zoneLoaded[z].allocFlags & freeFlags)) continue;

				if (!anyUnloaded) {
					anyUnloaded = true;
					T4::engine::DB_PreUnloadResources(); // sub_48F8D0 — flush renderer, GPU, D3D buffers

					if (!*T4::engine::g_dbInUse) {
						*T4::engine::g_dbInUse = true;
						// Clear client model weapon slots (renderer scene @ dword_3BF392C)
						T4::engine::R_ClearScene();                          // sub_6B1440
						T4::engine::CL_ClearState();                         // sub_59EA90
						T4::engine::Hunk_ClearTempMemory(*(int*)0x16D7AD0);  // sub_4B2F80
					}

					DB_WriterAcquire(); // pure C++ reconstruction — replaces the broken register-based call
				}

				// zoneFileIndex = word at [entry+0x00]
				T4_Reconstructed::DB_UnloadZoneAssets(T4::engine::g_zoneLoaded[z].zoneFileIndex, /*freeMemory=*/true); // sub_48F340
			}
		}

		if (!anyUnloaded)
			goto skipToSync;

		// ── PHASE 2: post-unload hash table cleanup ──────────────────────────
		T4::engine::DB_PostUnloadCleanup(); // sub_48F9B0 — walks db_hashTable, frees orphan entries

		// ── PHASE 3: remove zones from g_zoneLoaded (hardcoded bits) ─────────
		// Each bit = one independent LIFO pass.
		// NOTE: 0x004 and 0x400 are absent (permanent flags, never unloaded).
		// NOTE: 0x1000 is also absent → manual T4M Phase 3 is mandatory.
		static const int priorityBits[] =
		{
			T4::engine::XZoneFlags::ZONE_T4M_MAP_LOCA, T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX,
			T4::engine::XZoneFlags::ZONE_MOD, T4::engine::XZoneFlags::ZONE_COMMON,
			T4::engine::XZoneFlags::ZONE_MAP_PATCH, T4::engine::XZoneFlags::ZONE_RESERVED_80,
			T4::engine::XZoneFlags::ZONE_POST_LOAD, T4::engine::XZoneFlags::ZONE_MAP_LOAD,
			T4::engine::XZoneFlags::ZONE_UI, T4::engine::XZoneFlags::ZONE_LOC_COMMON,
			T4::engine::XZoneFlags::ZONE_LOCALIZED, T4::engine::XZoneFlags::ZONE_BASE
		};

		for (int i = 0; i < zoneCount; i++)
		{
			int freeFlags = zoneInfo[i].freeFlags;
			for (int b = 0; b < sizeof(priorityBits) / sizeof(int); b++)
			{
				if (!(freeFlags & priorityBits[b])) continue;

				for (int z = *T4::engine::g_zoneCount - 1; z >= 0; z--)
				{
					if (T4::engine::g_zoneLoaded[z].allocFlags & priorityBits[b])
						DB_RemoveZoneEntry(&T4::engine::g_zoneLoaded[z]); // sub_48F7D0
				}
			}
		}

		// ── PHASE 4: unload finalization ─────────────────────────────────────
		DB_WriterRelease(); // release the writer lock taken by DB_WriterAcquire() in Phase 1
		*T4::engine::g_dbInUse = false;
		*T4::engine::g_assetsDirty = true;
		T4::engine::R_BeginRegistration();    // sub_6B1500
		T4::engine::CL_BeginRegistration();   // sub_59EB00
		T4::engine::Hunk_BeginRegistration(); // sub_4B3090
		T4::engine::DB_SyncAssets();          // sub_41D3A0
		// (updates thread ID in TLS[0x20] if not set)

	skipToSync:
		// ── PHASE 5: renderer sync (if sync && not already active) ───────────
		if (sync && !*T4::engine::g_dbInUse)
		{
			*T4::engine::g_dbInUse = true;
			// Clear client model weapon slots (same code as Phase 1)
			T4::engine::R_ClearScene();                          // sub_6B1440
			T4::engine::CL_ClearState();                         // sub_59EA90
			T4::engine::Hunk_ClearTempMemory(*(int*)0x16D7AD0);  // sub_4B2F80
		}

		// ── PHASE 6: enqueue the new zones for loading ──────────────────────
		*T4::engine::g_syncValue = sync;
		T4::engine::DB_AddZonesToQueue(zoneInfo, zoneCount); // sub_48E4C0

		// ── PHASE 7: if sync, wait for the load to finish ───────────────────
		if (sync)
		{
			T4::engine::DB_PostLoadXZone();    // sub_5A3320
			*T4::engine::g_dbInUse = false;
			*T4::engine::g_assetsDirty = true;
			T4::engine::R_BeginRegistration();
			T4::engine::CL_BeginRegistration();
			T4::engine::Hunk_BeginRegistration();
			T4::engine::DB_SyncAssets();       // sub_41D3A0
			// (updates thread ID in TLS[0x20] if not set)
		}
	}

	// @new — call-site hook on 0x6D5672 (mod_ex / mod_patch / localized_mod, no vanilla equivalent)
	void __cdecl ModFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
	{
		static XZoneInfo* modZoneInfo;
		bool load_modEx = false;
		bool load_modPatch = false;
		bool load_modEx_Patch = false;
		bool load_localized_mod = false;
		int totalZoneCount = zoneCount;

		// Default local is english
		const char* selectedLocal = "english";
		if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string, *T4::engine::language_system)))
		{
			load_localized_mod = true;
			totalZoneCount = totalZoneCount + 1;
			selectedLocal = *T4::engine::language_system;
		}

		// try to fallback to localized_english_mod.ff if we haven't didn't succeed reading localization.txt or we haven't found a corresponding localized_mod.ff file
		if (load_localized_mod == false)
		{
			if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string, selectedLocal)))
			{
				load_localized_mod = true;
				totalZoneCount = totalZoneCount + 1;
			}
		}
 
		// in cod waw mods are loaded from appdata not base game
		if (FileExists(va("%s\\%s\\mod_ex.ff", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string)))
		{
			load_modEx = true;
			totalZoneCount = totalZoneCount + 1;
		}
		if (FileExists(va("%s\\%s\\mod_patch.ff", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string)))
		{
			load_modPatch = true;
			totalZoneCount = totalZoneCount + 1;

		}
		if (FileExists(va("%s\\%s\\mod_ex_patch.ff", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string)))
		{
			load_modEx_Patch = true;
			totalZoneCount = totalZoneCount + 1;
		}


		if (!load_modPatch && !load_modEx && !load_modEx_Patch && !load_localized_mod)
		{
			DB_LoadXAssets(zoneInfo, zoneCount, sync);
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
				modZoneInfo[currentIndex].allocFlags = T4::engine::XZoneFlags::ZONE_MOD; 
				modZoneInfo[currentIndex].freeFlags = 0;
				currentIndex = currentIndex + 1;
			}
			if (load_modPatch)
			{
				modZoneInfo[currentIndex].name = "mod_patch";
				modZoneInfo[currentIndex].allocFlags = T4::engine::XZoneFlags::ZONE_MOD;
				modZoneInfo[currentIndex].freeFlags = 0;
				currentIndex = currentIndex + 1;
			}
			if (load_modEx_Patch)
			{
				modZoneInfo[currentIndex].name = "mod_ex_patch";
				modZoneInfo[currentIndex].allocFlags = T4::engine::XZoneFlags::ZONE_MOD;
				modZoneInfo[currentIndex].freeFlags = 0;
				currentIndex = currentIndex + 1;
			}
			if (load_localized_mod)
			{
				modZoneInfo[currentIndex].name = va("localized_%s_mod", selectedLocal);
				modZoneInfo[currentIndex].allocFlags = T4::engine::XZoneFlags::ZONE_MOD;
				modZoneInfo[currentIndex].freeFlags = 0;
			}
			DB_LoadXAssets(modZoneInfo, totalZoneCount, sync);
		}
	}


	// @new — call-site hook on 0x6D5728 (vulkan dvar init + DB_LoadXAssets call)
	void __cdecl CodePostGFXFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
	{
		// Code Post gfx is loaded after reading player var, so we need to update them with the value of the .conf, we can do this only here, so let's hack this shit
		UINT enableVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
		vulkan = T4::dvar::Dvar_RegisterBool(0, "vulkan", DVAR_FLAG_ARCHIVE, "Use vulkan instead of DirectX 9.0c (only used for UI, if you want to change please use the change in the options or in the .conf file");
		vulkan->current.boolean = enableVulkan;

		DB_LoadXAssets(zoneInfo, zoneCount, sync);
	}

	void DB_LoadZoneGeneric(const char* name, int allocFlags, int freeFlags)
	{
		XZoneInfo zi;
		zi.name = name;
		zi.allocFlags = allocFlags;
		zi.freeFlags = freeFlags;
		T4M::DB_LoadXAssets(&zi, 1, 0);
	}

	// @modified — replaces sub_59E050 (FS_AddUserMapDir + PATCH_EX zones)
	void __cdecl DB_LoadMapZones(const char* mapName)
	{
		T4::engine::Com_Printf(0x10, "[T4M] - DB_LoadMapZones Start for map %s\n", mapName);
		// Reset the fastfile streaming progress counters
		T4::engine::db_streamReadBlocksTotal = 0;  // 0x957400
		T4::engine::db_streamReadBlocksDone = 0;  // 0x957408
		T4::engine::db_streamDecompBytesTotal = 0;  // 0x95740C
		T4::engine::db_streamDecompBytesDone = 0;  // 0x9571A4
		T4::engine::db_streamEnabled = 0;  // 0x957404

		T4M::resetFakeIntroSecondValue = true;

		if ((*T4::engine::fs_game)->current.string[0] != '\0' && FS_ZoneFileExists(mapName, 2))
		{
			const char* usermapPath = va("%s/%s", "usermaps", mapName); // sub_5F6D80
			T4::engine::FS_AddUserMapDir(usermapPath);    // sub_5DD8A0
		}

		char localBuf[0x40];
		_snprintf(localBuf, sizeof(localBuf) - 1, "%s%s", "localized_", mapName);
		localBuf[sizeof(localBuf) - 1] = '\0';
		DB_LoadZoneGeneric(localBuf, T4::engine::XZoneFlags::ZONE_LOCALIZED, 
			T4::engine::XZoneFlags::ZONE_MAP_PATCH | 
			T4::engine::XZoneFlags::ZONE_UI | 
			T4::engine::XZoneFlags::ZONE_LOCALIZED | 
			T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX |
			T4::engine::XZoneFlags::ZONE_T4M_MAP_LOCA);

		char locaXlBuf[0x40];
		_snprintf(locaXlBuf, sizeof(locaXlBuf) - 1, "%s_%s_%s", "localized", *T4::engine::language_system, mapName);
		locaXlBuf[sizeof(locaXlBuf) - 1] = '\0';
		DB_LoadZoneGeneric(locaXlBuf, T4::engine::XZoneFlags::ZONE_T4M_MAP_LOCA, 
			T4::engine::XZoneFlags::ZONE_MAP_PATCH | 
			T4::engine::XZoneFlags::ZONE_UI | 
			T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX | 
			T4::engine::XZoneFlags::ZONE_T4M_MAP_LOCA);

		char patchNameBuf[0x40];
		_snprintf(patchNameBuf, sizeof(patchNameBuf) - 1, "%s_patch", mapName);
		patchNameBuf[sizeof(patchNameBuf) - 1] = '\0';
		DB_LoadZoneGeneric(patchNameBuf, T4::engine::XZoneFlags::ZONE_MAP_PATCH,
			T4::engine::XZoneFlags::ZONE_MAP_PATCH |
			T4::engine::XZoneFlags::ZONE_POST_LOAD |
			T4::engine::XZoneFlags::ZONE_UI |
			T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX);

		char patchNameExBuf[0x40];
		_snprintf(patchNameExBuf, sizeof(patchNameExBuf) - 1, "%s_patch_ex", mapName);
		patchNameExBuf[sizeof(patchNameExBuf) - 1] = '\0';
		DB_LoadZoneGeneric(patchNameExBuf, T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX, 
			T4::engine::XZoneFlags::ZONE_POST_LOAD | 
			T4::engine::XZoneFlags::ZONE_UI | 
			T4::engine::XZoneFlags::ZONE_T4M_PATCH_EX);

		DB_LoadZoneGeneric(mapName, T4::engine::XZoneFlags::ZONE_UI, 
			T4::engine::XZoneFlags::ZONE_POST_LOAD | 
			T4::engine::XZoneFlags::ZONE_UI);
	}

	// =====================================================================
	// @modified — replaces sub_48F260
	// Removed the "_patch → default.ff" fallback
	// =====================================================================
	__declspec(noinline)
	void DB_ProcessZoneQueue()
	{
		// Vanilla first-check: if the counter is 0 on entry, return without
		// resetting or signalling. Fast path when the worker was woken by a
		// non-load event.
		if (*T4::engine::g_zonesToLoad == 0)
			return;

		// Re-read + reset (vanilla writes 0 unconditionally between the two
		// reads — preserves the race window with DB_AddZonesToQueue).
		int pendingCount = *T4::engine::g_zonesToLoad;
		*T4::engine::g_zonesToLoad = 0;

		if (pendingCount > 0)
		{
			XZoneQueueEntry*entry = &T4::engine::g_zoneLoadQueue[0];

			for (int i = 0; i < pendingCount; i++)
			{
				int ok = T4::engine::DB_OpenZoneFile(entry, entry->allocFlags);

				if (ok == 0)
				{
					// ─── Vanilla "_patch → default.ff" fallback ─────────────
					// Don't know why it was make like that. 
					// But it's disabled because it cause problem when trying to new .ff if some are inexistant (they can get a big priority and make everything crash)
					/*
					if (strstr(entry->name, "_patch") != nullptr)
					{
						strcpy(entry->name, "default");
						ok = T4::engine::DB_OpenZoneFile(entry, entry->allocFlags);
						if (ok == 0)
							*T4::engine::g_pendingZoneCount -= 1;
					}
					else*/
					{
						*T4::engine::g_pendingZoneCount -= 1;
					}
				}

				entry++;  // 0x44 stride via the struct
			}
		}

		SetEvent(*T4::engine::g_dbSecondaryEvent);
	}
} // namespace T4M

void PatchT4_Load()
{
	// Working DO NOT TOUCH
	Detours::X86::DetourFunction(T4M::GetAddress("Load_CodePostGFX"), (uintptr_t)&T4M::CodePostGFXFFLoadHook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction(T4M::GetAddress("Load_Mod"), (uintptr_t)&T4M::ModFFLoadHook, Detours::X86Option::USE_CALL);
	
	// Original address of T4M::DB_LoadMapZones
	Detours::X86::DetourFunction(T4M::GetAddress("DB_LoadMapZones"), (uintptr_t)&T4M::DB_LoadMapZones,Detours::X86Option::USE_JUMP);

	// Fully replace sub_48F720 (DB_UnloadAllZones) with our reconstruction
	Detours::X86::DetourFunction(T4M::GetAddress("DB_UnloadAllZones"), (uintptr_t)&T4M::DB_UnloadAllZones,Detours::X86Option::USE_JUMP);

	Detours::X86::DetourFunction(T4M::GetAddress("DB_LoadXAssets"), (uintptr_t)&T4M::DB_LoadXAssets, Detours::X86Option::USE_JUMP);

	// DB worker queue dispatch — modified C++ reconstruction of sub_48F260.
	// Disabled the "_patch → default.ff" fallback
	Detours::X86::DetourFunction(T4M::GetAddress("DB_ProcessZoneQueue"), (uintptr_t)&T4M::DB_ProcessZoneQueue, Detours::X86Option::USE_JUMP);

	//00644C5D, r_init
	//if ((*dedicated)->current.integer > 0)
	//nop(0x00644C5D, 5);
	// 
	// TESTING SHIT
	//Detours::X86::DetourFunction((uintptr_t)0x0059E0EA, (uintptr_t)&LoadMapPatchZoneHook, Detours::X86Option::USE_CALL);
	//Detours::X86::DetourFunction((uintptr_t)0x0048FB90, (uintptr_t)&LoadLocalizedMapZoneHook, Detours::X86Option::USE_CALL);

}