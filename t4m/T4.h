#pragma once

typedef enum
{
	DVAR_FLAG_ARCHIVE = 1 << 0,				// 0x0001
	DVAR_FLAG_USERINFO = 1 << 1,			// 0x0002
	DVAR_FLAG_SERVERINFO = 1 << 2,			// 0x0004
	DVAR_FLAG_SYSTEMINFO = 1 << 3,			// 0x0008
	DVAR_FLAG_INIT = 1 << 4,				// 0x0010
	DVAR_FLAG_LATCH = 1 << 5,				// 0x0020
	DVAR_FLAG_ROM = 1 << 6,					// 0x0040
	DVAR_FLAG_CHEAT = 1 << 7,				// 0x0080
	DVAR_FLAG_DEVELOPER = 1 << 8,			// 0x0100
	DVAR_FLAG_SAVED = 1 << 9,				// 0x0200
	DVAR_FLAG_NORESTART = 1 << 10,			// 0x0400
	DVAR_FLAG_CHANGEABLE_RESET = 1 << 12,	// 0x1000
	DVAR_FLAG_EXTERNAL = 1 << 14,			// 0x4000
	DVAR_FLAG_AUTOEXEC = 1 << 15,			// 0x8000
} dvar_flag;

enum dvarType_t : __int8
{
	DVAR_TYPE_BOOL = 0x0,
	DVAR_TYPE_FLOAT = 0x1,
	DVAR_TYPE_FLOAT_2 = 0x2,
	DVAR_TYPE_FLOAT_3 = 0x3,
	DVAR_TYPE_FLOAT_4 = 0x4,
	DVAR_TYPE_INT = 0x5,
	DVAR_TYPE_ENUM = 0x6,
	DVAR_TYPE_STRING = 0x7,
	DVAR_TYPE_COLOR = 0x8,
	DVAR_TYPE_INT64 = 0x9,
	DVAR_TYPE_LINEAR_COLOR_RGB = 0xA,
	DVAR_TYPE_COLOR_XYZ = 0xB,
	DVAR_TYPE_COUNT = 0xC,
};


union dvar_value_t {
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

union dvar_maxmin_t {
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

	inline bool isEnabled() {
		return (this && this->current.boolean);
	}

} dvar_t;

enum scriptInstance_t
{
	SCRIPTINSTANCE_SERVER = 0x0,
	SCRIPTINSTANCE_CLIENT = 0x1,
	SCRIPT_INSTANCE_MAX = 0x2,
};

typedef void(__cdecl * xcommand_t)(void);

#define cmd_functions_ADDR 0x01F416F4

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	char *name;
	char *autocomplete1;
	char *autocomplete2;
	xcommand_t function;
};

struct CmdArgs
{
	int nesting;
	int localClientNum[8];
	int controllerIndex[8];
	//itemDef_s *itemDef[8];
	int argshift[8];
	int argc[8];
	const char **argv[8];
	char textPool[8192];
	const char *argvPool[512];
	int usedTextPool[8];
	int totalUsedArgvPool;
	int totalUsedTextPool;
};

struct XZoneInfo {
	const char* name;
	int allocFlags;
	int freeFlags;
};

struct XZoneLoadedEntry                
{
	unsigned short zoneFileIndex;       
	unsigned short pad;                 
	int            allocFlags;          
	int            memHandle;           
	int            runtimeData[14];     
};

