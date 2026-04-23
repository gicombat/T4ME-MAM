// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: World at War related functions
//
// Initial author: UNKNOWN
// Started: 2015-07-08
// ==========================================================

#include "StdInc.h"
#include "T4.h"
// timeGetTime (used by T4_DB_FindXAssetHeader, matches vanilla sub_48DA30).
#include "Timeapi.h" 
#pragma comment(lib, "winmm.lib")

// console commands
DWORD* cmd_id = (DWORD*)0x01F41670;
DWORD* cmd_argc = (DWORD*)0x01F416B4;
DWORD** cmd_argv = (DWORD**)0x01F416D4;


extern "C"
{
	AddFunction_t AddFunction = (AddFunction_t)0x00682040;

	Com_Error_t Com_Error = (Com_Error_t)0x006C1CE0;
	Com_Printf_t Com_Printf = (Com_Printf_t)0x0059A2C0;
	Com_PrintMessage_t Com_PrintMessage = (Com_PrintMessage_t)0x59A170;
	Com_PrintfChannel_t Com_PrintfChannel = (Com_PrintfChannel_t)0x59A2C0;

	CScr_GetFunction_t CScr_GetFunction = (CScr_GetFunction_t)0x0066EA30;
	CScr_GetMethod_t CScr_GetMethod = (CScr_GetMethod_t)0x00671110;

	Dvar_FindMalleableVarT Dvar_FindMalleableVar = (Dvar_FindMalleableVarT)0x5EDE30;

	DB_EnumXAssets_t DB_EnumXAssets = (DB_EnumXAssets_t)0x006DA180;
	DB_EnumXAssets_FastFile_t DB_EnumXAssets_FastFile = (DB_EnumXAssets_FastFile_t)0x0048DEA0;
	DB_FindXAssetHeader_t DB_FindXAssetHeader = (DB_FindXAssetHeader_t)0x48DA30;
	DB_LoadXAssets_t DB_LoadXAssets = (DB_LoadXAssets_t)0x48E7B0;

	DB_InitAssetEntryPool_t DB_InitAssetEntryPool = (DB_InitAssetEntryPool_t)0x48D340;
	Sys_SyncDatabase_t      Sys_SyncDatabase = (Sys_SyncDatabase_t)0x6F6CE0;
	DB_PostLoadXZone_t      DB_PostLoadXZone = (DB_PostLoadXZone_t)0x5A3320;
	Sys_WakeDatabase_t      Sys_WakeDatabase = (Sys_WakeDatabase_t)0x6F6D60;
	DB_WaitForPendingLoads_t DB_WaitForPendingLoads = (DB_WaitForPendingLoads_t)0x5FDBF0;
	DB_CheckPendingComplete_t DB_CheckPendingComplete = (DB_CheckPendingComplete_t)0x48E560;
	DB_PreUnloadResources_t DB_PreUnloadResources = (DB_PreUnloadResources_t)0x48F8D0;
	// Replaced by C++ reconstruction (see T4_DB_UnloadZoneAssets in T4.cpp):
	// T4_DB_UnloadZoneAssets_t   T4_DB_UnloadZoneAssets = (T4_DB_UnloadZoneAssets_t)0x48F340;
	DB_UnloadAllZones_t     DB_UnloadAllZones = (DB_UnloadAllZones_t)0x48F720;
	DB_CleanupAssetRefs_t   DB_CleanupAssetRefs = (DB_CleanupAssetRefs_t)0x48F670;
	DB_PostUnloadCleanup_t  DB_PostUnloadCleanup = (DB_PostUnloadCleanup_t)0x48F9B0;
	DB_RemoveZoneEntry_t    DB_RemoveZoneEntry = (DB_RemoveZoneEntry_t)0x48F7D0;
	DB_ZoneEntryCleanup_t   DB_ZoneEntryCleanup = (DB_ZoneEntryCleanup_t)0x48F600;
	DB_FreeXZoneMemory_t    DB_FreeXZoneMemory = (DB_FreeXZoneMemory_t)0x5F5540;
	R_BeginRegistration_t   R_BeginRegistration = (R_BeginRegistration_t)0x6B1500;
	CL_BeginRegistration_t  CL_BeginRegistration = (CL_BeginRegistration_t)0x59EB00;
	Hunk_BeginRegistration_t Hunk_BeginRegistration = (Hunk_BeginRegistration_t)0x4B3090;
	DB_SyncAssets_t         DB_SyncAssets = (DB_SyncAssets_t)0x41D3A0;
	R_ClearScene_t          R_ClearScene = (R_ClearScene_t)0x6B1440;
	CL_ClearState_t         CL_ClearState = (CL_ClearState_t)0x59EA90;
	Hunk_ClearTempMemory_t  Hunk_ClearTempMemory = (Hunk_ClearTempMemory_t)0x4B2F80;
	DB_AddZonesToQueue_t    DB_AddZonesToQueue = (DB_AddZonesToQueue_t)0x48E4C0;
	Sys_BinaryPath_t Sys_BinaryPath = (Sys_BinaryPath_t)0x5F7F20;

	// ─── Vanilla engine pointers — replaced by C++ reconstructions ───
	// Commented out to document the original VAs. The matching C++
	// reconstructions live further down in this file (below the pool
	// tables). If we ever need to retest the vanilla behavior, uncomment
	// these and rename the reconstructions to avoid linkage collisions.
	//
	// T4_DB_HashAssetName_t               T4_DB_HashAssetName                = (T4_DB_HashAssetName_t)0x48D410;
	// T4_DB_AllocXAssetEntry_t            T4_DB_AllocXAssetEntry             = (T4_DB_AllocXAssetEntry_t)0x48D3B0;
	// T4M_DB_AllocAssetHeader_t            T4M_DB_AllocAssetHeader             = (T4M_DB_AllocAssetHeader_t)0x48D2A0;
	// T4_DB_FindXAssetByName_t            T4_DB_FindXAssetByName_ENGINE      = (T4_DB_FindXAssetByName_t)0x48D760;
	// T4_DB_FindDefaultAsset_t            T4_DB_FindDefaultAsset             = (T4_DB_FindDefaultAsset_t)0x48D7D0;
	// T4_DB_LinkXAssetEntry_t             T4_DB_LinkXAssetEntry              = (T4_DB_LinkXAssetEntry_t)0x48D860;
	// T4_DB_LinkXAssetEntryOverrideAware_t T4_DB_LinkXAssetEntryOverrideAware = (T4_DB_LinkXAssetEntryOverrideAware_t)0x48DFF0;
	DB_InUseHandlerDispatch_t        DB_InUseHandlerDispatch         = (DB_InUseHandlerDispatch_t)0x48CC10;
	DB_PromoteHelper_t               DB_PromoteHelper                = (DB_PromoteHelper_t)0x48D6D0;

	// Per-type tables — VAs verified in CoDWaW LanFixed.exe.asm (.data section).
	// Contiguous sequence with stride 0x90:
	//   0x8DC708  funcs_48D2B5  = DB_XAssetAllocHandlers
	//   0x8DC798  funcs_48E23F  = DB_XAssetFreeHandlers
	//   0x8DC828  off_8DC828    = DB_XAssetPool
	//   0x8DC8B8  off_8DC8B8    = DB_XAssetDefaultNames
	//   0x8DC948  dword_8DC948  = DB_XAssetUnloadHandlers
	//   0x8DC9D8  dword_8DC9D8  = DB_XAssetOverridePromoters
	//   0x8DCA68  off_8DCA68    = (type names, exposed via DB_GetXAssetTypeName)
	//   0x8DCAF8  funcs_48D282  = DB_XAssetGetNameHandlers
	//   0x8DCB88  funcs_48D966  = DB_XAssetSetNameHandlers
	//   0x8DCC18  funcs_48D6FD  = DB_GetXAssetSizeHandler
	DB_XAssetUnloadHandler_t*     DB_XAssetUnloadHandlers     = (DB_XAssetUnloadHandler_t*)     0x008DC948;
	DB_XAssetOverridePromoter_t*  DB_XAssetOverridePromoters  = (DB_XAssetOverridePromoter_t*)  0x008DC9D8;
	DB_XAssetSetNameHandler_t*    DB_XAssetSetNameHandlers    = (DB_XAssetSetNameHandler_t*)    0x008DCB88;
	DB_XAssetFreeHandler_t*       DB_XAssetFreeHandlers       = (DB_XAssetFreeHandler_t*)       0x008DC798;
	DB_XAssetAllocHandler_t*      DB_XAssetAllocHandlers      = (DB_XAssetAllocHandler_t*)      0x008DC708;
	const char**                  DB_XAssetDefaultNames       = (const char**)                  0x008DC8B8;
	void**                        DB_XAssetPool               = (void**)                        0x008DC828;

	// String table used by T4_DB_LinkXAssetEntry (sub_48D860)
	StringTable_Find_t       StringTable_Find        = (StringTable_Find_t)       0x0068DE50;
	char**                   g_stringTableBase       = (char**)                   0x03702390;

	// Override-system globals
	XAssetEntryPoolEntry**   g_freeAssetEntries     = (XAssetEntryPoolEntry**)   0x00957884;
	XAssetEntry**            g_inuseEntry           = (XAssetEntry**)            0x00957564;
	XAssetHeader**           g_inuseHeader          = (XAssetHeader**)           0x009575E8;
	unsigned int*            g_assetRefCount        = (unsigned int*)            0x046DEB28;

	EmitMethod_t EmitMethod = (EmitMethod_t)0x00682F40;

	RemoveRefToValue_t RemoveRefToValue = (RemoveRefToValue_t)0x00690130;

	DB_GetXAssetSizeHandler_t* DB_GetXAssetSizeHandler = (DB_GetXAssetSizeHandler_t*)0x008DCC18;

	Scr_GetFunction_t Scr_GetFunction = (Scr_GetFunction_t)0x0052F0B0;
	//Scr_GetMethod_t Scr_GetMethod = (Scr_GetMethod_t)0x00530630;

	// GetMethod stuff
	Player_GetMethod_t Player_GetMethod = (Player_GetMethod_t)0x004DEEA0;
	ScriptEnt_GetMethod_t ScriptEnt_GetMethod = (ScriptEnt_GetMethod_t)0x00567680;
	ScriptVehicle_GetMethod_t ScriptVehicle_GetMethod = (ScriptVehicle_GetMethod_t)0x004F3920;
	HudElem_GetMethod_t HudElem_GetMethod = (HudElem_GetMethod_t)0x00532BA0;
	Helicopter_GetMethod_t Helicopter_GetMethod = (Helicopter_GetMethod_t)0x00541F80;
	Actor_GetMethod_t Actor_GetMethod = (Actor_GetMethod_t)0x004FC320;
	BuiltIn_GetMethod_t BuiltIn_GetMethod = (BuiltIn_GetMethod_t)0x005305B0;

	XAssetEntryPoolEntry* g_assetEntryPool = (XAssetEntryPoolEntry *)0x00A51C50;
	XZoneName* g_zoneNames = (XZoneName *)0x00D04CB0;
	unsigned __int16 * db_hashTable = (unsigned __int16 *)0x00987088;

	bool* g_dbInitialized = (bool*)0x46DE3B7;  // byte_46DE3B7  — one-shot init guard
	bool* g_dbHasLoadedZones = (bool*)0x46DE3B6; // byte_46DE3B6 — "at least one zone loaded" flag
	int* g_zoneCount = (int*)0x986B3C;   // dword_986B3C  — active zone count (max 32)
	XZoneLoadedEntry* g_zoneLoaded = (XZoneLoadedEntry*)0xB54C00;   // active zone table (stride 0x44)
	bool* g_dbInUse = (bool*)0x9BD455;   // byte_9BD455   — "DB busy unloading/syncing" flag
	int* g_syncValue = (int*)0xA51C4C;   // dword_A51C4C  — sync value passed to DB_AddZonesToQueue
	int* g_dbReaderCount = (int*)0xBF0084;  // incremented by readers (sub_48D560)
	int* g_dbWriterCount = (int*)0xBF0088;  // incremented by writers (sub_48D020)
	// DB worker sync globals (used by T4_DB_FindXAssetHeader — faithful to sub_48DA30)
	HANDLE*       g_dbWorkerEvent       = (HANDLE*) 0x01FF5250;   // event signaled when DB worker finishes a zone
	DWORD*        g_dbWorkerThreadId    = (DWORD*)  0x01FF51F8;   // DB worker thread id (cached)
	DWORD*        g_waitStartTime       = (DWORD*)  0x022BEC34;   // timeGetTime() at first wait
	int*          g_waitTimerStarted    = (int*)    0x04DE7054;   // 1 after g_waitStartTime is initialized
	HANDLE*       g_dbSecondaryEvent    = (HANDLE*) 0x01FF51C4;   // secondary wait event
	DWORD*        g_dbAltThreadId       = (DWORD*)  0x01FF51C8;   // "ThreadId"
	DWORD*        g_dbSecondaryThreadId = (DWORD*)  0x01FF51CC;   // secondary thread id
	HANDLE*       g_dbPauseEventHandle  = (HANDLE*) 0x01FF5068;   // "hHandle" — used by SetEvent/ResetEvent
	int*          g_dbWorkerPausedFlag  = (int*)    0x01FF5244;   // 1 when DB worker is paused by this path
	uint8_t*      g_dbFlag951A02        = (uint8_t*)0x00951A02;   // flag in DB busy state check
	uint8_t*      g_dbFlag3BED85D       = (uint8_t*)0x03BED85D;   // flag in thread-id alt-check
	DWORD*        g_dbPtr99724C         = (DWORD*)  0x0099724C;   // pointer checked in alt-thread branch
	DWORD*        g_dbAltDefaultDvarPtr = (DWORD*)  0x01F55288;   // dvar* used by useDefault alt path
	bool* g_assetsDirty = (bool*)0x45C22C5;  // byte_45C22C5  — "assets need sync" flag

	ZoneFileEntry* g_zoneFileNames = (ZoneFileEntry*)0xD04CB0;
	PMem_Pool* g_pmem_pools = (PMem_Pool*)0x224F9D8;

	DB_XAssetGetNameHandler * DB_XAssetGetNameHandlers = (DB_XAssetGetNameHandler *)0x008DCAF8;

	int* g_currentZoneIndex = (int*)0x00987084;  // dword_987084 — zone index currently being loaded
}

