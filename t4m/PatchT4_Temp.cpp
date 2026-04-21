// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose:   Temporary / experimental detour installer isolated
//            for easy enable/disable during debugging.
//
//   The experimental functions themselves live in T4.cpp. This
//   file only handles detour installation, so uncommenting a
//   single line here is all that is needed to flip an experiment
//   on for the next rebuild.
// ==========================================================

#include "StdInc.h"
#include "T4.h"

// WSAStartup / WSADATA used by the Com_Init_Inner reconstruction.  StdInc.h
// sets WIN32_LEAN_AND_MEAN, so winsock is not pulled in by <windows.h>.
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// timeGetTime — same include pattern as T4.cpp.
#include <Timeapi.h>
#pragma comment(lib, "winmm.lib")

// ==========================================================
// Faithful C++ reconstructions of 6 engine functions identified from
// CoDWaW LanFixed.exe.asm:
//
//   sub_6E9C00  Material_RegisterHandle
//   sub_6E9CB0  Material_InitDefault
//   sub_6D6AD0  R_Init
//   sub_644BE0  Com_PostInit_Video   ("CL_InitCGame" per user hypothesis)
//   sub_59CEB0  Com_Init_Inner       (Common init body, prints build banner)
//   sub_59D710  Com_Init_TryBlock    (SEH wrapper around sub_59CEB0)
//
// Out of scope:
//   sub_59A2C0  Com_Printf           (already exposed in T4.cpp)
//   sub_59F780  Dvar_SetFromDvar_f   (console cmd handler)
//
// IMPORTANT — T4M init constraint:
//   These reconstructions call Com_Printf / Com_Error etc.  DO NOT invoke
//   them from PatchT4_*() bodies executed via Sys_RunInit → PatchT4().
//   Safe to call only at runtime (post-Com_Init, hooks, cbuf).
// ==========================================================

// ---------------------------------------------------------------------------
// Forward-declare every engine symbol we dispatch to.  Everything is
// __cdecl unless noted.  Typedef/static-pointer pattern matches T4.cpp.
// ---------------------------------------------------------------------------
namespace Engine
{
	// ---- Com / Sys / Cbuf --------------------------------------------------
	typedef void(__cdecl* Com_Error_t)         (int code, const char* fmt, ...);
	typedef void(__cdecl* Com_PrintError_t)    (int channel, const char* fmt, ...); // sub_59A440
	typedef void(__cdecl* Com_Frame_EventLoop_t)(int run);                           // sub_59B0D0
	typedef void(__cdecl* Com_Fatal_t)         (const char* msg);                   // sub_5FE8C0
	typedef char* (__cdecl* Com_FormatMsg_t)     (const char* fmt, ...);              // sub_5F6D80
	typedef void(__cdecl* Sys_SyncDatabase_t)  (void);                               // sub_6F6CE0
	typedef void(__cdecl* Sys_WakeDatabase_t)  (void);                               // sub_6F6D60
	typedef int(__cdecl* Sys_Milliseconds_t)  (void);                               // sub_59A6F0
	typedef bool(__cdecl* Sys_SpawnRenderThread_t)(void);                            // sub_5A3110
	typedef bool(__cdecl* Sys_SpawnDBThread_t)  (void);                               // sub_5A31F0
	typedef void(__cdecl* Cbuf_AddText_t)      (const char* text, int localCli);     // sub_594200 (usercall)
	typedef void(__cdecl* Cbuf_Init_t)         (void);                               // sub_594440
	typedef void(__cdecl* Cbuf_Execute_t)      (int a, int b);                       // sub_594660
	typedef int(__cdecl* Cbuf_ExistsCmd_t)    (const char* name);                   // sub_594DB0 — Cmd_Exists
	typedef void(__cdecl* Com_Init_Pre_t)      (void);                               // sub_5941B0
	typedef void(__cdecl* Com_StartupVariables_t)(int skipCfg);                      // sub_595200
	typedef void(__cdecl* Com_ParseCmdline_t)  (int hard);                           // sub_59B1E0
	typedef void(__cdecl* Com_PostInit2_t)     (void);                               // sub_644E60
	typedef void(__cdecl* Dvar_Init_t)         (void);                               // sub_59C8B0
	typedef void(__cdecl* Dvar_Modified_t)     (void);                               // sub_59C7C0
	typedef void(__cdecl* Dvar_ForEach_t)      (void);                               // sub_59C840 — DvarDump
	typedef void* (__cdecl* Dvar_RegisterString_t)(const char* name, int flags, void* dflt, const char* desc); // sub_5EED90
	typedef void* (__cdecl* Dvar_RegisterBool2_t)(bool value, const char* name, int flags, const char* desc);  // sub_5EEE20
	typedef void* (__cdecl* Dvar_FindVar_t)      (const char* name);                   // sub_5EA880
	typedef void(__cdecl* Dvar_PushToConfig_t) (int zero);                           // sub_5EF550
	typedef void(__cdecl* Hunk_InitMemory_t)   (void);                               // sub_5F5480
	typedef void(__cdecl* Sys_InitModeList_fs_t)(void);                              // sub_5F6910
	typedef void(__cdecl* Sys_InitModeList_wd_t)(void);                              // sub_5F6960
	typedef void(__cdecl* FS_InitFilesystem_t) (void);                               // sub_477EC0
	typedef void(__cdecl* FS_Startup_t)        (void);                               // sub_475260
	typedef void(__cdecl* FS_InitFilesystem2_t)(void);                               // sub_4753A0
	typedef void(__cdecl* LocalizeInit_t)      (void);                               // sub_5DE600
	typedef void(__cdecl* Snd_Init_t)          (void);                               // sub_479DB0
	typedef void(__cdecl* Con_Init_t)          (void);                               // sub_604F70
	typedef void(__cdecl* Com_EventLoopInit_t) (void);                               // sub_580B80
	typedef void(__cdecl* Com_SetQueueIndex_t) (int v);                              // sub_596D10
	typedef void(__cdecl* Session_Init_t)      (void);                               // sub_5ACB10
	typedef void(__cdecl* DedicatedInit_t)     (void);                               // sub_644F40
	typedef void(__cdecl* Net_Init_t)          (void);                               // sub_5E3CA0
	typedef void(__cdecl* NetChan_Init_t)      (void);                               // sub_5E4650
	typedef void(__cdecl* HandleCfg_t)         (void);                               // sub_5FB560
	typedef void(__cdecl* DB_RegisterDvars_t)  (void);                               // sub_5FEDE0
	typedef void(__cdecl* DB_ReloadAssets_t)   (void);                               // sub_5FC640
	typedef void(__cdecl* Com_AllocEventsBuf_t)(void);                               // sub_677E90
	typedef void(__cdecl* SV_Init_t)           (int zero);                           // sub_68EF90
	typedef void(__cdecl* SL_Init_t)           (int zero);                           // sub_693C20
	typedef void(__cdecl* Scr_InitSystem_t)    (void);                               // sub_60C5C0
	typedef void(__cdecl* Input_Init_t)        (int evtType, int zero, void* buf, int zero2); // sub_68DE50
	typedef void(__cdecl* UI_Init_t)           (void);                               // sub_632B10
	typedef void(__cdecl* Com_InitEventSystem_t)(void);                              // sub_68D9F0
	typedef void(__cdecl* Cinema_Init_t)       (void);                               // sub_600CC0
	typedef void(__cdecl* Cinema_SetInitialized_t)(int v);                           // sub_600ED0
	typedef void(__cdecl* Com_InitSessionWork_t)(void);                              // sub_645860
	typedef void(__cdecl* Com_InitSessionClient_t)(void);                            // sub_647710
	typedef void(__cdecl* Com_StartTrace_t)    (void);                               // sub_59FE70
	typedef void(__cdecl* Com_InitBuildStr_t)  (void* out, const char* fmt, ...);    // sub_7AA926 — sprintf-like
	typedef void(__cdecl* Com_PrintBanner_t)   (void* ebx0, int one);                // sub_6B47C0 — console banner
	typedef void* (__cdecl* Mem_Memset_t)        (void* dst, int c, int n);            // sub_7AFF40
	typedef void(__cdecl* SetBranding_t)       (int mode);                           // sub_59AFA0
	typedef void(__cdecl* FS_LoadEventsFile_t) (void);                               // sub_59FE70 ? kept separate
	typedef int(__cdecl* SEH_TryHandler_t)    (void* exReg, int one);               // sub_7E1894 — CRT _except_handler
	typedef void(__cdecl* R_ResetToDefault_t)  (void);                               // sub_72D000
	typedef void(__cdecl* R_FrameTailCleanup_t)(void);                               // sub_70E3A0  (tail jmp)
	typedef void(__cdecl* R_BeginRegistration_t)(void);                              // sub_70E340
	typedef int(__cdecl* R_CreateSunOcclusion_t)(void);                             // sub_72BFA0
	typedef void* (__cdecl* R_LoadFont_SP_t)     (const char* name, int param);        // sub_6E8CE0
	typedef void* (__cdecl* R_LoadFont_MP_t)     (const char* name, int param);        // sub_6E8D80
	typedef void(__cdecl* R_RegisterDvars_t)   (void);                               // sub_707A20
	typedef void(__cdecl* R_InitShaderSys_t)   (void);                               // sub_725170
	typedef void(__cdecl* R_CopyPresentConsts_t)(void);                              // sub_6D69D0
	typedef void(__cdecl* R_InitImageSys_t)    (void);                               // sub_724AC0
	typedef void(__cdecl* R_InitDeviceAndWindow_t)(void);                            // sub_6D6880
	typedef void(__cdecl* R_InitRTPool_t)      (void);                               // sub_70E8F0
	typedef void(__cdecl* UI_LoadCinematic_t)  (void);                               // sub_71EFC0
	typedef void(__cdecl* Com_Printf_Header_t) (int channel, const char* fmt, ...);  // sub_59A2C0
	typedef void* (__cdecl* DB_FindXAssetHeader_full_t)(int type, const char* name, int createIfMissing, int user); // sub_48DA30
	typedef void(__cdecl* MatReg_Singlethread_t)(const char* name, int flags);       // sub_6E9B80

