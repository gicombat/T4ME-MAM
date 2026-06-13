// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: Changes/functionality related to console.
//
// Initial author: DidUknowiPwn
// Started: 2015-07-12
// ==========================================================
#include "StdInc.h"
#include "t4_headers.h"
#include "T4.h"
#include <string>

vec4_t whiteColor = { 8.0f, 8.0f, 8.0f, 1.0f };

using T4::engine::logfile;  // symbol<dvar_t*> (dvars.hpp), vanilla 0x01F552BC — variant-aware

// Force `logfile` dvar to async-write mode. Engine registers it with default 0
// (no archive flag), so console.log is never written unless we flip it. Called
// from Cmd_Init_T4 — runs after Cvar_Init so *logfile is valid.
static void ForceLogfileEnabled()
{
	if (!logfile || !*logfile) 
		return;
	// 0=off, 1=sync, 2=async file write (preferred — flushes more often)
	(*logfile)->current.integer = 2;
	(*logfile)->latched.integer = 2;
	(*logfile)->modified = true;
}

void DrawDvarFlags(dvar_t* dvar)
{
	__int16 flags = dvar->flags;

	const char* flagsString = va("Flags: %s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		(flags & T4::dvar::DVAR_FLAG_ARCHIVE ? "Archive, " : ""),
		(flags & T4::dvar::DVAR_FLAG_USERINFO ? "UserInfo, " : ""),
		(flags & T4::dvar::DVAR_FLAG_SERVERINFO ? "ServerInfo, " : ""),
		(flags & T4::dvar::DVAR_FLAG_SYSTEMINFO ? "SystemInfo, " : ""),
		(flags & T4::dvar::DVAR_FLAG_INIT ? "Init, " : ""),
		(flags & T4::dvar::DVAR_FLAG_LATCH ? "Latch, " : ""),
		(flags & T4::dvar::DVAR_FLAG_ROM ? "Rom, " : ""),
		(flags & T4::dvar::DVAR_FLAG_CHEAT ? "Cheat, " : ""),
		(flags & T4::dvar::DVAR_FLAG_DEVELOPER ? "Developer, " : ""),
		(flags & T4::dvar::DVAR_FLAG_SAVED ? "Saved, " : ""),
		(flags & T4::dvar::DVAR_FLAG_NORESTART ? "NoRestart, " : ""),
		(flags & T4::dvar::DVAR_FLAG_CHANGEABLE_RESET ? "ChangeableReset, " : ""),
		(flags & T4::dvar::DVAR_FLAG_EXTERNAL ? "External, " : ""),
		(flags & T4::dvar::DVAR_FLAG_AUTOEXEC ? "AutoExec" : ""));

	// increase by one line and reset to left side
	T4::engine::conDraw->y += T4::engine::conDraw->fontHeight;
	T4::engine::conDraw->x = T4::engine::conDraw->leftX;

	T4::engine::ConDrawInput_TextLimitChars(flagsString, 40, &whiteColor);
}


void __declspec(naked) drawDetailedDvarMatchStub()
{
	__asm
	{
		push[esp + 12]
		push[esp + 12]
		push[esp + 12]
		call T4::engine::ConDrawInput_TextLimitChars_asm

		push edi
		call DrawDvarFlags
		add esp, 16

		retn
	}
}

void ShowExternalConsole()
{
	DWORD Sys_ShowConsole_f = T4M::GetAddress("Sys_ShowConsole");
	DWORD Sys_AllocHunk = T4M::GetAddress("Sys_AllocHunk");

	__asm call Sys_AllocHunk

	if (enable_scoreboard->current.value)
	{
		nop(T4M::GetAddress("CG_CheckHudObjectiveDisplay"), 5); // disable CG_CheckHudObjectiveDisplay call
		nop(T4M::GetAddress("jmp_onlinegame_dvar_check"), 2); // disable jmp for onlinegame dvar check
	}

	if (disable_intro->current.value)
	{
		nop(T4M::GetAddress("intro_video_call"), 5);	// don't play intro video
	}

	if (con_external->current.value)
		__asm call Sys_ShowConsole_f
}

