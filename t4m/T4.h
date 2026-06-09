#pragma once

#define cmd_functions_ADDR 0x01F416F4

typedef void(__cdecl * xcommand_t)(void);

enum bitsShit
{
	MEMORY_NODE_BITS = 0x10,
	MEMORY_NODE_COUNT = 0x10000,
	MT_SIZE = 0x100000,
	REFSTRING_STRING_OFFSET = 0x4,
};

struct __declspec(align(4)) RefString
{
	union bitsShit;
	char str[1];
};

#include "include/hexrays_defs.h"
#include "include/cod/enums.hpp"
#include "include/cod/structs.hpp"

// ── Global-scope type aliases — authoritative defs in T4::engine / T4::dvar ──
using XAssetType           = ::T4::engine::XAssetType;
using XAssetHeader         = ::T4::engine::XAssetHeader;
using XAsset               = ::T4::engine::XAsset;
using XAssetEntry          = ::T4::engine::XAssetEntry;
using XAssetEntryPoolEntry = ::T4::engine::XAssetEntryPoolEntry;
using T4::engine::ASSET_TYPE_MAX;
using XZoneInfo            = ::T4::engine::XZoneInfo;
using XZoneLoadedEntry     = ::T4::engine::XZoneLoadedEntry;
using XZoneQueueEntry      = ::T4::engine::XZoneQueueEntry;
using ZoneFileEntry        = ::T4::engine::ZoneFileEntry;
using PMem_Pool            = ::T4::engine::PMem_Pool;
using cmd_function_s       = ::T4::engine::cmd_function_s;
using ConDrawInputGlob     = ::T4::engine::ConDrawInputGlob;
using scr_localVar_t       = ::T4::engine::scr_localVar_t;
using scr_block_s          = ::T4::engine::scr_block_s;
using sval_u               = ::T4::engine::sval_u;
using scr_entref_t         = ::T4::engine::scr_entref_t;
using vec2_t               = ::T4::engine::vec2_t;
using vec3_t               = ::T4::engine::vec3_t;
using vec4_t               = ::T4::engine::vec4_t;
using dvarType_t           = ::T4::dvar::dvarType_t;
using scriptInstance_t     = ::T4::engine::scriptInstance_t;
using T4::engine::SCRIPTINSTANCE_SERVER;
using T4::engine::SCRIPTINSTANCE_CLIENT;
using T4::engine::SCRIPT_INSTANCE_MAX;

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
	BYTE	color[4];
	__int64 integer64;
};

union dvar_maxmin_t
{
	int i;
	float f;
};

typedef struct __declspec(align(4)) dvar_t
{
	const char*       name;        // 0x00
	const char*       description; // 0x04
	unsigned __int16  flags;       // 0x08
	dvarType_t        type;        // 0x0C
	char              modified;
	char              pad2[4];
	dvar_value_t      current;     // 0x10
	dvar_value_t      latched;     // 0x20
	dvar_value_t      defaulta;    // 0x30
	dvar_maxmin_t     min;         // 0x41
	dvar_maxmin_t     max;         // 0x44

	inline bool isEnabled()
	{
		return (this && this->current.boolean);
	}
} dvar_t;

// function pointer typedefs kept at global scope for external call sites
typedef void(__cdecl * CommandCB_t)(void);
typedef void(__cdecl * scr_function_t)(scr_entref_t);

// ═════════════════════════════════════════════════════════════════════════════
// namespace T4 — Engine vanilla functions, pointers, tables, global state
//   Convention: everything here is called directly through the vanilla engine
//   code. Function pointers are initialized with vanilla VAs; asm wrappers
//   expose a cdecl signature over usercall-convention engine entry points.
// ═════════════════════════════════════════════════════════════════════════════
namespace T4
{
	// ── All _t function pointer typedefs (engine + game + dvar) ──────────────
	// Kept at T4:: level so T4.cpp's flat namespace T4 { extern "C" { } } block
	// can reference them without sub-namespace qualification.