	// Com_PostInit_Video internals
	// sub_47A1C0 is an IDA __usercall (stack passes 4 floats, edi = dest).
	// We invoke it locally via a cdecl cast at the call site — no typedef needed here.
	typedef void(__cdecl* Com_InitSessionFinalize_t)(void);                          // sub_46FA40

	// ---- Resolved pointers -------------------------------------------------
	static const Com_Error_t                   Com_Error = (Com_Error_t)0x0059AC50;
	static const Com_PrintError_t              Com_PrintError = (Com_PrintError_t)0x0059A440;
	static const Com_Frame_EventLoop_t         Com_EnqueueEvent = (Com_Frame_EventLoop_t)0x0059B0D0;
	static const Com_Fatal_t                   Com_Fatal = (Com_Fatal_t)0x005FE8C0;
	static const Com_FormatMsg_t               Com_FormatMsg = (Com_FormatMsg_t)0x005F6D80;
	static const Sys_SyncDatabase_t            Sys_SyncDatabase = (Sys_SyncDatabase_t)0x006F6CE0;
	static const Sys_WakeDatabase_t            Sys_WakeDatabase = (Sys_WakeDatabase_t)0x006F6D60;
	static const Sys_Milliseconds_t            Sys_Milliseconds = (Sys_Milliseconds_t)0x0059A6F0;
	static const Sys_SpawnRenderThread_t       Sys_SpawnRenderThread = (Sys_SpawnRenderThread_t)0x005A3110;
	static const Sys_SpawnDBThread_t           Sys_SpawnDBThread = (Sys_SpawnDBThread_t)0x005A31F0;
	static const Cbuf_Init_t                   Cbuf_Init = (Cbuf_Init_t)0x00594440;
	static const Cbuf_Execute_t                Cbuf_Execute = (Cbuf_Execute_t)0x00594660;
	static const Cbuf_ExistsCmd_t              Cmd_Exists = (Cbuf_ExistsCmd_t)0x00594DB0;
	static const Com_Init_Pre_t                Com_Init_Pre = (Com_Init_Pre_t)0x005941B0;
	static const Com_StartupVariables_t        Com_StartupVariables = (Com_StartupVariables_t)0x00595200;
	static const Com_ParseCmdline_t            Com_ParseCmdline = (Com_ParseCmdline_t)0x0059B1E0;
	static const Com_PostInit2_t               Com_PostInit2 = (Com_PostInit2_t)0x00644E60;
	static const Dvar_Init_t                   Dvar_Init = (Dvar_Init_t)0x0059C8B0;
	static const Dvar_Modified_t               Dvar_Modified = (Dvar_Modified_t)0x0059C7C0;
	static const Dvar_ForEach_t                Dvar_ForEach = (Dvar_ForEach_t)0x0059C840;
	static const Dvar_RegisterString_t         Dvar_RegisterString = (Dvar_RegisterString_t)0x005EED90;
	static const Dvar_RegisterBool2_t          Dvar_RegisterBool2 = (Dvar_RegisterBool2_t)0x005EEE20;
	static const Dvar_FindVar_t                Dvar_FindVar = (Dvar_FindVar_t)0x005EA880;
	static const Dvar_PushToConfig_t           Dvar_PushToConfig = (Dvar_PushToConfig_t)0x005EF550;
	static const Hunk_InitMemory_t             Hunk_InitMemory = (Hunk_InitMemory_t)0x005F5480;
	static const Sys_InitModeList_fs_t         Sys_InitModeList_fs = (Sys_InitModeList_fs_t)0x005F6910;
	static const Sys_InitModeList_wd_t         Sys_InitModeList_wd = (Sys_InitModeList_wd_t)0x005F6960;
	static const FS_InitFilesystem_t           FS_InitFilesystem = (FS_InitFilesystem_t)0x00477EC0;
	static const FS_Startup_t                  FS_Startup = (FS_Startup_t)0x00475260;
	static const FS_InitFilesystem2_t          FS_InitFilesystem2 = (FS_InitFilesystem2_t)0x004753A0;
	static const LocalizeInit_t                LocalizeInit = (LocalizeInit_t)0x005DE600;
	static const Snd_Init_t                    Snd_Init = (Snd_Init_t)0x00479DB0;
	static const Con_Init_t                    Con_Init = (Con_Init_t)0x00604F70;
	static const Com_EventLoopInit_t           Com_EventLoopInit = (Com_EventLoopInit_t)0x00580B80;
	static const Com_SetQueueIndex_t           Com_SetQueueIndex = (Com_SetQueueIndex_t)0x00596D10;
	static const Session_Init_t                Session_Init = (Session_Init_t)0x005ACB10;
	static const DedicatedInit_t               DedicatedInit = (DedicatedInit_t)0x00644F40;
	static const Net_Init_t                    Net_Init = (Net_Init_t)0x005E3CA0;
	static const NetChan_Init_t                NetChan_Init = (NetChan_Init_t)0x005E4650;
	static const HandleCfg_t                   HandleCfg = (HandleCfg_t)0x005FB560;
	static const DB_RegisterDvars_t            DB_RegisterDvars = (DB_RegisterDvars_t)0x005FEDE0;
	static const DB_ReloadAssets_t             DB_ReloadAssets = (DB_ReloadAssets_t)0x005FC640;
	static const Com_AllocEventsBuf_t          Com_AllocEventsBuf = (Com_AllocEventsBuf_t)0x00677E90;
	static const SV_Init_t                     SV_Init = (SV_Init_t)0x0068EF90;
	static const SL_Init_t                     SL_Init = (SL_Init_t)0x00693C20;
	static const Scr_InitSystem_t              Scr_InitSystem = (Scr_InitSystem_t)0x0060C5C0;
	static const UI_Init_t                     UI_Init = (UI_Init_t)0x00632B10;
	static const Input_Init_t                  Input_Init = (Input_Init_t)0x0068DE50;
	static const Com_InitEventSystem_t         Com_InitEventSystem = (Com_InitEventSystem_t)0x0068D9F0;
	static const Cinema_Init_t                 Cinema_Init = (Cinema_Init_t)0x00600CC0;
	static const Cinema_SetInitialized_t       Cinema_SetInitialized = (Cinema_SetInitialized_t)0x00600ED0;
	static const Com_InitSessionWork_t         Com_InitSessionWork = (Com_InitSessionWork_t)0x00645860;
	static const Com_InitSessionClient_t       Com_InitSessionClient = (Com_InitSessionClient_t)0x00647710;
	static const Com_StartTrace_t              Com_StartTrace = (Com_StartTrace_t)0x0059FE70;
	static const Com_InitBuildStr_t            Com_InitBuildStr = (Com_InitBuildStr_t)0x007AA926;
	static const Com_PrintBanner_t             Com_PrintBanner = (Com_PrintBanner_t)0x006B47C0;
	static const Mem_Memset_t                  Mem_Memset = (Mem_Memset_t)0x007AFF40;
	static const SetBranding_t                 SetBranding = (SetBranding_t)0x0059AFA0;
	static const SEH_TryHandler_t              SEH_TryHandler = (SEH_TryHandler_t)0x007E1894;
	static const R_ResetToDefault_t            R_ResetToDefault = (R_ResetToDefault_t)0x0072D000;
	static const R_FrameTailCleanup_t          R_FrameTailCleanup = (R_FrameTailCleanup_t)0x0070E3A0;
	static const R_BeginRegistration_t         R_BeginRegistration = (R_BeginRegistration_t)0x0070E340;
	static const R_CreateSunOcclusion_t        R_CreateSunOcclusion = (R_CreateSunOcclusion_t)0x0072BFA0;
	static const R_LoadFont_SP_t               R_LoadFont_SP = (R_LoadFont_SP_t)0x006E8CE0;
	static const R_LoadFont_MP_t               R_LoadFont_MP = (R_LoadFont_MP_t)0x006E8D80;
	static const R_RegisterDvars_t             R_RegisterDvars = (R_RegisterDvars_t)0x00707A20;
	static const R_InitShaderSys_t             R_InitShaderSys = (R_InitShaderSys_t)0x00725170;
	static const R_CopyPresentConsts_t         R_CopyPresentConsts = (R_CopyPresentConsts_t)0x006D69D0;
	static const R_InitImageSys_t              R_InitImageSys = (R_InitImageSys_t)0x00724AC0;
	static const R_InitDeviceAndWindow_t       R_InitDeviceAndWindow = (R_InitDeviceAndWindow_t)0x006D6880;
	static const R_InitRTPool_t                R_InitRTPool = (R_InitRTPool_t)0x0070E8F0;
	static const UI_LoadCinematic_t            UI_LoadCinematic = (UI_LoadCinematic_t)0x0071EFC0;
	static const Com_Printf_Header_t           Com_Print = (Com_Printf_Header_t)0x0059A2C0;
	static const DB_FindXAssetHeader_full_t    DB_FindXAssetHeader4 = (DB_FindXAssetHeader_full_t)0x0048DA30;
	static const MatReg_Singlethread_t         Material_Register_ST = (MatReg_Singlethread_t)0x006E9B80;
	static const Com_InitSessionFinalize_t     Com_InitSessionFinalize = (Com_InitSessionFinalize_t)0x0046FA40;
}