void FilterConsoleSpam()
{
	nop(T4M::GetAddress("Com_Printf_call_."), 5); // disable Com_Printf call for "."
	nop(T4M::GetAddress("Com_Printf_call_1"), 5); // ^^
	nop(T4M::GetAddress("Com_Printf_call_2"), 5); // ^^
	nop(T4M::GetAddress("Com_Printf_call_3"), 5); // ^^
	// --- had to split this apart since the "." was used for other parts of code, i.e. was causing the "." to show in mods list
	nop(T4M::GetAddress("Com_Printf_call_DebugReportProfileDVars"), 5); // disable DebugReportProfileDVars call
	nop(T4M::GetAddress("Com_Printf_call_4"), 5); // ^^
	nop(T4M::GetAddress("Com_Printf_call_ragdoll"), 5); // disable Com_Printf call for "ragdoll allocation failed"
	nop(T4M::GetAddress("Com_Printf_call_g_numFriends"), 5); // disable Com_Printf call for "g_numFriends is now %i" (internal)
	nop(T4M::GetAddress("Com_Printf_call_nulling_friend"), 5); // disable Com_Printf call for "nulling invite info for friend %s"
	nop(T4M::GetAddress("Com_Printf_call_updating_profile_friend"), 5); // disable Com_Printf call for "updating profile info for friend %s"
	nop(T4M::GetAddress("Com_Printf_call_failed_log"), 5); // disable Com_Printf call for "Failed to log on."
	nop(T4M::GetAddress("Com_Printf_call_5"), 5); // ^^
	nop(T4M::GetAddress("Com_Printf_call_build_version_in_Com_Init_Try_Block_Function"), 5); // disable Com_Printf call for build version in Com_Init_Try_Block_Function
	nop(T4M::GetAddress("Com_Printf_call_upload_bandwidth"), 5); // disable Com_Printf call for "Upload Bandwidth:~"
	nop(T4M::GetAddress("Com_Printf_call_download_bandwidth"), 5); // disable Com_Printf call for "Download Bandwidth:~"
	nop(T4M::GetAddress("Com_sprintf_call_dvar_set"), 5); // disable Com_sprintf call for "dvar set"
	nop(T4M::GetAddress("Com_PrintMessage_call_dvar_set"), 5); // disable Com_PrintMessage call for "dvar set"
}

void PatchT4_ExternalConsole()
{
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("ShowExternalConsole_callsite"), (uintptr_t)&ShowExternalConsole, Detours::X86Option::USE_CALL);
}

void PatchT4_ConsoleBox()
{	
	// call our functionality to draw another line
	callp(T4M::GetAddress("drawDetailedDvarMatch_callsite"), drawDetailedDvarMatchStub, PATCH_CALL);
	*(BYTE*)T4M::GetAddress("con_box_lineCount") = 3; // increase line number for box
}

void testCmd_f()
{
	T4::engine::Com_Printf(0, "Surprise motherfucker. You have: ^3%i ^7args passed\n", T4::engine::Cmd_Argc() - 1);
}

void __cdecl DB_ListAssetPool_f()
{
	char* v0; // eax@4
	char* v1; // eax@6
	XAssetType type; // ST24_4@6
	signed int i; // [sp+10h] [bp-Ch]@2

	unsigned int* g_poolSize = (unsigned int*)T4M::GetAddress("g_poolSize");

	if (T4::engine::Cmd_Argc() >= 2)
	{
		v1 = T4::engine::Cmd_Argv(1);
		type = (XAssetType)atoi(v1);
		T4M::DB_ListAssetPool(type, false);
	}
	else
	{
		T4::engine::Com_Printf(0, "listassetpool <poolnumber>: lists all the assets in the specified pool\n");
		for (i = 0; i < ASSET_TYPE_MAX; ++i)
		{
			v0 = T4M::DB_GetXAssetTypeName(i);
			T4::engine::Com_Printf(0, "%d %s %i\n", i, v0, g_poolSize[i]);
		}
	}
}

void DB_ListAssetCounts_f()
{
	T4::engine::Com_Printf(0, "Listing assets in all pools.\n");

	for (int i = 0; i < ASSET_TYPE_MAX; ++i)
	{
		T4M::DB_ListAssetPool((XAssetType)i, true);
	}
}

extern "C" void __cdecl T4M_DumpConfigStrings(int start, int end); // PatchT4MAM_ConfigStrings.cpp

// listconfigstrings [start] [end] — dump the live config-string table (hex args ok, e.g. 0xBF0 0xFF0).
void ListConfigStrings_f()
{
	int start = -1, end = -1;
	if (T4::engine::Cmd_Argc() >= 2) start = (int)strtol(T4::engine::Cmd_Argv(1), 0, 0);
	if (T4::engine::Cmd_Argc() >= 3) end   = (int)strtol(T4::engine::Cmd_Argv(2), 0, 0);
	T4M_DumpConfigStrings(start, end);
}

void NullFunction()
{
}

void EnableVulkan()
{
	LPCSTR Str1;
	CHAR buf[255];
	wsprintf(buf, "%u", 1);
	Str1 = buf;

	WritePrivateProfileString("Options", "EnableVulkan", Str1, CONFIG_FILE_LOCATION);
}

void DisableVulkan()
{
	LPCSTR Str1;
	CHAR buf[255];
	wsprintf(buf, "%u", 0);
	Str1 = buf;

	WritePrivateProfileString("Options", "EnableVulkan", Str1, CONFIG_FILE_LOCATION);
}

void SwitchModes()
{
	if (is_watching_for_switch_mode_input->current.boolean == true)
	{
		switch_mode_input_pressed->current.boolean = true;
	}
}

