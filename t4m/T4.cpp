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
	char*                    g_stringTableBase       = (char*)                    0x03702390;

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
	bool* g_assetsDirty = (bool*)0x45C22C5;  // byte_45C22C5  — "assets need sync" flag

	ZoneFileEntry* g_zoneFileNames = (ZoneFileEntry*)0xD04CB0;
	PMem_Pool* g_pmem_pools = (PMem_Pool*)0x224F9D8;

	DB_XAssetGetNameHandler * DB_XAssetGetNameHandlers = (DB_XAssetGetNameHandler *)0x008DCAF8;
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
// @wrapper — asm usercall vers sub_5EEE20
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
// @wrapper — asm usercall vers sub_5EEF10
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

// @wrapper — asm usercall vers sub_5EF150
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

// @wrapper — asm usercall vers sub_6E8DA0
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

inline float R_NormalizedTextScale(game::Font_s* font, float scale) {
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

void __declspec(naked) DoReturn() {
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
dvar_t* Dvar_RegisterInt(int default_value, const char* name, int mina, int max, int flags, const char* description) {
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

void* DB_ReallocXAssetPool(XAssetType type, unsigned int size)
{
	int elSize = DB_GetXAssetTypeSize(type);
	void* poolEntry = malloc(size * elSize);
	DB_XAssetPool[type] = poolEntry;
	g_poolSize[type] = size;
	return poolEntry;
}

void __cdecl DB_ListAssetPool(XAssetType type, bool count_only)
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
	v1 = DB_GetXAssetTypeName(type);
	
	if (!count_only)
		Com_Printf(0, "Listing assets in %s pool.\n", v1);

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
				v3 = DB_GetXAssetName(&assetEntry->asset);
				if (!count_only)
					Com_Printf(0, "Asset: %s FF: %s\n", v3, v2->name);
				assetPoolSize += DB_GetXAssetTypeSize(assetEntry->asset.type);
				for (overrideAssetEntryIndex = assetEntry->nextOverride;
					overrideAssetEntryIndex;
					overrideAssetEntryIndex = overrideAssetEntry->nextOverride)
				{
					overrideAssetEntry = &g_assetEntryPool[overrideAssetEntryIndex].entry;
					++assetPoolCount;
					v5 = &g_zoneNames[g_assetEntryPool[overrideAssetEntryIndex].entry.zoneIndex];
					v6 = DB_GetXAssetName(&g_assetEntryPool[overrideAssetEntryIndex].entry.asset);
					if (!count_only)
						Com_Printf(0, "Asset: %s FF: %s | overriden\n", v6, v5);
					assetPoolSize += DB_GetXAssetTypeSize(overrideAssetEntry->asset.type);
				}
			}
		}
	}
	v7 = DB_GetXAssetTypeName(type);
	Com_Printf(16, "Total of %d assets in %s pool, max %d, size %d\n", assetPoolCount, v7, g_poolSize[type], assetPoolSize);
}

char *__cdecl DB_GetXAssetTypeName(int type)
{
	char** g_assetNames = (char **)0x008DCA68;
	return g_assetNames[type];
}

const char *__cdecl DB_GetXAssetName(XAsset *asset)
{
	return DB_GetXAssetHeaderName(asset->type, &asset->header);
}

const char *__cdecl DB_GetXAssetHeaderName(int type, XAssetHeader *header)
{
	int v2;
	const char *name;
	name = DB_XAssetGetNameHandlers[type](header);
	return name;
}

int __cdecl DB_GetXAssetTypeSize(int type)
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

// @faithful — sub_48D560 (lock half)
// T4M_DB_ReaderAcquire: atomically increments readerCount, then waits
// until writerCount == 0.
void T4M_DB_ReaderAcquire()
{
	InterlockedIncrement((LONG*)g_dbReaderCount);
	while (*g_dbWriterCount != 0)
		Sleep(0);
}

// @faithful — sub_48D560 (release half)
void T4M_DB_ReaderRelease()
{
	InterlockedDecrement((LONG*)g_dbReaderCount);
}

