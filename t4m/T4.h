#pragma once

#include "enums.hpp"
#include "structs.hpp"
#include "xasset.hpp"
#include "clientscript/clientscript_public.hpp"
using namespace T4;  // unqualified access to T4:: types in this header

// DVAR_FLAG_* enum DvarFlags defined in cod/enums.hpp under T4::, exposed via `using namespace T4;` below.

using dvarType_t = ::T4::dvarType_t;


union dvar_value_t 
{
	char*	string;
	int		integer;
	float	value;
	bool	boolean;
	bool    enabled;
	float	vec2[2];
	float	vec3[3];
	float	vec4[4];
	BYTE	color[4]; //to get float: multiply by 0.003921568859368563 - BaberZz
	__int64 integer64; // only in Tx
};

union dvar_maxmin_t 
{
	int i;
	float f;
};




typedef struct __declspec(align(4)) dvar_t
{
	//startbyte:endbyte
	const char*		name; //0:3
	const char*		description; //4:7
	unsigned __int16	flags; //8:11
	dvarType_t			type; //12:12
	char			modified;
	char			pad2[4]; //13:15
	dvar_value_t	current; //16:31
	dvar_value_t	latched; //32:47
	dvar_value_t	defaulta; //48:64
	dvar_maxmin_t min; //65:67
	dvar_maxmin_t max; //68:72 woooo

	inline bool isEnabled() 
	{
		return (this && this->current.boolean);
	}

} dvar_t;

using scriptInstance_t = ::T4::scriptInstance_t;

typedef void(__cdecl * xcommand_t)(void);

#define cmd_functions_ADDR 0x01F416F4

using cmd_function_s = ::T4::cmd_function_s;

using CmdArgs = ::T4::CmdArgs;

using XZoneInfo = ::T4::XZoneInfo;

// XZoneLoadedEntry defined in cod/xasset.hpp under T4::, exposed via `using namespace T4;` above.

// Load queue entry — populated by DB_AddZonesToQueue (sub_48E4C0),
// drained by DB_ProcessZoneQueue (sub_48F260) on the worker thread.
// Stride 0x44: name[0x40] + allocFlags.
struct XZoneQueueEntry
{
	char name[0x40];     // +0x00 — file name (max 63 chars + null)
	int  allocFlags;     // +0x40 — priority for the loaded zone
};

// Zone flags — used both as allocFlags (assigned type) and freeFlags (type to unload)
// NOTE: ZONE_CODE_POST_GFX (0x04) and ZONE_RESERVED_400 (0x400) are "permanent" flags
//       → never unloaded via freeFlags in DB_LoadXAssets
enum XZoneFlags : int 
{
	ZONE_BASE = 0x001,   // (base/inconnu)
	ZONE_LOCALIZED = 0x002,   // localized_<map>
	ZONE_CODE_POST_GFX = 0x004,   // [PERMANENT — never unloaded]
	ZONE_LOC_COMMON = 0x008,   // localized_common.ff
	ZONE_UI = 0x010,   // mainly ui but can be used for something else
	ZONE_MAP_LOAD = 0x020,   // <map>_load.ff
	ZONE_POST_LOAD = 0x040,   
	ZONE_RESERVED_80 = 0x080,   // (reserved, unknow usage)
	ZONE_MAP_PATCH = 0x100,   // <map>_patch.ff
	ZONE_COMMON = 0x200,   // common.ff
	ZONE_RESERVED_400 = 0x400,   // [PERMANENT — never unloaded]
	ZONE_MOD = 0x800,   // everything related to mod.ff
	// ── Extension T4M ─────────────────────────────────────────────────
	ZONE_T4M_PATCH_EX = 0x1000,  // <map>_patch_ex.ff
	ZONE_T4M_MAP_LOCA = 0x2000,  // localized_<language>_<map>.ff
};

