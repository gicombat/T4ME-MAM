// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose:   Temporary / experimental detour and recreation of original function
//            for easy enable/disable during debugging.
//
//   The experimental functions themselves live in T4.cpp. This
//   file only handles detour installation, so uncommenting a
//   single line here is all that is needed to flip an experiment
//   on for the next rebuild.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>

// Keep in sync with PatchT4MemoryLimits.cpp definition.
#define NEW_MAX_WEAPONS 148

// WSAStartup / WSADATA used by the Com_Init_Inner reconstruction.  StdInc.h
// sets WIN32_LEAN_AND_MEAN, so winsock is not pulled in by <windows.h>.
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// timeGetTime — same include pattern as T4.cpp.
#include <Timeapi.h>
#pragma comment(lib, "winmm.lib")

// fopen_s / fprintf / vfprintf for the meld diagnostic logger.
#include <cstdio>
#include <cstdarg>

// bg_weaponDefs is the relocated weapon-defs pointer table (defined in
// PatchT4Script.cpp, updated to the new heap by SetupBgWeaponDefsTable in
// PatchT4MemoryLimits.cpp). We dereference [bg_weaponDefs[idx] + 0] to get
// the weapon's internal name for diagnostic logs.
extern T4::WeaponDef** bg_weaponDefs;
namespace T4M { extern void** g_dword8F44A8; extern void** g_dword35D0DC8; }

// cg_weaponInfo is the relocated CG-side per-weapon table (see
// SetupCgWeaponInfoTable in PatchT4MemoryLimits.cpp). Used by the
// CG_RegisterWeapon reconstruction at the bottom of this file.
namespace T4M { extern BYTE* cg_weaponInfo; }

// Forward declaration : T4_Reconstructed::CG_RegisterWeapon is defined at
// the bottom of this file but referenced inside PatchT4_Temp() for the
// safetyhook detour install.
namespace T4_Reconstructed {
    extern "C" void __cdecl CG_RegisterWeapon(void* arg_0_ctx, int weapIdx);
}

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
	T4::Com_Printf(8, "----- R_Init -----\n");

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
		T4::Com_Printf(8, "Sun sprite occlusion query calibration failed.\n");
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

	T4::Com_Printf(0x10, "%s %s build %s %s\n",
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
		T4::Com_Printf(7, "begin $init\n");

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
				T4::Com_Printf(0x10, "Cmd_AddCommand: %s already defined\n", entries[i].name);
			else
				T4M::Cmd_AddCommand(entries[i].name, entries[i].func);
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
		T4::Com_Printf(0x10, "Winsock Initialized\n");
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

		T4::Com_Printf(0x10, "%s %d\n",
			"Number of worker threads", workers);

		*G::g_smpWorkerThreads = (DWORD)Engine::Dvar_RegisterString(
			"r_smp_worker_threads", 5, (void*)(uintptr_t)workers, (const char*)2);
	}

	T4::Com_Printf(8, "Trying SMP acceleration...\n");
	if (!Engine::Sys_SpawnRenderThread())
		Engine::Com_Error(0, "Failed to create render thread");

	Engine::UI_LoadCinematic();                           // sub_71EFC0
	T4::Com_Printf(8, "...succeeded.\n");

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
		T4::Com_Printf(0x10, "end $init %d ms\n", end);
	}

	// ---- Final banner + trace session ------------------------------------
	T4::Com_Printf(0x10, "--- Common Initialization Complete ---\n");
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
			T4::Sys_SyncDatabase();                            // sub_6F6CE0
			Engine::Com_PostInit2();                      // sub_644E60
			T4::Sys_WakeDatabase();                            // sub_6F6D60
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

// sub_5C7530  __usercall: eax=ctx, edx=cmd, [esp+4]=0, [esp+8]=0  → binding handle
// Returns non-zero if cmd is currently bound to a key.
static int T4M_Key_GetBinding(void* ctx, const char* cmd)
{
    int result;
    const void* fn = (const void*)0x005C7530;
    __asm {
        push 0
        push 0
        mov  edx, cmd
        mov  eax, ctx
        call fn
        add  esp, 8
        mov  result, eax
    }
    return result;
}

// sub_5BB830  __usercall: eax=bindHandle, [esp+4]=bufSize
// Writes the display key string for bindHandle into the caller-owned buffer.
// TODO: output buffer pointer mechanism unverified — may need arg adjustment.
static void T4M_Key_BindingToString(int bindHandle, int bufSize)
{
    const void* fn = (const void*)0x005BB830;
    __asm {
        push bufSize
        mov  eax, bindHandle
        call fn
        add  esp, 4
    }
}

// sub_5BB5E0  __usercall: eax=keyStr  → eax (canonical display name)
static const char* T4M_Key_KeynumToString(const char* keyStr)
{
    const char* result;
    const void* fn = (const void*)0x005BB5E0;
    __asm {
        mov  eax, keyStr
        call fn
        mov  result, eax
    }
    return result;
}

// sub_44B860  __cdecl: (inputCmd, cmdNameBuf, argsBuf) — splits "cmd[:args]"
static void T4M_Key_SplitBindCmd(const char* inputCmd, char* cmdNameBuf, char* argsBuf)
{
    typedef void(__cdecl* fn_t)(const char*, char*, char*);
    ((fn_t)0x0044B860)(inputCmd, cmdNameBuf, argsBuf);
}

// ===========================================================================
// 8. sub_44B8E0 — Key binding display string  (0x44B8E0)
// @faithful  __usercall: eax=outBuf, ecx=ctx, [esp+4]=inputCmd
//
// Resolution order:
//   1. Direct binding via sub_5C7530 → display string via sub_5BB830.
//   2. Parse "cmd[:args]" via sub_44B860.
//   3. "FAKE_INTRO_SECONDS" → T4M::Key_FormatIntroSeconds.
//   4. "gocrouch" alias → try "togglecrouch" then "+movedown" bindings.
//   5. Fallback → output "\"KEY_UNBOUND\"" via sub_5BB5E0 + sub_5F6D00.
// ===========================================================================
static void __cdecl T4_Key_GetBindStringForCmd(char* outBuf, void* ctx, const char* inputCmd)  // @faithful
{
    char cmd_name[256];  // var_200
    char args[256];      // var_100

    // Phase 1 — direct binding
    int binding = T4M_Key_GetBinding(ctx, inputCmd);
    if (binding != 0)
    {
        // TODO: sub_5BB830 output-buffer handoff unverified; size arg may differ.
        T4M_Key_BindingToString(binding, 32);
        return;
    }

    // Phase 2 — split "cmd[:args]"
    T4M_Key_SplitBindCmd(inputCmd, cmd_name, args);

    // Phase 3 — FAKE_INTRO_SECONDS
    if (T4M::Q_stricmpn(cmd_name, "FAKE_INTRO_SECONDS", 0x7FFFFFFF) == 0)
    {
        T4M::Key_FormatIntroSeconds(args, outBuf);
        return;
    }

    // Phase 4 — "gocrouch" alias
    if (T4M::Q_stricmpn(cmd_name, "gocrouch", 0x7FFFFFFF) == 0)
    {
        binding = T4M_Key_GetBinding(ctx, "togglecrouch");
        if (!binding)
            binding = T4M_Key_GetBinding(ctx, "+movedown");
        if (binding)
        {
            T4M_Key_BindingToString(binding, 32); // TODO: same uncertainty as Phase 1
            return;
        }
    }

    // Phase 5 — fallback: "KEY_UNBOUND"
    const char* unboundStr = T4M_Key_KeynumToString("KEY_UNBOUND");
    T4::Com_sprintf(outBuf, 32, "\"%s\"", (DWORD)(uintptr_t)unboundStr);
}

// Persistent meld diagnostic logger — writes to t4m_meld_diag.log next to the
// .exe. Console scrolls past, this file does not. File-scope so the static
// FILE* survives across hook callbacks and static auto initialization.
static FILE* g_meld_log = nullptr;
static void MeldLog(const char* fmt, ...)
{
    if (!g_meld_log) {
        fopen_s(&g_meld_log, "t4m_meld_diag.log", "a");
        if (g_meld_log) {
            fprintf(g_meld_log, "\n=== T4M meld diag (new session) ===\n");
        }
    }
    if (!g_meld_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_meld_log, fmt, ap);
    va_end(ap);
    fflush(g_meld_log);
}