// ---------------------------------------------------------------------------
// Raw globals referenced by the reconstructions.
// Each address is annotated with the ASM label from the disassembly.
// ---------------------------------------------------------------------------
namespace G
{
	// --- engine-wide ---------------------------------------------------------
	static dvar_t** const dvar_singlethreadRender = (dvar_t**)0x01F552FC; // dword_1F552FC
	static dvar_t** const dvar_developer = (dvar_t**)0x01F55288; // dword_1F55288
	static dvar_t** const dvar_dedicated = (dvar_t**)0x0212B2F4; // dword_212B2F4
	static dvar_t** const dvar_comLogVerbose = (dvar_t**)0x01F964B4; // dword_1F964B4
	static dvar_t** const dvar_comRecommendedSet = (dvar_t**)0x01F96490; // dword_1F96490 — stored cache
	static dvar_t** const dvar_debugCurves = (dvar_t**)0x021ACF28; // dword_21ACF28 — stored cache
	static dvar_t** const dvar_sv_running = (dvar_t**)0x01F96494; // dword_1F96494
	static void** const firstHunkTable = (void**)0x01F3FAB8; // unk_1F3FAB8 — zero'd, 0x200 bytes

	static DWORD* const    g_initStartMs = (DWORD*)0x022BEC34; // dword_22BEC34
	static DWORD* const    g_initHasStart = (DWORD*)0x04DE7054; // dword_4DE7054
	static BYTE* const    g_comInitDone = (BYTE*)0x00951A02; // byte_951A02
	static DWORD* const    g_hunkIndex = (DWORD*)0x0224FAE8; // dword_224FAE8
	static DWORD* const    g_hunkBaseRel = (DWORD*)0x0224FAE4; // dword_224FAE4
	static DWORD* const    g_hunkLimit = (DWORD*)0x0224FAEC; // dword_224FAEC
	static DWORD* const    g_hunkEntryTable = (DWORD*)0x0224FAF0; // ds:224FAF0h  (pairs of 8 bytes)
	static const char* const c_strInit = "$init";
	static DWORD* const    g_eventSysVer = (DWORD*)0x00488BE5C; // dword_488BE5C (= 0x10)
	static DWORD* const    g_eventSysPool = (DWORD*)0x00488BE60; // dword_488BE60 (= dword_1F3FA84)
	static DWORD* const    g_eventSysName = (DWORD*)0x00488BE40; // dword_488BE40 (= "impcount")
	static DWORD* const    g_queueSlot0 = (DWORD*)0x03058404; // dword_3058404
	static DWORD* const    g_cmdListHead = (DWORD*)0x01F416F4; // dword_1F416F4
	static DWORD* const    g_flagsX21ACF30 = (DWORD*)0x021ACF30; // dword_21ACF30
	static DWORD* const    g_debugCurvesFlag = (DWORD*)0x021ACF2C; // dword_21ACF2C
	static DWORD* const    g_shortVersionDvar = (DWORD*)0x01F552F8; // dword_1F552F8
	static DWORD* const    g_versionDvar = (DWORD*)0x01F552D8; // dword_1F552D8
	static float* const    g_1F5529C = (float*)0x01F5529C; // dword_1F5529C
	static DWORD* const    g_46E5110 = (DWORD*)0x046E5110; // dword_46E5110
	static DWORD* const    g_46E50B0_netUp = (DWORD*)0x046E50B0; // dword_46E50B0
	static DWORD* const    g_1F9648C_initMs = (DWORD*)0x01F9648C; // dword_1F9648C
	static DWORD* const    g_1F964B0_done = (DWORD*)0x01F964B0; // dword_1F964B0
	static DWORD* const    g_1F964B4_flag = (DWORD*)0x01F964B4; // dword_1F964B4
	static DWORD* const    g_ioRingBase = (DWORD*)0x0488E874; // unk_488E874 base of Input system ring
	static DWORD* const    g_ioRingEnd = (DWORD*)0x048AB374; // unk_48AB374 end
	static DWORD* const    g_defaultSfxBankCount = (DWORD*)0x01FF51C0; // dword_1FF51C0 — recommended worker hint
	static DWORD* const    g_smpWorkerThreads = (DWORD*)0x042B71A4; // dword_42B71A4 — "r_smp_worker_threads" dvar ptr
	static DWORD* const    g_48AE4D4_inCgame = (DWORD*)0x048AE4D4; // dword_48AE4D4
	static DWORD* const    g_48AE4D8 = (DWORD*)0x048AE4D8; // dword_48AE4D8
	static DWORD* const    g_lastErrorBuf = (DWORD*)0x01F95458; // byte_1F95458 — error text buffer