	// engine
	typedef void(__cdecl * Com_Error_t)(int type, const char* message, ...);
	typedef void(*Com_Printf_t)(int channel, const char* format, ...);
	typedef void(__cdecl * Com_PrintMessage_t)(int channel, const char *fmt, int error);
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
	typedef void*(__cdecl* Sys_EnterRecursiveLock_t)(void);                          // sub_70E340 (eax = recursion ctx)
	typedef int(__cdecl* Sys_LeaveRecursiveLock_t)(void);                            // sub_70E3A0 (returns ctx int)
	typedef void(__cdecl* Com_DvarDump_t)(int channel, int arg);                     // sub_59FE70
	typedef void(__cdecl* NET_RegisterDvars_t)(DWORD arg);                           // sub_677E90
	typedef int(__cdecl* DB_GetXAssetSizeHandler_t)();
	typedef const char *(__cdecl *DB_XAssetGetNameHandler)(XAssetHeader *);
	typedef int(__cdecl* DB_OpenZoneFile_t)(XZoneQueueEntry* entry, int allocFlags);

	// game
	typedef int(__cdecl * AddFunction_t)(scriptInstance_t inst, int func);
	typedef void(__cdecl * EmitMethod_t)(scriptInstance_t inst, sval_u expr, sval_u func_name, sval_u params, sval_u methodSourcePos, bool bStatement, scr_block_s *block);
	typedef void* (*Scr_GetFunction_t)(const char **pName, int *type);
	typedef void* (*CScr_GetFunction_t)(const char **pName, int *type);
	typedef void* (*CScr_GetMethod_t)(const char **pName, int *type);
	typedef int(__cdecl * Player_GetMethod_t)(const char **pName);
	typedef int(__cdecl * ScriptEnt_GetMethod_t)(const char **pName);
	typedef int(__cdecl * ScriptVehicle_GetMethod_t)(const char **pName);
	typedef int(__cdecl * HudElem_GetMethod_t)(const char **pName);
	typedef int(__cdecl * Helicopter_GetMethod_t)(const char **pName);
	typedef int(__cdecl * Actor_GetMethod_t)(const char **pName);
	typedef int(__cdecl * BuiltIn_GetMethod_t)(const char **pName, int *type);
	typedef void(*RemoveRefToValue_t)(scriptInstance_t inst, int type, T4::engine::VariableUnion u);

	// dvar
	typedef dvar_t* (__cdecl*Dvar_FindMalleableVarT)(const char* name);