void CL_ResetViewport();

void Cmd_Init_T4()
{
	DWORD Cmd_Init_T4 = T4M::GetAddress("Com_StartupVariables");

	__asm call Cmd_Init_T4

	ForceLogfileEnabled();

	//Cmd_AddCommand("testcmd", testCmd_f);
	T4M::Cmd_AddCommand("listassetpool", DB_ListAssetPool_f);
	T4M::Cmd_AddCommand("listassetcounts", DB_ListAssetCounts_f);
	T4M::Cmd_AddCommand("listconfigstrings", ListConfigStrings_f);
	// UINT disableIntro = GetPrivateProfileInt("Options", "DisableIntro", 0, CONFIG_FILE_LOCATION);
	// if (disableIntro == 1)
	// {
	// 	nop(0x59D68B, 5);	// don't play intro video
	// }

	T4M::Cmd_AddCommand("enable_vulkan", EnableVulkan);
	T4M::Cmd_AddCommand("disable_vulkan", DisableVulkan);
	T4M::Cmd_AddCommand("switch_modes", SwitchModes);
	//Cmd_AddCommand("load_t4m", LoadConfig);
	T4M::Cmd_AddCommand("resetviewport", CL_ResetViewport);
}

void ShitTest()
{
	for (int i = 0; i < ASSET_TYPE_MAX; ++i)
	{
		T4M::DB_ListAssetPool((XAssetType)i, true);
	}
}


void PatchT4_ConsoleCommands()
{
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Cmd_Init_callsite"), (uintptr_t)&Cmd_Init_T4, Detours::X86Option::USE_CALL);
	//Detours::X86::DetourFunction((PBYTE)0x00608D16, (PBYTE)&ShitTest, Detours::X86Option::USE_CALL);
}

const char* Draw_G_Ents()
{
	int entityCount = *(WORD*)T4M::GetAddress("numGEntities"); // or known as numGEntities or max_ents, it's the highest amount of entities loaded
	gentity_s* freeEntity = *(gentity_s**)T4M::GetAddress("e"); // or known as the 'e' from t6r
	const char* s;

	while (freeEntity)
	{
		entityCount--;
		freeEntity = (gentity_s*)freeEntity->nextFree;
	}

	s = va("%i/2047", entityCount + 1); // current / max (expanded from 1023 to 2047 by T4M entity pool patch)

	return s;
}

void PatchT4_GetGEnts()
{
	// override the lvl free msg
	PatchMemory(T4M::GetAddress("lvl_free_msg"), (PBYTE)" total ents", 11);
	// detour the va call
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Draw_G_Ents_callsite"), (uintptr_t)&Draw_G_Ents, Detours::X86Option::USE_CALL);
	// remove verbose from cg_drawfps array, ends due to null terminator
	*(DWORD*)T4M::GetAddress("cg_drawfps_array_verbose") = 0;
	// change jl to jmp, never executes cg_drawfps 3
	PatchMemory(T4M::GetAddress("stop_cg_drawfps_3"), (PBYTE)"\xE9\x12\x03\x00\x00", 5);
}

void PatchT4_Console()
{
	con_external = T4::dvar::Dvar_RegisterBool(0, "con_external", T4::dvar::DVAR_FLAG_ARCHIVE, "Enable the external console (requires restart).");
	enable_scoreboard = T4::dvar::Dvar_RegisterBool(0, "enable_scoreboard", T4::dvar::DVAR_FLAG_ARCHIVE, "Enable the scoreboard in solo play (requires restart).");
	disable_intro = T4::dvar::Dvar_RegisterBool(0, "disable_intro", T4::dvar::DVAR_FLAG_ARCHIVE, "Show the intro video.");
	is_watching_for_switch_mode_input = T4::dvar::Dvar_RegisterBool(0, "i_watching_sm_input", T4::dvar::DVAR_FLAG_CHANGEABLE_RESET, "Hackou boolean to help register a new input.");
	switch_mode_input_pressed = T4::dvar::Dvar_RegisterBool(0, "i_sm_pressed", T4::dvar::DVAR_FLAG_CHANGEABLE_RESET, "Hackou boolean to help register a new input.");
	loadout_preset_usa = T4::dvar::Dvar_RegisterInt(0, "loadout_preset_usa", 0, 25, T4::dvar::DVAR_FLAG_ARCHIVE, "Parameter for loadoutsetup");
	loadout_preset_rus = T4::dvar::Dvar_RegisterInt(0, "loadout_preset_rus", 0, 25, T4::dvar::DVAR_FLAG_ARCHIVE, "Parameter for loadoutsetup");

	*(BYTE*)T4M::GetAddress("ingame_console_enable") = 0xEB; // force enable ingame console
	FilterConsoleSpam();

	PatchT4_GetGEnts();
	PatchT4_ConsoleCommands();
	PatchT4_ConsoleBox();
	PatchT4_ExternalConsole();
}