	// --- sub_644BE0 locals --------------------------------------------------
	static DWORD* const    g_18ECF8C = (DWORD*)0x018ECF8C; // dword_18ECF8C — connect dvar cached
	static BYTE* const    g_46E54F6 = (BYTE*)0x046E54F6; // byte_46E54F6 — SMP flag
	static BYTE* const    g_4DA90C4_connState = (BYTE*)0x04DA90C4; // byte_4DA90C4 — connection latched
	static DWORD* const    g_46E5688 = (DWORD*)0x046E5688; // dword_46E5688
	static DWORD* const    g_3BED828_mapRects = (DWORD*)0x03BED828; // dword_3BED828 — rect pool head (0x34 bytes)
	static DWORD* const    g_3BED830_mapW = (DWORD*)0x03BED830; // dword_3BED830 — map width
	static DWORD* const    g_3BED834_mapH = (DWORD*)0x03BED834; // dword_3BED834 — map height
	static BYTE* const    g_3BED85C_mapReady = (BYTE*)0x03BED85C; // byte_3BED85C
	static DWORD* const    g_4DA90B0_viewportSet = (DWORD*)0x04DA90B0; // unk_4DA90B0 — 13 dword viewport
	static DWORD* const    g_4DA90B8 = (DWORD*)0x04DA90B8; // dword_4DA90B8
	static DWORD* const    g_9573A8_viewport1 = (DWORD*)0x009573A8; // unk_9573A8
	static DWORD* const    g_957318_viewport2 = (DWORD*)0x00957318; // dword_957318
	static DWORD* const    g_957360_viewport3 = (DWORD*)0x00957360; // dword_957360
	static DWORD* const    g_4DA8F4C_whiteMat = (DWORD*)0x04DA8F4C; // dword_4DA8F4C
	static DWORD* const    g_4DA8F50_consoleMat = (DWORD*)0x04DA8F50; // dword_4DA8F50
	static DWORD* const    g_4DA8F54_consoleFont = (DWORD*)0x04DA8F54; // dword_4DA8F54
	static DWORD* const    g_8DD570 = (DWORD*)0x008DD570; // dword_8DD570
	static DWORD* const    g_951A14 = (DWORD*)0x00951A14; // dword_951A14
	static DWORD* const    g_951A18_msaaCfg = (DWORD*)0x00951A18; // dword_951A18 — ds:dword_83CE2C
	static DWORD* const    g_951A1C_ready = (DWORD*)0x00951A1C; // dword_951A1C
	static DWORD* const    g_1FF5018 = (DWORD*)0x01FF5018; // dword_1FF5018 — zero'd for memset
	static DWORD* const    g_1FF5014 = (DWORD*)0x01FF5014; // dword_1FF5014

	static const float     k_83CE2C = 1.0f; // placeholder: ds:dword_83CE2C constant used for msaa float

	// --- R_Init locals ------------------------------------------------------
	static DWORD* const    g_3BF6768_sunQuery = (DWORD*)0x03BF6768; // dword_3BF6768
	static DWORD* const    g_3DCB4DC_fontDefault = (DWORD*)0x03DCB4DC; // qword_3DCB4D8+4 (high dword of qword)
}

// ---------------------------------------------------------------------------
// Struct layout for Material_InitDefault's table at 0x82AD60..0x82AFA0
// ---------------------------------------------------------------------------
struct DefaultMaterialEntry
{
	const char* name;     // [esi+0]
	void** destPtr;  // [esi+4]
};

static const uintptr_t ADDR_defaultMaterialTable = 0x0082AD60;
static const uintptr_t ADDR_defaultMaterialTable_end = 0x0082AFA0;

// ===========================================================================
// 1. Material_RegisterHandle  —  sub_6E9C00  (0x6E9C00)
// ---------------------------------------------------------------------------
// ASM:
//    mov   eax, [esp+4]
//    push  0FFFFFFFFh          ; user=-1
//    push  1                   ; createIfMissing=true
//    push  eax                 ; name
//    push  6                   ; ASSET_TYPE_MATERIAL
//    call  sub_48DA30          ; DB_FindXAssetHeader
//    add   esp, 10h
//    retn
// ---------------------------------------------------------------------------
extern "C" void* __cdecl Material_RegisterHandle(const char* name)
{
	return Engine::DB_FindXAssetHeader4(/*ASSET_TYPE_MATERIAL=*/6, name, /*create=*/1, /*user=*/-1);
}

