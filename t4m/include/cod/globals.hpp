#pragma once

// Vanilla data-global pointers moved from T4.cpp extern "C". All in T4::engine;
// re-sort into proper cod/*.hpp later. symbol<T> resolves the address lazily (AddrMap).

struct __declspec(align(4)) dvar_t;  // global struct (T4.h) — fwd-decl for symbol<dvar_t*>

namespace T4
{
	namespace engine
	{
		WEAK symbol<cmd_function_s*> cmd_functions{ "g_cmdListHead" };  // head of cmd linked list (dword_1F416F4)
		WEAK symbol<DWORD> cmd_id{ "cmd_id" };
		WEAK symbol<DWORD> cmd_argc{ "cmd_argc" };
		WEAK symbol<DWORD*> cmd_argv{ "cmd_argv" };
		WEAK symbol<XAssetEntryPoolEntry*> g_freeAssetEntries{ "g_freeAssetEntries" };
		WEAK symbol<XAssetEntry*> g_inuseEntry{ "g_inuseEntry" };
		WEAK symbol<XAssetHeader*> g_inuseHeader{ "g_inuseHeader" };
		WEAK symbol<unsigned int> g_assetRefCount{ "g_assetRefCount" };
		WEAK symbol<XAssetEntryPoolEntry> g_assetEntryPool{ "g_assetEntryPool" };
		WEAK symbol<unsigned __int16> db_hashTable{ "db_hashTable" };
		WEAK symbol<unsigned int> com_frameTime{ "com_frameTime" };
		WEAK symbol<bool> g_dbInitialized{ "g_dbInitialized" };
		WEAK symbol<bool> g_dbHasLoadedZones{ "g_dbHasLoadedZones" };
		WEAK symbol<int> g_zoneCount{ "g_zoneCount" };
		WEAK symbol<XZoneLoadedEntry> g_zoneLoaded{ "g_zoneLoaded" };
		WEAK symbol<bool> g_dbInUse{ "g_dbInUse" };
		WEAK symbol<int> g_syncValue{ "g_syncValue" };
		WEAK symbol<int> g_dbReaderCount{ "g_dbReaderCount" };
		WEAK symbol<int> g_dbWriterCount{ "g_dbWriterCount" };
		WEAK symbol<HANDLE> g_dbWorkerEvent{ "g_dbWorkerEvent" };
		WEAK symbol<DWORD> g_dbWorkerThreadId{ "g_dbWorkerThreadId" };
		WEAK symbol<DWORD> g_waitStartTime{ "g_waitStartTime" };
		WEAK symbol<int> g_waitTimerStarted{ "g_waitTimerStarted" };
		WEAK symbol<HANDLE> g_dbSecondaryEvent{ "g_dbSecondaryEvent" };
		WEAK symbol<DWORD> g_dbAltThreadId{ "g_dbAltThreadId" };
		WEAK symbol<DWORD> g_dbSecondaryThreadId{ "g_dbSecondaryThreadId" };
		WEAK symbol<HANDLE> g_dbPauseEventHandle{ "g_dbPauseEventHandle" };
		WEAK symbol<int> g_dbWorkerPausedFlag{ "g_dbWorkerPausedFlag" };
		WEAK symbol<uint8_t> g_comInitDone{ "g_comInitDone" };
		WEAK symbol<uint8_t> g_dbFlag3BED85D{ "g_dbFlag3BED85D" };
		WEAK symbol<DWORD> g_dbPtr99724C{ "g_dbPtr99724C" };
		WEAK symbol<bool> g_assetsDirty{ "g_assetsDirty" };
		WEAK symbol<int> g_copyInfoCount{ "g_copyInfoCount" };
		WEAK symbol<XAssetEntry*> g_copyInfo{ "g_copyInfo" };
		WEAK symbol<gentity_s> g_entities{ "g_entities" };
		WEAK symbol<WeaponDef*> bg_weaponDefs{ "bg_weaponDefs" };
		WEAK symbol<AimAssistGlobals> aaGlobArray{ "aaGlobArray" };
		WEAK symbol<ZoneFileEntry> g_zoneFileNames{ "g_zoneFileNames" };
		WEAK symbol<PMem_Pool> g_pmem_pools{ "g_pmem_pools" };
		WEAK symbol<XZoneQueueEntry> g_zoneLoadQueue{ "g_zoneLoadQueue" };
		WEAK symbol<int> g_zonesToLoad{ "g_zonesToLoad" };
		WEAK symbol<int> g_pendingZoneCount{ "g_pendingZoneCount" };
		WEAK symbol<int> g_currentZoneIndex{ "g_currentZoneIndex" };
		WEAK symbol<unsigned int> g_poolSize{ "g_poolSize" };
		WEAK symbol<dvar_t*> fs_localAppData{ "fs_localAppData" };
		WEAK symbol<dvar_t*> fs_game{ "fs_game" };
		WEAK symbol<dvar_t*> fs_basepath{ "fs_basepath" };
		WEAK symbol<dvar_t*> dedicated{ "dedicated" };
		WEAK symbol<dvar_t*> dvar_singlethreadRender{ "dvar_singlethreadRender" };
		WEAK symbol<dvar_t*> developer{ "developer" };
		WEAK symbol<dvar_t*> loc_language{ "loc_language" };
		WEAK symbol<const char*> language_system{ "language_system" };
		WEAK symbol<char*> g_assetNames{ "g_assetNames" };
		// db stream-progress counters (were DWORD& refs; unused -> symbol<DWORD>, deref with * if used)
		WEAK symbol<DWORD> db_streamEnabled{ "db_streamEnabled" };
		WEAK symbol<DWORD> db_streamReadBlocksTotal{ "db_streamReadBlocksTotal" };
		WEAK symbol<DWORD> db_streamReadBlocksDone{ "db_streamReadBlocksDone" };
		WEAK symbol<DWORD> db_streamDecompBytesTotal{ "db_streamDecompBytesTotal" };
		WEAK symbol<DWORD> db_streamDecompBytesDone{ "db_streamDecompBytesDone" };
	}
}