// Zone flags — used both as allocFlags (assigned type) and freeFlags (type to unload)
// NOTE: ZONE_CODE_POST_GFX (0x04) and ZONE_RESERVED_400 (0x400) are "permanent" flags
//       → not tested in Phase 3 (priorityBits[]), never unloaded via freeFlags.
enum XZoneFlags : int {
	ZONE_BASE = 0x001,   // (base/inconnu)
	ZONE_LOCALIZED = 0x002,   // localized_* (ex: localized_ber1)
	ZONE_CODE_POST_GFX = 0x004,   // code_post_gfx  [PERMANENT — jamais déchargé]
	ZONE_LOC_COMMON = 0x008,   // localized_common
	ZONE_UI = 0x010,   // ui / nom de map brut / _patch (usage multiple)
	ZONE_MAP_LOAD = 0x020,   // <map>_load.ff
	ZONE_POST_LOAD = 0x040,   // assets post-load (chargés après la map)
	ZONE_RESERVED_80 = 0x080,   // (réservé)
	ZONE_MAP_PATCH = 0x100,   // <map>_patch.ff
	ZONE_COMMON = 0x200,   // common.ff
	ZONE_RESERVED_400 = 0x400,   // [PERMANENT — jamais déchargé]
	ZONE_MOD = 0x800,   // mod.ff + localized_mod
	// ── Extension T4M ───────────────────────────────────────────────── 
	ZONE_T4M_PATCH_EX = 0x1000,  // T4M — type générique 1
	ZONE_T4M_MAP_LOCA = 0x2000,  // T4M — nouveau type 2
	// Phase 3 handled by the T4M hook (manual DB_RemoveZoneEntry)
	// Phase 1 works natively (AND without bit restriction)
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

struct ConDrawInputGlob
{
	char autoCompleteChoice[64];
	int matchIndex;
	int matchCount;
	const char *inputText;
	int inputTextLen;
	bool hasExactMatch;
	bool mayAutoComplete;
	float x;
	float y;
	float leftX;
	float fontHeight;
};

struct scr_localVar_t
{
	unsigned int name;
	unsigned int sourcePos;
};

struct scr_block_s
{
	int abortLevel;
	int localVarsCreateCount;
	int localVarsPublicCount;
	int localVarsCount;
	char localVarsInitBits[8];
	scr_localVar_t localVars[64];
};

union sval_u
{
	char type;
	unsigned int stringValue;
	unsigned int idValue;
	float floatValue;
	int intValue;
	sval_u *node;
	unsigned int sourcePosValue;
	const char *codePosValue;
	const char *debugString;
	scr_block_s *block;
};

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

struct __declspec(align(4)) VariableStackBuffer
{
	const char *pos;
	unsigned __int16 size;
	unsigned __int16 bufLen;
	unsigned int localId;
	char time;
	char buf[1];
};

union VariableUnion
{
	int intValue;
	float floatValue;
	unsigned int stringValue;
	const float *vectorValue;
	const char *codePosValue;
	unsigned int pointerValue;
	VariableStackBuffer *stackValue;
	unsigned int entityOffset;
};


struct XZoneName
{
	char name[64];
	int flags;
	int fileSize;
	BYTE dir; //enum FF_DIR
	bool loaded;
	BYTE pad[2];
};

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

#include "xasset.h"

// types
typedef void(__cdecl * CommandCB_t)(void);

/*
	Engine Functions
*/

extern "C"
{

	typedef int(__cdecl * AddFunction_t)(scriptInstance_t inst, int func);
	extern AddFunction_t AddFunction;

	extern void Cbuf_AddText(const char* text, int localClientNum);

	extern double CG_CornerDebugPrint(const char* text, float x, float y, float label_width, char* label, float* color);

	extern void FS_AddUserMapDir(const char* dirPath);

	//typedef void(__cdecl * Cmd_AddCommand_t)(const char* name, CommandCB_t callback, cmd_function_s* data, char);
	//extern Cmd_AddCommand_t Cmd_AddCommand;

	typedef void(__cdecl * Com_Error_t)(int type, const char* message, ...);
	extern Com_Error_t Com_Error;

	typedef void(*Com_Printf_t)(int channel, const char* format, ...);
	extern Com_Printf_t Com_Printf;

	typedef void(__cdecl * Com_PrintMessage_t)(int channel, const char *fmt, int error);
	extern Com_PrintMessage_t Com_PrintMessage;

	typedef void(__cdecl* Com_PrintfChannel_t)(int channel, const char* fmt, ...);
	extern Com_PrintfChannel_t Com_PrintfChannel;

	typedef const char *(__cdecl * DB_EnumXAssets_t)(XAssetType type, void(__cdecl *func)(XAssetHeader, void *), void *inData, bool includeOverride);
	extern DB_EnumXAssets_t DB_EnumXAssets;

	typedef void(__cdecl * DB_EnumXAssets_FastFile_t)(XAssetType type, void(__cdecl *func)(XAssetHeader, void *), void *inData);
	extern DB_EnumXAssets_FastFile_t DB_EnumXAssets_FastFile;

	typedef void* (__cdecl * DB_FindXAssetHeader_t)(int type, const char* filename);
	extern DB_FindXAssetHeader_t DB_FindXAssetHeader;

	typedef void(*DB_LoadXAssets_t)(XZoneInfo* data, int count, int sync);
	extern DB_LoadXAssets_t DB_LoadXAssets;

	typedef char* (__cdecl* Sys_BinaryPath_t)();
	extern Sys_BinaryPath_t Sys_BinaryPath;

	// Phase 0
	typedef void(*DB_InitAssetEntryPool_t)();
	extern DB_InitAssetEntryPool_t DB_InitAssetEntryPool;

	// Phase 0.5 — sync moteur
	typedef void(*Sys_SyncDatabase_t)();
	extern Sys_SyncDatabase_t Sys_SyncDatabase;

	typedef void(*DB_PostLoadXZone_t)();
	extern DB_PostLoadXZone_t DB_PostLoadXZone;

	typedef void(*Sys_WakeDatabase_t)();
	extern Sys_WakeDatabase_t Sys_WakeDatabase;

	typedef void(*DB_WaitForPendingLoads_t)();
	extern DB_WaitForPendingLoads_t DB_WaitForPendingLoads;

	typedef void(*DB_CheckPendingComplete_t)();
	extern DB_CheckPendingComplete_t DB_CheckPendingComplete;

	// Phase 1
	typedef void(*DB_PreUnloadResources_t)();
	extern DB_PreUnloadResources_t DB_PreUnloadResources;

	// Replaced by C++ reconstruction (see T4_DB_UnloadZoneAssets in T4.cpp):
	// typedef void(*DB_UnloadZoneAssets_t)(int zoneFileIndex, bool freeMemory);
	// extern DB_UnloadZoneAssets_t DB_UnloadZoneAssets;

	// Shutdown — unloads ALL loaded zones (called via sub_59E530+0x94 at
	// map cleanup). Its internal LIFO loop crashes when T4M custom zones
	// (flag 0x1000) are at the top of the PMem pool but absent from the
	// vanilla sequence. Fully replaced by T4M_DB_UnloadAllZones (pure C++).
	typedef void(*DB_UnloadAllZones_t)();
	extern DB_UnloadAllZones_t DB_UnloadAllZones;

	// sub_48F670 — iterates db_hashTable (0x987088, 32K entries) and releases every
	// asset reference by returning it to the g_assetEntryPool free list.
	// Appelée par DB_UnloadAllZones entre la LIFO 1 (unload assets) et la LIFO 2
	// (final cleanup + PMem_Free).
	typedef void(*DB_CleanupAssetRefs_t)();
	extern DB_CleanupAssetRefs_t DB_CleanupAssetRefs;

	// Phase 2
	typedef void(*DB_PostUnloadCleanup_t)();
	extern DB_PostUnloadCleanup_t DB_PostUnloadCleanup;

	// Phase 3
	typedef void(*DB_RemoveZoneEntry_t)(XZoneLoadedEntry* entry);
	extern DB_RemoveZoneEntry_t DB_RemoveZoneEntry;
	// └─ calls DB_ZoneEntryCleanup (sub_48F600) → DB_FreeXZoneMemory (sub_5F5540)

	typedef void(*DB_ZoneEntryCleanup_t)();
	extern DB_ZoneEntryCleanup_t DB_ZoneEntryCleanup;

	typedef void(*DB_FreeXZoneMemory_t)();
	extern DB_FreeXZoneMemory_t DB_FreeXZoneMemory;

	// Phase 4 + 7 — renderer registration
	typedef void(*R_BeginRegistration_t)();
	extern R_BeginRegistration_t R_BeginRegistration;

	typedef void(*CL_BeginRegistration_t)();
	extern CL_BeginRegistration_t CL_BeginRegistration;

	typedef void(*Hunk_BeginRegistration_t)();
	extern Hunk_BeginRegistration_t Hunk_BeginRegistration;

	typedef void(*DB_SyncAssets_t)();
	extern DB_SyncAssets_t DB_SyncAssets;

	// Phase 5 — renderer clear (sync mode)
	typedef void(*R_ClearScene_t)();
	extern R_ClearScene_t R_ClearScene;

	typedef void(*CL_ClearState_t)();
	extern CL_ClearState_t CL_ClearState;

	typedef void(*Hunk_ClearTempMemory_t)(int handle);
	extern Hunk_ClearTempMemory_t Hunk_ClearTempMemory;

	// Phase 6
	typedef void(*DB_AddZonesToQueue_t)(XZoneInfo* zones, int count);
	extern DB_AddZonesToQueue_t DB_AddZonesToQueue;

	// =================================================================
	// Asset Override System
	//
	// The 7 vanilla functions below (sub_48D410, 48D3B0, 48D2A0, 48D760,
	// 48D7D0, 48D860, 48DFF0) are REPLACED by C++ reconstructions with
	// the same name in T4.cpp. The original typedefs + externs are
	// commented out to preserve a record of the vanilla conventions and
	// original VAs.
	//
	// Forward declarations for the reconstructions are in the section
	// "Asset DB — C++ reconstructions" at the bottom of this file.
	//
	// sub_48CC10 / sub_48D6D0 are NOT reconstructed → engine pointers
	// remain active.
	// =================================================================

	// sub_48D410 — DB_HashAssetName(type, name) via EAX/ECX usercall
	// typedef unsigned int(__fastcall * DB_HashAssetName_t)(int type, const char* name);
	// extern DB_HashAssetName_t DB_HashAssetName;

	// sub_48D3B0 — DB_AllocXAssetEntry(type in EAX, zoneIndex as arg_0)
	// typedef XAssetEntry*(__fastcall * DB_AllocXAssetEntry_t)(int type, int zero_edx, unsigned char zoneIndex);
	// extern DB_AllocXAssetEntry_t DB_AllocXAssetEntry;

	// sub_48D2A0 — DB_AllocAssetHeader(type): allocates a typed header in DB_XAssetPool
	// typedef void*(__cdecl * DB_AllocAssetHeader_t)();
	// extern DB_AllocAssetHeader_t DB_AllocAssetHeader;

	// sub_48D760 — DB_FindXAssetByName(type in EDI, name as arg_0)
	// typedef XAssetEntry*(__fastcall * DB_FindXAssetByName_t)(int type, int zero_edx, const char* name);
	// extern DB_FindXAssetByName_t DB_FindXAssetByName_ENGINE;

	// sub_48D7D0 — DB_FindDefaultAsset(type in EDI)
	// Returns the header (dereferenced) of the default asset for the type, following nextOverride.
	// typedef void*(__fastcall * DB_FindDefaultAsset_t)(int type);
	// extern DB_FindDefaultAsset_t DB_FindDefaultAsset;

	// sub_48D860 — DB_LinkXAssetEntry(type in EAX, name as arg_0)
	// typedef XAssetEntry*(__fastcall * DB_LinkXAssetEntry_t)(int type, int zero_edx, const char* name);
	// extern DB_LinkXAssetEntry_t DB_LinkXAssetEntry;

	// sub_48DFF0 — DB_LinkXAssetEntryOverrideAware(newEntry, copyData)
	//   Core of the allocFlags-based priority system. Returns the "active" entry.
	// typedef XAssetEntry*(__cdecl * DB_LinkXAssetEntryOverrideAware_t)(XAssetEntry* newEntry, int copyData);
	// extern DB_LinkXAssetEntryOverrideAware_t DB_LinkXAssetEntryOverrideAware;

	// sub_48CC10 — InUseHandlerDispatch: per-type dispatch via dword_957564/9575E8
	typedef void(__cdecl * DB_InUseHandlerDispatch_t)();
	extern DB_InUseHandlerDispatch_t DB_InUseHandlerDispatch;

	// sub_48D6D0 — DB_PromoteHelper: internal helper for sub_48DFF0 (promote + memcpy)
	typedef void(__cdecl * DB_PromoteHelper_t)();
	extern DB_PromoteHelper_t DB_PromoteHelper;

	// =================================================================
	// Per-asset-type tables (35 entries × 4 bytes each).
	// Naming convention: DB_XAsset[Verb]Handlers (plural) for function-
	// pointer tables, consistent with DB_XAssetGetNameHandlers.
	// =================================================================
	typedef void(__cdecl * DB_XAssetUnloadHandler_t)(void* header);
	typedef void(__cdecl * DB_XAssetOverridePromoter_t)(void* dstHeader, void* srcHeader, bool isPermZone);
	typedef void(__cdecl * DB_XAssetSetNameHandler_t)(XAssetHeader* header, const char* stringTableEntry);
	typedef void(__cdecl * DB_XAssetFreeHandler_t)(void* pool, void* header);
	typedef void*(__cdecl * DB_XAssetAllocHandler_t)(void* pool);

	extern DB_XAssetUnloadHandler_t*     DB_XAssetUnloadHandlers;      // 0x8DC948  (dword_8DC948)
	extern DB_XAssetOverridePromoter_t*  DB_XAssetOverridePromoters;   // 0x8DC9D8  (dword_8DC9D8)
	extern DB_XAssetSetNameHandler_t*    DB_XAssetSetNameHandlers;     // 0x8DCB88  (funcs_48D966)
	extern DB_XAssetFreeHandler_t*       DB_XAssetFreeHandlers;        // 0x8DC798  (funcs_48E23F) — before DB_XAssetPool
	extern DB_XAssetAllocHandler_t*      DB_XAssetAllocHandlers;       // 0x8DC708  (funcs_48D2B5)
	extern const char**                  DB_XAssetDefaultNames;        // 0x8DC8B8  (off_8DC8B8)
	extern void**                        DB_XAssetPool;                // 0x8DC828  (off_8DC828)

	// Internal string table (sub_68DE50): used by DB_LinkXAssetEntry to
	// register the asset name in the shared string pool.
	// Returns the index (> 0 if found/created, 0 otherwise).
	typedef int(__cdecl * StringTable_Find_t)(int arg_0, const char* name, int arg_2, int len);
	extern StringTable_Find_t       StringTable_Find;         // 0x68DE50

	// String table base — each entry is 12 bytes, name at +4.
	extern char*                    g_stringTableBase;        // dword_3702390

	// =================================================================
	// Override-system globals
	// =================================================================
	extern XAssetEntryPoolEntry**   g_freeAssetEntries;       // 0x957884 — free list head
	extern XAssetEntry**            g_inuseEntry;             // 0x957564
	extern XAssetHeader**           g_inuseHeader;            // 0x9575E8
	extern unsigned int*            g_assetRefCount;          // 0x46DEB28

	//typedef dvar_t* (__fastcall* DvarRegisterFloatFunc)(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description);
	//extern DvarRegisterFloatFunc Dvar_RegisterFloat;

	extern dvar_t* Dvar_RegisterBool(bool value, const char *dvarName, int flags, const char *description = "");

	extern dvar_t* Dvar_RegisterFloat(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description = "");

	extern dvar_t* Dvar_RegisterInt(int default_value, const char* name, int min, int max, int flags, const char* description = "");

	extern dvar_t* Dvar_RegisterEnum(const char** valueList, int defaultIndex, const char* dvarName, int flags, const char* description);

	typedef void(__cdecl * EmitMethod_t)(scriptInstance_t inst, sval_u expr, sval_u func_name, sval_u params, sval_u methodSourcePos, bool bStatement, scr_block_s *block);
	extern EmitMethod_t EmitMethod;

	typedef void* (*Scr_GetFunction_t)(const char **pName, int *type);
	extern Scr_GetFunction_t Scr_GetFunction;

	typedef void* (*CScr_GetFunction_t)(const char **pName, int *type);
	extern CScr_GetFunction_t CScr_GetFunction;

	typedef void* (*CScr_GetMethod_t)(const char **pName, int *type);
	extern CScr_GetMethod_t CScr_GetMethod;

	typedef dvar_t* (__cdecl*Dvar_FindMalleableVarT)(const char* name);
	extern Dvar_FindMalleableVarT Dvar_FindMalleableVar;

	//typedef void* (*Scr_GetMethod_t)(const char **pName, int *type);
	extern int Scr_GetMethod(int *type, const char **pName);

	typedef int(__cdecl * Player_GetMethod_t)(const char **pName);
	extern Player_GetMethod_t Player_GetMethod;

	typedef int(__cdecl * ScriptEnt_GetMethod_t)(const char **pName);
	extern ScriptEnt_GetMethod_t ScriptEnt_GetMethod;

	typedef int(__cdecl * ScriptVehicle_GetMethod_t)(const char **pName);
	extern ScriptVehicle_GetMethod_t ScriptVehicle_GetMethod;

	typedef int(__cdecl * HudElem_GetMethod_t)(const char **pName);
	extern HudElem_GetMethod_t HudElem_GetMethod;

	typedef int(__cdecl * Helicopter_GetMethod_t)(const char **pName);
	extern Helicopter_GetMethod_t Helicopter_GetMethod;

	typedef int(__cdecl * Actor_GetMethod_t)(const char **pName);
	extern Actor_GetMethod_t Actor_GetMethod;

	typedef int(__cdecl * BuiltIn_GetMethod_t)(const char **pName, int *type);
	extern BuiltIn_GetMethod_t BuiltIn_GetMethod;
	
	typedef void(*RemoveRefToValue_t)(scriptInstance_t inst, int type, VariableUnion u);
	extern RemoveRefToValue_t RemoveRefToValue;

	typedef int(__cdecl* DB_GetXAssetSizeHandler_t)();
	extern DB_GetXAssetSizeHandler_t* DB_GetXAssetSizeHandler;
}

struct scr_entref_t
{
	unsigned __int16 entnum;
	unsigned __int16 classnum;
	unsigned __int16 client;
};

typedef void(__cdecl * scr_function_t)(scr_entref_t);

// inline cmd functions
extern DWORD* cmd_id;
extern DWORD* cmd_argc;
extern DWORD** cmd_argv;

inline int Cmd_Argc(void)
{
	return cmd_argc[*cmd_id];
}

inline char *Cmd_Argv(int arg)
{
	if ((unsigned)arg >= cmd_argc[*cmd_id]) {
		return (char*)"";
	}
	return (char*)(cmd_argv[*cmd_id][arg]);
}

int __cdecl Scr_GetNumParam(scriptInstance_t inst);


/*
	Source Functions
*/
void* DB_ReallocXAssetPool(XAssetType type, unsigned int newSize);
char *__cdecl SL_ConvertToString(unsigned int stringValue, scriptInstance_t inst);
void Cmd_AddCommand(const char *cmd_name, xcommand_t function);

char *__cdecl DB_GetXAssetTypeName(int type);

extern XAssetEntryPoolEntry* g_assetEntryPool;
extern XZoneName* g_zoneNames;
extern bool* g_dbInitialized;
extern bool* g_dbHasLoadedZones;  // (0x46DE3B6) — set à 1 quand au moins une zone est loaded, reset à 0 par DB_UnloadAllZones
extern int* g_zoneCount;
extern XZoneLoadedEntry* g_zoneLoaded;
extern ZoneFileEntry* g_zoneFileNames;
extern PMem_Pool* g_pmem_pools;
extern bool* g_dbInUse;
extern int* g_syncValue; 
extern int* g_dbReaderCount;   // (0xBF0084) — incrémenté par readers (sub_48D560)
extern int* g_dbWriterCount;   // (0xBF0088) — incrémenté par writers (sub_48D020)
extern bool* g_assetsDirty;
extern unsigned __int16 * db_hashTable;

typedef const char *(__cdecl *DB_XAssetGetNameHandler)(XAssetHeader *);
extern DB_XAssetGetNameHandler *DB_XAssetGetNameHandlers;
void __cdecl DB_ListAssetPool(XAssetType type, bool count_only);
const char *__cdecl DB_GetXAssetHeaderName(int type, XAssetHeader *header);
const char *__cdecl DB_GetXAssetName(XAsset *asset);

int __cdecl DB_GetXAssetTypeSize(int type);


bool isZombieMode();

bool Com_SessionMode_IsZombiesGame();

bool IsReflectionMode();

namespace Dvars {
	namespace Functions {
		dvar_t* Dvar_FindVar(const char* name);
	}
}

void DoReturn();

#define retptr (uintptr_t)&DoReturn
////////////////////////////////////////////
////////  Category 2b: @faithful helpers  //
////////////////////////////////////////////
// Faithful C++ reconstructions, NOT detoured (WaW engine still uses the
// vanilla code; only T4M calls our versions).

// @faithful — sub_48E3D0
void T4M_FS_BuildZonePath(char* dst, int mode, const char* mapName);
// @faithful — sub_48FC10
bool T4M_FS_ZoneFileExists(const char* mapName, int mode);
// @faithful — sub_5F69E0 : case-insensitive n-char string compare.
// Return : 0 if equal, -1 if s1 < s2, 1 if s1 > s2.
int  T4M_Q_stricmpn(const char* s1, const char* s2, int maxLen);
// @faithful — sub_48D020 (lock) / (release)
void T4M_DB_WriterAcquire();
void T4M_DB_WriterRelease();
// @faithful — sub_48D560 (lock) / (release)
void T4M_DB_ReaderAcquire();
void T4M_DB_ReaderRelease();

////////////////////////////////////////////
////////  Category 2a: @faithful + hook   //
////////////////////////////////////////////
// Faithful C++ reconstructions DETOURED (WaW engine executes our code via
// DetourFunction). Behavior identical to vanilla, clean cdecl convention.
// Detours are installed in PatchT4_Override().

// @faithful — sub_48D410
unsigned int     T4_DB_HashAssetName(int type, const char* name);
// @faithful — sub_48D3B0
XAssetEntry*     T4_DB_AllocXAssetEntry(int type, unsigned char zoneIndex);
// @faithful — sub_48D760 (via T4M_DB_FindXAssetByName_Wrapper)
XAssetEntry*     T4_DB_FindXAssetByName(int type, const char* name);
// @faithful — sub_48D7D0 (via T4M_DB_FindDefaultAsset_Wrapper)
void*            T4_DB_FindDefaultAsset(int type);
// @faithful — sub_48D860 (naked __usercall wrapper)
XAssetEntry*     T4_DB_LinkXAssetEntry(int type, const char* name);
// @faithful — sub_48DFF0
XAssetEntry*     T4_DB_LinkXAssetEntryOverrideAware(XAssetEntry* newEntry, int copyData);
// @faithful — loc_48F4C3 (helper of T4_DB_UnloadZoneAssets)
void             T4_DB_PromoteOverride(XAssetEntry* main, XAssetEntry* override);
// @faithful — sub_48F340
void             T4_DB_UnloadZoneAssets(int zoneFileIndex, bool copyDefaults);

// =================================================================
// T4M inline helpers — shared by DB_* reconstructions and debug tools.
// All @new (T4M inventions, no direct vanilla equivalent — even though
// T4M_GetName / T4M_GetSize wrap vanilla tables, the C++ helpers
// themselves do not exist in the engine).
// =================================================================

// @new — pool ↔ 16-bit index conversion in g_assetEntryPool (stride 0x10).
inline unsigned short T4M_POOL_INDEX(XAssetEntry* e)
{
	XAssetEntryPoolEntry* pe = reinterpret_cast<XAssetEntryPoolEntry*>(e);
	return static_cast<unsigned short>(pe - g_assetEntryPool);
}
inline XAssetEntry* T4M_POOL_ENTRY(unsigned short idx)
{
	return &g_assetEntryPool[idx].entry;
}

// Zone priority: read from g_zoneFileNames[zoneIndex].loadOrder
// (= dword at D04CB0 + zoneIndex * 0x4C + 0x40).
inline int T4M_ZonePriority(unsigned char zoneIndex)
{
	return g_zoneFileNames[zoneIndex].loadOrder;
}

// Header getName via engine table.
inline const char* T4M_GetName(XAssetEntry* e)
{
	return DB_XAssetGetNameHandlers[e->asset.type](&e->asset.header);
}

// getSize via engine table.
inline int T4M_GetSize(int type)
{
	return DB_GetXAssetSizeHandler[type]();
}

////////////////////////////////////////////
////////  T4M-specific utilities         ///
////////////////////////////////////////////

// Debug utilities (PatchT4Override.cpp) — all @new
void T4M_DB_DumpOverrideChain(int type, const char* name);
void T4M_DB_DumpHashBucket(unsigned int bucket);
int T4M_DB_CountActiveEntries();

// __usercall → __cdecl convention wrappers (PatchT4Override.cpp) — @wrapper
// (naked qualifier only on the definition, not on the declaration)
void T4M_DB_FindDefaultAsset_Wrapper();   // → T4_DB_FindDefaultAsset
void T4M_DB_FindXAssetByName_Wrapper();   // → T4_DB_FindXAssetByName

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

extern dvar_t** fs_game;
extern dvar_t** fs_localAppData;
extern dvar_t** fs_basepath;
extern dvar_t** dedicated;
// add here the language dvar

extern unsigned long& db_streamEnabled;
extern unsigned long& db_streamReadBlocksTotal;

extern unsigned long& db_streamReadBlocksDone;
extern unsigned long& db_streamDecompBytesTotal;
extern unsigned long& db_streamDecompBytesDone;
extern const char** language_system; // Dereference to use: *language_system gives "english"/"french"/etc.
                                     // Filled by sub_5FE000 AFTER the DLL loads (localization.txt read).


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