// ===========================================================================
// 2. Material_InitDefault  —  sub_6E9CB0  (0x6E9CB0)
// ---------------------------------------------------------------------------
// ASM walks the {name, Material**} table at 0x82AD60..0x82AFA0, picking
// Material_RegisterHandle (singlethread) or sub_6E9B80 (multithread)
// based on the dvar at 0x1F552FC.  Com_Error if the material is missing.
// ===========================================================================
extern "C" void __cdecl Material_InitDefault()
{
	DefaultMaterialEntry* e = (DefaultMaterialEntry*)ADDR_defaultMaterialTable;
	DefaultMaterialEntry* end = (DefaultMaterialEntry*)ADDR_defaultMaterialTable_end;

	for (; e < end; ++e)
	{
		const dvar_t* dvSt = *G::dvar_singlethreadRender;
		void* mat;

		if (dvSt && dvSt->current.boolean)
			mat = Material_RegisterHandle(e->name);     // sub_6E9C00 path
		else
		{
			// sub_6E9B80 takes (name, 0) per ASM (`push 0; push ecx; call eax`).
			Engine::Material_Register_ST(e->name, 0);
			// sub_6E9B80 returns the material in EAX too (same ABI).
			// We can't capture EAX via the C++ typedef above (void return) —
			// so drop to a call-through cast that returns void*.
			typedef void* (__cdecl* MatRegPair_t)(const char*, int);
			mat = ((MatRegPair_t)Engine::Material_Register_ST)(e->name, 0);
		}

		*e->destPtr = mat;

		if (*e->destPtr == nullptr)
			Engine::Com_Error(0, "Could not find material '%s'", e->name);
	}
}

// ===========================================================================
// 3. R_Init  —  sub_6D6AD0  (0x6D6AD0)
// ---------------------------------------------------------------------------
// Top-level renderer init. Called from sub_644BE0+0x82.  Opens the banner,
// sets a local "fullscreen" bool (vanilla hard-codes 1), initialises mode
// lists, subsystem registers, creates window/device, loads default font,
// tries to build the sun-sprite occlusion query.
// ===========================================================================
extern "C" void __cdecl R_Init()
{
	Engine::Com_Print(8, "----- R_Init -----\n");

	// var_4 is a local word; asm sets byte[var_4] = 1, byte[var_4+1] = 0
	// then compares the word with 1 → taken branch always (fullscreen path).
	const bool fullscreen = true;
	if (fullscreen)
		Engine::Sys_InitModeList_fs();    // sub_5F6910
	else
		Engine::Sys_InitModeList_wd();    // sub_5F6960

	Engine::R_RegisterDvars();            // sub_707A20
	Engine::R_InitShaderSys();            // sub_725170
	Engine::R_CopyPresentConsts();        // sub_6D69D0
	Engine::R_InitImageSys();             // sub_724AC0
	Engine::R_InitDeviceAndWindow();      // sub_6D6880

	// Load default UI font — SP vs MP loader dispatch by dvar_singlethreadRender
	const dvar_t* dvSt = *G::dvar_singlethreadRender;
	void* fontDefault = (dvSt && dvSt->current.boolean)
		? Engine::R_LoadFont_MP("fonts/smalldevfont", 1)   // sub_6E8D80
		: Engine::R_LoadFont_SP("fonts/smalldevfont", 1);  // sub_6E8CE0
	*G::g_3DCB4DC_fontDefault = (DWORD)fontDefault;        // qword_3DCB4D8 high dword

	Engine::R_InitRTPool();               // sub_70E8F0

	// Try to create the sun-sprite occlusion query.  sub_70E340 returns a
	// barrier/flush handle in EAX (preserved in ESI), then sub_72BFA0 runs.
	void* gpuBarrier = (void*)(uintptr_t)1;
	Engine::R_BeginRegistration();        // sub_70E340 — sets esi = eax
	// Note: in asm we have `call sub_70E340; mov esi, eax; call sub_72BFA0`.
	// R_BeginRegistration is void() in our typedef; we can't recover esi.
	// We inline via the engine pointer instead:
	{
		typedef void* (__cdecl* R_Barrier_t)(void);
		R_Barrier_t R_Barrier = (R_Barrier_t)0x0070E340;
		gpuBarrier = R_Barrier();
	}

	int sunOK = Engine::R_CreateSunOcclusion(); // sub_72BFA0
	*G::g_3BF6768_sunQuery = (DWORD)sunOK;

	if (!sunOK)
	{
		Engine::Com_Print(8, "Sun sprite occlusion query calibration failed.\n");
		Engine::R_ResetToDefault();       // sub_72D000
	}

	// If the barrier handle is non-null, tail-call R_FrameTailCleanup.
	if (gpuBarrier != nullptr)
		Engine::R_FrameTailCleanup();     // jmp sub_70E3A0 (tail-call)
}

