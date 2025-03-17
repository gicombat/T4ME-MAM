// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: branding for T4M
//
// Initial author: DidUknowiPwn
// Started: 2015-07-12
// ==========================================================

#include "StdInc.h"
#include "SDLLP.h"


const char* SetConsoleVersion()
{
	UINT isVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	if (isVulkan == 1)
	{
#ifdef IS_BETA
		return va("CoD WaW %s", VERSION_BETA_VULKAN_STR);	
#else
		return va("CoD WaW %s", VERSION_VULKAN_STR);		
#endif		
	}
	else
	{
#ifdef IS_BETA
		return va("CoD WaW %s", VERSION_BETA_STR);	
#else
		return va("CoD WaW %s", VERSION_STR);	
#endif		
	}
}

const char* SetShortVersion()
{
	UINT isVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	if (isVulkan == 1)
	{
#ifdef IS_BETA
		return va(SHORTVERSION_BETA_VULKAN_STR);
#else
		return va(SHORTVERSION_VULKAN_STR );
#endif		
	}
	else
	{
#ifdef IS_BETA
		return va(SHORTVERSION_BETA_STR);
#else
		return va(SHORTVERSION_STR);
#endif		
	}
}

void PatchT4_Branding()
{
	// TODO: Replace shortversion DVars and other version related locations
	UINT disableIntro = GetPrivateProfileInt("Options", "DisableIntro", 0, CONFIG_FILE_LOCATION);
	if (disableIntro == 1)
	{
		nop(0x59D68B, 5);	// don't play intro video
	}

	UINT isVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	if (isVulkan == 1)
	{
		nop(0x5FD91B, 5);										// disable pc_newversionavailable check
#ifdef IS_BETA
		PatchMemory(0x851208, (PBYTE)CONSOLEVERSION_BETA_VULKAN_STR, 14);	// change the console input version
		PatchMemory(0x871EE8, (PBYTE)va("T4Me-MAM-SP r%i%s\n", VERSION, BETA), 32);
#else		
		PatchMemory(0x851208, (PBYTE)CONSOLEVERSION_VULKAN_STR, 14);	// change the console input version
		PatchMemory(0x871EE8, (PBYTE)va("T4Me-MAM-SP %i\n", VERSION), 32);
#endif
		Detours::X86::DetourFunction((uintptr_t)0x5B5A20, (uintptr_t)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
		Detours::X86::DetourFunction((uintptr_t)0x4743D2, (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
		Detours::X86::DetourFunction((uintptr_t)0x59D393, (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 

	}
	else
	{
		nop(0x5FD91B, 5);									// disable pc_newversionavailable check
#ifdef IS_BETA
		PatchMemory(0x851208, (PBYTE)CONSOLEVERSION_BETA_STR, 14);	// change the console input version
		PatchMemory(0x871EE8, (PBYTE)va("T4Me-MAM-SP r%i%s\n", VERSION, BETA), 32);
#else		
		PatchMemory(0x851208, (PBYTE)CONSOLEVERSION_STR, 14);	// change the console input version
		PatchMemory(0x871EE8, (PBYTE)va("T4Me-MAM-SP %i\n", VERSION), 32);
#endif
		Detours::X86::DetourFunction((uintptr_t)0x5B5A20, (uintptr_t)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
		Detours::X86::DetourFunction((uintptr_t)0x4743D2, (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
		Detours::X86::DetourFunction((uintptr_t)0x59D393, (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 
	}
}