// Hardware watchpoint on `unk_351FFFC + 0x1B0` (= absolute 0x352101AC). This
// is the centity_t.weapon field that the FP draw path reads via sub_4697A0
// and that ends up truncated to 7 bits (165 → 37) for high-idx weapons.
//
// Strategy:
//   - VEH catches EXCEPTION_SINGLE_STEP raised by Dr0 on write.
//   - Dr7 enables Dr0 with R/W=01 (write), LEN=11 (4 bytes).
//   - On hit, log EIP + the value being stored. Set RF in EFlags so the
//     offending instruction is allowed to complete without re-trigger.
//   - Dr0 is armed lazily on the first invocation of the FP draw midhook
//     (= we're on the game thread there, so SetThreadContext(self) covers it).
// Step 1 was 0x035201AC (= unk_351FFFC + 0x1B0). Writer found at 0x004106C2:
//   movzx edx, byte ptr [eax+104h]   ; eax=cg=0x0351DF50, so source = 0x0351E054
//   mov   [ecx+0E0h], edx            ; ecx+0xE0 = unk_351FFFC + 0x1B0
// edx already arrives = 0x25 (=37). Truncation is upstream — chase it by
// watching the SOURCE byte at 0x0351E054 (= cg + 0x104).
constexpr DWORD WEAPON_WATCH_ADDR = 0x0351E054;  // = cg_state + 0x104 (ps.weapon byte)

