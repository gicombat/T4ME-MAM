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
	IsUsingVulkan = SDLLP::UseVulkan();
	if (IsUsingVulkan == 1)
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
	IsUsingVulkan = SDLLP::UseVulkan();
	if (IsUsingVulkan == 1)
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
	//UINT disableIntro = GetPrivateProfileInt("Options", "DisableIntro", 0, CONFIG_FILE_LOCATION);
	//if (disableIntro == 1)
	//{
	//	nop(0x59D68B, 5);	// don't play intro video
	//}

	nop(T4M::GetAddress("pc_newversionavailable_check"), 5); // disable pc_newversionavailable check

	IsUsingVulkan = SDLLP::UseVulkan();
	if (IsUsingVulkan == 1)
	{
		#ifdef IS_BETA
		PatchMemory(T4M::GetAddress("console_input_version"), (PBYTE)CONSOLEVERSION_BETA_VULKAN_STR, 14);	// change the console input version
		PatchMemory(T4M::GetAddress("version_banner"), (PBYTE)va("T4Me-MAM-SP r%i%s\n", VERSION, BETA), 32);
#else		
		PatchMemory(T4M::GetAddress("console_input_version"), (PBYTE)CONSOLEVERSION_VULKAN_STR, 14);	// change the console input version
		PatchMemory(T4M::GetAddress("version_banner"), (PBYTE)va("T4Me-MAM-SP %i\n", VERSION), 32);
#endif
		Detours::X86::DetourFunction(T4M::GetAddress("shortVersion_call_site"), (uintptr_t)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
		Detours::X86::DetourFunction(T4M::GetAddress("consoleVersionWindow_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
		Detours::X86::DetourFunction(T4M::GetAddress("versionInfo_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 
	}
	else
	{
#ifdef IS_BETA
		PatchMemory(T4M::GetAddress("console_input_version"), (PBYTE)CONSOLEVERSION_BETA_STR, 14);	// change the console input version
		PatchMemory(T4M::GetAddress("version_banner"), (PBYTE)va("T4Me-MAM-SP r%i%s\n", VERSION, BETA), 32);
#else		
		PatchMemory(T4M::GetAddress("console_input_version"), (PBYTE)CONSOLEVERSION_STR, 14);	// change the console input version
		PatchMemory(T4M::GetAddress("version_banner"), (PBYTE)va("T4Me-MAM-SP %i\n", VERSION), 32);
#endif
		Detours::X86::DetourFunction(T4M::GetAddress("shortVersion_call_site"), (uintptr_t)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
		Detours::X86::DetourFunction(T4M::GetAddress("consoleVersionWindow_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
		Detours::X86::DetourFunction(T4M::GetAddress("versionInfo_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 
	}
}