// Decomposition of observed freeFlags:
//   0x112 = ZONE_MAP_PATCH | ZONE_UI | ZONE_LOCALIZED
//   0x160 = ZONE_MAP_PATCH | ZONE_POST_LOAD | ZONE_MAP_LOAD
//   0x150 = ZONE_MAP_PATCH | ZONE_POST_LOAD | ZONE_UI
//   0x050 = ZONE_POST_LOAD | ZONE_UI
//   0x172 = ZONE_MAP_PATCH | ZONE_POST_LOAD | ZONE_MAP_LOAD | ZONE_UI | ZONE_LOCALIZED
//   0x3000 = ZONE_T4M_ALL  0x1000 | 0x2000

struct vec2_t { float x, y; };
struct vec3_t { float x, y, z; };
struct vec4_t { float x, y, z, w; };

using ConDrawInputGlob = ::T4::ConDrawInputGlob;

using scr_localVar_t = ::T4::scr_localVar_t;

using scr_block_s = ::T4::scr_block_s;

using sval_u = ::T4::sval_u;

// MT_SIZE is also defined as a #define in clientscript_public.hpp; undef before enum.
#ifdef MT_SIZE
#undef MT_SIZE
#endif
enum bitsShit
{
	MEMORY_NODE_BITS = 0x10,
	MEMORY_NODE_COUNT = 0x10000,
	MT_SIZE = 0x100000,
	REFSTRING_STRING_OFFSET = 0x4,
};

// RefString defined in cod/clientscript/clientscript_public.hpp under T4::, exposed via `using namespace T4;` above.
// VariableStackBuffer defined in cod/clientscript/clientscript_public.hpp under T4::, exposed via `using namespace T4;` above.

using VariableUnion = ::T4::VariableUnion;


using XZoneName = ::T4::XZoneName;

struct PMem_Pool
{
	int   type;           // +0x00: pool identifier (unused in this function)
	int   count;          // +0x04: number of active entries
	int   freePtr;        // +0x08: next free slot (index or ptr depending on version)
	struct {
		const char* name; // +0x00: pointer to the zone name (NULL = free slot)
		int         next; // +0x04: index of the next free slot in the free-list
	} entries[32];        // +0x0C: 32 × 8 = 0x100 bytes → total 0x0C + 0x100 = 0x10C ✓
};

struct ZoneFileEntry               // stride 0x4C
{
	char name[0x40];               // +0x00: file name (max 63 chars + null)
	int  loadOrder;                // +0x40: override priority (dword_D04CF0)
	int  fileSize;                 // +0x44: GetFileSize (dword_D04CF4)
	int  fileSource;               // +0x48: file origin (0=direct, 1=main path, 2=fallback)
};

// types
typedef void(__cdecl * CommandCB_t)(void);

// scr_entref_t defined in cod/clientscript/clientscript_public.hpp under T4::, exposed via `using namespace T4;` above.

typedef void(__cdecl * scr_function_t)(scr_entref_t);

