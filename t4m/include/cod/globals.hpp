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

		// cgame / script globals
		WEAK symbol<cg_s> cgArray{ "cgArray" };                      // cgArray[0].clientNum picks the rendered overlay
		WEAK symbol<int> g_precacheOpen{ "dword_18F6DB8" };          // non-zero while precaching is still allowed
		// SL string pool: gScrMemTreePub->mt_buffer (clientscript_public.hpp) — do NOT redeclare here.

		// friendly-name overlay style dvars (read by CG_DrawFriendOverlay)
		WEAK symbol<dvar_t*> friendlyNameFontColor{ "dvar_friendlyNameFontColor" };
		WEAK symbol<dvar_t*> hostileNameFontColor{ "dvar_hostileNameFontColor" };

		// --- AI limit phase 8.2 (globals) ------------------------------
		// level_locals_t level. The recon reads only level.time (+0x1040 = 0x18F6DC8).
		WEAK symbol<level_locals_s> level{ "level" };

		// CoD4 g_threatBias (actor_threat.cpp). threatTable[enemyGroup][selfGroup] @+0x20.
		WEAK symbol<threat_bias_t> g_threatBias{ "g_threatBias" };

		// level.time (level_locals_s + 0x1040, level = 0x18F5D88)
		WEAK symbol<int> level_time{ "level_time" };

		// Com_GetServerDObj tables: serverObjMap[entnum] -> 1-based slot in objBuf (DObj_s, stride 0x68)
		WEAK symbol<unsigned short> serverObjMap{ "serverObjMap" };

		WEAK symbol<BYTE>           objBuf{ "objBuf" };

		// non-zero while the client side is up; gates every G_Debug* draw
		WEAK symbol<int> g_48AE4D4_inCgame{ "g_48AE4D4_inCgame" };

		// cl_debug: server-side debug string / line lists and their "fromServer" flag
		WEAK symbol<int>  clsDebug_fromServer{ "clsDebug_fromServer" };

		WEAK symbol<BYTE> clsDebug_svStrings{ "clsDebug_svStrings" };

		WEAK symbol<BYTE> clsDebug_svLines{ "clsDebug_svLines" };

		// CL_GetViewPos source (float[3])
		WEAK symbol<float> clViewPos{ "clViewPos" };

		// shared debug colors (float[4] each, .rdata)
		WEAK symbol<float> colorRed{ "colorRed" };

		WEAK symbol<float> colorGreen{ "colorGreen" };

		WEAK symbol<float> colorBlue{ "colorBlue" };

		WEAK symbol<float> colorYellow{ "colorYellow" };

		WEAK symbol<float> colorMagenta{ "colorMagenta" };

		WEAK symbol<float> colorCyan{ "colorCyan" };

		WEAK symbol<float> colorOrange{ "colorOrange" };

		WEAK symbol<float> colorWhite{ "cornerDebug_color" };

		// entinfo / AI debug dvars (names from CoD4 usage; see notes.md)
		WEAK symbol<dvar_t*> g_entinfo{ "dvar_g_entinfo" };

		WEAK symbol<dvar_t*> g_entinfo_maxdist{ "dvar_g_entinfo_maxdist" };

		WEAK symbol<dvar_t*> g_entinfo_scale{ "dvar_g_entinfo_scale" };

		WEAK symbol<dvar_t*> g_entinfo_AItext{ "dvar_g_entinfo_AItext" };

		WEAK symbol<dvar_t*> ai_debugEntIndex{ "dvar_ai_debugEntIndex" };

		WEAK symbol<dvar_t*> ai_debugAccuracy{ "dvar_ai_debugAccuracy" };

		WEAK symbol<dvar_t*> ai_showClaimedNode{ "dvar_ai_showClaimedNode" };

		WEAK symbol<dvar_t*> ai_debugCoverSelection{ "dvar_ai_debugCoverSelection" };

		WEAK symbol<dvar_t*> ai_debugThreatSelection{ "dvar_ai_debugThreatSelection" };

		WEAK symbol<dvar_t*> ai_showRegion{ "dvar_ai_showRegion" };

		// WaW-only: pointer to the proximity visitor (colgeom_visitor_inlined_t, a local of
		// sub_4BEA40) that Actor_SightTrace hands to SV_SightTrace as its prim list; null
		// outside that call.
		WEAK symbol<colgeom_visitor_inlined_t*> actorSightProximity{ "actorSightProximity" };

		// g_scr_data.anim.weapons[128] (scr_animscript_t, 8 bytes). Indexed by turret weapon.
		WEAK symbol<scr_animscript_t> g_scr_animWeapons{ "g_scr_animWeapons" };

		// EntHandleList g_entitiesHandleList[MAX_GENTITIES] (CoD4 game/enthandle.cpp).
		WEAK symbol<EntHandleList>  g_entitiesHandleList{ "g_entitiesHandleList" };

		// scr_const.enemy (scr_const + 0x244). cscr_const_t only describes the first 0x20 bytes.
		WEAK symbol<unsigned short> scr_const_enemy{ "scr_const_enemy" };

		// --- AI limit phase 9 (client compass) ------------------------------
		// cg_entities[localClientNum << 10 | entnum] (centity_s, 0x2D4), entnum < 1024.
		WEAK symbol<centity_s> cg_entities{ "cg_entities" };
		// Same indexing for entnum >= 1024: a 0x31C-byte element whose leading fields match centity_s.
		WEAK symbol<BYTE>      cg_entitiesHigh{ "cg_entitiesHigh" };
	}
}