dvar_t emptydvar{};

dvar_t* Dvar_FindVars(const char* name) {
	auto dvar = Dvar_FindMalleableVar(name);
	if (dvar)
		return dvar;


		return &emptydvar;

}

namespace Dvars {
	namespace Functions {
			dvar_t* Dvar_FindVar(const char* name) {
			return Dvar_FindVars(name);
		}
	}
}

// fucking __usercall
// @wrapper — asm usercall to sub_5EEE20
dvar_t* Dvar_RegisterBool(bool value, const char *dvarName, int flags, const char *description)
{
	DWORD func = 0x5EEE20;
	dvar_t* ret;
	__asm
	{
		push description
		push flags
		mov al, value
		mov edi, dvarName
		call func
		add esp, 8
		mov ret, eax
	}
	return ret;
}

//typedef dvar_t* (__fastcall* DvarRegisterFloatFunc)(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description);
//DvarRegisterFloatFunc Dvar_RegisterFloat = (DvarRegisterFloatFunc)0x5EEF10;
// fucking __usercall -- Clippy95 ~10 years later.
// @wrapper — asm usercall to sub_5EEF10
dvar_t* Dvar_RegisterFloat(const char* dvarName, float defaultValue, float min, float max, int flags, const char* description)
{
	DWORD func = 0x5EEF10;
	dvar_t* ret;

	__asm
	{
		movss xmm0, defaultValue
		push description
		push flags
		sub esp, 8
		fld max
		fstp dword ptr[esp + 4]
		fld min
		fstp dword ptr[esp]
		mov edi, dvarName
		call func
		add esp, 0x10
		mov ret, eax
	}

	return ret;
}

// @wrapper — asm usercall to sub_5EF150
dvar_t* Dvar_RegisterEnum(const char** valueList, int defaultIndex, const char* dvarName, int flags, const char* description)
{
	DWORD func = 0x5EF150;
	dvar_t* ret;

	__asm
	{
		push description;
		push flags;
		push dvarName;
		mov eax, defaultIndex;
		mov ecx, valueList;
		call func;
		add esp, 0xC;
		mov ret, eax;
	}

	return ret;
}

// @wrapper — asm usercall to sub_6E8DA0
int R_TextWidth(const char* text, int maxChars, game::Font_s* font)
{
	int result;
	static uintptr_t textwidth_addr = 0x6E8DA0;
	__asm
	{
		push font;
		push maxChars;
		mov eax, text;
		call textwidth_addr;
		add esp, 0x8;
		mov result, eax;
	}

	return result;
}

inline float R_NormalizedTextScale(game::Font_s* font, float scale) 
{
	return scale * 48.0 / (double)font->pixelHeight;
}

int __cdecl UI_TextWidth(const char* text, int maxChars, game::Font_s* font, float scale)
{
	float v4; // xmm0_4
	float actualScale; // [esp+10h] [ebp-4h]

	actualScale = R_NormalizedTextScale(font, scale);
	v4 = (float)((float)R_TextWidth(text, maxChars, font) * actualScale) + 0.5;
	return (int)(float)floor(v4);
}

// void* returns are always in eax
uintptr_t Dvar_RegisterInt_addr = 0x5EEEA0;

void __declspec(naked) DoReturn() 
{
	__asm retn
}

uintptr_t __declspec(naked) Dvar_RegisterInt_asm(int default_value, const char* name, int min, int max, int flags, const char* description) {
	
	__asm {
		push ebp
		mov ebp, esp
		sub esp, __LOCAL_SIZE

		push eax
		push edi

		mov eax, default_value
		mov edi, name

		push description
		push flags
		push max
		push min


		call Dvar_RegisterInt_addr


		pop edi
		pop eax

		mov esp, ebp
		pop ebp
		ret
	}
}

// @wrapper — asm usercall vers sub_5EEEA0
dvar_t* Dvar_RegisterInt(int default_value, const char* name, int mina, int max, int flags, const char* description) 
{
	//return (dvar_t*)Dvar_RegisterInt_asm(default_value, name, min, max, flags, description);
	__asm pushad

	DWORD func = 0x5EEEA0;
	dvar_t* rete;
	__asm
	{
		push description
		push flags
		push max
		push mina

		mov eax, default_value
		mov edi, name

		call func
		add esp, 16
		mov rete, eax
	}
	__asm popad
	return rete;
	


}

//int __usercall CBuf_AddText@<eax>(int a1@<eax>, int a2@<ecx>), a1 = text, a2 = localClientNum
// @wrapper — asm usercall vers sub_594200
void Cbuf_AddText(const char* text, int localClientNum)
{
	DWORD func = 0x00594200;

	__asm
	{
		mov eax, text
		mov ecx, localClientNum
		call func
	}
}

// @wrapper — asm usercall vers sub_439160
double CG_CornerDebugPrint(const char* text, float x, float y,float label_width, char* label, float* color)
{
	DWORD func = 0x00439160;
	float result;
	__asm
	{
		mov eax, text
		
		push color
		push label
		push label_width
		push y
		push x

		call func
		add esp, 20
		movss result, xmm0
	}
	return result;
}
// Vanilla function address
uintptr_t pFS_AddUserMapDir = 0x5DD8A0;

// Naked wrapper — exposes a plain cdecl signature to T4M callers
// @wrapper — naked asm to sub_5DD8A0
__declspec(naked) void FS_AddUserMapDir(const char* dirPath)
{
	__asm
	{
		push    edi
		mov     edi, [esp + 8]
		call    pFS_AddUserMapDir
		pop     edi
		ret
	}
}

/*
============
Cmd_AddCommand
============
*/
cmd_function_s** cmd_functions = ((cmd_function_s**)(cmd_functions_ADDR));
void Cmd_AddCommand(const char *cmd_name, xcommand_t function) {
	cmd_function_s *cmd;

	// fail if the command already exists
	for (cmd = *cmd_functions; cmd; cmd = cmd->next) {
		if (!strcmp(cmd_name, cmd->name)) {
			// allow completion-only commands to be silently doubled
			if (function != NULL) {
				Com_Printf(0, "Cmd_AddCommand: %s already defined\n", cmd_name);
			}
			return;
		}
	}

	// use a small malloc to avoid zone fragmentation
	cmd = (cmd_function_s *)malloc(sizeof(cmd_function_s) + strlen(cmd_name) + 1);
	strcpy((char*)(cmd + 1), cmd_name);
	cmd->name = (char*)(cmd + 1);
	cmd->autocomplete1 = NULL;
	cmd->autocomplete2 = NULL;
	cmd->function = function;
	cmd->next = *cmd_functions;
	*cmd_functions = cmd;
}