	// ═════════════════════════════════════════════════════════════════════════
	// T4::engine — plomberie moteur, globals, tables, wrappers asm
	// ═════════════════════════════════════════════════════════════════════════
	namespace engine
	{
		extern "C"
		{
			// ── Vanilla engine function pointers (non-detoured) ─────────────────────
			extern Com_Error_t Com_Error;
			extern Com_Printf_t Com_Printf;
			extern Com_PrintMessage_t Com_PrintMessage;
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

			// ── Misc engine functions (true names; see addr_mapping.csv) ────────────
			extern Sys_EnterRecursiveLock_t      Sys_EnterRecursiveLock;  // 0x70E340
			extern Sys_LeaveRecursiveLock_t      Sys_LeaveRecursiveLock;  // 0x70E3A0
			extern Com_DvarDump_t                Com_DvarDump;            // 0x59FE70
			extern NET_RegisterDvars_t           NET_RegisterDvars;       // 0x677E90

			// ── Override-system globals ────────────────────────────────────────────
			extern XAssetEntryPoolEntry**   g_freeAssetEntries;       // 0x957884
			extern XAssetEntry**            g_inuseEntry;             // 0x957564
			extern XAssetHeader**           g_inuseHeader;            // 0x9575E8
			extern unsigned int*            g_assetRefCount;          // 0x46DEB28

			// ── Engine state globals ────────────────────────────────────────────────
			extern XAssetEntryPoolEntry* g_assetEntryPool;
			extern bool*                 g_dbInitialized;
			extern bool*                 g_dbHasLoadedZones;
			extern uint8_t*              g_comInitDone;     // byte_951A02 — set during Com_Init, gates DB busy check
			extern int*                  g_zoneCount;
			extern XZoneLoadedEntry*     g_zoneLoaded;
			extern ZoneFileEntry*        g_zoneFileNames;
			extern PMem_Pool*            g_pmem_pools;
			extern bool*                 g_dbInUse;
			extern DWORD*                g_waitStartTime;     // 0x22BEC34 — timeGetTime() at first DB wait
			extern int*                  g_waitTimerStarted;  // 0x4DE7054 — set once g_waitStartTime initialized
			extern int*                  g_syncValue;
			extern int*                  g_dbReaderCount;
			extern int*                  g_dbWriterCount;
			extern bool*                 g_assetsDirty;
			extern unsigned __int16*     db_hashTable;
			extern unsigned int*         com_frameTime;          // 0x00351DF34 — process-wide ms timer (timeGetTime), monotonic

			extern gentity_s*            g_entities;

			// ── Weapon / aim-assist globals ────────────────────────────────────────
			extern WeaponDef**           bg_weaponDefs;     // 0x8F6770 — array of WeaponDef* indexed by weapon index
			extern AimAssistGlobals*     aaGlobArray;       // 0x8E8690 — per-client aim-assist state (stride 0xE3C × 2)

			// ── DB load queue (consumed by DB_ProcessZoneQueue / sub_48F260) ──────
			extern XZoneQueueEntry*      g_zoneLoadQueue;        // 0x9592B8 — load queue (max 32, stride 0x44)
			extern int*                  g_zonesToLoad;          // 0xA51A44 — count consumed by the worker
			extern int*                  g_pendingZoneCount;     // 0x957314 — outstanding zones (decremented on failure)
			extern HANDLE*               g_dbSecondaryEvent;     // 0x1FF51C4 — completion signal (set when worker drains the batch)

			// Vanilla pointer — kept until/unless we reconstruct sub_48EE10.
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

			// ── fs/dedicated dvar pointer arrays (dvar_t is at file global scope) ──
			extern dvar_t** fs_game;
			extern dvar_t** fs_localAppData;
			extern dvar_t** fs_basepath;
			extern dvar_t** dedicated;
			extern dvar_t** dvar_singlethreadRender;       // dword_1F552FC
			extern dvar_t** developer;                     // dword_1F55288

			// ── db stream globals ───────────────────────────────────────────────────
			extern unsigned long& db_streamEnabled;
			extern unsigned long& db_streamReadBlocksTotal;
			extern unsigned long& db_streamReadBlocksDone;
			extern unsigned long& db_streamDecompBytesTotal;
			extern unsigned long& db_streamDecompBytesDone;
			extern const char**   language_system;

			// ── Asm/naked wrappers (cdecl bridges over vanilla usercall) ───────────
			extern void Cbuf_AddText(const char* text, int localClientNum);
			extern void FS_AddUserMapDir(const char* dirPath);
			// sub_5F6D00  __usercall: edi=dest, esi=size, [esp+4]=fmt, [esp+8]=val.
			// Equivalent to _snprintf(dest, size, fmt, val).
			extern void Com_sprintf(char* dest, int nBytes, const char* fmt, DWORD val);
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
	} // namespace engine

	// ═════════════════════════════════════════════════════════════════════════
	// T4::game — gameplay + scripting GSC (CG_/G_/BG_/PM_/Scr_/SL_/_GetMethod)
	// ═════════════════════════════════════════════════════════════════════════
	namespace game
	{
		extern "C"
		{
			// ── Vanilla function pointers (script/game) ─────────────────────────────
			extern AddFunction_t AddFunction;
			extern EmitMethod_t EmitMethod;
			extern Scr_GetFunction_t Scr_GetFunction;
			extern CScr_GetFunction_t CScr_GetFunction;
			extern CScr_GetMethod_t CScr_GetMethod;
			extern Player_GetMethod_t Player_GetMethod;
			extern ScriptEnt_GetMethod_t ScriptEnt_GetMethod;
			extern ScriptVehicle_GetMethod_t ScriptVehicle_GetMethod;
			extern HudElem_GetMethod_t HudElem_GetMethod;
			extern Helicopter_GetMethod_t Helicopter_GetMethod;
			extern Actor_GetMethod_t Actor_GetMethod;
			extern BuiltIn_GetMethod_t BuiltIn_GetMethod;
			extern RemoveRefToValue_t RemoveRefToValue;

			// ── Asm/naked wrappers ──────────────────────────────────────────────────
			extern double CG_CornerDebugPrint(const char* text, float x, float y, float label_width, char* label, float* color);

			// ── Scr_GetMethod — defined in PatchT4Script.cpp:178 ───────────────────
			extern int Scr_GetMethod(int *type, const char **pName);
		} // extern "C"
	} // namespace game