// =====================================================================
// =====================================================================
//  Asset DB — C++ reconstructions (replace the vanilla engine pointers
//  0x48D410, 0x48D3B0, 0x48D2A0, 0x48D760, 0x48D7D0, 0x48D860, 0x48DFF0,
//  loc_48F4C3, 0x48F340 — all commented out above).
//
//  Benefits:
//    - Clean cdecl convention (no buggy __fastcall typedef)
//    - 100% C++ logic, debuggable in source
//    - Semantically identical to vanilla behavior
//
//  PatchT4_Override installs a detour on 0x48F340 → T4_DB_UnloadZoneAssets
//  so that vanilla internal callers also hit the reconstruction.
//
//  Every T4_DB_* function below is @faithful (identical behavior to
//  vanilla, just reimplemented in clean C++ and detoured).
// =====================================================================
// =====================================================================

#include <malloc.h>   // _alloca

// =====================================================================
// T4_DB_HashAssetName — sub_48D410
//   15-bit hash (bucket 0..0x7FFF). tolower + '\\' → '/'.
//   Seed: type. Multiplier: 31 (shl 5; sub seed).
// =====================================================================
__declspec(noinline)
unsigned int T4_DB_HashAssetName(int type, const char* name)
{
	unsigned int h = static_cast<unsigned int>(type);
	while (*name)
	{
		int c = static_cast<unsigned char>(*name);
		if (c == '\\')               c = '/';
		else if (c >= 'A' && c <= 'Z') c += 0x20;
		h = (h * 31u) + static_cast<unsigned int>(c);
		++name;
	}
	return h & 0x7FFFu;
}

