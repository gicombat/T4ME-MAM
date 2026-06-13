#include "StdInc.h"

void PatchT4MP_SteamDRM()
{
	if (*(DWORD*)0x401000 != 0x7FE5ED21)
		return;

	// Replace encrypted .text segment
	DWORD size = 0x3E5000;
	std::string data = GetBinaryResource(IDB_TEXT);
	uncompress((unsigned char*)0x401000, &size, (unsigned char*)data.data(), data.size());

	// Apply new entry point
	HMODULE hModule = GetModuleHandle(NULL);
	PIMAGE_DOS_HEADER header = (PIMAGE_DOS_HEADER)hModule;
	PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD)hModule + header->e_lfanew);
	ntHeader->OptionalHeader.AddressOfEntryPoint = 0x3A8256;
}

const char* SetConsoleVersion();
const char* SetShortVersion();

void loadGameOverlay();

static dvar_t* r_noborder;
static StompHook windowedWindowStyleHook;

static void __declspec(naked) WindowedWindowStyleHookStub()
{
	if (r_noborder->current.boolean)
	{
		__asm mov ebp, WS_VISIBLE | WS_POPUP
	}
	else
	{
		__asm mov ebp, WS_VISIBLE | WS_SYSMENU | WS_CAPTION
	}

	__asm retn
}

dvar_t* Dvar_RegisterBoolMP(bool value, const char *dvarName, int flags, const char *description)
{
	DWORD _Dvar_RegisterBool = T4M::GetAddress("Dvar_RegisterBool");
	dvar_t* result = 0;

	__asm
	{
		push description
		push flags
		mov al, value
		mov edi, dvarName
		call _Dvar_RegisterBool
		add     esp, 8h
		mov result, eax
	}

	return result;
}

void PatchT4MP()
{
	//PatchT4MP_SteamDRM();

	//if (!GetModuleHandle("gameoverlayrenderer.dll"))
	//	loadGameOverlay();


	*(BYTE*)T4M::GetAddress("ingame_console_enable") = 0xEB; // force enable ingame console
	//DWORD func = 0x5D5470;
	//__asm
	//{
	//	call func
	//}


	//nop(0x564CB9, 5); // don't play intro video
	//nop(0x5CEB56, 5); // disable pc_newversionavailable check


	nop(T4M::GetAddress("mp_optimalSettingsPopup_nop_site"), 5); // remove optimal settings popup
	*(BYTE*)T4M::GetAddress("mp_safeModeCheck_site") = (BYTE)0xEB; // skip safe mode check

	PatchMemory(T4M::GetAddress("console_input_version"), (PBYTE)CONSOLEVERSION_STR, 14);	// change the console input version

	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("shortVersion_call_site"), (uintptr_t)&SetShortVersion, Detours::X86Option::USE_CALL); // change version number bottom right of main
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("consoleVersionWindow_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of console window
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("versionInfo_call_site"), (uintptr_t)&SetConsoleVersion, Detours::X86Option::USE_CALL); // change the version info of version 
}