unsigned int* g_poolSize = (unsigned int*)0x8DC5D0;

dvar_t** fs_localAppData = (dvar_t**)0x2122AF0;
dvar_t** fs_game = (dvar_t**)0x2122B00;
dvar_t** fs_basepath = (dvar_t**)0x02123C14;
dvar_t** dedicated = (dvar_t**)0x0212B2F4;
dvar_t** loc_language = (dvar_t**)0x208B2EC;
DWORD& db_streamEnabled = *(DWORD*)0x957404;
DWORD& db_streamReadBlocksTotal = *(DWORD*)0x957400;
DWORD& db_streamReadBlocksDone = *(DWORD*)0x957408;
DWORD& db_streamDecompBytesTotal = *(DWORD*)0x95740C;
DWORD& db_streamDecompBytesDone = *(DWORD*)0x9571A4;
// Pointer to the slot containing the "english"/"french"/etc. string pointer.
// Filled by sub_5FE000 (localization.txt read) AFTER the DLL loads.
// A direct snapshot at init reads NULL (BSS) because localization.txt is
// not yet loaded when the T4M DLL initializes. Always dereference at use.
const char** language_system = (const char**)0x22BA528;

void* T4M_DB_ReallocXAssetPool(XAssetType type, unsigned int size)
{
	int elSize = T4M_DB_GetXAssetTypeSize(type);
	void* poolEntry = malloc(size * elSize);
	DB_XAssetPool[type] = poolEntry;
	g_poolSize[type] = size;
	return poolEntry;
}

void __cdecl T4M_DB_ListAssetPool(XAssetType type, bool count_only)
{
	char *v1;
	XZoneName *v2;
	const char *v3;
	XAssetEntry *overrideAssetEntry;
	XZoneName *v5;
	const char *v6;
	char *v7;
	unsigned int nextAssetEntryIndex;
	unsigned int hash;
	unsigned int assetPoolSize;
	unsigned int assetEntryIndex;
	unsigned int assetPoolCount;
	XAssetEntry *assetEntry;
	unsigned int overrideAssetEntryIndex;
	assetPoolCount = 0;
	assetPoolSize = 0;
	v1 = T4M_DB_GetXAssetTypeName(type);
	
	if (!count_only)
		Com_Printf(0, "[T4M] Listing assets in %s pool.\n", v1);

	for (hash = 0; hash < 0x8000; ++hash)
	{
		for (assetEntryIndex = db_hashTable[hash]; assetEntryIndex; assetEntryIndex = nextAssetEntryIndex)
		{
			assetEntry = &g_assetEntryPool[assetEntryIndex].entry;
			nextAssetEntryIndex = g_assetEntryPool[assetEntryIndex].entry.nextHash;
			if (g_assetEntryPool[assetEntryIndex].entry.asset.type == type)
			{
				++assetPoolCount;
				v2 = &g_zoneNames[assetEntry->zoneIndex];
				v3 = T4M_DB_GetXAssetName(&assetEntry->asset);
				if (!count_only)
					Com_Printf(0, "[T4M] Asset: %s FF: %s\n", v3, v2->name);
				assetPoolSize += T4M_DB_GetXAssetTypeSize(assetEntry->asset.type);
				for (overrideAssetEntryIndex = assetEntry->nextOverride;
					overrideAssetEntryIndex;
					overrideAssetEntryIndex = overrideAssetEntry->nextOverride)
				{
					overrideAssetEntry = &g_assetEntryPool[overrideAssetEntryIndex].entry;
					++assetPoolCount;
					v5 = &g_zoneNames[g_assetEntryPool[overrideAssetEntryIndex].entry.zoneIndex];
					v6 = T4M_DB_GetXAssetName(&g_assetEntryPool[overrideAssetEntryIndex].entry.asset);
					if (!count_only)
						Com_Printf(0, "[T4M] Asset: %s FF: %s | overriden\n", v6, v5);
					assetPoolSize += T4M_DB_GetXAssetTypeSize(overrideAssetEntry->asset.type);
				}
			}
		}
	}
	v7 = T4M_DB_GetXAssetTypeName(type);
	Com_Printf(16, "[T4M] Total of %d assets in %s pool, max %d, size %d\n", assetPoolCount, v7, g_poolSize[type], assetPoolSize);
}

char *__cdecl T4M_DB_GetXAssetTypeName(int type)
{
	char** g_assetNames = (char **)0x008DCA68;
	return g_assetNames[type];
}

const char *__cdecl T4M_DB_GetXAssetName(XAsset *asset)
{
	return T4M_DB_GetXAssetHeaderName(asset->type, &asset->header);
}

const char *__cdecl T4M_DB_GetXAssetHeaderName(int type, XAssetHeader *header)
{
	int v2;
	const char *name;
	name = DB_XAssetGetNameHandlers[type](header);
	return name;
}

int __cdecl T4M_DB_GetXAssetTypeSize(int type)
{
	return DB_GetXAssetSizeHandler[type]();
}

bool isZombieMode() {
	return (*(dvar_t**)0x030520E4)->isEnabled();
}

bool Com_SessionMode_IsZombiesGame() {
	return isZombieMode();
}

#include "shellapi.h"

// from linkermod
bool IsReflectionMode()
{
	static bool hasChecked = false;
	static bool isReflectionMode = false;

	if (!hasChecked)
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

		int flags = 0;
		for (int i = 0; i < argc - 1; i++)
		{
			if (wcscmp(argv[i], L"+devmap") == 0 && flags ^ 1)
			{
				flags |= 1;
			}
			else if (wcscmp(argv[i], L"r_reflectionProbeGenerate") == 0 && wcscmp(argv[i + 1], L"0") != 0 && flags ^ 2)
			{
				isReflectionMode = true;
				flags |= 2;
			}
			else if (flags == 3)
			{
				return isReflectionMode;
			}
		}
	}
	return isReflectionMode;
}