// ═════════════════════════════════════════════════════════════════════════════
// namespace T4 — Engine vanilla functions, pointers, tables, global state
//   Convention: everything here is called directly through the vanilla engine
//   code. Function pointers are initialized with vanilla VAs; asm wrappers
//   expose a cdecl signature over usercall-convention engine entry points.
// ═════════════════════════════════════════════════════════════════════════════
namespace T4
{
	extern "C"
	{
		// ── Function pointer typedefs ───────────────────────────────────────────
		typedef int(__cdecl * AddFunction_t)(scriptInstance_t inst, int func);
		typedef void(__cdecl * Com_Error_t)(int type, const char* message, ...);
		typedef void(*Com_Printf_t)(int channel, const char* format, ...);
		typedef void(__cdecl * Com_PrintMessage_t)(int channel, const char *fmt, int error);
		typedef void(__cdecl* Com_PrintfChannel_t)(int channel, const char* fmt, ...);
		typedef void(__cdecl* Com_PrintError_t)(int channel, const char* fmt, ...);
		typedef int (__cdecl* Com_sscanf_t)(const char* src, const char* fmt, ...);
		typedef const char *(__cdecl * DB_EnumXAssets_t)(XAssetType type, void(__cdecl *func)(XAssetHeader, void *), void *inData, bool includeOverride);
		typedef void(__cdecl * DB_EnumXAssets_FastFile_t)(XAssetType type, void(__cdecl *func)(XAssetHeader, void *), void *inData);
		typedef void(*DB_LoadXAssets_t)(XZoneInfo* data, int count, int sync);
		typedef char* (__cdecl* Sys_BinaryPath_t)();
		typedef void(*DB_InitAssetEntryPool_t)();
		typedef void(*Sys_SyncDatabase_t)();
		typedef void(*DB_PostLoadXZone_t)();
		typedef void(*Sys_WakeDatabase_t)();
		typedef void(*DB_WaitForPendingLoads_t)();
		typedef void(*DB_CheckPendingComplete_t)();
		typedef void(*DB_PreUnloadResources_t)();
		typedef void(*DB_UnloadAllZones_t)();
		typedef void(*DB_CleanupAssetRefs_t)();
		typedef void(*DB_PostUnloadCleanup_t)();
		typedef void(*DB_RemoveZoneEntry_t)(XZoneLoadedEntry* entry);
		typedef void(*DB_ZoneEntryCleanup_t)();
		typedef void(*DB_FreeXZoneMemory_t)();
		typedef void(*R_BeginRegistration_t)();
		typedef void(*CL_BeginRegistration_t)();
		typedef void(*Hunk_BeginRegistration_t)();
		typedef void(*DB_SyncAssets_t)();
		typedef void(*R_ClearScene_t)();
		typedef void(*CL_ClearState_t)();
		typedef void(*Hunk_ClearTempMemory_t)(int handle);
		typedef void(*DB_AddZonesToQueue_t)(XZoneInfo* zones, int count);
		typedef void(__cdecl * DB_InUseHandlerDispatch_t)();
		typedef void(__cdecl * DB_PromoteHelper_t)();
		typedef void(__cdecl * DB_XAssetUnloadHandler_t)(void* header);
		typedef void(__cdecl * DB_XAssetOverridePromoter_t)(void* dstHeader, void* srcHeader, bool isPermZone);
		typedef void(__cdecl * DB_XAssetSetNameHandler_t)(XAssetHeader* header, const char* stringTableEntry);
		typedef void(__cdecl * DB_XAssetFreeHandler_t)(void* pool, void* header);
		typedef void*(__cdecl * DB_XAssetAllocHandler_t)(void* pool);
		typedef int(__cdecl * StringTable_Find_t)(int arg_0, const char* name, int arg_2, int len);
		typedef void(__cdecl * EmitMethod_t)(scriptInstance_t inst, sval_u expr, sval_u func_name, sval_u params, sval_u methodSourcePos, bool bStatement, scr_block_s *block);
		typedef void* (*Scr_GetFunction_t)(const char **pName, int *type);
		typedef void* (*CScr_GetFunction_t)(const char **pName, int *type);
		typedef void* (*CScr_GetMethod_t)(const char **pName, int *type);
		typedef dvar_t* (__cdecl*Dvar_FindMalleableVarT)(const char* name);
		typedef int(__cdecl * Player_GetMethod_t)(const char **pName);
		typedef int(__cdecl * ScriptEnt_GetMethod_t)(const char **pName);
		typedef int(__cdecl * ScriptVehicle_GetMethod_t)(const char **pName);
		typedef int(__cdecl * HudElem_GetMethod_t)(const char **pName);
		typedef int(__cdecl * Helicopter_GetMethod_t)(const char **pName);
		typedef int(__cdecl * Actor_GetMethod_t)(const char **pName);
		typedef int(__cdecl * BuiltIn_GetMethod_t)(const char **pName, int *type);
		typedef void(*RemoveRefToValue_t)(scriptInstance_t inst, int type, VariableUnion u);
		typedef int(__cdecl* DB_GetXAssetSizeHandler_t)();
		typedef const char *(__cdecl *DB_XAssetGetNameHandler)(XAssetHeader *);

		// ── Vanilla engine function pointers (non-detoured) ─────────────────────
		extern AddFunction_t AddFunction;
		extern Com_Error_t Com_Error;
		extern Com_Printf_t Com_Printf;
		extern Com_PrintMessage_t Com_PrintMessage;
		extern Com_PrintfChannel_t Com_PrintfChannel;
		extern Com_PrintError_t Com_PrintError;        // sub_59A440
		extern Com_sscanf_t Com_sscanf;                // sub_7AB559 (static-CRT sscanf)
		extern DB_EnumXAssets_t DB_EnumXAssets;
		extern DB_EnumXAssets_FastFile_t DB_EnumXAssets_FastFile;
		// DB_FindXAssetHeader (sub_48DA30) is DETOURED — see T4_Reconstructed::DB_FindXAssetHeader.
		extern DB_LoadXAssets_t DB_LoadXAssets;
		extern Sys_BinaryPath_t Sys_BinaryPath;
		extern DB_InitAssetEntryPool_t DB_InitAssetEntryPool;
		extern Sys_SyncDatabase_t Sys_SyncDatabase;
		extern DB_PostLoadXZone_t DB_PostLoadXZone;
		extern Sys_WakeDatabase_t Sys_WakeDatabase;
		extern DB_WaitForPendingLoads_t DB_WaitForPendingLoads;
		extern DB_CheckPendingComplete_t DB_CheckPendingComplete;
		extern DB_PreUnloadResources_t DB_PreUnloadResources;
		// DB_UnloadZoneAssets (sub_48F340) is DETOURED — see T4_Reconstructed::DB_UnloadZoneAssets.
		extern DB_UnloadAllZones_t DB_UnloadAllZones;
		extern DB_CleanupAssetRefs_t DB_CleanupAssetRefs;
		extern DB_PostUnloadCleanup_t DB_PostUnloadCleanup;
		extern DB_RemoveZoneEntry_t DB_RemoveZoneEntry;
		extern DB_ZoneEntryCleanup_t DB_ZoneEntryCleanup;
		extern DB_FreeXZoneMemory_t DB_FreeXZoneMemory;
		extern R_BeginRegistration_t R_BeginRegistration;
		extern CL_BeginRegistration_t CL_BeginRegistration;
		extern Hunk_BeginRegistration_t Hunk_BeginRegistration;
		extern DB_SyncAssets_t DB_SyncAssets;
		extern R_ClearScene_t R_ClearScene;
		extern CL_ClearState_t CL_ClearState;
		extern Hunk_ClearTempMemory_t Hunk_ClearTempMemory;
		extern DB_AddZonesToQueue_t DB_AddZonesToQueue;
		extern DB_InUseHandlerDispatch_t DB_InUseHandlerDispatch;
		extern DB_PromoteHelper_t DB_PromoteHelper;
		extern EmitMethod_t EmitMethod;
		extern Scr_GetFunction_t Scr_GetFunction;
		extern CScr_GetFunction_t CScr_GetFunction;
		extern CScr_GetMethod_t CScr_GetMethod;
		extern Dvar_FindMalleableVarT Dvar_FindMalleableVar;
		extern Player_GetMethod_t Player_GetMethod;
		extern ScriptEnt_GetMethod_t ScriptEnt_GetMethod;
		extern ScriptVehicle_GetMethod_t ScriptVehicle_GetMethod;
		extern HudElem_GetMethod_t HudElem_GetMethod;
		extern Helicopter_GetMethod_t Helicopter_GetMethod;
		extern Actor_GetMethod_t Actor_GetMethod;
		extern BuiltIn_GetMethod_t BuiltIn_GetMethod;
		extern RemoveRefToValue_t RemoveRefToValue;

		// ── Per-type handler tables ─────────────────────────────────────────────
		extern DB_XAssetUnloadHandler_t*     DB_XAssetUnloadHandlers;      // 0x8DC948
		extern DB_XAssetOverridePromoter_t*  DB_XAssetOverridePromoters;   // 0x8DC9D8
		extern DB_XAssetSetNameHandler_t*    DB_XAssetSetNameHandlers;     // 0x8DCB88
		extern DB_XAssetFreeHandler_t*       DB_XAssetFreeHandlers;        // 0x8DC798
		extern DB_XAssetAllocHandler_t*      DB_XAssetAllocHandlers;       // 0x8DC708
		extern const char**                  DB_XAssetDefaultNames;        // 0x8DC8B8
		extern void**                        DB_XAssetPool;                // 0x8DC828
		extern DB_XAssetGetNameHandler*      DB_XAssetGetNameHandlers;
		extern DB_GetXAssetSizeHandler_t*    DB_GetXAssetSizeHandler;

		// ── String table ────────────────────────────────────────────────────────
		extern StringTable_Find_t            StringTable_Find;        // 0x68DE50
		extern char**                        g_stringTableBase;       // &dword_3702390

		// ── Override-system globals ────────────────────────────────────────────
		extern XAssetEntryPoolEntry**   g_freeAssetEntries;       // 0x957884
		extern XAssetEntry**            g_inuseEntry;             // 0x957564
		extern XAssetHeader**           g_inuseHeader;            // 0x9575E8
		extern unsigned int*            g_assetRefCount;          // 0x46DEB28

		// ── Engine state globals ────────────────────────────────────────────────
		extern XAssetEntryPoolEntry* g_assetEntryPool;
		extern XZoneName*            g_zoneNames;
		extern bool*                 g_dbInitialized;
		extern bool*                 g_dbHasLoadedZones;
		extern int*                  g_zoneCount;
		extern XZoneLoadedEntry*     g_zoneLoaded;
		extern ZoneFileEntry*        g_zoneFileNames;
		extern PMem_Pool*            g_pmem_pools;
		extern bool*                 g_dbInUse;
		extern int*                  g_syncValue;
		extern int*                  g_dbReaderCount;
		extern int*                  g_dbWriterCount;
		extern bool*                 g_assetsDirty;
		extern unsigned __int16*     db_hashTable;
		extern unsigned int*         com_frameTime;          // 0x00351DF34 — process-wide ms timer (timeGetTime), monotonic

		// ── DB load queue (consumed by DB_ProcessZoneQueue / sub_48F260) ──────
		extern XZoneQueueEntry*      g_zoneLoadQueue;        // 0x9592B8 — load queue (max 32, stride 0x44)
		extern int*                  g_zonesToLoad;          // 0xA51A44 — count consumed by the worker
		extern int*                  g_pendingZoneCount;     // 0x957314 — outstanding zones (decremented on failure)
		extern HANDLE*               g_dbSecondaryEvent;     // 0x1FF51C4 — completion signal (set when worker drains the batch)

		// Vanilla pointer — kept until/unless we reconstruct sub_48EE10.
		typedef int(__cdecl* DB_OpenZoneFile_t)(XZoneQueueEntry* entry, int allocFlags);
		extern DB_OpenZoneFile_t     DB_OpenZoneFile;        // 0x48EE10

		// Copy-info deferred-link queue (used by T4_Reconstructed::DB_PushCopyInfo
		// and T4_Reconstructed::DB_FlushCopyInfoQueue — sub_48D720 / sub_48E560).
		//   g_copyInfo[i] = XAssetEntry* enqueued while g_syncValue == 0.
		//   g_copyInfoCount ∈ [0, 0xC00]. Past 0xC00 → Com_Error "g_copyInfo exceeded".
		extern int*                  g_copyInfoCount;    // dword_957E8C
		extern XAssetEntry**         g_copyInfo;         // dword_AD1DD8

		// ── cmd_* pointers ──────────────────────────────────────────────────────
		extern DWORD*  cmd_id;
		extern DWORD*  cmd_argc;
		extern DWORD** cmd_argv;

		// ── dvar pointer arrays ────────────────────────────────────────────────
		extern dvar_t** fs_game;
		extern dvar_t** fs_localAppData;
		extern dvar_t** fs_basepath;
		extern dvar_t** dedicated;

		// ── db stream globals ───────────────────────────────────────────────────
		extern unsigned long& db_streamEnabled;
		extern unsigned long& db_streamReadBlocksTotal;
		extern unsigned long& db_streamReadBlocksDone;
		extern unsigned long& db_streamDecompBytesTotal;
		extern unsigned long& db_streamDecompBytesDone;
		extern const char**   language_system;

		// ── Asm/naked wrappers (cdecl bridges over vanilla usercall) ───────────
		extern void Cbuf_AddText(const char* text, int localClientNum);
		extern double CG_CornerDebugPrint(const char* text, float x, float y, float label_width, char* label, float* color);
		extern void FS_AddUserMapDir(const char* dirPath);
		extern dvar_t* Dvar_RegisterBool(bool value, const char *dvarName, int flags, const char *description = "");
		extern dvar_t* Dvar_RegisterFloat(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description = "");
		extern dvar_t* Dvar_RegisterInt(int default_value, const char* name, int min, int max, int flags, const char* description = "");
		extern dvar_t* Dvar_RegisterEnum(const char** valueList, int defaultIndex, const char* dvarName, int flags, const char* description);
		// sub_5F6D00  __usercall: edi=dest, esi=size, [esp+4]=fmt, [esp+8]=val.
		// Equivalent to _snprintf(dest, size, fmt, val).
		extern void Com_sprintf(char* dest, int nBytes, const char* fmt, DWORD val);

		// ── Scr_GetMethod — defined in PatchT4Script.cpp:178 ───────────────────
		extern int Scr_GetMethod(int *type, const char **pName);
	} // extern "C"

