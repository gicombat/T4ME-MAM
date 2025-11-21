// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: branding for T4M
//
// Initial author: DidUknowiPwn
// Started: 2015-07-12
// temp: manually set branding as r48 instead of using vars defined with COMMIT_STR
// ==========================================================

#include "StdInc.h"

const char* SetConsoleVersion();
const char* SetShortVersion();

void PatchT4_Branding()
{
	Dvar_RegisterInt(VERSION_T4ME, "version_t4me", 0, VERSION_T4ME, DVAR_FLAG_ROM);
	// TODO: Replace shortversion DVars and other version related locations
	//nop(0x59D68B, 5);										// commented out to re-enable intro video because it's cool
	nop(0x5FD91B, 5);										// disable pc_newversionavailable check
	PatchMemory(0x851208, (PBYTE)va("T4Me r%d> ", VERSION_T4ME), 14);	// change the console input version
	PatchMemory(0x871EE8, (PBYTE)va("T4-SP (r%i)\n", VERSION), 32);
	Detours::X86::DetourFunction((PBYTE)0x5B5A20, (PBYTE)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
	Detours::X86::DetourFunction((PBYTE)0x4743D2, (PBYTE)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
	Detours::X86::DetourFunction((PBYTE)0x59D393, (PBYTE)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 
}

const char* SetConsoleVersion()
{
	return va("Call of Duty %s", va("T4-SP (r%d) (built " DATE " " TIME " by Clippy95 and Nazi Zombies remastered developer JB)", VERSION_T4ME));
}

const char* SetShortVersion()
{
	return va("2.0.%d", VERSION_T4ME);
}