// @faithful — sub_48E3D0
void T4M_FS_BuildZonePath(char* dst, int mode, const char* mapName)
{
	const char* binaryPath = Sys_BinaryPath();                       // sub_5F7F20

	switch (mode)
	{
	case 0: // zone stock du jeu
		_snprintf(dst, 256, "%s\\zone\\%s\\%s.ff", binaryPath, *language_system, mapName);
		dst[255] = '\0';
		break;

	case 1: // zone dans le dossier fs_game (mod)
		_snprintf(dst, 256, "%s\\%s\\%s.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string, mapName);
		dst[255] = '\0';
		break;

	case 2: // zone usermap
	{
		char parent[64];
		const char* loadSuffix = strstr(mapName, "_load");
		if (loadSuffix)
		{
			size_t n = (size_t)(loadSuffix - mapName);     // caractères avant "_load"
			strncpy(parent, mapName, n);
			parent[n] = '\0';
		}
		else
		{
			strncpy(parent, mapName, 63);
			parent[63] = '\0';
		}
		_snprintf(dst, 256, "%s\\%s\\%s\\%s.ff", (*fs_localAppData)->current.string, "usermaps", parent, mapName);
		dst[255] = '\0';
		break;
	}

	default:
		// mode ≥ 3 : ne touche pas dst (comportement vanilla)
		break;
	}
}

// @faithful — sub_48FC10
bool T4M_FS_ZoneFileExists(const char* mapName, int mode)
{
	char localPath[256];
	T4M_FS_BuildZonePath(localPath, mode, mapName); // BuildZonePath

	HANDLE h = CreateFileA(localPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		0x60000000,
		nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	CloseHandle(h);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pure C++ reconstructions of vanilla functions with non-standard conventions.
//
// These replace the register-based vanilla calls with C++ versions that
// compile using MSVC's standard conventions. Avoids silent bugs like the
// sub_48D020 freeze (see sleep_infini_T4M_DB_Load.md).
// ═══════════════════════════════════════════════════════════════════════════════

// @faithful — sub_5F69E0
// T4M_Q_stricmpn: case-insensitive n-char string compare.
int T4M_Q_stricmpn(const char* s1, const char* s2, int maxLen)
{
	if (maxLen <= 0) return 0;

	do
	{
		int a = (unsigned char)*s1++;
		int b = (unsigned char)*s2++;

		if (a != b)
		{
			// ASCII tolower (A-Z → a-z)
			if (a >= 'A' && a <= 'Z') a += 32;
			if (b >= 'A' && b <= 'Z') b += 32;
			if (a != b)
				return (a < b) ? -1 : 1;
		}

		if (a == 0) return 0;  // both strings terminated
	} while (--maxLen > 0);

	return 0;
}

// @faithful — sub_48D020
// T4M_DB_WriterAcquire: waits until readerCount == 0, then increments
// writerCount to 1 (exclusive lock).
void T4M_DB_WriterAcquire()
{
	for (;;)
	{
		// 1. Wait until no reader is active
		if (*g_dbReaderCount != 0)
		{
			Sleep(0);
			continue;
		}

		// 2. Try to atomically increment the writer count
		LONG newCount = InterlockedIncrement((LONG*)g_dbWriterCount);

		// 3. If we are the first writer and no reader raced in, lock acquired.
		if (newCount == 1 && *g_dbReaderCount == 0)
			return;

		// 4. Rollback and retry
		InterlockedDecrement((LONG*)g_dbWriterCount);
		Sleep(0);
	}
}

// @faithful — sub_48D020 (release half)
void T4M_DB_WriterRelease()
{
	InterlockedDecrement((LONG*)g_dbWriterCount);
}

// @faithful — sub_48D560 (reader lock acquire)
void T4M_DB_ReaderAcquire()
{
	InterlockedIncrement((LONG*)g_dbReaderCount);
	while (*g_dbWriterCount != 0)
		Sleep(0);
}

// @faithful — sub_48D560 (reader lock release)
void T4M_DB_ReaderRelease()
{
	InterlockedDecrement((LONG*)g_dbReaderCount);
}

// =====================================================================
// T4M_DB_EnumAssetPool — full C++ reconstruction of sub_48D560
// @faithful — sub_48D560
//
//   Acquires the reader lock, walks every one of the 0x8000 hash
//   buckets, and calls callback(entry->asset.header.data, pType) for
//   each entry whose type matches.  When followOverrides != 0, the
//   nextOverride chain of each matching entry is also walked.
//   Releases the reader lock on exit.
//
//   Signature (__cdecl, derived from ASM):
//     sub_48D560(int type,
//                void (__cdecl* callback)(void*, int*),
//                int* pType,
//                int  followOverrides)   ; arg_C — byte in vanilla
// =====================================================================
__declspec(noinline)
void T4M_DB_EnumAssetPool(int type,
                           void (__cdecl* callback)(void*, int*),
                           int* pType,
                           int  followOverrides)
{
	T4M_DB_ReaderAcquire();

	for (unsigned int b = 0; b < 0x8000; ++b)
	{
		unsigned short idx = db_hashTable[b];
		while (idx != 0)
		{
			XAssetEntry* e = T4M_POOL_ENTRY(idx);
			if (e->asset.type == type)
			{
				callback(e->asset.header.data, pType);

				if (followOverrides != 0)
				{
					unsigned short ovIdx = e->nextOverride;
					while (ovIdx != 0)
					{
						XAssetEntry* ov = T4M_POOL_ENTRY(ovIdx);
						callback(ov->asset.header.data, pType);
						ovIdx = ov->nextOverride;
					}
				}
			}
			idx = e->nextHash;
		}
	}

	T4M_DB_ReaderRelease();
}

#include <malloc.h>   // _alloca

// =====================================================================
// T4_DB_HashAssetName — sub_48D410
//   15-bit hash (bucket 0..0x7FFF). tolower + '\\' → '/'.
//   Seed: type. Multiplier: 31 (shl 5; sub seed).
//
// TODO(@faithful rewrite): this is currently a naked wrapper that delegates
// to vanilla sub_48D410 at 0x48D410 as a safety measure. A previous C++
// reimplementation diverged for some material names and caused
// FindXAssetByName to miss (→ LinkXAssetEntry fallback → d3d9 freeze during
// R_Init). Reconstruct in C++ using **CRT `tolower` on movsx-signed bytes**
// (`(int)(signed char)*name`), matching vanilla's sub_7AA75E call exactly,
// then restore the naked wrapper only if byte-for-byte equivalence fails.
// =====================================================================
__declspec(naked)
unsigned int T4_DB_HashAssetName(int /*type*/, const char* /*name*/)
{
	__asm
	{
		mov     eax, [esp+4]        ; eax = type  (__usercall arg)
		mov     ecx, [esp+8]        ; ecx = name  (__usercall arg)
		mov     edx, 0x0048D410
		call    edx                 ; vanilla sub_48D410 — eax = bucket
		ret                          ; cdecl: caller cleans args
	}
}

// =====================================================================
// Pool-dump helpers — sub_48D2A0 error path
//
// sub_48D270: per-asset print callback.
//   Vanilla: `lea ecx, [esp+arg_0]` → getName takes the address of the
//   first stack arg as an XAssetHeader* (XAssetHeader = { void* data }).
// =====================================================================
static void __cdecl DB_PoolDumpAsset_cb(void* assetData, int* pType)
{
	const char* name = DB_XAssetGetNameHandlers[*pType]((XAssetHeader*)&assetData);
	Com_Printf(0, "%s\n", name);
}

// sub_59A380 — non-fatal error channel print (^1Error: prefix, channel 1).
typedef void (__cdecl* DB_PrintError_t)(int, const char*, ...);
static const DB_PrintError_t DB_PrintError = (DB_PrintError_t)0x0059A380;

// sub_59AC50 — fatal error handler (terminates process; distinct from Com_Error @ 0x6C1CE0).
typedef void (__cdecl* DB_FatalError_t)(int, const char*, ...);
static const DB_FatalError_t DB_FatalError = (DB_FatalError_t)0x0059AC50;

// =====================================================================
// T4M_DB_EnumAssetPoolB — full C++ reconstruction of sub_5E3FC0
// @faithful — sub_5E3FC0
//
//   Type-specific enumerator for types 5–8 only; all other types: no-op.
//   Does NOT use the global hash table — each case has its own storage:
//
//   type 5 — walks a 1024-slot pointer array at 0x21AB318 (dword_21AB318).
//             Each slot heads a singly-linked list; nodes have the layout:
//               [+0] void*   data     → passed to callback as arg0
//               [+4] node*   next     → next node in list
//               [+8] uint8_t nodeType → only nodes where nodeType==5 are emitted
//
//   type 6 — delegates to sub_6E9C80(callback)
//   type 7 — delegates to sub_6E9C50(callback)
//   type 8 — delegates to sub_70FCC0(callback)
//   default — no-op (return immediately)
//
//   arg_C (followOverrides) is accepted for DB_EnumPoolForDump_t compat
//   but is not used (vanilla ignores it too).
// =====================================================================
struct DB_Type5PoolNode
{
    void*              data;     // [+0] — passed to callback
    DB_Type5PoolNode*  next;     // [+4] — next in linked list
    uint8_t            nodeType; // [+8] — must equal 5 to emit
};
static DB_Type5PoolNode** const s_type5Pool =
    reinterpret_cast<DB_Type5PoolNode**>(0x021AB318); // dword_21AB318[0..1023]

typedef void (__cdecl* R_EnumImagePool_t)       (void (__cdecl*)(void*, int*));  // type 6 = IMAGE,      2048 entries @ 0x3BF6984
typedef void (__cdecl* Snd_EnumSoundPool_t)     (void (__cdecl*)(void*, int*));  // type 7 = SOUND,      4096 entries @ 0x3BED87C
typedef void (__cdecl* Snd_EnumSoundCurvePool_t)(void (__cdecl*)(void*, int*));  // type 8 = SOUND_CURVE, 2048 entries @ 0x45C22D0 (range-filtered)
static const R_EnumImagePool_t        R_EnumImagePool        = (R_EnumImagePool_t)       0x006E9C80;
static const Snd_EnumSoundPool_t      Snd_EnumSoundPool      = (Snd_EnumSoundPool_t)     0x006E9C50;
static const Snd_EnumSoundCurvePool_t Snd_EnumSoundCurvePool = (Snd_EnumSoundCurvePool_t)0x0070FCC0;

__declspec(noinline)
void T4M_DB_EnumAssetPoolB(int type,
                            void (__cdecl* callback)(void*, int*),
                            int* pType,
                            int  /*followOverrides*/)
{
    switch (type)
    {
    case 5:
        for (int i = 0; i < 1024; ++i)
        {
            DB_Type5PoolNode* node = s_type5Pool[i];
            while (node)
            {
                if (node->nodeType == 5)
                    callback(node->data, pType);
                node = node->next;
            }
        }
        break;
    case 6: R_EnumImagePool(callback);        break;
    case 7: Snd_EnumSoundPool(callback);      break;
    case 8: Snd_EnumSoundCurvePool(callback); break;
    default: break;
    }
}

// dword_1F552FC — dvar pointer; byte at +0x10 selects the enumeration path.
static const DWORD* const s_dvar_1F552FC_ptr = (const DWORD*)0x01F552FC;

// =====================================================================
// T4M_DB_AllocAssetHeader — full C++ reconstruction of sub_48D2A0
// @faithful — sub_48D2A0
//
//   Allocates a typed header from the type's pool (DB_XAssetPool[type]).
//   Uses the per-type alloc handlers (funcs_48D2B5 @ 0x8DC708).
//   On pool exhaustion:
//     1. Releases the DB writer lock (lock xadd -1 on dword_BF0088).
//     2. Non-fatal print via sub_59A380 (channel 1, "Exceeded limit…").
//     3. Dumps every live pool entry via sub_48D270 callback through
//        sub_48D560 or sub_5E3FC0 (gated on dword_1F552FC[+0x10]).
//     4. Fatal error via sub_59AC50 (same message, terminates process).
// =====================================================================
__declspec(noinline)
static void* T4M_DB_AllocAssetHeader(int type)
{
	void* pool = DB_XAssetPool[type];
	void* slot = DB_XAssetAllocHandlers[type](pool);

	if (slot)
		return slot;

	InterlockedDecrement((LONG*)g_dbWriterCount);

	DB_PrintError(1, "Exceeded limit of %d '%s' assets.\n",
		g_poolSize[type], T4M_DB_GetXAssetTypeName(type));

	{
		const uint8_t* dvar = reinterpret_cast<const uint8_t*>(*s_dvar_1F552FC_ptr);
		if (dvar && dvar[0x10] != 0)
			T4M_DB_EnumAssetPool(type, DB_PoolDumpAsset_cb, &type, 1);
		else
			T4M_DB_EnumAssetPoolB(type, DB_PoolDumpAsset_cb, &type, 1);
	}

	DB_FatalError(1, "Exceeded limit of %d '%s' assets.\n",
		g_poolSize[type], T4M_DB_GetXAssetTypeName(type));

	return nullptr;
}

// =====================================================================
// T4_DB_AllocXAssetEntry — full C++ reconstruction of sub_48D3B0
//
//   Pops the head of g_freeAssetEntries (global XAssetEntry free list).
//   Allocates a typed header via T4M_DB_AllocAssetHeader.
//   Initializes every field (zoneIndex, inuse=0, chains=0).
// =====================================================================
__declspec(noinline)
XAssetEntry* T4_DB_AllocXAssetEntry(int type, unsigned char zoneIndex)
{
	// 1. Pop from free list
	XAssetEntryPoolEntry* head = *g_freeAssetEntries;
	if (!head)
	{
		InterlockedDecrement((LONG*)g_dbWriterCount);
		DB_FatalError(1, "Could not allocate asset - increase XASSET_SIZE or XASSET_ENTRY_SIZE.\n");
		return nullptr;
	}

	// The `next` field (union) overlaps asset.type — read BEFORE writing type
	*g_freeAssetEntries = head->next;

	// 2. Fill the entry
	XAssetEntry* e = &head->entry;
	e->asset.type        = (XAssetType)type;
	e->asset.header.data = T4M_DB_AllocAssetHeader(type);
	e->zoneIndex         = (char)zoneIndex;
	e->inuse             = false;
	e->nextHash          = 0;
	e->nextOverride      = 0;
	return e;
}

// =====================================================================
// T4_DB_FindXAssetByName — sub_48D760
//   Lookup HEAD nextHash. Does NOT follow nextOverride (the HEAD already
//   holds the data of the highest-priority zone).
// =====================================================================
__declspec(noinline)
XAssetEntry* T4_DB_FindXAssetByName(int type, const char* name)
{
	unsigned int bucket = T4_DB_HashAssetName(type, name);
	unsigned short idx  = db_hashTable[bucket];
	while (idx != 0)
	{
		XAssetEntry* e = T4M_POOL_ENTRY(idx);
		if (e->asset.type == type)
		{
			const char* n = T4M_GetName(e);
			if (T4M_Q_stricmpn(n, name, 0x7FFFFFFF) == 0)
				return e;
		}
		idx = e->nextHash;
	}
	return nullptr;
}

// =====================================================================
// T4_DB_FindDefaultAsset — sub_48D7D0
//   Looks up the asset named DB_XAssetDefaultNames[type] in the bucket,
//   then walks the nextOverride chain to the LAST node (= most recent
//   override). Returns the header pointer (not the entry).
// =====================================================================
__declspec(noinline)
void* T4_DB_FindDefaultAsset(int type)
{
	const char* defName = DB_XAssetDefaultNames[type];
	if (!defName) return nullptr;

	unsigned int bucket = T4_DB_HashAssetName(type, defName);
	unsigned short idx  = db_hashTable[bucket];

	XAssetEntry* e = nullptr;
	while (idx != 0)
	{
		e = T4M_POOL_ENTRY(idx);
		if (e->asset.type == type)
		{
			const char* n = T4M_GetName(e);
			if (T4M_Q_stricmpn(n, defName, 0x7FFFFFFF) == 0)
				goto found;
		}
		idx = e->nextHash;
	}
	return nullptr;

found:
	while (e->nextOverride != 0)
		e = T4M_POOL_ENTRY(e->nextOverride);
	return e->asset.header.data;
}

// =====================================================================
// External helper signatures invoked during the T4_DB_FindXAssetHeader
// wait loop. Called directly through their VAs to stay byte-equivalent
// with vanilla sub_48DA30. "?" comments mark functions whose exact
// semantics are unknown but whose side effects are reproduced faithfully.
// =====================================================================
typedef int   (__cdecl* Sys_LeaveRecursiveLock_t) ();         // sub_70E3A0 — recursive lock release (ref-count + LeaveCriticalSection×2)
typedef void  (__cdecl* fn_void_void_t)();  // shared void(void) — Sys_EnterRecursiveLock / DB_WaitForWorkerEvent
typedef void  (__cdecl* Sys_DequeueLoadEvent_t)   (void* buf, int one);
typedef DWORD (__cdecl* Sys_GetTimeDelta_t)        ();         // timeGetTime() - g_waitStartTime (lazy-init base)
// DB_WarnMissingAsset (sub_48D980) is __usercall(eax = delta, edi = type, esi = name).
// Naked thunk below translates from our cdecl call site.

static const Sys_LeaveRecursiveLock_t Sys_LeaveRecursiveLock = (Sys_LeaveRecursiveLock_t) 0x0070E3A0;
static const fn_void_void_t           Sys_EnterRecursiveLock = (fn_void_void_t)           0x0070E340;
static const Sys_DequeueLoadEvent_t   Sys_DequeueLoadEvent   = (Sys_DequeueLoadEvent_t)   0x005FEC60;
static const fn_void_void_t           DB_WaitForWorkerEvent  = (fn_void_void_t)           0x005A32B0;
static const Sys_GetTimeDelta_t       Sys_GetTimeDelta        = (Sys_GetTimeDelta_t)       0x00603D40;

// DB_RecordMissingAsset (sub_48D460) is __usercall(ecx = type). Naked thunk to translate from cdecl.
__declspec(naked) static void T4M_Call_DB_RecordMissingAsset(int /*type*/)
{
	__asm
	{
		mov     ecx, [esp + 4]        ; ecx = type (cdecl arg_0)
		mov     eax, 0x0048D460
		jmp     eax                    ; tail-jump; callee returns to our caller
	}
}

// DB_WarnMissingAsset (sub_48D980) is __usercall(eax = delta, edi = type, esi = name). Naked thunk
// translates from cdecl (delta, type, name) and preserves callee-saved edi/esi.
__declspec(naked) static void T4M_Call_DB_WarnMissingAsset(int /*delta*/, int /*type*/, const char* /*name*/)
{
	__asm
	{
		push    edi                    ; preserve caller's edi (cdecl callee-saved)
		push    esi                    ; preserve caller's esi (cdecl callee-saved)
		; stack: [0]=esi_saved, [4]=edi_saved, [8]=retaddr, [12]=delta, [16]=type, [20]=name
		mov     eax, [esp + 12]        ; eax = delta
		mov     edi, [esp + 16]        ; edi = type
		mov     esi, [esp + 20]        ; esi = name
		mov     edx, 0x0048D980
		call    edx                     ; __usercall(eax, edi, esi)
		pop     esi                    ; restore esi
		pop     edi                    ; restore edi
		ret                             ; cdecl: caller cleans 3 stack args
	}
}

// Read `[TLS[0] + 0x20]` — vanilla caches the current thread id in that
// slot and reuses it across calls. Replicated byte-for-byte so other
// engine code that reads/writes the same slot stays coherent.
static DWORD* T4M_DB_TlsCachedTidSlot()
{
	DWORD tls0;
	__asm
	{
		mov     eax, fs:[0x2C]    ; TEB.ThreadLocalStoragePointer
		mov     eax, [eax]         ; first module's TLS block
		mov     tls0, eax
	}
	return (DWORD*)(tls0 + 0x20);
}

// =====================================================================
// T4_DB_FindXAssetHeader — sub_48DA30
// @faithful (full reconstruction).
//
//   Signature (__cdecl):
//     void* DB_FindXAssetHeader(int type, const char* name,
//                               bool useDefault, int timeoutMs);
//
//   Returns XAssetHeader.data (= entry->asset.header.data), or nullptr
//   for type ∈ {0x17, 0x20} when the entry is truly missing.
//
//   Faithful port of vanilla sub_48DA30 including:
//     - TLS-cached current-thread-id at [TLS[0] + 0x20]
//     - Infinite timeout loops forever (no artificial cap)
//     - Dual event waits on g_dbWorkerEvent and g_dbSecondaryEvent
//     - DB-worker pause bookkeeping via g_dbPauseEventHandle +
//       g_dbWorkerPausedFlag
//     - Full pumping sequence: sub_70E3A0 → Sys_SyncDatabase →
//       sub_5FEC60 → sub_5A32B0 → Sleep(0) → Sys_WakeDatabase →
//       (optional) sub_70E340
//     - useDefault alt-lookup via sub_48D460 when the 0x1F55288 dvar is set
//     - "Waited %i msec for asset..." logging path
// =====================================================================
__declspec(noinline)
void* T4_DB_FindXAssetHeader(int type, const char* name, bool useDefault, int timeoutMs)
{
	DWORD waitStartDelta = 0;          // var_1C
	// var_18 in vanilla sub_48DA30. sub_5FEC60 writes 24 bytes there (three
	// 8-byte movq stores at offsets 0, 8, 0x10). Must be ≥ 24 bytes or the
	// /GS cookie is corrupted → STATUS_STACK_BUFFER_OVERRUN on return.
	char  sub5FEC60Buf[32] = { 0 };
	XAssetEntry* entry = nullptr;

loc_48DA44:
	// Reader acquire: inc reader count, then spin while writer count != 0.
	InterlockedIncrement((LONG*)g_dbReaderCount);
	while (*g_dbWriterCount != 0) Sleep(0);

	// Lookup
	entry = T4_DB_FindXAssetByName(type, name);

	// Reader release
	InterlockedDecrement((LONG*)g_dbReaderCount);

	if (entry == nullptr) goto loc_48DAC9;

	// Fast path: fully-loaded entry
	if (entry->zoneIndex != 0) goto loc_48DD4F;

	// Entry exists but zoneIndex == 0: poll the worker event once.
	{
		DWORD w = WaitForSingleObject(*g_dbWorkerEvent, 0);
		int signaled = (w == 0) ? 1 : 0;     // mirrors neg/sbb/inc idiom
		if (signaled == 0) DB_WaitForPendingLoads();  // sub_5FDBF0
		if (signaled != 0) goto loc_48DD4F;
	}
	// fall through

loc_48DAC9:
	{
		DWORD* tid = T4M_DB_TlsCachedTidSlot();
		if (*tid == 0) *tid = GetCurrentThreadId();
		if (*tid == *g_dbWorkerThreadId) goto loc_48DD20;
	}

	if (waitStartDelta != 0) goto loc_48DB76;

	if (*g_waitTimerStarted == 0)
	{
		*g_waitStartTime = timeGetTime();
		*g_waitTimerStarted = 1;
	}
	waitStartDelta = timeGetTime() - *g_waitStartTime;

	{
		DWORD w = WaitForSingleObject(*g_dbWorkerEvent, 0);
		int signaled = (w == 0) ? 1 : 0;
		if (signaled == 0) DB_WaitForPendingLoads();
		// Vanilla loc_48DB4E: `test esi, esi; jnz loc_48DA44` — retry if signaled.
		if (signaled != 0) goto loc_48DA44;

		// Not signaled: cache current-thread-id if slot is empty.
		DWORD* tid = T4M_DB_TlsCachedTidSlot();
		if (*tid == 0) *tid = GetCurrentThreadId();
	}
	// fall through

loc_48DB76:
	{
		DWORD w = WaitForSingleObject(*g_dbWorkerEvent, 0);
		int signaled = (w == 0) ? 1 : 0;
		if (signaled == 0) DB_WaitForPendingLoads();
		// Vanilla loc_48DB9C: `test esi, esi; jnz loc_48DD20` — bail if signaled.
		if (signaled != 0) goto loc_48DD20;
	}

	if (*g_dbHasLoadedZones != 0 && *g_dbFlag951A02 != 0) goto loc_48DD20;

	{
		DWORD w = WaitForSingleObject(*g_dbSecondaryEvent, 0);
		int signaled = (w == 0) ? 1 : 0;
		if (signaled == 0) DB_WaitForPendingLoads();
		// Vanilla loc_48DBDB: `test esi, esi; jz loc_48DC45` — go to pump path if NOT signaled.
		if (signaled == 0) goto loc_48DC45;
	}

	// Secondary event signaled: ensure TLS tid cached, then branch on tid.
	// Vanilla loc_48DBE3..loc_48DBFC: read [TLS[0]+0x20]; if 0, call
	// GetCurrentThreadId and cache it.
	{
		DWORD* tid = T4M_DB_TlsCachedTidSlot();
		if (*tid == 0) *tid = GetCurrentThreadId();
	}

	// Vanilla loc_48DBFC:
	//   cached = [TLS[0]+0x20]
	//   if cached == ThreadId (alt):     goto loc_48DC3B (call sub_48E560)
	//   if cached == 0:                  re-fetch + cache GetCurrentThreadId
	//   if cached != dword_1FF51CC:      goto loc_48DC45
	//   if byte_3BED85D == 0:            goto loc_48DC45
	//   if dword_99724C == 0:            goto loc_48DC45
	//   else:                            fall through to loc_48DC3B
	{
		DWORD* tid = T4M_DB_TlsCachedTidSlot();
		DWORD cached = *tid;
		if (cached == *g_dbAltThreadId) goto loc_48DC3B;
		if (cached == 0)
		{
			*tid = GetCurrentThreadId();
			cached = *tid;
		}
		if (cached != *g_dbSecondaryThreadId) goto loc_48DC45;
		if (*g_dbFlag3BED85D == 0)            goto loc_48DC45;
		if (*g_dbPtr99724C == 0)              goto loc_48DC45;
	}

loc_48DC3B:
	DB_CheckPendingComplete();   // sub_48E560
	goto loc_48DCDF;

loc_48DC45:
	{
		DWORD* tid = T4M_DB_TlsCachedTidSlot();
		if (*tid == 0) *tid = GetCurrentThreadId();
	}
	{
		// If the DB worker is currently paused (flag==1), temporarily unpause
		// it, pump, and re-pause at the end.
		int needRepause = (*g_dbWorkerPausedFlag == 1) ? 1 : 0;
		if (needRepause)
		{
			// Vanilla order: clear flag first, then SetEvent
			*g_dbWorkerPausedFlag = 0;
			SetEvent(*g_dbPauseEventHandle);
		}

		int lockCtx = Sys_LeaveRecursiveLock();
		Sys_SyncDatabase();                     // sub_6F6CE0
		Sys_DequeueLoadEvent(sub5FEC60Buf, 1);
		DB_WaitForWorkerEvent();
		Sleep(0);
		Sys_WakeDatabase();                     // sub_6F6D60
		if (lockCtx != 0) Sys_EnterRecursiveLock();

		if (needRepause)
		{
			// Vanilla order: set flag first, then ResetEvent
			*g_dbWorkerPausedFlag = 1;
			ResetEvent(*g_dbPauseEventHandle);
		}
	}
	// fall through

loc_48DCDF:
	if (timeoutMs == (int)0xFFFFFFFF) goto loc_48DA44;   // infinite → retry forever

	if (*g_waitTimerStarted == 0)
	{
		*g_waitStartTime = timeGetTime();
		*g_waitTimerStarted = 1;
	}
	{
		DWORD elapsed = timeGetTime() - *g_waitStartTime - waitStartDelta;
		if ((int)elapsed < timeoutMs) goto loc_48DA44;
	}
	// fall through to timeout bailout

loc_48DD20:
	if (entry != nullptr) goto loc_48DD4F;

	// Take writer lock and re-search. If still missing → synthesize default.
	T4M_DB_WriterAcquire();
	entry = T4_DB_FindXAssetByName(type, name);
	if (entry == nullptr) goto loc_48DDB0;
	InterlockedDecrement((LONG*)g_dbWriterCount);
	// fall through

loc_48DD4F:
	entry->inuse = true;
	if (waitStartDelta != 0)
	{
		if (*g_waitTimerStarted == 0)
		{
			*g_waitStartTime = timeGetTime();
			*g_waitTimerStarted = 1;
		}
		DWORD waited = timeGetTime() - *g_waitStartTime - waitStartDelta;
		Com_PrintfChannel(0x0A, "Waited %i msec for asset '%s' of type '%s'\n",
			(int)waited, name, T4M_DB_GetXAssetTypeName(type));
	}
	return entry->asset.header.data;

loc_48DDB0:
	// Under writer lock. Optional alt-default via sub_48D460 when the
	// 0x1F55288 dvar is set.
	{
		bool useAlt = useDefault;
		if (useAlt)
		{
			// [edx+0x10] = dvar->current (first byte) — vanilla: cmp byte ptr [edx+0x10], 0
			const uint8_t* dvar = reinterpret_cast<const uint8_t*>(*g_dbAltDefaultDvarPtr);
			useAlt = dvar && dvar[0x10] != 0;
		}
		if (useAlt) T4M_Call_DB_RecordMissingAsset(type);
	}

	if (waitStartDelta != 0 && useDefault)
	{
		DWORD waited = Sys_GetTimeDelta() - waitStartDelta;
		T4M_Call_DB_WarnMissingAsset((int)waited, type, name);
	}

	if (type == 0x17 || type == 0x20)
	{
		InterlockedDecrement((LONG*)g_dbWriterCount);
		return nullptr;
	}

	entry = T4_DB_LinkXAssetEntry(type, name);
	InterlockedDecrement((LONG*)g_dbWriterCount);

	if (!entry) return nullptr;
	return entry->asset.header.data;
}

// T4M_Sys_MemCpyFix — detour of vanilla sub_7AFFC0 (optimised memmove, installed in
// PatchT4_PreLoad). Called unconditionally even when defaultHdr==NULL.

// =====================================================================
// T4_DB_LinkXAssetEntry — sub_48D860
// @faithful — sub_48D860
//
//   Vanilla __usercall: eax = type (register), arg_0 (stack) = name.
//   T4M takes (type, name) __cdecl.
//
//   Behavior:
//     1. T4_DB_FindDefaultAsset(type) → defaultHdr.
//        If NULL: lock xadd -1 on g_dbWriterCount, then sub_59AC50(1,…):
//          types 0xB/0xC → BSP message ("…build the fast file…") with name.
//          others        → "Could not load default asset…" with defName/typeName/name.
//        Execution continues regardless (code falls to loc_48D8BD).
//     2. g_assetRefCount++; T4_DB_AllocXAssetEntry(type, 0) → entry.
//     3. sub_7AFFC0(entry->header.data, defaultHdr, getSize(type))
//        — unconditional (vanilla calls it even when defaultHdr==NULL).
//     4. type==9 quirk: zero dword at header.data+8 FIRST, then +4.
//     5. Hash-insert entry at HEAD of bucket (nextHash chain).
//     6. strlen(name)+1 → StringTable_Find(0, name, 4, len) → intern;
//        call DB_XAssetSetNameHandlers[type](&header, internedAddr).
//     7. entry->inuse = 1; return entry.
// =====================================================================
__declspec(noinline)
XAssetEntry* T4_DB_LinkXAssetEntry(int type, const char* name)
{
	// Com_Printf(0, "T4M Link[%d] %s ...\n", type, name ? name : "<null>");

	// 1. Find default; on miss release writer lock and report via sub_59AC50
	void* defaultHdr = T4_DB_FindDefaultAsset(type);
	if (!defaultHdr)
	{
		InterlockedDecrement((LONG*)g_dbWriterCount);
		if (type == 0xB || type == 0xC)
		{
			DB_FatalError(1,
				"Couldn't find the bsp for this map.  "
				"Please build the fast file associated with %s and try again.",
				name);
		}
		else
		{
			DB_FatalError(1,
				"Could not load default asset '%s' for asset type '%s'.\n"
				"Tried to load asset '%s'.",
				DB_XAssetDefaultNames[type],
				T4M_DB_GetXAssetTypeName(type),
				name);
		}
	}

	// 2. Increment asset ref-count; allocate entry (zoneIndex=0 = permanent zone)
	*g_assetRefCount += 1;
	XAssetEntry* entry = T4_DB_AllocXAssetEntry(type, 0);

	// 3. Copy default data — unconditional (T4M_Sys_MemCpyFix = detour of vanilla sub_7AFFC0).
	// ASM: `push eax/size; push ebp/defaultHdr; push edx/entry->header.data; call sub_7AFFC0`.
	// `src` must be the defaultHdr pointer VALUE — passing `&defaultHdr` copies bytes from
	// our own stack frame and fills the entry header with garbage, which leaves name-hash
	// lookups pointing at a bucket where sub_48E370's chain scan never terminates.
	T4M_Sys_MemCpyFix(entry->asset.header.data, (void**)defaultHdr, (int)T4M_GetSize(type));

	// 4. Type-9 (LIGHTDEF) quirk: zero offset+8 first, then offset+4 (ASM order)
	if (type == 9)
	{
		uint32_t* hdr = reinterpret_cast<uint32_t*>(entry->asset.header.data);
		hdr[2] = 0;
		hdr[1] = 0;
	}

	// 5. Hash-insert at HEAD of bucket
	unsigned int   bucket = T4_DB_HashAssetName(type, name);
	unsigned short newIdx = T4M_POOL_INDEX(entry);
	entry->nextHash      = db_hashTable[bucket];
	db_hashTable[bucket] = newIdx;

	// 6. Intern name in global string table; call setName handler.
	// ASM: test eax,eax / jz loc_48D956 → loc_48D956: xor eax,eax (NULL).
	// When StringTable_Find returns 0, vanilla explicitly passes NULL to the handler.
	int nameLen   = static_cast<int>(strlen(name)) + 1;
	int stringIdx = StringTable_Find(0, name, 4, nameLen);
	const char* stringAddr = nullptr;
	if (stringIdx != 0)
		stringAddr = *g_stringTableBase + stringIdx * 12 + 4;
	DB_XAssetSetNameHandlers[entry->asset.type](&entry->asset.header, stringAddr);

	// 7. Mark inuse; return entry
	entry->inuse = true;
	// Com_Printf(0, "T4M Link[%d] %s OK\n", type, name ? name : "<null>");
	return entry;
}

// =====================================================================
// T4_DB_PromoteOverride — loc_48F4C3 (inside sub_48F340)
//
//   Promotes the FIRST node of the nextOverride chain to HEAD of the bucket:
//     1. Calls DB_XAssetOverridePromoters[type] if defined (e.g. sync D3D
//        state for materials).
//     2. memcpy(override->header → main->header) [size = getSize(type)]
//     3. main->zoneIndex    = override->zoneIndex
//     4. main->nextOverride = override->nextOverride  (unchains override)
//     5. DB_XAssetFreeHandlers[type] frees override's data
//     6. override is returned to g_freeAssetEntries
// =====================================================================
__declspec(noinline)
void T4_DB_PromoteOverride(XAssetEntry* main, XAssetEntry* override)
{
	int type = main->asset.type;

	// 1. Custom promoter
	//    Vanilla loc_48F4C3:
	//      push ebx (isPerm); push ecx (OVERRIDE->header); push edx (MAIN->header); call
	//    Call order: promoter(MAIN, OVERRIDE, isPermZone)
	DB_XAssetOverridePromoter_t promoter = DB_XAssetOverridePromoters[type];
	if (promoter)
	{
		bool isPermZone = (main->zoneIndex == 0);
		promoter(main->asset.header.data, override->asset.header.data, isPermZone);
	}

	// 2. Copy header override → main
	int size = T4M_GetSize(type);
	memcpy(main->asset.header.data, override->asset.header.data, static_cast<size_t>(size));

	// 3. Copy metadata + unchain override
	main->zoneIndex    = override->zoneIndex;
	main->nextOverride = override->nextOverride;

	// 4. Free the override's typed data
	void* pool = DB_XAssetPool[type];
	DB_XAssetFreeHandlers[type](pool, override->asset.header.data);

	// 5. Return the override entry to the free-list
	XAssetEntryPoolEntry* ovPE = reinterpret_cast<XAssetEntryPoolEntry*>(override);
	ovPE->next = *g_freeAssetEntries;
	*g_freeAssetEntries = ovPE;
}

// DB_PushCopyInfo (sub_48D720) — if g_syncValue!=0: immediately re-links entry via
// T4_DB_LinkXAssetEntryOverrideAware(entry, copyData=1); otherwise enqueues entry
// into g_copyInfo[g_copyInfoCount++] (max 3072) for deferred processing.
// __fastcall: ecx = XAssetEntry*. Called when copyData==0 and existing is found.
typedef void (__fastcall* DB_PushCopyInfo_t)(XAssetEntry*);
static const DB_PushCopyInfo_t DB_PushCopyInfo = (DB_PushCopyInfo_t)0x0048D720;

// =====================================================================
// T4_DB_LinkXAssetEntryOverrideAware — sub_48DFF0
//
//   Called for EVERY asset while loading a zone.
//   Arbitrates between two concurrent zones by allocFlags (priority):
//
//     pNew >= pOld  → new WINS:
//       - new data → HEAD->header (via swap)
//       - old entry is moved into the HEAD's nextOverride chain
//
//     pNew <  pOld  → new LOSES:
//       - inserted into the nextOverride chain at the correct position
//         (descending priority)
//
//   The bucket HEAD ALWAYS holds the data of the max-priority zone.
//
//   Faithful to sub_48DFF0 with 3 main cases:
//     - copyData=0 and ',' prefix: soft-override (no-op if exists)
//     - copyData=0 normal:         alloc + insert
//     - copyData≠0:                full arbitration
// =====================================================================
__declspec(noinline)
XAssetEntry* T4_DB_LinkXAssetEntryOverrideAware(XAssetEntry* newEntry, int copyData)
{

	int type = newEntry->asset.type;
	/*
	// Guard: catch corrupted entries before the name handler dereferences header.data
	if ((unsigned)type >= ASSET_TYPE_MAX || (uintptr_t)newEntry->asset.header.data < 0x1000)
	{
		Com_PrintfChannel(0, "[T4M] CORRUPT ENTRY: newEntry=%p type=%d header.data=%p copyData=%d\n",
			newEntry, type, newEntry->asset.header.data, copyData);
		__debugbreak();
		return newEntry;
	}
	*/
	// --- 1. Read name, detect ',' soft-override prefix ---
	const char* name    = DB_XAssetGetNameHandlers[type](&newEntry->asset.header);
	bool softOverride   = (name[0] == ',');
	if (softOverride) name = name + 1;

	//Com_PrintfChannel(0, "[T4M] - Link Asset Entry Override Aware, type %d name %s copyData %d", (int)type, name, copyData);

	// --- 2. Look up an existing entry in the bucket (same name & type) ---
	unsigned int bucket = T4_DB_HashAssetName(type, name);
	unsigned short idx  = db_hashTable[bucket];
	XAssetEntry* existing = nullptr;
	while (idx != 0)
	{
		XAssetEntry* e = T4M_POOL_ENTRY(idx);
		if (e->asset.type == type)
		{
			const char* n = DB_XAssetGetNameHandlers[type](&e->asset.header);
			if (T4M_Q_stricmpn(n, name, 0x7FFFFFFF) == 0) { existing = e; break; }
		}
		idx = e->nextHash;
	}

	// =================================================================
	// BRANCH A — copyData == 0 with softOverride
	//   ',name' prefix: if it exists, no-op; otherwise T4_DB_LinkXAssetEntry
	// =================================================================
	if (copyData == 0 && softOverride)
	{
		if (existing) return existing;
		return T4_DB_LinkXAssetEntry(type, name);
	}

	// =================================================================
	// BRANCH B — copyData == 0 !softOverride: alloc fresh + copy data
	//   (vanilla loc_48E0C2 — newEntry is discarded, fresh takes over)
	// =================================================================
	XAssetEntry* entry = newEntry;  // default: newEntry (copyData != 0 case)
	if (copyData == 0)
	{
		// Allocate a fresh entry and copy newEntry's data into it.
		// zoneIndex: vanilla sub_48DFF0 BRANCH B (loc_48E0C2) reads dword_987084
		// (current loading zone). newEntry is a stack-local in sub_48E300 with only
		// type and header.data initialized — its zoneIndex byte is garbage.
		//XAssetEntry* fresh = T4_DB_AllocXAssetEntry(type, (unsigned char)*g_currentZoneIndex);

		XAssetEntry* fresh = T4_DB_AllocXAssetEntry(type, (unsigned char)*g_currentZoneIndex);
		int size = T4M_GetSize(type);
		memcpy(fresh->asset.header.data, newEntry->asset.header.data, static_cast<size_t>(size));
		// Ensure getName(fresh->header) hashes to the same bucket that fresh is
		// inserted into below. The zone data may carry a comma-prefixed name;
		// without this setName the invariant breaks through any later C.0/C.2
		// memcpy that promotes fresh's data into an existing entry.
		//if (softOverride)
		//	DB_XAssetSetNameHandlers[type](&fresh->asset.header, name);
		entry = fresh;
	}

	// =================================================================
	// COMMON — loc_48E101: if !existing → insert, else arbitrate
	// =================================================================
	if (!existing)
	{
		entry->nextHash      = db_hashTable[bucket];
		db_hashTable[bucket] = T4M_POOL_INDEX(entry);
		return entry;
	}

	// From here on: existing != NULL, entry = newEntry (copyData != 0)
	//              OR entry = fresh (copyData == 0).
	// The C.1/C.2 arbitration uses `entry` in place of `newEntry` everywhere.
	newEntry = entry;

	// =================================================================
	// BRANCH C — existing present
	// =================================================================

	// ----- C.0 — existing is in zone 0 (permanent zone) → loc_48E1FB -----
	//
	// When existing is in zone 0 (code_post_gfx / permanent zone), vanilla
	// takes a COMPLETELY DIFFERENT path from the C.1/C.2 arbitration below:
	// REPLACE + FREE newEntry (no nextOverride chaining at all).
	//
	// Vanilla loc_48E1FB flow:
	//   1. g_assetRefCount--
	//   2. inuse dispatch if existing->inuse
	//   3. sub_48D6D0(existing, newEntry):
	//        a. promoter(existing, newEntry, isPerm=true)
	//        b. memcpy(existing->header, newEntry->header, getSize(type))
	//        c. existing->zoneIndex = newEntry->zoneIndex
	//   4. free newEntry->header via DB_XAssetFreeHandlers[type]
	//   5. newEntry entry → g_freeAssetEntries
	//   6. return existing
	//
	// Missing this branch was the cause of the mak.ff freeze: without
	// freeing newEntry's typed header, per-type asset pools (materials,
	// images…) fill up quickly with ghost override entries whose data
	// is never released.
	if (existing->zoneIndex == 0)
	{
		// copyData==0: discard newEntry (fresh alloc) and return existing unchanged
		// (ASM loc_48E1FB: cmp [ebp+arg_4], 0 / jz loc_48E2E4)
		if (copyData == 0)
		{
			DB_PushCopyInfo(newEntry);
			return existing;
		}

		// g_assetRefCount--
		*g_assetRefCount -= 1;

		// inuse dispatch
		if (existing->inuse)
		{
			*g_inuseEntry  = existing;
			*g_inuseHeader = &existing->asset.header;
			DB_InUseHandlerDispatch();
		}

		// Emulate sub_48D6D0: promoter + memcpy + zoneIndex transfer
		int type0 = existing->asset.type;
		DB_XAssetOverridePromoter_t promoterZ = DB_XAssetOverridePromoters[type0];
		if (promoterZ)
		{
			// isPerm = (existing->zoneIndex == 0) which is TRUE in this branch
			promoterZ(existing->asset.header.data, newEntry->asset.header.data, /*isPerm=*/true);
		}
		int sizeZ = T4M_GetSize(type0);
		memcpy(existing->asset.header.data, newEntry->asset.header.data, static_cast<size_t>(sizeZ));
		existing->zoneIndex = newEntry->zoneIndex;

		// Free newEntry's typed header
		void* poolZ = DB_XAssetPool[newEntry->asset.type];
		DB_XAssetFreeHandlers[newEntry->asset.type](poolZ, newEntry->asset.header.data);

		// Return newEntry entry to the free list
		XAssetEntryPoolEntry* pe = reinterpret_cast<XAssetEntryPoolEntry*>(newEntry);
		pe->next = *g_freeAssetEntries;
		*g_freeAssetEntries = pe;

		return existing;
	}

	// "Attempting to override" warning — vanilla behavior: only fires when
	// the asset has NO default name AND type is not 0x10 / 0x20.
	// (ASM loc_48E131: cmp [defaultName], 0 / jnz skip — skips the normal case)
	{
		const char* defName = DB_XAssetDefaultNames[type];
		bool hasEmptyDefault = (!defName || defName[0] == '\0');
		if (hasEmptyDefault && type != 0x20 && type != 0x10)
		{
			InterlockedDecrement((LONG*)g_dbWriterCount);
			DB_FatalError(1,
				"Attempting to override asset '%s' from '%s' with '%s'",
				name,
				g_zoneFileNames[existing->zoneIndex].name,
				g_zoneFileNames[newEntry->zoneIndex].name);
		}
	}

	int pNew = T4M_ZonePriority(newEntry->zoneIndex);
	int pOld = T4M_ZonePriority(existing->zoneIndex);

	// ----- C.1 — NOUVEAU PERD : insertion dans nextOverride -----
	if (pNew < pOld)
	{
		// Cherche la position d'insertion : on descend tant que la zone
		// du nœud courant est plus prioritaire que pNew.
		unsigned short* link = &existing->nextOverride;
		while (*link != 0)
		{
			XAssetEntry* cur = T4M_POOL_ENTRY(*link);
			int pCur = T4M_ZonePriority(cur->zoneIndex);
			if (pNew >= pCur) break;
			link = &cur->nextOverride;
		}
		newEntry->nextOverride = *link;
		*link = T4M_POOL_INDEX(newEntry);
		return existing;
	}

	// ----- C.2 — NEW WINS: swap data, chain existing as override -----
	//
	// Vanilla flow (sub_48DFF0:loc_48E25A + sub_48D6D0):
	//   1. If copyData==0 → sub_48D720(newEntry); return existing
	//   2. If existing->inuse → InUse dispatch
	//   3. Swap nextOverride: newEntry.nextOverride = existing.nextOverride
	//                          existing.nextOverride = idx(newEntry)
	//   4. size = getSize(existing->type)
	//   5. memcpy(tmp, existing->header, size)        ← save existing's data
	//   6. bl = existing->zoneIndex                    ← save existing's zone
	//   7. sub_48D6D0(esi=existing, edi=newEntry):
	//        a. promoter(existing, newEntry, existing->zoneIndex == 0)
	//        b. memcpy(existing->header, newEntry->header, getSize(new->type))
	//        c. existing->zoneIndex = newEntry->zoneIndex
	//   8. memcpy(newEntry->header, tmp, getSize(new->type))
	//   9. newEntry->zoneIndex = bl
	//
	// CRITICAL: the PROMOTER (step 7a) transfers GPU state between
	// existing and newEntry for materials/images/techsets. Skipping it
	// leaks or strands the D3D state → render pipeline deadlock.

	// 1. copyData==0: discard newEntry (fresh alloc) and return existing unchanged
	// (ASM loc_48E25A: cmp [ebp+arg_4], 0 / jz loc_48E2E4)
	if (copyData == 0)
	{
		DB_PushCopyInfo(newEntry);
		return existing;
	}

	// 2. InUse dispatch
	if (existing->inuse)
	{
		*g_inuseEntry  = existing;
		*g_inuseHeader = &existing->asset.header;
		DB_InUseHandlerDispatch();
	}

	// 3. Swap nextOverride
	newEntry->nextOverride = existing->nextOverride;
	existing->nextOverride = T4M_POOL_INDEX(newEntry);

	// 4+5. Save existing's data
	int size = T4M_GetSize(type);
	void* tmp = _alloca(static_cast<size_t>(size));
	memcpy(tmp, existing->asset.header.data, static_cast<size_t>(size));

	// 6. Save existing's zoneIndex
	unsigned char savedExistingZone = existing->zoneIndex;

	// 7a. Call promoter BEFORE data overwrite (existing still has old data)
	DB_XAssetOverridePromoter_t promoter = DB_XAssetOverridePromoters[type];
	if (promoter)
	{
		bool isPermZone = (existing->zoneIndex == 0);
		promoter(existing->asset.header.data, newEntry->asset.header.data, isPermZone);
	}

	// 7b. Copy new's data into existing
	memcpy(existing->asset.header.data, newEntry->asset.header.data, static_cast<size_t>(size));

	// 7c. existing->zoneIndex = newEntry->zoneIndex
	existing->zoneIndex = newEntry->zoneIndex;

	// 8. Copy saved existing data into newEntry (newEntry becomes the losing override)
	memcpy(newEntry->asset.header.data, tmp, static_cast<size_t>(size));

	// 9. newEntry->zoneIndex = saved existing zone
	newEntry->zoneIndex = savedExistingZone;

	// NOTE: vanilla does NOT swap the inuse flag — we leave it out too.
	return existing;
}

// =====================================================================
// T4_DB_UnloadZoneAssets — sub_48F340
//
//   Walks every one of the 0x8000 buckets. For each entry whose
//   zoneIndex matches zoneToUnload:
//     A.1 — if inuse && copyDefaults → dispatch inuse-handler
//     A.2 — call DB_XAssetUnloadHandlers[type](header) [frees GPU state]
//     A.3 — if nextOverride != 0 → PROMOTE (loc_48F4C3)
//     A.4 — otherwise:
//            a) copyDefaults=false → free + remove from bucket
//            b) copyDefaults=true  → look up default, rebind
//     B   — also cleans out any overrides in the same chain that
//            belong to the zone being unloaded.
// =====================================================================
__declspec(noinline)
void T4_DB_UnloadZoneAssets(int zoneToUnload, bool copyDefaults)
{
	Com_PrintfChannel(0x10,
		"[T4M] T4_DB_UnloadZoneAssets(zone=%d, copyDefaults=%d)\n",
		zoneToUnload, copyDefaults ? 1 : 0);

	for (int bucket = 0; bucket < 0x8000; ++bucket)
	{
		unsigned short* prev = &db_hashTable[bucket];
		while (*prev != 0)
		{
			XAssetEntry* e = T4M_POOL_ENTRY(*prev);

			// --- Not our zone: walk down the nextHash chain ---
			if (e->zoneIndex != zoneToUnload)
			{
				// First clean up any overrides of this HEAD that
				// belong to the zone being unloaded.
				unsigned short* ovPrev = &e->nextOverride;
				while (*ovPrev != 0)
				{
					XAssetEntry* ov = T4M_POOL_ENTRY(*ovPrev);
					if (ov->zoneIndex == zoneToUnload)
					{
						DB_XAssetUnloadHandler_t u = DB_XAssetUnloadHandlers[ov->asset.type];
						if (u) u(ov->asset.header.data);
						*ovPrev = ov->nextOverride;

						void* pool = DB_XAssetPool[ov->asset.type];
						DB_XAssetFreeHandlers[ov->asset.type](pool, ov->asset.header.data);

						XAssetEntryPoolEntry* ovPE = reinterpret_cast<XAssetEntryPoolEntry*>(ov);
						ovPE->next = *g_freeAssetEntries;
						*g_freeAssetEntries = ovPE;
					}
					else
					{
						ovPrev = &ov->nextOverride;
					}
				}
				prev = &e->nextHash;
				continue;
			}

			// ======= This entry belongs to the zone being unloaded =======
			int type = e->asset.type;

			// A.1 — inuse dispatch
			if (e->inuse && copyDefaults)
			{
				*g_inuseEntry  = e;
				*g_inuseHeader = &e->asset.header;
				DB_InUseHandlerDispatch();
			}

			// A.2 — unload GPU handler
			DB_XAssetUnloadHandler_t u = DB_XAssetUnloadHandlers[type];
			if (u) u(e->asset.header.data);

			// A.3 — Promote an override if the chain is non-empty
			if (e->nextOverride != 0)
			{
				XAssetEntry* ov = T4M_POOL_ENTRY(e->nextOverride);
				T4_DB_PromoteOverride(e, ov);
				prev = &e->nextHash;
				continue;
			}

			// A.4.a — No override + copyDefaults=false: plain free
			if (!copyDefaults)
			{
				*prev = e->nextHash;
				void* pool = DB_XAssetPool[type];
				DB_XAssetFreeHandlers[type](pool, e->asset.header.data);

				XAssetEntryPoolEntry* pe = reinterpret_cast<XAssetEntryPoolEntry*>(e);
				pe->next = *g_freeAssetEntries;
				*g_freeAssetEntries = pe;
				// *prev was updated: do not advance
				continue;
			}

			// A.4.b — No override + copyDefaults=true: rebind default
			{
				void* defHdr = T4_DB_FindDefaultAsset(type);
				if (!defHdr)
				{
					// No default available: fully remove
					*prev = e->nextHash;
					void* pool = DB_XAssetPool[type];
					DB_XAssetFreeHandlers[type](pool, e->asset.header.data);

					XAssetEntryPoolEntry* pe = reinterpret_cast<XAssetEntryPoolEntry*>(e);
					pe->next = *g_freeAssetEntries;
					*g_freeAssetEntries = pe;

					const char* defName = DB_XAssetDefaultNames[type];
					if (defName && defName[0] != '\0')
					{
						Com_PrintfChannel(1,
							"[T4M] No default asset for type %s during unload\n",
							T4M_DB_GetXAssetTypeName(type));
					}
					continue;
				}

				// Default found: save name → memcpy default → restore name
				//
				// Vanilla loc_48F475:
				//   name = getName(&header)   (before memcpy)
				//   g_assetRefCount += 1
				//   zoneIndex = 0
				//   memcpy(&header, defHdr, size)
				//   setName(&header, name)    ← restore original name
				//
				// CRITICAL: without setName the header carries the default's
				// name (e.g. "$default") while the entry stays in the bucket
				// of the original name. Any hash+getName lookup fails → hang
				// in sub_48E370 (hash-chain scan not terminated on 0).
				const char* savedName = DB_XAssetGetNameHandlers[type](&e->asset.header);
				*g_assetRefCount += 1;
				e->zoneIndex = 0;

				int size = T4M_GetSize(type);
				memcpy(e->asset.header.data, defHdr, static_cast<size_t>(size));

				DB_XAssetSetNameHandlers[type](&e->asset.header, savedName);

				prev = &e->nextHash;
			}
		}
	}

	Com_PrintfChannel(0x10, "[T4M] T4_DB_UnloadZoneAssets(zone=%d) done\n", zoneToUnload);
}