	// ── Inline accessors for cmd args ───────────────────────────────────────────
	inline int Cmd_Argc(void)
	{
		return cmd_argc[*cmd_id];
	}

	inline char *Cmd_Argv(int arg)
	{
		if ((unsigned)arg >= cmd_argc[*cmd_id]) 
		{
			return (char*)"";
		}
		return (char*)(cmd_argv[*cmd_id][arg]);
	}
} // namespace T4

// ═════════════════════════════════════════════════════════════════════════════
// namespace T4_Reconstructed — C++ reconstructions detoured at vanilla VAs
//   The engine executes our code via DetourFunction. Behavior is faithful to
//   vanilla (tags @faithful / @modified / @new / @wrapper preserved in .cpp).
//   Detours are installed in PatchT4_Override() / PatchT4_Load() / etc.
// ═════════════════════════════════════════════════════════════════════════════
namespace T4_Reconstructed
{
	extern "C"
	{
		// @faithful — sub_48D410
		unsigned int     DB_HashAssetName(int type, const char* name);
		// @faithful — sub_48D3B0
		XAssetEntry*     DB_AllocXAssetEntry(int type, unsigned char zoneIndex);
		// @faithful — sub_48D760 (via T4M::DB_FindXAssetByName_Wrapper)
		XAssetEntry*     DB_FindXAssetByName(int type, const char* name);
		// @faithful — sub_48D7D0 (via T4M::DB_FindDefaultAsset_Wrapper)
		void*            DB_FindDefaultAsset(int type);
		// @faithful — sub_48D860 (full C++; T4M::DB_LinkXAssetEntry_Wrapper bridges __usercall)
		XAssetEntry*     DB_LinkXAssetEntry(int type, const char* name);
		// @faithful (simplified) — sub_48DA30 (+ zoneIndex==0 bypass for T4M)
		void*            DB_FindXAssetHeader(int type, const char* name, bool useDefault, int timeoutMs);
		// @faithful — sub_48DFF0
		XAssetEntry*     DB_LinkXAssetEntryOverrideAware(XAssetEntry* newEntry, int copyData);
		// @faithful — loc_48F4C3 (helper of DB_UnloadZoneAssets)
		void             DB_PromoteOverride(XAssetEntry* main, XAssetEntry* override);
		// @faithful — sub_48F340
		void             DB_UnloadZoneAssets(int zoneFileIndex, bool copyDefaults);
		// @faithful — sub_48D720 (only caller: DB_LinkXAssetEntryOverrideAware)
		void             DB_PushCopyInfo(XAssetEntry* entry);
		// @faithful — sub_48E560 (same flat symbol as T4::DB_CheckPendingComplete
		//             would be used, so the recon uses a different C name).
		void             DB_FlushCopyInfoQueue();
		// @faithful — sub_48F260 (DB worker queue dispatch + "_patch → default" fallback)
		void             DB_ProcessZoneQueue();
	} // extern "C"
} // namespace T4_Reconstructed