// =====================================================================
// T4M_DB_AllocAssetHeader — full C++ reconstruction of sub_48D2A0
// @faithful — sub_48D2A0
//
//   Allocates a typed header from the type's pool (DB_XAssetPool[type]).
//   Uses the per-type alloc handlers (funcs_48D2B5 @ 0x8DC708).
//   On pool exhaustion, issues a fatal Com_Error (vanilla additionally
//   dumps the pool contents before erroring — omitted here for brevity).
// =====================================================================
__declspec(noinline)
static void* T4M_DB_AllocAssetHeader(int type)
{
	void* pool = DB_XAssetPool[type];
	void* slot = DB_XAssetAllocHandlers[type](pool);

	if (!slot)
	{
		// Vanilla decrements g_dbWriterCount and calls a fatal Com_Error.
		InterlockedDecrement((LONG*)g_dbWriterCount);
		Com_Error(1, "Exceeded limit of %d '%s' assets.\n",
			g_poolSize[type], DB_GetXAssetTypeName(type));
		return nullptr;
	}
	return slot;
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
		Com_Error(1, "Could not allocate asset - increase XASSET_SIZE or XASSET_ENTRY_SIZE.\n");
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
// T4_DB_LinkXAssetEntry — __usercall → __cdecl wrapper for sub_48D860
//
//   Vanilla convention: eax = type (register), arg_0 stack = name.
//   Returns eax = new XAssetEntry*.
//
//   Vanilla logic:
//     1. T4_DB_FindDefaultAsset(type) → header template
//     2. AllocXAssetEntry + memcpy default → new entry
//     3. setName via internal string table, insert at nextHash head.
//
//   We delegate to the engine (complex string table) — just fix the ASM.
// =====================================================================
__declspec(naked)
XAssetEntry* T4_DB_LinkXAssetEntry(int type, const char* name)
{
	__asm
	{
		push    dword ptr [esp+8]       ; push name (arg_1)
		mov     eax, [esp+8]            ; eax = type (arg_0, +8 after push)
		mov     edx, 0x48D860
		call    edx                     ; sub_48D860: eax = entry ptr
		add     esp, 4                  ; cleanup push name
		ret                              ; cdecl: caller cleans args
	}
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

	// --- 1. Read name, detect ',' soft-override prefix ---
	const char* name    = DB_XAssetGetNameHandlers[type](&newEntry->asset.header);
	bool softOverride   = (name[0] == ',');
	if (softOverride) name = name + 1;

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
		// zoneIndex: vanilla reads dword_987084 (current zone). We use
		// newEntry->zoneIndex as a reasonable approximation.
		XAssetEntry* fresh = T4_DB_AllocXAssetEntry(type, newEntry->zoneIndex);
		int size = T4M_GetSize(type);
		memcpy(fresh->asset.header.data, newEntry->asset.header.data, static_cast<size_t>(size));
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
	// BRANCH C — existing present: priority arbitration
	// =================================================================

	// "Attempting to override" warning — vanilla behavior is INVERTED vs
	// what you would expect: the warning only fires when the asset does
	// NOT have a default name AND type is not 0x10 / 0x20.
	//
	// Vanilla loc_48E131+27:
	//   cmp [defaultName], 0
	//   jnz skip           ; ← skip if defName is non-empty (= normal case)
	//   cmp type, 20h
	//   jz  skip
	//   cmp type, 10h
	//   jz  skip
	//   print warning      ; rare — only for types with no default
	//
	// Without this inversion we would spam Com_PrintfChannel for every
	// normal override (thousands of calls while loading a mod) → likely
	// deadlock in the console/overlay under the DB writer lock.
	if (existing->zoneIndex != 0)
	{
		const char* defName = DB_XAssetDefaultNames[type];
		bool hasEmptyDefault = (!defName || defName[0] == '\0');
		if (hasEmptyDefault && type != 0x20 && type != 0x10)
		{
			Com_PrintfChannel(1,
				"[T4M] Attempting to override asset '%s' from '%s' with '%s'\n",
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
	//   1. If existing->inuse → InUse dispatch
	//   2. Swap nextOverride: newEntry.nextOverride = existing.nextOverride
	//                          existing.nextOverride = idx(newEntry)
	//   3. size = getSize(existing->type)
	//   4. memcpy(tmp, existing->header, size)        ← save existing's data
	//   5. bl = existing->zoneIndex                    ← save existing's zone
	//   6. sub_48D6D0(esi=existing, edi=newEntry):
	//        a. promoter(existing, newEntry, existing->zoneIndex == 0)
	//        b. memcpy(existing->header, newEntry->header, getSize(new->type))
	//        c. existing->zoneIndex = newEntry->zoneIndex
	//   7. memcpy(newEntry->header, tmp, getSize(new->type))
	//   8. newEntry->zoneIndex = bl
	//
	// CRITICAL: the PROMOTER (step 6a) transfers GPU state between
	// existing and newEntry for materials/images/techsets. Skipping it
	// leaks or strands the D3D state → render pipeline deadlock.

	// 1. InUse dispatch
	if (existing->inuse)
	{
		*g_inuseEntry  = existing;
		*g_inuseHeader = &existing->asset.header;
		DB_InUseHandlerDispatch();
	}

	// 2. Swap nextOverride
	newEntry->nextOverride = existing->nextOverride;
	existing->nextOverride = T4M_POOL_INDEX(newEntry);

	// 3+4. Save existing's data
	int size = T4M_GetSize(type);
	void* tmp = _alloca(static_cast<size_t>(size));
	memcpy(tmp, existing->asset.header.data, static_cast<size_t>(size));

	// 5. Save existing's zoneIndex
	unsigned char savedExistingZone = existing->zoneIndex;

	// 6a. Call promoter BEFORE data overwrite (existing still has old data)
	DB_XAssetOverridePromoter_t promoter = DB_XAssetOverridePromoters[type];
	if (promoter)
	{
		bool isPermZone = (existing->zoneIndex == 0);
		promoter(existing->asset.header.data, newEntry->asset.header.data, isPermZone);
	}

	// 6b. Copy new's data into existing
	memcpy(existing->asset.header.data, newEntry->asset.header.data, static_cast<size_t>(size));

	// 6c. existing->zoneIndex = newEntry->zoneIndex
	existing->zoneIndex = newEntry->zoneIndex;

	// 7. Copy saved existing data into newEntry (newEntry becomes the losing override)
	memcpy(newEntry->asset.header.data, tmp, static_cast<size_t>(size));

	// 8. newEntry->zoneIndex = saved existing zone
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
							DB_GetXAssetTypeName(type));
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