// ===========================================================================
// 4. Com_PostInit_Video  (user: "CL_InitCGame")  —  sub_644BE0  (0x644BE0)
// ---------------------------------------------------------------------------
// Registers the "g_connectpaths" bool dvar twice (latched connection-state
// pattern), calls R_Init, memcpy'es a 52-byte rect/float block from the
// map-config pool into three viewport slots, registers the "white" and
// "console" default materials + the console font, and finally tail-calls
// sub_46FA40 (Com_InitSessionFinalize) after zero-initialising a 0x48-byte
// block at dword_1FF5018.
// ===========================================================================
extern "C" void __cdecl Com_PostInit_Video()
{
	// --- Phase 1: register "g_connectpaths" (first pass) -------------------
	*G::g_48AE4D4_inCgame = 1;
	dvar_t* dvConnect1 = (dvar_t*)Engine::Dvar_RegisterBool2(
		/*value=*/false, "g_connectpaths", /*flags=*/2, "Connect paths");

	*G::g_18ECF8C = (DWORD)dvConnect1;

	// ASM: `cmp [eax+0x10], esi` with esi=2 → compares current.integer to 2.
	// Vanilla logic is dead for a bool dvar (value is 0/1) but we mirror it.
	if (dvConnect1 && dvConnect1->current.integer == 2)
		*G::g_46E54F6 = 1;

	// --- Phase 2: register a second time, this time latching state --------
	*G::g_4DA90C4_connState = 0;
	BYTE latchedFlag = 0;
	{
		dvar_t* dvPrior = *(dvar_t**)0x03BFD478; // dword_3BFD478 — prior dvar entry
		if (dvPrior)
			latchedFlag = (BYTE)dvPrior->current.boolean;
	}
	*G::g_4DA90C4_connState = latchedFlag;

	dvar_t* dvConnect2 = (dvar_t*)Engine::Dvar_RegisterBool2(
		/*value=*/false, "g_connectpaths", /*flags=*/2, "Connect paths");
	*G::g_18ECF8C = (DWORD)dvConnect2;
	*G::g_4DA90C4_connState = (BYTE)(dvConnect2 && dvConnect2->current.integer == 2);

	// --- Phase 3: R_Init ---------------------------------------------------
	R_Init();

	if (*G::g_4DA90C4_connState)
		*G::g_46E5688 = 1;

	// --- Phase 4: copy map/view rect block ---------------------------------
	//   13 dwords from dword_3BED828 → dword_4DA90B0 (rep movsd ecx=0Dh)
	memcpy(G::g_4DA90B0_viewportSet, G::g_3BED828_mapRects, 13 * sizeof(DWORD));

	// --- Phase 5: build 3 float viewports from map width/height ------------
	//   var_4  = eax = g_3BED830 (mapW int)
	//   var_8  = ecx = g_3BED834 (mapH int)
	//   fild var_8; fst  var_C (mapH float)
	//   fild var_4; fst  var_8 (mapW float)
	//   then: for each of g_9573A8 / g_957318 / g_957360 push {var_24=0,
	//         var_20=0, var_1C=mapW, var_18=mapH} and call sub_47A1C0(edi=slot)
	//
	// We can't exactly rebuild sub_47A1C0's __usercall without its body; we
	// do the moral-equivalent reset: width/height into its first two floats,
	// zero the rest.  (sub_47A1C0 is called 3× with the same stack.)
	*G::g_3BED85C_mapReady = 1;

	struct ViewRect { float a, b, w, h; };
	ViewRect vr;
	vr.a = 0.0f;
	vr.b = 0.0f;
	vr.w = (float)*G::g_3BED830_mapW;
	vr.h = (float)*G::g_3BED834_mapH;

	{
		typedef void(__cdecl* UI_Register_t)(void* slot, float a, float b, float w, float h);
		UI_Register_t UI_Register = (UI_Register_t)0x0047A1C0;
		UI_Register(G::g_9573A8_viewport1, vr.a, vr.b, vr.w, vr.h);
		UI_Register(G::g_957318_viewport2, vr.a, vr.b, vr.w, vr.h);
		UI_Register(G::g_957360_viewport3, vr.a, vr.b, vr.w, vr.h);
	}

	// --- Phase 6: register default "white" + "console" materials ----------
	const dvar_t* dvSt = *G::dvar_singlethreadRender;

	typedef void* (__cdecl* MatReg_t)(const char* name, int flags);
	MatReg_t MatRegister = (dvSt && dvSt->current.boolean)
		? (MatReg_t)0x006E9C00    // sub_6E9C00 — singlethread (MatReg takes (name, dummy))
		: (MatReg_t)0x006E9B80;   // sub_6E9B80 — multithread

	*G::g_4DA8F4C_whiteMat = (DWORD)MatRegister("white", 3);
	*G::g_4DA8F50_consoleMat = (DWORD)MatRegister("console", 3);

	// --- Phase 7: register the console font -------------------------------
	{
		typedef void* (__cdecl* FontReg_t)(const char* name, int flags);
		FontReg_t FontRegister = (dvSt && dvSt->current.boolean)
			? (FontReg_t)0x006E8D80   // sub_6E8D80 — MP
			: (FontReg_t)0x006E8CE0;  // sub_6E8CE0 — SP
		*G::g_4DA8F54_consoleFont = (DWORD)FontRegister("fonts/consoleFont", 3);
	}

	// --- Phase 8: view-rect / msaa globals ---------------------------------
	DWORD halfHeight = *G::g_4DA90B8 + 0xFFFFFFE0u; // lea eax, [dword_4DA90B8-0x20]
	*G::g_8DD570 = halfHeight;
	*G::g_951A14 = halfHeight;
	*(float*)G::g_951A18_msaaCfg = G::k_83CE2C;   // ds:dword_83CE2C (float const)
	*G::g_951A1C_ready = 1;

	// --- Phase 9: memset(dword_1FF5018, 0, 0x48) --------------------------
	Engine::Mem_Memset(G::g_1FF5018, 0, 0x48);
	*G::g_1FF5014 = 0;

	// --- Phase 10: tail-call Com_InitSessionFinalize -----------------------
	Engine::Com_InitSessionFinalize();     // jmp sub_46FA40
}

// Convenience alias — the user labelled this "CL_InitCGame".
extern "C" void __cdecl CL_InitCGame() { Com_PostInit_Video(); }

