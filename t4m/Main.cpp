// ==========================================================
// project 'secretSchemes'
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Manages the initialization of t4cli.
//
// Initial author: NTAuthority
// Adapated: 2015-07-08
// Started: 2011-05-04
// ==========================================================

#include "SDLLP.h"
#include "StdInc.h"
#include <sstream>

void Sys_RunInit();

static BYTE originalCode[5];
static PBYTE originalEP = 0;

void Main_UnprotectModule(HMODULE hModule)
{
	PIMAGE_DOS_HEADER header = (PIMAGE_DOS_HEADER)hModule;
	PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD)hModule + header->e_lfanew);

	// unprotect the entire PE image
	SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
	DWORD oldProtect;
	VirtualProtect((LPVOID)hModule, size, PAGE_EXECUTE_READWRITE, &oldProtect);
}

void Main_DoInit()
{
	// return to the original EP
	memcpy(originalEP, &originalCode, sizeof(originalCode));

	// unprotect our entire PE image
	HMODULE hModule;
	if (SUCCEEDED(GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)Main_DoInit, &hModule)))
	{
		Main_UnprotectModule(hModule);
	}
	
	// Check if config file exist
	if (!FileExists(va(CONFIG_FILE_LOCATION)))
	{
		const char* path = CONFIG_FILE_LOCATION;
		
		std::ofstream file (path, std::ofstream::out);
		file << DEFAULT_CONFIG_FILE;

		file.close();
	}
	// If it exist, check his version, if revision is < at current, update
	else
	{
		UINT versionNumber = GetPrivateProfileInt("Version", "Number", 0, CONFIG_FILE_LOCATION);

		// Check and retrieve applicable value for next version
		if (versionNumber < INTERNAL_VERSION_NUMBER)
		{
			const char* path = CONFIG_FILE_LOCATION;

			std::ofstream file(path, std::ofstream::out);
			file << DEFAULT_CONFIG_FILE;

			file.close();
		}
		else if (versionNumber > INTERNAL_VERSION_NUMBER)
		{
			const char* path = CONFIG_FILE_LOCATION;

			std::ofstream file(path, std::ofstream::out);
			file << DEFAULT_CONFIG_FILE;

			file.close();
		}
	}
	
	// load in another dll when attempting to run two d3d9s, such as ReShade (if installed, and must be renamed to d3d9r.dll, otherwise this does nothing)
	LoadLibrary("d3d9r.dll");

	IsUsingVulkan = SDLLP::UseVulkan();

	Sys_RunInit();

	hModule = GetModuleHandle(NULL);
	PIMAGE_DOS_HEADER header = (PIMAGE_DOS_HEADER)hModule;
	PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD)hModule + header->e_lfanew);

	// back up original code
	originalEP = (PBYTE)((DWORD)hModule + ntHeader->OptionalHeader.AddressOfEntryPoint);

	__asm jmp originalEP
}

void Main_SetSafeInit()
{
	// find the entry point for the executable process, set page access, and replace the EP
	HMODULE hModule = GetModuleHandle(NULL); // passing NULL should be safe even with the loader lock being held (according to ReactOS ldr.c)

	if (hModule)
	{
		PIMAGE_DOS_HEADER header = (PIMAGE_DOS_HEADER)hModule;
		PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD)hModule + header->e_lfanew);

		Main_UnprotectModule(hModule);

		// back up original code
		PBYTE ep = (PBYTE)((DWORD)hModule + ntHeader->OptionalHeader.AddressOfEntryPoint);
		memcpy(originalCode, ep, sizeof(originalCode));

		// patch to call our EP
		int newEP = (int)Main_DoInit - ((int)ep + 5);
		ep[0] = 0xE9; // for some reason this doesn't work properly when run under the debugger
		memcpy(&ep[1], &newEP, 4);

		originalEP = ep;
	}
}

//extern "C" void __declspec(dllimport) DependencyFunctionCCAPI();

bool __stdcall DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		// T4M must stay inert under the WaW mod tools (sp_tool.exe / mp_tool.exe):
		// they load this DLL but are not the game. Checked BEFORE the 0x401000 sig
		// read so neither activation nor the "incompatible exe" MessageBox fires there.
		if (T4M::IsModToolProcess())
			return true;

		DWORD sig = *(DWORD*)0x401000;
		if (sig == T4M::ExeVariant::LanFixed || sig == T4M::ExeVariant::SteamDefault || sig == T4M::ExeVariant::SteamGer) // LanFixed | Steam | Steam-GER
		{
			// Detect & cache the exe variant (default/GER) BEFORE PatchT4_SteamDRM decrypts .text.
			T4M::SetExeVariant(sig);
			T4M::AddrMap_Load(true);
			T4M::InitASMRef();
			Main_SetSafeInit();
		}
		else
		{
			// We don't support Steam MP for now, but we know they exist, so don't throw an error at user
			if (sig != T4M::ExeVariant::SteamDefaultMP && sig != T4M::ExeVariant::SteamGerMP)
			{
				MessageBoxA(0, "Incompatible Call of Duty World at War exe.\nT4M will not be loaded.\nCheck if you have updated or contact dev to tell them about this exe"
					, "Error", MB_OK);
			}
		}
	}
	return true;
}