// ═════════════════════════════════════════════════════════════════════════════
// namespace T4M — T4M-only code
//   Helpers that T4M calls (not detoured), T4M C++ reconstructions with
//   extended behavior, T4M inventions (debug dumps), and naked __usercall
//   wrappers. No vanilla engine code lives here.
// ═════════════════════════════════════════════════════════════════════════════
namespace T4M
{
	extern "C"
	{
		// ── @faithful helpers (not detoured) ───────────────────────────────────
		void FS_BuildZonePath(char* dst, int mode, const char* mapName);
		bool FS_ZoneFileExists(const char* mapName, int mode);
		int  Q_stricmpn(const char* s1, const char* s2, int maxLen);
		void DB_WriterAcquire();
		void DB_WriterRelease();
		void DB_ReaderAcquire();
		void DB_ReaderRelease();
		void DB_EnumAssetPool(int type, void (__cdecl* callback)(void*, int*), int* pType, int followOverrides);
		void DB_EnumAssetPoolB(int type, void (__cdecl* callback)(void*, int*), int* pType, int followOverrides);

		// ── Project helpers / asset pool utilities ─────────────────────────────
		void*         DB_ReallocXAssetPool(XAssetType type, unsigned int newSize);
		char*         __cdecl DB_GetXAssetTypeName(int type);
		void          __cdecl DB_ListAssetPool(XAssetType type, bool count_only);
		const char*   __cdecl DB_GetXAssetHeaderName(int type, XAssetHeader *header);
		const char*   __cdecl DB_GetXAssetName(XAsset *asset);
		int           __cdecl DB_GetXAssetTypeSize(int type);

		// ── Detour targets (replacements for vanilla bugged functions) ─────────
		// @modified — detour of sub_7AFFC0 (optimised memmove → CRT memcpy fix)
		void* Sys_MemCpyFix(void* dst, void** src, int len);

		extern bool resetFakeIntroSecondValue;
		extern int timeAtMapStart;

		// @modified — detour of sub_44B7E0 (FAKE_INTRO_SECONDS formatter,
		//   uses cl.serverTime instead of vanilla com_frameTime so the value
		//   resets between scenes). Hooked via Key_FormatIntroSeconds_Wrapper.
		void Key_FormatIntroSeconds(const char* argStr, char* outBuf);

		void Key_FormatIntroHourMinSec(const char* argStr, char* outBuf);

		// ── Debug utilities (PatchT4MAM_Override.cpp) — all @new ───────────────
		void DB_DumpOverrideChain(int type, const char* name);
		void DB_DumpHashBucket(unsigned int bucket);
		int  DB_CountActiveEntries();

		// ── __usercall → __cdecl wrappers (PatchT4MAM_Override.cpp) — @wrapper ─
		// (naked qualifier only on the definition, not on the declaration)
		void DB_FindDefaultAsset_Wrapper();      // → T4_Reconstructed::DB_FindDefaultAsset
		void DB_FindXAssetByName_Wrapper();      // → T4_Reconstructed::DB_FindXAssetByName
		void DB_LinkXAssetEntry_Wrapper();       // → T4_Reconstructed::DB_LinkXAssetEntry
		void Key_FormatIntroSeconds_Wrapper();   // → T4M::Key_FormatIntroSeconds

		// ── Project C++ helpers (unprefixed in the legacy convention) ──────────
		int           __cdecl Scr_GetNumParam(scriptInstance_t inst);
		char*         __cdecl SL_ConvertToString(unsigned int stringValue, scriptInstance_t inst);
		// PatchT4Script.cpp helpers — namespaced T4M:: to avoid clash with cod/* T4:: declarations
		RefString*    __cdecl GetRefString(scriptInstance_t inst, unsigned int stringValue);
		void          Scr_ClearOutParams(scriptInstance_t inst);
		void          __cdecl IncInParam(scriptInstance_t inst);
		void          __cdecl Scr_AddInt(int value, scriptInstance_t inst);
		int           __cdecl Scr_GetInt(scriptInstance_t inst, unsigned int index);
		void          Cmd_AddCommand(const char *cmd_name, xcommand_t function);
		bool          isZombieMode();
		bool          Com_SessionMode_IsZombiesGame();
		bool          IsReflectionMode();
		void          DoReturn();
	} // extern "C"