// ===========================================================================
// 5. Com_Init_Inner  —  sub_59CEB0  (0x59CEB0)
// ---------------------------------------------------------------------------
// Called from sub_59D710 (SEH wrapper).  Very long: prints the build banner,
// initialises every Common/Net/Sound/FS/SV/SL subsystem, spins the SMP and
// database worker threads, registers the crash-reporting Cmd_AddCommand
// handlers ("error", "crash", "freeze", "assert", "quit", "writeconfig",
// "writedefaults"), runs the end-of-init timestamp print, and finally
// tail-calls Com_StartTrace(6) to open the trace/event session.
//
// Reconstruction strategy:  mirror the ASM flow block-by-block, calling
// engine sub_* via Engine::* typedefs.  We keep the vanilla control flow
// including the dedicated-server branch.
// ===========================================================================
extern "C" void __cdecl Com_Init_Inner(void* cmdLineTail)
{
	// ---- Build banner (10h = channel "init" to Com_Printf) ----------------
	char banner[256];
	Engine::Com_InitBuildStr(
		/*out*/ (void*)0x04E55B20,
		"%s.%s.%d CL(%s) %s %s",
		"1", "7", 0x4EF, "350073", "JADAMS2", "Thu Oct 29 15:43:55 2009");
	(void)banner;

	Engine::Com_Print(0x10, "%s %s build %s %s\n",
		"COD_WaW", "win-x86", "7", "Oct 29 2009");

	// ---- Preliminaries ----------------------------------------------------
	Engine::SetBranding((int)(intptr_t)cmdLineTail);    // sub_59AFA0 ((cmdLineTail))
	Engine::Com_InitEventSystem();                       // sub_68D9F0 (eax = 0 then 1)
	Engine::Com_InitEventSystem();

	// ---- Fullscreen/windowed mode list -----------------------------------
	const bool fullscreen = true;
	if (fullscreen) Engine::Sys_InitModeList_fs();       // sub_5F6910
	else            Engine::Sys_InitModeList_wd();       // sub_5F6960

	Engine::Com_Init_Pre();                              // sub_5941B0
	Engine::Com_StartupVariables(0);                     // sub_595200
	Engine::Com_EnqueueEvent(0);                         // sub_59B0D0
	Engine::Dvar_Init();                                 // sub_59C8B0

	// ---- Dedicated-only $init hunk block ---------------------------------
	if (*G::dvar_singlethreadRender
		&& (*G::dvar_singlethreadRender)->current.boolean)
	{
		Engine::Hunk_InitMemory();                       // sub_5F5480
		Engine::Com_Print(7, "begin $init\n");

		*G::g_comInitDone = 1;
		if (*G::g_initHasStart == 0)
		{
			*G::g_initStartMs = timeGetTime();
			*G::g_initHasStart = 1;
		}

		// Push $init marker into the hunk event ring:
		DWORD now = timeGetTime() - *G::g_initStartMs;
		// (storing the delta + "$init" into the hunk ring)
		*G::g_hunkBaseRel = (DWORD)(uintptr_t)G::c_strInit;
		DWORD idx = *G::g_hunkIndex;
		*G::g_hunkIndex = idx + 1;
		DWORD* slot = &G::g_hunkEntryTable[idx * 2];
		slot[0] = (DWORD)(uintptr_t)G::c_strInit;
		slot[1] = *G::g_hunkLimit;
		(void)now;
	}

	// ---- Database worker thread (dedicated only) --------------------------
	if (*G::dvar_singlethreadRender
		&& (*G::dvar_singlethreadRender)->current.boolean)
	{
		if (!Engine::Sys_SpawnDBThread())
			Engine::Com_Fatal("Failed to create database thread");
	}

	// ---- FS / Localize / Sound / Console ---------------------------------
	Engine::FS_InitFilesystem();                         // sub_477EC0
	Engine::FS_Startup();                                // sub_475260
	Engine::FS_InitFilesystem2();                        // sub_4753A0
	Engine::LocalizeInit();                              // sub_5DE600
	Engine::Snd_Init();                                  // sub_479DB0
	Engine::Con_Init();                                  // sub_604F70

	// ---- Zero a 0x200 event pool, record event-sys name/version ----------
	Engine::Mem_Memset(G::firstHunkTable, 0, 0x200);
	*G::g_eventSysPool = (DWORD)(uintptr_t)(DWORD*)0x01F3FA84;
	*G::g_eventSysVer = 0x10;
	*G::g_eventSysName = (DWORD)(uintptr_t)"impcount";

	// ---- Com_EventLoopInit + slot search ---------------------------------
	Engine::Com_EventLoopInit();                         // sub_580B80
	Engine::Com_SetQueueIndex(0);                        // sub_596D10

	int slot = -1;
	for (int i = 0; i < 1; ++i)
	{
		if (G::g_queueSlot0[i] == 0) { slot = i; break; }
	}

	*(BYTE*)0x009BD457 = 1;                             // byte_9BD457
	Engine::Cbuf_Execute(0, slot);                       // sub_594660
	*(BYTE*)0x009BD457 = 0;
	Engine::Cbuf_Init();                                 // sub_594440

	if (*G::g_flagsX21ACF30 & 0x20)
		Engine::Dvar_Init();                             // sub_59C8B0 (re-init if flag)

	// ---- Register com_recommendedSet dvar ---------------------------------
	*G::dvar_comRecommendedSet = (dvar_t*)Engine::Dvar_RegisterBool2(
		false, "com_recommendedSet", 0, "Use recommended settings");

	Engine::Dvar_Modified();                             // sub_59C7C0
	Engine::Com_EnqueueEvent(0);

	// ---- Session init (non-dedicated path) --------------------------------
	if (!(*G::dvar_singlethreadRender
		&& (*G::dvar_singlethreadRender)->current.boolean))
		Engine::Session_Init();                           // sub_5ACB10

	if ((*G::dvar_dedicated)->current.boolean)
		Engine::DedicatedInit();                          // sub_644F40

	// ---- Net/NetChan ------------------------------------------------------
	Engine::Net_Init();                                   // sub_5E3CA0
	Engine::NetChan_Init();                               // sub_5E4650
	*G::g_1F5529C = *(float*)0x00826A4C;                 // ds:dword_826A4C constant
	*G::g_flagsX21ACF30 &= ~1u;
	Engine::HandleCfg();                                  // sub_5FB560

	// ---- Developer-only crash/quit/writeconfig cmds ----------------------
	if ((*G::dvar_developer)->current.boolean)
	{
		// A series of vanilla Cmd_AddCommand calls — we funnel through the
		// engine's Cmd_AddCommand equivalent when the sub_594DB0 check says
		// "not yet registered".  The ASM inlines both branches; we can use
		// the in-tree T4M helper instead.
		struct Reg { const char* name; xcommand_t func; };
		Reg entries[] = {
			{ "error",         (xcommand_t)0x0059B700 },
			{ "crash",         (xcommand_t)0x0059B7E0 },
			{ "freeze",        (xcommand_t)0x0059B730 },
			{ "assert",        (xcommand_t)0x004013E0 }, // nullsub_3
			{ "quit",          (xcommand_t)0x0059ADC0 },
			{ "writeconfig",   (xcommand_t)0x0059D950 },
			{ "writedefaults", (xcommand_t)0x0059D9D0 },
		};
		for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i)
		{
			if (Engine::Cmd_Exists(entries[i].name))
				Engine::Com_Print(0x10, "Cmd_AddCommand: %s already defined\n", entries[i].name);
			else
				Cmd_AddCommand(entries[i].name, entries[i].func);
		}
	}

	// ---- Register "version" + "shortversion" dvars -----------------------
	char* fmtVersion = Engine::Com_FormatMsg(
		"%s %s build %s %s", "Call of Duty", "COD_WaW",
		(const char*)0x04E55B20, "win-x86");
	*G::g_versionDvar = (DWORD)Engine::Dvar_RegisterString(
		"version", 7, fmtVersion, "Game version");
	Engine::Dvar_PushToConfig(0);

	*G::g_shortVersionDvar = (DWORD)Engine::Dvar_RegisterString(
		"shortversion", 7, (void*)"1", "Short game version");
	Engine::HandleCfg();                                  // sub_5FEE60 in asm

	// ---- Seed PRNG from perf counter -------------------------------------
	LARGE_INTEGER perf;
	QueryPerformanceCounter(&perf);
	{
		typedef void(__cdecl* Seed_t)(DWORD seed);
		Seed_t Seed = (Seed_t)0x00677E90;
		Seed(perf.LowPart);
	}

	// ---- SV_Init pairs ----------------------------------------------------
	Engine::SV_Init(0);                                   // (esi=0x5FFF, edi=0)
	Engine::SV_Init(0);                                   // (esi=0x15FFE, edi=0x6000)

	Engine::SL_Init(0);                                   // sub_693C20
	Engine::Dvar_ForEach();                               // sub_59C840
	Engine::Scr_InitSystem();                             // sub_60C5C0

	// ---- Input_Init with a 32-byte cleared struct on stack ---------------
	{
		BYTE inputBuf[0x20] = { 0 };
		Engine::Input_Init(0x11, 0, inputBuf, 0);         // sub_68DE50
		*G::g_46E5110 = 0;
	}

	Engine::UI_Init();                                    // sub_632B10

	// ---- Winsock ----------------------------------------------------------
	WSADATA wsa;
	int wsaRc = WSAStartup(0x0101, &wsa);
	if (wsaRc != 0)
		Engine::Com_PrintError(0x10, "WARNING: Winsock initialization failed, returned %d\n", wsaRc);
	else
	{
		Engine::Com_Print(0x10, "Winsock Initialized\n");
		*G::g_46E50B0_netUp = 1;
		Engine::Cinema_Init();                            // sub_600CC0
		Engine::Cinema_SetInitialized(1);                 // sub_600ED0 (eax=1)
	}

	// ---- debugCurves dvar -------------------------------------------------
	*G::dvar_debugCurves = (dvar_t*)Engine::Dvar_RegisterBool2(
		false, "debugCurves", 0, "Draw active curves.");
	*G::g_debugCurvesFlag = 0;

	// ---- Zero IO ring slots every 0xE58 bytes ----------------------------
	for (DWORD* p = G::g_ioRingBase; p < G::g_ioRingEnd; p = (DWORD*)((BYTE*)p + 0xE58))
	{
		Engine::Dvar_FindVar((const char*)((BYTE*)p - 0xE54));
		*p = 0; // asm does `mov [esi], edi` with edi = running index; we zero
	}

	// ---- Dedicated server bypasses session client setup ------------------
	if ((*G::dvar_dedicated)->current.boolean)
	{
		*(BYTE*)(((DWORD)*G::dvar_dedicated) + 0x0B) = 0;
		Engine::Com_InitSessionWork();                    // sub_645860
	}
	else
	{
		*(BYTE*)(((DWORD)*G::dvar_dedicated) + 0x0B) = 0;
		Engine::Com_InitSessionWork();                    // sub_645860
		Engine::Com_InitSessionClient();                  // sub_647710
	}

	// ---- Init timestamp baseline -----------------------------------------
	if (*G::g_initHasStart == 0)
	{
		*G::g_initStartMs = timeGetTime();
		*G::g_initHasStart = 1;
	}
	*G::g_1F9648C_initMs = timeGetTime() - *G::g_initStartMs;
	Engine::Com_EnqueueEvent(0);

	// ---- Dedicated path shortcut (skip SMP banner) ------------------------
	if ((*G::dvar_dedicated)->current.boolean)
		goto finish_banner;

	// ---- Worker-threads recommendation -----------------------------------
	{
		DWORD rec = *G::g_defaultSfxBankCount;
		DWORD workers;
		if (rec <= 4)      workers = 2;
		else if (rec <= 10) workers = rec - 2;
		else                workers = 8;

		Engine::Com_Print(0x10, "%s %d\n",
			"Number of worker threads", workers);

		*G::g_smpWorkerThreads = (DWORD)Engine::Dvar_RegisterString(
			"r_smp_worker_threads", 5, (void*)(uintptr_t)workers, (const char*)2);
	}

	Engine::Com_Print(8, "Trying SMP acceleration...\n");
	if (!Engine::Sys_SpawnRenderThread())
		Engine::Com_Error(0, "Failed to create render thread");

	Engine::UI_LoadCinematic();                           // sub_71EFC0
	Engine::Com_Print(8, "...succeeded.\n");

	// ---- Video post-init ---------------------------------------------------
	Com_PostInit_Video();                                 // sub_644BE0
	*G::g_48AE4D8 = 1;
	Engine::Com_PrintBanner((void*)1, 1);                // sub_6B47C0