static LONG CALLBACK WeaponWriteWatchVEH(EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        if (ep->ContextRecord->Dr6 & 0x1) {
            DWORD eip = ep->ContextRecord->Eip;
            DWORD curVal = *(volatile DWORD*)WEAPON_WATCH_ADDR;
            MeldLog("[WriteWatch] EIP=0x%08X  postVal=%u  eax=0x%08X ebx=0x%08X ecx=0x%08X edx=0x%08X esi=0x%08X edi=0x%08X ebp=0x%08X\n",
                eip, curVal,
                ep->ContextRecord->Eax, ep->ContextRecord->Ebx, ep->ContextRecord->Ecx,
                ep->ContextRecord->Edx, ep->ContextRecord->Esi, ep->ContextRecord->Edi,
                ep->ContextRecord->Ebp);
            ep->ContextRecord->Dr6 &= ~0xFu;
            ep->ContextRecord->EFlags |= 0x10000u;  // RF: don't re-trigger on instruction restart
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Worker-thread payload: suspends the captured game thread, sets DR0, resumes.
// SetThreadContext on GetCurrentThread() silently no-ops for debug registers
// on x86 user mode, so we *must* operate from a different thread.
static DWORD WINAPI ArmWeaponWatchWorker(LPVOID lpParam)
{
    DWORD gameTid = (DWORD)(uintptr_t)lpParam;
    MeldLog("[WriteWatch] worker started, gameTid=%u\n", gameTid);
    HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, gameTid);
    if (!h) {
        MeldLog("[WriteWatch] OpenThread tid=%u failed err=%u\n", gameTid, GetLastError());
        return 1;
    }
    if (SuspendThread(h) == (DWORD)-1) {
        MeldLog("[WriteWatch] SuspendThread failed err=%u\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    CONTEXT c = {0};
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(h, &c)) {
        MeldLog("[WriteWatch] GetThreadContext failed err=%u\n", GetLastError());
        ResumeThread(h);
        CloseHandle(h);
        return 1;
    }
    c.Dr0 = WEAPON_WATCH_ADDR;
    c.Dr1 = 0;
    c.Dr2 = 0;
    c.Dr3 = 0;
    c.Dr6 = 0;
    c.Dr7 = (1u << 0) | (1u << 16) | (3u << 18);  // L0, R/W=write, LEN=4
    BOOL ok = SetThreadContext(h, &c);
    DWORD setErr = ok ? 0 : GetLastError();
    ResumeThread(h);
    CloseHandle(h);
    if (!ok) {
        MeldLog("[WriteWatch] SetThreadContext failed err=%u\n", setErr);
        return 1;
    }
    MeldLog("[WriteWatch] DR0 armed at 0x%08X on tid=%u (write, 4 bytes)\n", WEAPON_WATCH_ADDR, gameTid);
    return 0;
}

static void EnableWeaponWriteWatch()
{
    static bool s_armed = false;
    if (s_armed) return;
    s_armed = true;
    DWORD gameTid = GetCurrentThreadId();
    MeldLog("[WriteWatch] arming requested, gameTid=%u\n", gameTid);
    HANDLE worker = CreateThread(nullptr, 0, ArmWeaponWatchWorker, (LPVOID)(uintptr_t)gameTid, 0, nullptr);
    if (!worker) {
        MeldLog("[WriteWatch] CreateThread failed err=%u\n", GetLastError());
    } else {
        CloseHandle(worker);
    }
}

// =====================================================================
// PatchT4_Temp — installs experimental / temporary detours.
//
// Currently holds two byte-level patches that emerged from the 158-weapon
// debugging session (2026-05-08). Both are wired in by default so that
// removing PatchT4_Temp() from the init chain disables them cleanly.
// =====================================================================

void PatchT4_Temp()
{
    // Register VEH for the weapon-field write watchpoint. DR0 itself is armed
    // lazily inside the sub_4697A0 midhook (game thread).
    AddVectoredExceptionHandler(1, WeaponWriteWatchVEH);

    // =====================================================================
    // Extend the item_id encoding in sub_440890 from 7-bit to 9-bit weap_idx.
    //
    // Vanilla packing: weap_idx = item_id & 0x7F (max 128), model_idx = item_id >> 7.
    // With WEAPON pool raised to NEW_MAX_WEAPONS (512), user maps spawn items
    // with item_id >= 128 expecting direct weapon index. The vanilla decode
    // mis-interprets these as (weap=N&0x7F, model_slot=N>>7), failing the
    // weaponDef[0]→required_models[1] validation.
    //
    // New packing: weap_idx = item_id & 0x1FF (max 512), model_idx = item_id >> 9.
    // For item_id 0..511 (= every weapon in T4M's expanded pool, model slot 0).
    // Items packed into model slot 1 (id 512..1023) still work but are
    // currently unused.
    //
    // Caveat: if vanilla content uses item_id 128..255 (= weap=N&0x7F, model=1
    // under old encoding), they will now be reinterpreted as (weap=128..255,
    // model=0). Acceptable for this build since we have direct-index items only.
    //
    // Patch sites:
    //   sub_440890+0x37 = 0x4408C7  and ecx, 8000007Fh → and ecx, 800001FFh
    //   sub_440890+0x51 = 0x4408E1  sar eax, 7         → sar eax, 9
    // Both encodings are same byte length (6 and 3 bytes resp.), in-place safe.
    //
    // The validation jnz at 0x4408EF is intentionally LEFT INTACT — any
    // item_id that still mis-decodes will fatal-error visibly so we can
    // diagnose remaining issues instead of silently rendering NULL models.
    // =====================================================================
    Memory::VP::Patch(0x004408C7 + 2, (uint32_t)0x800001FF); // displacement of `and ecx, imm32`
    Memory::VP::Patch<uint8_t>(0x004408E1 + 2, 9);           // shift count of `sar eax, imm8`

    // ---- 2026-05-11 — targeted pickup decoder patch ----
    // sub_4FCBA0 (CG item-pickup iterator, called from sub_4FCEB0 + sub_4FCFE0)
    // has the same `and edi, 8000007Fh ; ... ; sar eax, 7` decoder pattern at
    // 0x4FCBD5 / 0x4FCBF7. It reads `item_id` from `[ent+0x260]` and passes
    // the decoded weap_idx to sub_4FC570 (give-ammo). For weap_idx > 127 the
    // 7-bit mask truncates → player picks up wrong weapon.
    //
    // The previous scan-replace attempts failed because they couldn't isolate
    // this site from unrelated 7-bit masks. With the exact VA in hand we just
    // patch the two immediates directly — same minimum-byte approach as
    // sub_440890 above.
    //
    // Layout at sub_4FCBA0+0x35..+0x57 (= loc_4FCBD0 block):
    //   0x4FCBD0  mov   eax, [esi+4]                           (3 bytes)
    //   0x4FCBD3  mov   edi, eax                               (2 bytes)
    //   0x4FCBD5  and   edi, 8000007Fh    ← imm32 @ +2 = 0x4FCBD7
    //   0x4FCBDB  jns   short loc_4FCBE2
    //   0x4FCBDD  dec   edi
    //   0x4FCBDE  or    edi, 0FFFFFF80h
    //   0x4FCBE1  inc   edi
    //   0x4FCBE2  test  edi, edi
    //   0x4FCBE4  jle   loc_4FCC8C
    //   0x4FCBEA  mov   ebx, dword_8F6770[edi*4]
    //   0x4FCBF1  cdq
    //   0x4FCBF2  and   edx, 7Fh           ← left alone (cdq=0 for positive eax)
    //   0x4FCBF5  add   eax, edx
    //   0x4FCBF7  sar   eax, 7             ← imm8 @ +2 = 0x4FCBF9
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // Memory::VP::Patch(0x004FCBD5 + 2, (uint32_t)0x800001FF);
    // Memory::VP::Patch<uint8_t>(0x004FCBF7 + 2, 9);

    // ---- sub_4FD080 — pickup entry decoder ----
    // Top-of-chain pickup handler (called from sub_4FD210). Reads `item_id`
    // from `[ent+0xA8]` and decodes weap_idx as 7-bit signed. Same fix as
    // sub_440890 / sub_4FCBA0.
    //
    // Layout at sub_4FD080+0xF:
    //   0x4FD080  push  ebx
    //   0x4FD081  push  ebp
    //   0x4FD082  mov   ebp, [esp+8+arg_0]
    //   0x4FD086  push  esi
    //   0x4FD087  mov   esi, eax
    //   0x4FD089  mov   eax, [esi+0A8h]
    //   0x4FD08F  and   eax, 8000007Fh     ← imm32 @ +1 = 0x4FD090
    //   0x4FD094  jns   short loc_4FD09B   (sign-extend block follows)
    // The `and eax, imm32` uses the short eax-specific encoding (0x25 imm32 = 5 bytes),
    // so the immediate is at +1 (not +2 like the generic `and r/m32, imm32`).
    // PAUSED 2026-05-11
    // Memory::VP::Patch(0x004FD080 + 0xF + 1, (uint32_t)0x800001FF);

    // ---- sub_4FCEB0 — pickup model_idx decoder ----
    // Called by sub_4FD080. Reads `item_id = [esi+0xA8]` and extracts the
    // high bits via `sar eax, 7` for model_idx. For 9-bit weap_idx packing
    // the corresponding shift is 9.
    //
    // Layout at sub_4FCEB0+0x18:
    //   0x4FCEBC  mov   eax, [esi+0A8h]
    //   0x4FCEC2  cdq
    //   0x4FCEC3  and   edx, 7Fh           ← left alone (cdq=0 for positive eax)
    //   0x4FCEC6  add   eax, edx
    //   0x4FCEC8  sar   eax, 7             ← imm8 @ +2 = 0x4FCECA
    // PAUSED 2026-05-11
    // Memory::VP::Patch<uint8_t>(0x004FCEC8 + 2, 9);

    // =====================================================================
    // Latent vanilla bug fix in sub_464F90 (CG model-attachment array build).
    //
    // sub_464F90 builds a 5-entry array of {modelPtr, word, byte} structs at
    // var_24 (= entry-0x24). Each entry is 8 bytes wide, so 5 entries span
    // [entry-0x24 .. entry+0x10). But the function only allocates 0x2C bytes
    // of locals (`sub esp, 2Ch`), so entry [4] (at entry-0x04..entry+0x04)
    // overlaps the function's OWN return address (corrupting bytes 0..2 of
    // the saved ret addr).
    //
    // The 5th entry is only written when [esi+0x10] (the 4th attachment
    // slot of the per-weapon table) is non-zero. By forcing ecx=0 at the
    // [esi+0x10] check, we make the jz fall-through and skip the entry [4]
    // write block + the `add eax,1` after it. Cap is now 4 attachments.
    //
    // Patch site: sub_464F90+0xFF = 0x46508F
    //   Original: 8B 4E 10        mov  ecx, [esi+10h]   (3 bytes)
    //   Patched:  33 C9 90        xor  ecx, ecx ; nop   (3 bytes)
    // ebx is reliably 0 from `xor ebx, ebx` at function start, so the
    // following `cmp ecx, ebx` always tests 0 == 0 → jz taken.
    // =====================================================================
    Memory::VP::Patch(0x0046508F, { 0x33, 0xC9, 0x90 });

    // =====================================================================
    // sub_464F90 (CG attachment-meld builder) — bail when ANY entry.ptr is invalid.
    //
    // First crash (2026-05-08): array[0].ptr = NULL (cached cg_weaponInfo+0x4
    // was 0). Fixed by NULL guard.
    // Second crash (2026-05-08): array[1].ptr = 5 (small-integer garbage in
    // `[ebp+0]` = WeaponDef.someArray[edi] read). NULL guard didn't catch.
    //
    // Generalized guard: validate ALL `count` entries (count = eax at loc_4650AD).
    // Any pointer < 0x10000 (= null page, invalid on Win32) → bail to loc_4650F6.
    //
    // Layout at loc_4650AD: var_24..var_E is a count×8 byte array. Entry i has
    //   .ptr at  [esp + 0x18 + i*8]
    //   .word at [esp + 0x1C + i*8]
    //   .byte at [esp + 0x1E + i*8]
    // count is in eax (set to 2..5 by the upstream branches).
    //
    // The crash data showed weapon names like "viewmodel_weapon_usa_springfielda3a4"
    // in the stack at the time, so the failing path is per-weapon attachment
    // resolution where one of the model pointers is corrupted.
    // =====================================================================
    // sub_4697A0 entry diag — captures weapon idx from arg_8 and arg_C structs.
    // sub_4697A0 reads:
    //   esi = [arg_8+0xFC]   (some weapon idx field)
    //   esi = [arg_C+0x1B0]  (alt weapon idx field, when first branch not taken)
    // Then bg_weaponDefs[esi*4] for the weapon def. If esi here is truncated
    // (= 37 for thompson 165), we've found the truncation downstream of all
    // our previous hooks.
    static auto sub_4697A0_diag = safetyhook::create_mid(0x004697A0, [](SafetyHookContext& ctx) {
        EnableWeaponWriteWatch();  // arm DR0 on game thread (no-op after first call)
        static DWORD lastIdx = 0xFFFFFFFF;
        DWORD arg_0 = *(DWORD*)(ctx.esp + 4);
        DWORD arg_4 = *(DWORD*)(ctx.esp + 8);
        DWORD arg_8 = *(DWORD*)(ctx.esp + 0xC);
        DWORD arg_C = *(DWORD*)(ctx.esp + 0x10);
        DWORD arg_10 = *(DWORD*)(ctx.esp + 0x14);

        DWORD idx_FC = 0;
        DWORD idx_1B0 = 0;
        if (arg_8 > 0x10000) idx_FC = *(DWORD*)(arg_8 + 0xFC);
        if (arg_C > 0x10000) idx_1B0 = *(DWORD*)(arg_C + 0x1B0);

        DWORD reportIdx = idx_FC;
        if (reportIdx == lastIdx) return;
        lastIdx = reportIdx;

        const char* nmFC = "<n/a>";
        const char* nm1B0 = "<n/a>";
        if (idx_FC < 512 && ::bg_weaponDefs && ::bg_weaponDefs[idx_FC]) {
            const char* n = *(const char**)::bg_weaponDefs[idx_FC];
            if (n && (uintptr_t)n > 0x10000) nmFC = n;
        }
        if (idx_1B0 < 512 && ::bg_weaponDefs && ::bg_weaponDefs[idx_1B0]) {
            const char* n = *(const char**)::bg_weaponDefs[idx_1B0];
            if (n && (uintptr_t)n > 0x10000) nm1B0 = n;
        }
        MeldLog("[Render4697A0] arg_8+0xFC=%u(%s)  arg_C+0x1B0=%u(%s)  arg_0=0x%08X arg_4=0x%08X arg_8=0x%08X arg_C=0x%08X arg_10=0x%08X\n",
            idx_FC, nmFC, idx_1B0, nm1B0,
            arg_0, arg_4, arg_8, arg_C, arg_10);
    });

    // sub_465CE0 entry diag — captures actual weapon idx from playerState
    // pointed by edx+0AAC98h (= the cg-side player struct). edx is the input.
    // The function then does:
    //   mov eax, [esi+0FCh]    OR [esi+104h]  (= ps.weapon)
    //   mov ebx, bg_weaponDefs[eax*4]
    // We log eax to see the EXACT idx used at render.
    static auto sub_465CE0_diag = safetyhook::create_mid(0x00465CE0, [](SafetyHookContext& ctx) {
        static DWORD lastIdx = 0xFFFFFFFF;
        // edx = input (= client struct ptr)
        char* clientStruct = (char*)ctx.edx;
        if ((uintptr_t)clientStruct < 0x10000) return;
        // [edx+0AAC98h] = inner struct (= ps?)
        char* psPtr = *(char**)((char*)clientStruct + 0x0AAC98 + 0); // wait, let me re-read asm
        // Actually the asm: mov ecx, [edx+0AAC78h]; lea esi, [edx+0AAC98h]
        // So esi = edx + 0xAAC98 (= POINTER ARITHMETIC, not deref).
        char* esi = clientStruct + 0xAAC98;
        DWORD ps_weapon_FC = *(DWORD*)(esi + 0xFC);   // pseudo "weapon" field
        DWORD ps_weapon_104 = *(DWORD*)(esi + 0x104);  // ps.weapon-style field

        DWORD curIdx = ps_weapon_104;
        if (curIdx == lastIdx) return;
        lastIdx = curIdx;

        const char* nm104 = "<n/a>";
        const char* nmFC = "<n/a>";
        if (ps_weapon_104 < 512 && ::bg_weaponDefs && ::bg_weaponDefs[ps_weapon_104]) {
            const char* n = *(const char**)::bg_weaponDefs[ps_weapon_104];
            if (n && (uintptr_t)n > 0x10000) nm104 = n;
        }
        if (ps_weapon_FC < 512 && ::bg_weaponDefs && ::bg_weaponDefs[ps_weapon_FC]) {
            const char* n = *(const char**)::bg_weaponDefs[ps_weapon_FC];
            if (n && (uintptr_t)n > 0x10000) nmFC = n;
        }
        MeldLog("[Render465CE0] esi+0x104=%u(%s)  esi+0xFC=%u(%s)  edx=0x%08X\n",
            ps_weapon_104, nm104, ps_weapon_FC, nmFC, (DWORD)ctx.edx);
    });

    // sub_469B50 entry diag — dump candidate weapon idx globals AT RENDER
    // time. When user equips thompson_m1_wet (idx=165) and FP shows colt (37),
    // we want to see which global has 37 vs 165.
    //
    // dword_351DF54 = state flag
    // dword_351DF50 = weapon control struct (= argument to many sub_46XXXX)
    // dword_351E04C = MP idx (we saw stuck at 93)
    // dword_351E054 = SP idx (we saw track correctly)
    // dword_34732B8 = per-client struct (esi base)
    // dword_34732E0 = per-client struct + 0x28
    // dword_34732C4 = ??
    // dword_34732DC = ??
    // dword_34732E0 + 0xB5C = the byte we know is per-weapon attach slot
    //
    // Logs each frame's idx state. Use rate-limit: log only when [+0x104] of
    // dword_351DF50 (= playerState_t.weapon) CHANGES.
    static auto sub_469B50_diag = safetyhook::create_mid(0x00469B50, [](SafetyHookContext& ctx) {
        static DWORD lastWeap = 0xFFFFFFFF;
        DWORD* psPtr = (DWORD*)0x0351DF50;
        if (!psPtr) return;
        DWORD ps_weapon = *(DWORD*)((char*)psPtr + 0x104);  // read playerState_t.weapon
        BYTE  ps_weapon_byte = *(BYTE*)((char*)psPtr + 0x104);
        DWORD mp_idx = *(DWORD*)0x0351E04C;
        DWORD sp_idx = *(DWORD*)0x0351E054;
        DWORD client_struct = *(DWORD*)0x034732E0;
        DWORD c4 = *(DWORD*)0x034732C4;
        DWORD dc = *(DWORD*)0x034732DC;

        if (ps_weapon == lastWeap) return;
        lastWeap = ps_weapon;

        const char* nm_full = "<n/a>";
        const char* nm_byte = "<n/a>";
        if (ps_weapon < 512 && ::bg_weaponDefs && ::bg_weaponDefs[ps_weapon]) {
            const char* n = *(const char**)::bg_weaponDefs[ps_weapon];
            if (n && (uintptr_t)n > 0x10000) nm_full = n;
        }
        if (ps_weapon_byte < 512 && ::bg_weaponDefs && ::bg_weaponDefs[ps_weapon_byte]) {
            const char* n = *(const char**)::bg_weaponDefs[ps_weapon_byte];
            if (n && (uintptr_t)n > 0x10000) nm_byte = n;
        }
        MeldLog("[Render469B50] ps.weapon (full)=%u(%s) ps.weapon (byte)=%u(%s)  mp=%u sp=%u  client=0x%X c4=0x%X dc=0x%X\n",
            ps_weapon, nm_full, (DWORD)ps_weapon_byte, nm_byte,
            mp_idx, sp_idx, client_struct, c4, dc);
    });

    // sub_469AB0 entry diag — log the FP weapon idx whenever it changes, plus
    // both source globals (dword_351E04C / dword_351E054) and the resolved
    // weapon name. Goal: see if equipping springfield_scoped (idx=148) makes
    // esi land on 20 (= 148 & 0x7F) → 7-bit truncation in snapshot path.
    //
    // Hook at sub_469AB0+0x38 = 0x00469AE8 = right after esi is loaded with
    // the selected idx (the jnz fork on byte_351DF60 has resolved by then).
    static auto sub_469AB0_diag = safetyhook::create_mid(0x00469AE8, [](SafetyHookContext& ctx) {
        static DWORD lastSeenIdx = 0xFFFFFFFF;
        DWORD curIdx = (DWORD)ctx.esi;
        if (curIdx != lastSeenIdx) {
            lastSeenIdx = curIdx;
            DWORD mp_idx = *(DWORD*)0x0351E04C;
            DWORD sp_idx = *(DWORD*)0x0351E054;
            const char* name = "<n/a>";
            if (curIdx > 0 && curIdx < 512 && ::bg_weaponDefs) {
                T4::WeaponDef* d = ::bg_weaponDefs[curIdx];
                if (d) {
                    const char* n = *(const char**)d;
                    if (n && (uintptr_t)n > 0x10000) name = n;
                }
            }
            MeldLog("[FPWeap] esi(used)=%u  ('%s')   mp(351E04C)=%u  sp(351E054)=%u   esi&0x7F=%u\n",
                curIdx, name, mp_idx, sp_idx, curIdx & 0x7F);
        }
    });

    #if 0  // DISABLED — m1garand was actually correct, no cl=1 issue to chase
    static auto sub_464F90_entry_diag = safetyhook::create_mid(0x00464F90, [](SafetyHookContext& ctx) {
        DWORD weapIdx = ctx.eax;
        DWORD cl      = ctx.ecx & 0xFF;
        DWORD retAddr = *(DWORD*)(ctx.esp);
        if (weapIdx == 0 || weapIdx >= 512) return;
        // Only log when cl != 0 (= attachment slot != base). If we see cl=1
        // for m1garand (71), that explains the bayonet variant rendering.
        if (cl == 0) return;
        // Dedupe: 1 bit per (idx, cl<=15) pair = 512*16 bits = 1024 bytes
        static unsigned char seen[1024] = {0};
        if (cl > 15) return;
        unsigned bit = weapIdx * 16 + cl;
        unsigned byteIdx = bit / 8;
        unsigned bitMask = 1 << (bit & 7);
        if (seen[byteIdx] & bitMask) return;
        seen[byteIdx] |= bitMask;

        const char* weapName = "<n/a>";
        if (::bg_weaponDefs && ::bg_weaponDefs[weapIdx]) {
            const char* n = *(const char**)::bg_weaponDefs[weapIdx];
            if (n && (uintptr_t)n > 0x10000) weapName = n;
        }
        MeldLog("[464F90 cl] weap='%s' (idx=%u) cl=%u ret=0x%08X\n",
            weapName, weapIdx, cl, retAddr);
    });
    #endif  // disabled m1garand cl diag

    // sub_464F90 site (per-weapon attachment-meld builder).
    // Resolves weapon name from bg_weaponDefs[idx] when available so the log
    // tells us *which* weapon is corrupted, not just its index. When a bail
    // triggers, dumps bg_weaponDefs[idx].gunXModel[0..15] + cl AT THAT MOMENT
    // so we can compare against the registration-time snapshot — proves
    // whether corruption is at parse time (= present at registration) or
    // dynamic (= clean at registration, garbage at meld time).
    static auto sub_464F90_meld_null_guard = safetyhook::create_mid(0x004650AD, [](SafetyHookContext& ctx) {
        DWORD count = (DWORD)ctx.eax;
        if (count == 0 || count > 5) { ctx.eip = 0x004650F6; return; }
        DWORD weapIdx = (DWORD)(ctx.edi - 0x800);
        const char* weapName = "<unknown>";
        T4::WeaponDef* wpdef = nullptr;
        if (weapIdx < 512 && ::bg_weaponDefs) {
            wpdef = ::bg_weaponDefs[weapIdx];
            if (wpdef) {
                const char* n = *(const char**)wpdef;
                if (n && (uintptr_t)n > 0x10000) weapName = n;
            }
        }
        bool didBail = false;
        for (DWORD i = 0; i < count; i++) {
            DWORD ptr = *(DWORD*)(ctx.esp + 0x18 + i * 8);
            if (ptr < 0x10000) {
                MeldLog("[464F90] BAD ptr: weap='%s' (idx=%u) entry[%u].ptr=0x%08X count=%u  wpdef=0x%08X\n",
                    weapName, weapIdx, i, ptr, count, (DWORD)wpdef);
                didBail = true; break;
            }
            BYTE numBones = *(BYTE*)(ptr + 4);
            if (numBones > 128) {
                const char* name = *(const char**)(ptr + 0);
                MeldLog("[464F90] CORRUPT model: weap='%s' (idx=%u) entry[%u].ptr=0x%08X numBones=%u modelName='%s'  wpdef=0x%08X\n",
                    weapName, weapIdx, i, ptr, (DWORD)numBones,
                    (name && (uintptr_t)name > 0x10000) ? name : "<bad name ptr>",
                    (DWORD)wpdef);
                didBail = true; break;
            }
        }
        if (didBail && wpdef) {
            // cl was stashed in cg_weaponInfo[idx]+0x14 by sub_464F90 itself.
            BYTE cl = T4M::cg_weaponInfo
                ? *(BYTE*)(T4M::cg_weaponInfo + (size_t)weapIdx * 0x48 + 0x14)
                : 0xFF;
            MeldLog("[464F90]   ↳ NOW (meld-time) wpdef=0x%08X cl(=cgwInfo+0x14)=%u\n",
                (DWORD)wpdef, (unsigned)cl);
            for (int i = 0; i < 16; i++) {
                DWORD v = *(DWORD*)((char*)wpdef + 0x0C + i * 4);
                if (v == 0) continue;
                const char* nm = "<bad>";
                unsigned bn = 0;
                if (v > 0x10000) {
                    const char* n = *(const char**)v;
                    if (n && (uintptr_t)n > 0x10000) nm = n;
                    bn = *(unsigned char*)(v + 4);
                }
                MeldLog("[464F90]      gunXModel[%d] = 0x%08X  (name='%s' bones=%u)\n", i, v, nm, bn);
            }
            DWORD handM = *(DWORD*)((char*)wpdef + 0x4C);
            MeldLog("[464F90]      handXModel    = 0x%08X\n", handM);
            ctx.eip = 0x004650F6;
        } else if (didBail) {
            ctx.eip = 0x004650F6;
        }
    });

    // Universal sub_608D80 entry hook — LOG-ONLY (no bypass). Bypass via
    // `count=0 → loc_60901E` writes [entry+64h] = dword_3702400 (slot 0)
    // which the destructor (sub_6093C0 → sub_68A750 → sub_68A0E0) can't
    // handle, causing a delayed crash. Better to let the meld run and crash
    // here than corrupt cleanup state. Logging tells us which paths beyond
    // sub_464F90 have bad data so we can guard those specifically.
    static auto sub_608D80_logger = safetyhook::create_mid(0x00608D80, [](SafetyHookContext& ctx) {
        DWORD count = *(DWORD*)(ctx.esp + 0xC);
        char* array = *(char**)(ctx.esp + 8);
        DWORD retAddr = *(DWORD*)(ctx.esp);
        if (count == 0 || count > 8 || (uintptr_t)array < 0x10000) return;
        for (DWORD i = 0; i < count; i++) {
            DWORD ptr = *(DWORD*)(array + i * 8);
            if (ptr < 0x10000) {
                MeldLog("[608D80] BAD ptr (log only): ret=0x%08X entry[%u].ptr=0x%08X count=%u\n",
                    retAddr, i, ptr, count);
                return;
            }
            BYTE numBones = *(BYTE*)(ptr + 4);
            if (numBones > 128) {
                const char* name = *(const char**)(ptr + 0);
                MeldLog("[608D80] CORRUPT model (log only): ret=0x%08X entry[%u].ptr=0x%08X numBones=%u name='%s'\n",
                    retAddr, i, ptr, (DWORD)numBones,
                    (name && (uintptr_t)name > 0x10000) ? name : "<bad name ptr>");
                return;
            }
        }
    });

    // =====================================================================
    // CG_RegisterWeapon (sub_464BF0) diagnostic hook — log every registration
    // with weapon name + both XModel ptrs that the bail logic checks. Goal: see
    // if the failing weapons (shotgun_1912_wet, springfield_scoped, etc.) have
    // gunXModel[0] or handXModel = NULL at registration, OR if the pointers are
    // valid but point to corrupt XModel data (= memory was overwritten between
    // load and registration).
    //
    // Args at function entry: [esp+0]=ret, [esp+4]=arg_0, [esp+8]=arg_4=weapIdx.
    // For each XModel ptr, also dump XModel.name (offset 0) and numBones (offset
    // 4) so we know if the underlying XModel struct is intact.
    // =====================================================================
    #if 0  // DISABLED — registration log was useful initially but now pollutes file
    static auto cg_register_weapon_diag = safetyhook::create_mid(0x00464BF0, [](SafetyHookContext& ctx) {
        // (disabled body)
    });
    #endif

    // =====================================================================
    // Map-init reset hook (sub_4659B0 entry) — force re-registration on
    // every map restart by zeroing all cgwInfo[+0x34] (registered) flags
    // before the per-weapon iteration starts.
    //
    // BUG : on map restart, vanilla CG_RegisterWeapon's `if registered, bail`
    // early-out preserves cgwInfo entries from the previous map. cgw[+0]
    // points to a dobj that was freed during map unload. Per-frame
    // sub_465160 detects state mismatch → calls sub_464F90 → uses stale
    // dobj → cascades to sub_68A0E0 which crashes (movq [eax=0x28], xmm0).
    //
    // FIX : sub_4659B0 is called once per map init (via sub_459410 from
    // asset registration). Zeroing the registered flags here forces every
    // active weapon's CG_RegisterWeapon call to do a fresh memset +
    // re-fetch dobj from sub_59E8F0. No more stale pointers.
    //
    // Cost : 512 * 4 bytes zeroed per map init (= trivial). Re-registers
    // all active weapons (= dozens, negligible).
    //
    // 2026-05-10 — fixes map_restart crash in sub_68A0E0.
    // =====================================================================
    static auto cg_register_weapon_map_reset = safetyhook::create_mid(0x004659B0, [](SafetyHookContext& /*ctx*/) {
        BYTE* cgw_base = T4M::cg_weaponInfo;
        if (!cgw_base) return;
        // 2026-05-11 : reduced from 512 to NEW_MAX_WEAPONS (= 148) after
        // pausing the weapon-pool-expansion stack. cg_weaponInfo is now sized
        // NEW_MAX_WEAPONS × 0x48 ; iterating past NEW_MAX_WEAPONS-1 would OOB.
        for (int idx = 0; idx < NEW_MAX_WEAPONS; ++idx) {
            *(uint32_t*)(cgw_base + idx * 0x48 + 0x34) = 0;
        }
    });
    (void)cg_register_weapon_map_reset;

    // =====================================================================
    // CG_RegisterWeapon (sub_464BF0) — install C++ reconstruction detour
    // 2026-05-10 (P4 phase). Replaces vanilla with T4_Reconstructed::
    // CG_RegisterWeapon which : (a) handles NULL bg_weaponDefs[idx], (b)
    // uses correct __usercall ABIs for sub_464A50/611110/60C420/60F990/
    // 6103E0/610F50 via naked wrappers, (c) skips the early-return bug in
    // the AI overlay localize path. Same wire-level signature as vanilla
    // (cdecl 2 args), so SafetyHook inline replace is sufficient.
    //
    // OBSOLETED by this detour (kept disabled-in-source to document) :
    //   - PatchT4MAM_WeaponDef.cpp : sub_464BF0_null_def_guard midhook
    //     (NULL guard now in C++ reconstruction)
    //   - PatchT4MemoryLimits.cpp : byte patches at 0x464C4B / 0x464E52
    //     (inside vanilla body — bytes never executed once detoured ; left
    //     in place as defensive measure if detour install ever fails)
    // =====================================================================
    static auto cg_register_weapon_hook = safetyhook::create_inline(
        (void*)0x00464BF0,
        (void*)&T4_Reconstructed::CG_RegisterWeapon);
    (void)cg_register_weapon_hook;
}

// =====================================================================
// CG_RegisterWeapon (vanilla sub_464BF0) — @faithful reconstruction.
//
// NOT detoured — kept as a reference so we can compare against the vanilla
// asm and later evolve into a @modified version that handles partial-data
// WeaponDefs (the failure mode causing weap_idx 142, 148, 154, 180, 181, 182
// to leave cg_weaponInfo[idx]+0x4 = 0).
//
// Signature reverse-engineered from sub_4659B0 / sub_469AB0 caller sites:
//   void CG_RegisterWeapon(int clientNum_or_ctx, int weaponIdx);
//
// Key bug surface: at the `[ebx+0Ch] == 0 || [ebx+4Ch] == 0` test, vanilla
// jumps to the partial-init path (loc_464E40) which still sets
// `[ebp+34h] = 1` (the "registered" sentinel) so subsequent calls early-out.
// The unbuilt cache leaves `cg_weaponInfo[idx]+0x4 = 0`, which sub_464F90
// later reads as a model pointer → NULL deref in the meld.
//
// WeaponDef offsets used (vanilla, stride 0x9AC):
//   +0x00  = const char*  szInternalName            (e.g. "shotgun_1912_wet")
//   +0x04  = const char*  szDisplayName             (locale key)
//   +0x08  = const char*  szAIOverlayName
//   +0x0C  = XModel*      pGunXModel                ← NULL for failing weapons
//   +0x4C  = XModel*      pHandModel                ← NULL for failing weapons
//   +0xDC  = const char*  szModeName                (locale key)
//   +0xE0  = uint16[8]    boneTagNames              (string-id bone names)
//   +0x3D8 = const char*  szDisplayNameFallback
//
// cg_weaponInfo entry layout (stride 0x48, 4096 bytes total post-T4M):
//   +0x00 = DObj*  meldDObj                         (sub_59E8F0 result)
//   +0x04 = XModel* attach[0]                       (= WeaponDef.pHandModel)
//   +0x08 = XModel* attach[1]
//   +0x0C = XModel* attach[2]
//   +0x10 = XModel* attach[3]
//   +0x14 = byte    currentSlot
//   +0x18 = uint32[4] boneTagBitmask                (8 bones x 4 bits each = 32)
//   +0x28 = int     someStateInt                    (= 0xFFFFFFFF)
//   +0x30 = void*   attachmentList                  (sub_464A50 result)
//   +0x34 = int     registered                      (1 = init done)
//   +0x38 = void*   itemSlotPtr                     (= &dword_8F44A8[idx*4])
//   +0x3C = const char* localizedDisplayName
//   +0x40 = const char* localizedModeName
//   +0x44 = const char* localizedAIOverlayName
// =====================================================================
namespace T4_Reconstructed {

    // Vanilla helper pointers used by CG_RegisterWeapon. Most have __usercall
    // ABIs (register inputs) ; we use naked wrappers below to translate to
    // standard cdecl from the C++ caller side.
    typedef void  (__cdecl* sub_479370_t)(void);                                  // pre-init guard, no register args
    typedef const char* (__cdecl* sub_5ACD40_t)(const char* key);                 // localize lookup (cdecl)
    typedef void  (__cdecl* Com_PrintError_t)(int channel, const char* fmt, ...); // sub_59A440
    typedef void  (__cdecl* Com_Printf_chan_t)(int channel, const char* fmt, ...);// sub_59A380 (warn channel)
    typedef void  (__cdecl* Com_ErrorMsg_t)(int level, const char* fmt, ...);     // sub_59AC50

    static const sub_479370_t      pSub_479370       = (sub_479370_t)     0x00479370;
    static const sub_5ACD40_t      pLocalize         = (sub_5ACD40_t)     0x005ACD40;
    static const Com_PrintError_t  pCom_PrintError   = (Com_PrintError_t) 0x0059A440;
    static const Com_Printf_chan_t pCom_PrintWarn    = (Com_Printf_chan_t)0x0059A380;
    static const Com_ErrorMsg_t    pCom_ErrorMsg     = (Com_ErrorMsg_t)   0x0059AC50;

    // ----- naked __usercall wrappers for register-arg helpers -----

    // sub_464A50 — __cdecl(arg_0=wpnDef) -> eax (attachment list ptr).
    // The existing typedef `void(*)(void*)` discarded eax. This wrapper
    // captures it.
    extern "C" __declspec(naked) static void* Sub_464A50_call(void* /*wpnDef*/)
    {
        __asm
        {
            push    [esp+4]
            mov     ecx, 0x00464A50
            call    ecx
            add     esp, 4
            retn
        }
    }

    // sub_6103E0 — __usercall(eax=dobj, ecx=0, esi=attachList). The body reads
    // `movzx eax, word ptr [esi+4]` at entry — esi must be a valid struct ptr.
    extern "C" __declspec(naked) static void Sub_6103E0_call(int /*dobj*/, void* /*attachList*/)
    {
        __asm
        {
            push    esi
            mov     eax, [esp+8]            // dobj (slots shifted +4 by push)
            mov     esi, [esp+0x0C]          // attachList
            xor     ecx, ecx
            mov     edx, 0x006103E0
            call    edx
            pop     esi
            retn
        }
    }

    // sub_611110 — __usercall(eax=layerIdx) + cdecl(dobj, f1, f2, f3, i1, i2, i3).
    // Vanilla pushes 7 stack args (28 bytes). f1/f2/f3 are float bits stored as
    // dwords — the caller passes them as int from C++.
    extern "C" __declspec(naked) static void Sub_611110_call(
        int /*layerIdx*/, int /*dobj*/,
        int /*f1_bits*/, int /*f2_bits*/, int /*f3_bits*/,
        int /*i1*/, int /*i2*/, int /*i3*/)
    {
        __asm
        {
            mov     eax, [esp+4]            // layerIdx
            push    [esp+0x20]               // i3
            push    [esp+0x20]               // i2
            push    [esp+0x20]               // i1
            push    [esp+0x20]               // f3
            push    [esp+0x20]               // f2
            push    [esp+0x20]               // f1
            push    [esp+0x20]               // dobj
            mov     ecx, 0x00611110
            call    ecx
            add     esp, 28
            retn
        }
    }

    // sub_60C420 — __usercall(ecx=dobj) + cdecl(strId, outIdx) -> eax (0/1).
    extern "C" __declspec(naked) static int Sub_60C420_call(
        int /*dobj*/, unsigned int /*strId*/, unsigned char* /*outIdx*/)
    {
        __asm
        {
            mov     ecx, [esp+4]
            push    [esp+0x0C]               // outIdx
            push    [esp+0x0C]               // strId (slot shifted +4 by push)
            mov     edx, 0x0060C420
            call    edx
            add     esp, 8
            retn
        }
    }

    // sub_60F990 — __usercall(edi=dobj) + cdecl(float_bits, zero).
    extern "C" __declspec(naked) static void Sub_60F990_call(
        int /*dobj*/, int /*float_bits*/, int /*zero*/)
    {
        __asm
        {
            push    edi
            mov     edi, [esp+8]             // dobj (after push edi)
            push    [esp+0x10]                // zero
            push    [esp+0x10]                // float_bits
            mov     eax, 0x0060F990
            call    eax
            add     esp, 8
            pop     edi
            retn
        }
    }

    // sub_610F50 — __usercall(ecx=attachList, edx=arg_X, xmm0=float). No stack args.
    extern "C" __declspec(naked) static void Sub_610F50_call(
        void* /*attachList*/, int /*arg_X*/, int /*xmm_float_bits*/)
    {
        __asm
        {
            mov     ecx, [esp+4]
            mov     edx, [esp+8]
            movss   xmm0, dword ptr [esp+0x0C]
            mov     eax, 0x00610F50
            jmp     eax
        }
    }

    // Vanilla globals referenced.
    #define DWORD_1F552FC    (*(void**)0x01F552FC)        // some flag struct (mp/sp gate)
    #define DWORD_1F552D0    (*(void**)0x01F552D0)
    #define WORD_1F33C5A     (*(uint16_t*)0x01F33C5A)     // template word for entry init
    #define DWORD_3702390    ((char*)0x03702390)          // string pool base (12-byte stride)
    #define DWORD_8F44A8     (T4M::g_dword8F44A8)         // per-weapon item-slot table (relocated by SetupBgWeaponDefsTable)
    #define DWORD_35D0DBC    (*(void**)0x035D0DBC)        // default localized display name
    #define DWORD_35D0DC8    (T4M::g_dword35D0DC8)        // per-weapon display name cache (relocated by SetupBgWeaponDefsTable)
    #define DWORD_208B2E8    (*(void**)0x0208B2E8)        // print-warning gate
    #define DWORD_208B2E4    (*(void**)0x0208B2E4)        // dev-mode gate
    #define DWORD_8AF370     (*(float*)0x008AF370)        // 1.0f or similar default

    // __usercall wrapper for sub_59E8F0 (eax = weap_idx_biased).
    extern "C" __declspec(naked) static int Sub_59E8F0_call(
        void* /*arg_0 modelArray*/, int /*arg_4 count*/, void* /*arg_8*/, char /*arg_C*/, int /*eax weapIdxBiased*/)
    {
        __asm {
            mov     eax, [esp + 14h]   ; eax = 5th arg = weapIdxBiased
            push    [esp + 10h]        ; push arg_C (byte sized but pushed as dword)
            push    [esp + 10h]        ; push arg_8
            push    [esp + 10h]        ; push arg_4 (count)
            push    [esp + 10h]        ; push arg_0 (modelArray)
            mov     ecx, 0x0059E8F0
            call    ecx
            add     esp, 10h
            retn
        }
    }

    extern "C" void __cdecl CG_RegisterWeapon(void* arg_0_ctx, int weapIdx)
    {
        // Early bail: weapon idx 0 is "no weapon".
        if (weapIdx == 0) return;

        T4::WeaponDef* wpnDef = ::bg_weaponDefs[weapIdx];
        unsigned char* cgwInfo = (unsigned char*)T4M::cg_weaponInfo + (size_t)weapIdx * 0x48;

        // Already registered? cg_weaponInfo[idx]+0x34 is the sentinel.
        // Vanilla reads dword_3463C74[ebp*8] which is &cg_weaponInfo[idx]+0x34.
        if (*(int*)(cgwInfo + 0x34) != 0) return;

        // T4M : NULL guard for unregistered idx ≥ NUM_WEAPONS_LOADED. Vanilla
        // would NULL-deref at `mov eax, [ebx+0Ch]`. The detoured midhook in
        // PatchT4MAM_WeaponDef.cpp does this same check ; once we install
        // this reconstruction, that midhook becomes redundant.
        if (wpnDef == nullptr) return;

        // First-time global init guard (mp/sp gate).
        if (*(unsigned char*)((char*)DWORD_1F552FC + 0x10) == 0
         || *(unsigned char*)((char*)DWORD_1F552D0 + 0x10) == 0) {
            pSub_479370();
        }

        // Wipe the entry: 0x48 bytes of zero, then re-init the marker fields.
        memset(cgwInfo, 0, 0x48);
        *(int*)(cgwInfo + 0x34) = 1;                                 // registered
        *(void**)(cgwInfo + 0x38) = &DWORD_8F44A8[weapIdx];           // itemSlotPtr
        *(int*)(cgwInfo + 0x28) = -1;

        // ---------------------------------------------------------------
        // BUG SURFACE: bail if either gun-XModel or hand-XModel is NULL.
        // For weapons whose .gdt or .iwd lacks one of these fields (= the
        // 6 weapons crashing the user), vanilla jumps to the partial-init
        // tail without filling [+0x4] (= cgw.attach[0]). The "registered"
        // sentinel is already set above, so subsequent rebuild attempts
        // (sub_465160 / sub_465200 / sub_465270 → sub_464F90) read
        // [+0x4] = 0 and feed NULL into the meld.
        // ---------------------------------------------------------------
        void* pGunXModel  = *(void**)((char*)wpnDef + 0x0C);
        void* pHandModel  = *(void**)((char*)wpnDef + 0x4C);
        if (pGunXModel == nullptr || pHandModel == nullptr) {
            // Vanilla: skip the meld build, fall through to the localize
            // section at loc_464E40. We replicate that behavior — see below.
            goto L_localize_strings;
        }

        {
            // Build the {handModel, gunXModel} 2-entry array on the local stack
            // (this is what gets passed to sub_59E8F0 as arg_0). Layout matches
            // sub_464BF0 stack vars var_14/var_10/var_E (entry 0) + var_C/var_8/
            // var_6 (entry 1) : 8 bytes per entry.
            struct MeldEntry { void* model; uint16_t boneId; uint8_t flag1; uint8_t flag2; };
            MeldEntry models[2];
            models[0].model = pHandModel;       // [+0x4C]
            models[0].boneId = 0;
            models[0].flag1 = 0;
            models[0].flag2 = 0;
            models[1].model = pGunXModel;       // [+0x0C]
            models[1].boneId = WORD_1F33C5A;
            models[1].flag1 = 0;
            models[1].flag2 = 0;

            // Build the per-weapon attachment dobj list (= sub_464A50).
            // FIX 2026-05-10 : vanilla passes ebx (= wpnDef), not the local
            // model array. The previous `pBuildAttachList(&models)` was wrong
            // and the eax return value was discarded.
            void* attachList = Sub_464A50_call(wpnDef);
            *(void**)(cgwInfo + 0x30) = attachList;

            // Meld build. weapIdx | 0x800 is the biased index used by sub_59E8F0
            // (caller stores it in word_1FE58C8 keyed on this hash).
            int dobj = Sub_59E8F0_call(&models[0], /*count*/2, /*arg_8*/attachList,
                                       /*arg_C*/(char)(uintptr_t)arg_0_ctx,
                                       weapIdx + 0x800);

            *(void**)(cgwInfo + 0x00) = (void*)(intptr_t)dobj;
            *(void**)(cgwInfo + 0x04) = pHandModel;             // cgw.attach[0]
            *(unsigned char*)(cgwInfo + 0x14) = 0;
        }

        // sub_6103E0 (model attach finalizer). Vanilla : __usercall(eax=dobj,
        // ecx=0, esi=attachList).
        Sub_6103E0_call(*(int*)(cgwInfo + 0x00), *(void**)(cgwInfo + 0x30));

        // Two unconditional sub_611110 calls (anim-layer init for hand + gun).
        // Vanilla ABI : __usercall(eax=layerIdx) + cdecl(dobj, 1.0f, 0.0f, 1.0f, 0, 1, 0).
        // FIX 2026-05-10 : vanilla pushes 7 stack args (not 8), and uses eax for
        // the layer idx (= 0 / 1 / 0x22). f1=1.0, f2=0.0, f3=1.0 (FPU stack reuses
        // the initial fld1).
        {
            int dobj = *(int*)(cgwInfo + 0x00);
            const int f_1_0 = 0x3F800000;       // 1.0f as bits
            const int f_0_0 = 0x00000000;       // 0.0f as bits
            Sub_611110_call(/*layerIdx*/0, dobj, f_1_0, f_0_0, f_1_0, 0, 1, 0);
            Sub_611110_call(/*layerIdx*/1, dobj, f_1_0, f_0_0, f_1_0, 0, 1, 0);
        }

        // Optional 3rd sub_611110 + sub_610F50 (if WeaponDef[+0xD8] != 0 byte).
        // Vanilla pushes 0/1/0 + 0.0f/0.0f/1.0f then sub_611110(eax=0x22),
        // followed by sub_610F50(ecx=attachList, edx=0x22, xmm0=dword_826A4C).
        if (*(unsigned char*)((char*)wpnDef + 0xD8) != 0) {
            int dobj = *(int*)(cgwInfo + 0x00);
            const int f_1_0 = 0x3F800000;
            const int f_0_0 = 0x00000000;
            // var_4C=1.0, var_48=0.0, var_44=0.0 (per fldz then fstp x2 chain).
            Sub_611110_call(/*layerIdx*/0x22, dobj, f_1_0, f_0_0, f_0_0, 0, 1, 0);
            void* attachList = *(void**)(cgwInfo + 0x30);
            const int dword_826A4C_bits = *(int*)0x00826A4C;
            Sub_610F50_call(attachList, 0x22, dword_826A4C_bits);
        }

        // Process bone tag list at WeaponDef[+0xE0..+0xF0] (8 entries × 2 bytes).
        {
            uint16_t* tags = (uint16_t*)((char*)wpnDef + 0xE0);
            int dobj = *(int*)(cgwInfo + 0x00);
            for (int i = 0; i < 8; i++) {
                uint16_t strId = tags[i];
                if (strId == 0) break;
                unsigned char boneIdx = 0xFE;
                // FIX 2026-05-10 : vanilla passes dobj via ecx (__usercall) ;
                // the previous pBoneTagLookup typedef was cdecl and missed it.
                if (Sub_60C420_call(dobj, strId, &boneIdx) == 0) {
                    const char* tagName = strId
                        ? (const char*)(DWORD_3702390 + (size_t)strId * 12 + 4)
                        : nullptr;
                    pCom_PrintWarn(0x0E, "CG_RegisterWeapon: No such bone tag (%s) in model (%s)\n",
                        tagName, *(const char**)wpnDef);
                } else {
                    uint32_t bit = 0x80000000u >> (boneIdx & 0x1F);
                    *(uint32_t*)(cgwInfo + 0x18 + (boneIdx >> 5) * 4) |= bit;
                }
            }

            // Final attach-finalize pass : vanilla copies the bone bitmap from
            // cgw[+0x18..+0x28] into dobj+0x50..+0x60 via xmm, then calls
            // sub_60F990(__usercall edi=dobj, stack=float_bits + 0).
            uint8_t* dobj_b = (uint8_t*)(intptr_t)dobj;
            *(uint64_t*)(dobj_b + 0x50) = *(uint64_t*)(cgwInfo + 0x18);
            *(uint64_t*)(dobj_b + 0x58) = *(uint64_t*)(cgwInfo + 0x20);
            const int dword_8AF370_bits = *(int*)0x008AF370;
            Sub_60F990_call(dobj, dword_8AF370_bits, 0);
        }

    L_localize_strings:
        // Vanilla loc_464E40 : write display-name fallback ptr to
        // dword_35D0DC8[idx] (= per-weapon HUD pickup-hint cache).
        //   eax = def[+0x3D8] (szDisplayNameFallback) ; if NULL, fallback to
        //   dword_35D0DBC (global default).
        //   dword_35D0DC8[idx] = eax
        // FIX 2026-05-10 : missing from earlier reconstruction → pickup hints
        // disappeared once the detour replaced vanilla.
        {
            uint32_t fallback = *(uint32_t*)((char*)wpnDef + 0x3D8);
            if (fallback == 0) {
                fallback = (uint32_t)(uintptr_t)DWORD_35D0DBC;
            }
            ((uint32_t*)DWORD_35D0DC8)[weapIdx] = fallback;
        }

        // ----- partial-init path (and tail of full-init path) -----
        // Localize szDisplayName, szModeName, szAIOverlayName. Each lookup
        // either returns the localized string or hits a fallback chain with
        // a console warning.
        {
            const char* loc;
            // Display name (WeaponDef[+0x04])
            loc = pLocalize(*(const char**)((char*)wpnDef + 0x04));
            if (loc) {
                *(const char**)(cgwInfo + 0x3C) = loc;
            } else {
                void* gateA = DWORD_208B2E8;
                if (*(unsigned char*)((char*)gateA + 0x10) != 0) {
                    void* gateB = DWORD_208B2E4;
                    if (*(unsigned char*)((char*)gateB + 0x10) != 0) {
                        pCom_ErrorMsg(6, "Weapon %s: Could not translate display name '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0x04));
                    } else {
                        pCom_PrintError(0x11, "WARNING: Weapon %s: Could not translate display name '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0x04));
                    }
                }
                *(const char**)(cgwInfo + 0x3C) = *(const char**)((char*)wpnDef + 0x04);
            }

            // Mode name (WeaponDef[+0xDC])
            loc = pLocalize(*(const char**)((char*)wpnDef + 0xDC));
            if (loc) {
                *(const char**)(cgwInfo + 0x40) = loc;
            } else {
                void* gateA = DWORD_208B2E8;
                if (*(unsigned char*)((char*)gateA + 0x10) != 0) {
                    void* gateB = DWORD_208B2E4;
                    if (*(unsigned char*)((char*)gateB + 0x10) != 0) {
                        pCom_ErrorMsg(6, "Weapon %s: Could not translate mode name '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0xDC));
                    } else {
                        pCom_PrintError(0x11, "WARNING: Weapon %s: Could not translate mode name '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0xDC));
                    }
                }
                *(const char**)(cgwInfo + 0x40) = *(const char**)((char*)wpnDef + 0xDC);
            }

            // AI overlay name (WeaponDef[+0x08])
            // FIX 2026-05-10 : vanilla DOES set [+0x44] = raw name even after
            // pCom_ErrorMsg ; the previous early-return was incorrect.
            loc = pLocalize(*(const char**)((char*)wpnDef + 0x08));
            if (loc) {
                *(const char**)(cgwInfo + 0x44) = loc;
            } else {
                void* gateA = DWORD_208B2E8;
                if (*(unsigned char*)((char*)gateA + 0x10) != 0) {
                    void* gateB = DWORD_208B2E4;
                    if (*(unsigned char*)((char*)gateB + 0x10) != 0) {
                        pCom_ErrorMsg(6, "Weapon %s: Could not translate AI overlay '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0x08));
                    } else {
                        pCom_PrintError(0x11, "WARNING: Weapon %s: Could not translate AI overlay '%s'\n",
                            *(const char**)wpnDef, *(const char**)((char*)wpnDef + 0x08));
                    }
                }
                *(const char**)(cgwInfo + 0x44) = *(const char**)((char*)wpnDef + 0x08);
            }
        }
    }

    #undef DWORD_1F552FC
    #undef DWORD_1F552D0
    #undef WORD_1F33C5A
    #undef DWORD_3702390
    #undef DWORD_8F44A8
    #undef DWORD_35D0DBC
    #undef DWORD_35D0DC8
    #undef DWORD_208B2E8
    #undef DWORD_208B2E4
    #undef DWORD_8AF370

} // namespace T4_Reconstructed