	// ─── Inline helpers (non extern-C; inline mangling OK) ─────────────────────
	// @new — pool ↔ 16-bit index conversion in g_assetEntryPool (stride 0x10).
	inline unsigned short POOL_INDEX(XAssetEntry* e)
	{
		XAssetEntryPoolEntry* pe = reinterpret_cast<XAssetEntryPoolEntry*>(e);
		return static_cast<unsigned short>(pe - T4::g_assetEntryPool);
	}
	inline XAssetEntry* POOL_ENTRY(unsigned short idx)
	{
		return &T4::g_assetEntryPool[idx].entry;
	}

	// Zone priority: read from g_zoneFileNames[zoneIndex].loadOrder
	// (= dword at D04CB0 + zoneIndex * 0x4C + 0x40).
	inline int ZonePriority(unsigned char zoneIndex)
	{
		return T4::g_zoneFileNames[zoneIndex].loadOrder;
	}

	// Header getName via engine table.
	inline const char* GetName(XAssetEntry* e)
	{
		return T4::DB_XAssetGetNameHandlers[e->asset.type](&e->asset.header);
	}

	// getSize via engine table.
	inline int GetSize(int type)
	{
		return T4::DB_GetXAssetSizeHandler[type]();
	}
} // namespace T4M

#define retptr (uintptr_t)&T4M::DoReturn

