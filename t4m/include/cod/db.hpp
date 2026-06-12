#pragma once

namespace T4
{
	namespace engine
	{
		// sub_48F980 — pre-flush subsystem clear (R_ClearScene/CL_ClearState/Hunk_ClearTempMemory; g_dbInUse reentrancy guard).
		WEAK symbol<void()> DB_PreFlushClearScene{ "DB_PreFlushClearScene" };

		// sub_48FB00 — post-flush: R_/CL_/Hunk_BeginRegistration, g_assetsDirty=1, DB_SyncAssets, caches thread id.
		WEAK symbol<void()> DB_PostFlushBeginRegistration{ "DB_PostFlushBeginRegistration" };

		// sub_7126C0 — renderer "assets-updated" notification (alt-thread gated; clears g_assetsDirty).
		WEAK symbol<void()> DB_NotifyRendererAssetChange{ "DB_NotifyRendererAssetChange" };

		// dword_3BED864 — nested render-sync recursion counter (Sys_WakeDatabase/Sys_SyncDatabase).
		WEAK symbol<int> g_renderSyncRecursion{ "g_renderSyncRecursion" };


		WEAK symbol<int(*)()> DB_GetXAssetSizeHandler{ "DB_GetXAssetSizeHandler" };
		WEAK symbol<void(*)(void* header)> DB_XAssetUnloadHandlers{"DB_XAssetUnloadHandlers"};
		WEAK symbol<void(*)(void* dstHeader, void* srcHeader, bool isPermZone)> DB_XAssetOverridePromoters{"DB_XAssetOverridePromoters"};
		WEAK symbol<void(*)(XAssetHeader* header, const char* stringTableEntry)> DB_XAssetSetNameHandlers{"DB_XAssetSetNameHandlers"};
		WEAK symbol<void(*)(void* pool, void* header)> DB_XAssetFreeHandlers{"DB_XAssetFreeHandlers"};
		WEAK symbol<void*(*)(void* pool)> DB_XAssetAllocHandlers{ "DB_XAssetAllocHandlers" };
		WEAK symbol<const char*> DB_XAssetDefaultNames{ "DB_XAssetDefaultNames" };  // data: const char** (array of names)
		WEAK symbol<void*> DB_XAssetPool{ "DB_XAssetPool" };                        // data: void** (array of pools)
		WEAK symbol<const char*(*)(XAssetHeader*)> DB_XAssetGetNameHandlers{ "DB_XAssetGetNameHandlers" };

		// --- DB load/unload/zone engine functions (moved from T4.cpp extern "C") ---
		WEAK symbol<const char*(XAssetType, void(__cdecl*)(XAssetHeader, void*), void*, bool)> DB_EnumXAssets{ "DB_EnumXAssets" };
		WEAK symbol<void(XAssetType, void(__cdecl*)(XAssetHeader, void*), void*)> DB_EnumXAssets_FastFile{ "DB_EnumXAssets_FastFile" };
		WEAK symbol<void(XZoneInfo*, int, int)> DB_LoadXAssets{ "DB_LoadXAssets" };
		WEAK symbol<void()> DB_InitAssetEntryPool{ "DB_InitAssetEntryPool" };
		WEAK symbol<void()> DB_PostLoadXZone{ "DB_PostLoadXZone" };
		WEAK symbol<void()> DB_WaitForPendingLoads{ "DB_WaitForPendingLoads" };
		WEAK symbol<void()> DB_CheckPendingComplete{ "DB_CheckPendingComplete" };
		WEAK symbol<void()> DB_PreUnloadResources{ "DB_PreUnloadResources" };
		WEAK symbol<void()> DB_UnloadAllZones{ "DB_UnloadAllZones" };
		WEAK symbol<void()> DB_CleanupAssetRefs{ "DB_CleanupAssetRefs" };
		WEAK symbol<void()> DB_PostUnloadCleanup{ "DB_PostUnloadCleanup" };
		WEAK symbol<void(XZoneLoadedEntry*)> DB_RemoveZoneEntry{ "DB_RemoveZoneEntry" };
		WEAK symbol<void()> DB_ZoneEntryCleanup{ "DB_ZoneEntryCleanup" };
		WEAK symbol<void()> DB_FreeXZoneMemory{ "DB_FreeXZoneMemory" };
		WEAK symbol<void()> DB_SyncAssets{ "DB_SyncAssets" };
		WEAK symbol<void(XZoneInfo*, int)> DB_AddZonesToQueue{ "DB_AddZonesToQueue" };
		WEAK symbol<void()> DB_InUseHandlerDispatch{ "DB_InUseHandlerDispatch" };
		WEAK symbol<void()> DB_PromoteHelper{ "DB_PromoteHelper" };
		WEAK symbol<int(XZoneQueueEntry*, int)> DB_OpenZoneFile{ "DB_OpenZoneFile" };
	}
}