finish_banner:
	Engine::DB_RegisterDvars();                            // sub_5FEDE0
	Engine::DB_ReloadAssets();                             // sub_5FC640

	// ---- Treyarch cinematic splash (unless dedicated / sv_running) -------
	if (!(*G::dvar_dedicated)->current.boolean
		&& !(*G::dvar_sv_running)->current.boolean)
	{
		typedef void(__cdecl* Cbuf_AddText_uc_t)(const char* text, int localCli);
		Cbuf_AddText_uc_t Cbuf_Add = (Cbuf_AddText_uc_t)0x00594200;
		Cbuf_Add("cinematic Treyarch\n", 0);
	}

	// ---- $init end marker (dedicated only) -------------------------------
	if (*G::dvar_singlethreadRender
		&& (*G::dvar_singlethreadRender)->current.boolean)
	{
		if (*G::g_initHasStart == 0)
		{
			*G::g_initStartMs = timeGetTime();
			*G::g_initHasStart = 1;
		}
		DWORD end = timeGetTime() - *G::g_initStartMs;
		Engine::Com_Print(0x10, "end $init %d ms\n", end);
	}

	// ---- Final banner + trace session ------------------------------------
	Engine::Com_Print(0x10, "--- Common Initialization Complete ---\n");
	*G::g_1F964B0_done = 1;

	{
		typedef void(__cdecl* Com_FS_t)(int zero, int six);
		Com_FS_t Com_FS = (Com_FS_t)0x0059FE70;
		Com_FS(0, 6);                                      // sub_59FE70 — tail call
	}
}

// ===========================================================================
// 6. Com_Init_TryBlock  —  sub_59D710  (0x59D710)
// ---------------------------------------------------------------------------
// __try/__except wrapper around Com_Init_Inner.  On exception: prints the
// fatal "Error during initialization:\n%s\n" via Com_FormatMsg/Com_Fatal,
// using the buffer at 0x1F95458.  On success: runs Sys_SyncDatabase,
// Com_PostInit2 (sub_644E60) and Sys_WakeDatabase.
// The SEH scope uses CRT _except_handler (sub_7E1894 at fs:[0x2C]).
// ===========================================================================
extern "C" void __cdecl Com_Init_TryBlock(void* cmdLineTail)
{
	__try
	{
		Com_Init_Inner(cmdLineTail);

		// Successful path post-ops — in vanilla these run *inside* the __try
		// but only when sub_7E1894 confirms the scope is still unwound OK.
		if (!(*G::dvar_singlethreadRender
			&& (*G::dvar_singlethreadRender)->current.boolean))
			Engine::Sys_Milliseconds();                   // sub_59A6F0

		if (!(*G::dvar_dedicated)->current.boolean)
		{
			if (*G::g_48AE4D4_inCgame == 0)
				Com_PostInit_Video();                     // sub_644BE0
			Engine::Sys_SyncDatabase();                   // sub_6F6CE0
			Engine::Com_PostInit2();                      // sub_644E60
			Engine::Sys_WakeDatabase();                   // sub_6F6D60
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// Match the vanilla fatal-error path.
		char* msg = Engine::Com_FormatMsg(
			"Error during initialization:\n%s\n",
			(const char*)G::g_lastErrorBuf);
		Engine::Com_Fatal(msg);
	}
}

// ---------------------------------------------------------------------------
// No PatchT4_Temp() init entry is exported.
// These wrappers run the full vanilla init path — they invoke Com_Printf,
// Com_Error, etc., which means they MUST NOT be called from Sys_RunInit.
// ---------------------------------------------------------------------------

// =====================================================================
// PatchT4_Temp — installs experimental / temporary detours.
//
// Not called by default. To activate, add `PatchT4_Temp();` inside
// PatchT4() (PatchT4.cpp) and uncomment the detour line(s) below.
// =====================================================================
void PatchT4_Temp()
{
	// Full C++ reconstruction of DB_FindXAssetHeader (sub_48DA30) with
	// T4M fix: zoneIndex==0 entries return immediately instead of waiting
	// forever on g_dbWorkerEvent for a promotion that may never come.
	//
	// DISABLED — suspected of causing a d3d9.retry hang during R_Init
	// (Material_InitDefault → Material_RegisterHandle → DB_FindXAssetHeader).
	// Re-enable to test fixes.
	//
	// Detours::X86::DetourFunction((uintptr_t)0x0048DA30, (uintptr_t)&T4_DB_FindXAssetHeader, Detours::X86Option::USE_JUMP);
}