namespace Dvars 
{
	namespace Functions 
	{
		dvar_t* Dvar_FindVar(const char* name);
	}
}

// Installation (called by Sys_RunInit). Installs the active detours.
void PatchT4_Override();

static bool IsUsingVulkan;
// I don't know what i am doing episode 9999, but there is a problem of order execution and the value of IsUsingVulkan is not correctly saved
// so for now a dirty fix how i like them as usual (that's false)
static bool AlreadySaidPopupNoVulkan = false;

static dvar_t* loadout_preset_usa;
static dvar_t* loadout_preset_rus;
static dvar_t* con_external;
static dvar_t* enable_scoreboard;
static dvar_t* disable_intro;
static dvar_t* vulkan;
// Tweak switch Mode
static dvar_t* is_watching_for_switch_mode_input;
static dvar_t* switch_mode_input_pressed;

#define CONFIG_FILE_LOCATION ".\\T4M-MAM.conf"
#define IS_BETA
#ifdef IS_BETA
#define DEFAULT_CONFIG_FILE_HEADER "// " SHORTVERSION_CONFIG_BETA_STR "  Config File"
#else
#define DEFAULT_CONFIG_FILE_HEADER "// " SHORTVERSION_CONFIG_STR "  Config File"
#endif

#define DEFAULT_CONFIG_FILE "// " DEFAULT_CONFIG_FILE_HEADER "\n\
// If you experience problem with the game like suttering, crash, vertex corruption etc\n\
// Its advise to enable Vulkan, however it's only recommand for Windows 10 and bove\n\
// You can enable it too on older version of Windows but be aware that your graphical card may be not compatible\n\
// since it need a Vulkan 1.3 capable driver and graphical card\n\
// Min Driver version need for the version of the vulkan driver bundled :\n\
// NVIDIA 510.47.03\n\
// AMD 22.0\n\
// Intel 22.0\n\
\n\
[Version] // Do not modify this, you can lost your custom parameter if modified \n\
Number = " TO_STRING(INTERNAL_VERSION_NUMBER) "\n\
[Options]\n\
EnableVulkan = 0\n\
[Fixes]\n\
UseFixedXAudio = 1"