	// ═════════════════════════════════════════════════════════════════════════
	// T4::dvar — DVar subsystem (types live in cod/enums.hpp + cod/structs.hpp)
	// ═════════════════════════════════════════════════════════════════════════
	namespace dvar
	{
		extern "C"
		{
			extern Dvar_FindMalleableVarT Dvar_FindMalleableVar;

			// Asm/naked wrappers (cdecl bridges over vanilla usercall)
			extern dvar_t* Dvar_RegisterBool(bool value, const char *dvarName, int flags, const char *description = "");
			extern dvar_t* Dvar_RegisterFloat(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description = "");
			extern dvar_t* Dvar_RegisterInt(int default_value, const char* name, int min, int max, int flags, const char* description = "");
			extern dvar_t* Dvar_RegisterEnum(const char** valueList, int defaultIndex, const char* dvarName, int flags, const char* description);
		} // extern "C"
	} // namespace dvar
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
		T4::engine::XAssetEntry*     DB_AllocXAssetEntry(int type, unsigned char zoneIndex);
		// @faithful — sub_48D760 (via T4M::DB_FindXAssetByName_Wrapper)
		T4::engine::XAssetEntry*     DB_FindXAssetByName(int type, const char* name);
		// @faithful — sub_48D7D0 (via T4M::DB_FindDefaultAsset_Wrapper)
		void*            DB_FindDefaultAsset(int type);
		// @faithful — sub_48D860 (full C++; T4M::DB_LinkXAssetEntry_Wrapper bridges __usercall)
		T4::engine::XAssetEntry*     DB_LinkXAssetEntry(int type, const char* name);
		// @faithful (simplified) — sub_48DA30 (+ zoneIndex==0 bypass for T4M)
		void*            DB_FindXAssetHeader(int type, const char* name, bool useDefault, int timeoutMs);
		// @faithful — sub_48DFF0
		T4::engine::XAssetEntry*     DB_LinkXAssetEntryOverrideAware(XAssetEntry* newEntry, int copyData);
		// @faithful — loc_48F4C3 (helper of DB_UnloadZoneAssets)
		void             DB_PromoteOverride(XAssetEntry* main, XAssetEntry* override);
		// @faithful — sub_48F340
		void             DB_UnloadZoneAssets(int zoneFileIndex, bool copyDefaults);
		// @faithful — sub_48D720 (only caller: DB_LinkXAssetEntryOverrideAware)
		void             DB_PushCopyInfo(XAssetEntry* entry);
		// @faithful — sub_48E560 (same flat symbol as T4::engine::DB_CheckPendingComplete
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
		return static_cast<unsigned short>(pe - T4::engine::g_assetEntryPool);
	}

	inline XAssetEntry* POOL_ENTRY(unsigned short idx)
	{
		return &T4::engine::g_assetEntryPool[idx].entry;
	}

	// Zone priority: read from g_zoneFileNames[zoneIndex].loadOrder
	// (= dword at D04CB0 + zoneIndex * 0x4C + 0x40).
	inline int ZonePriority(unsigned char zoneIndex)
	{
		return T4::engine::g_zoneFileNames[zoneIndex].loadOrder;
	}

	// Header getName via engine table.
	inline const char* GetName(XAssetEntry* e)
	{
		return T4::engine::DB_XAssetGetNameHandlers[e->asset.type](&e->asset.header);
	}

	// getSize via engine table.
	inline int GetSize(int type)
	{
		return T4::engine::DB_GetXAssetSizeHandler[type]();
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
