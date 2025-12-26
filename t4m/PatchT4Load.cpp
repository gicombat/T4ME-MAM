// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: various loading changes for T4M
//
// Initial author: DidUknowiPwn
// Started: 2015-10-06
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <sstream>

dvar_t** fs_localAppData = (dvar_t**)0x2122AF0;
dvar_t** fs_game = (dvar_t**)0x2122B00;
dvar_t** fs_basepath = (dvar_t**)0x02123C14;
dvar_t** dedicated = (dvar_t**)0x0212B2F4;

void makeFFLoadStruct(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	static XZoneInfo *newZoneInfo;

	newZoneInfo = new XZoneInfo[zoneCount];
	memset(newZoneInfo, 0, sizeof(XZoneInfo) * zoneCount);

	// continue this when better understood
}

void __cdecl ModFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	static XZoneInfo* modZoneInfo;
	bool load_modEx = false;
	bool load_modPatch = false;
	bool load_modEx_Patch = false;
	bool load_localized_mod = false;
	int totalZoneCount = zoneCount;
	std::string locale = "english";


	if (FileExists(va("%s\\localization.txt", (*fs_basepath)->current.string)))
	{
		const char* path = va("%s\\localization.txt", (*fs_basepath)->current.string);

		std::ifstream file(path);
		if (!file.is_open())
		{
			Com_Printf(0, "localization.txt exist but could not be open, please reload mod to try again loading localization config\n");
		}
		else
		{
			Com_Printf(0, "Reading localization.txt File\n");
			std::string line = "";
			int lineNumber = 0;
			while (std::getline(file, line))
			{
				if (lineNumber == 0)
				{
					locale = line;
				}
				lineNumber++;
			}

			file.close();

			if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string, locale.c_str())))
			{
				load_localized_mod = true;
				totalZoneCount = totalZoneCount + 1;
			}
		}
	}

	// try to fallback to localized_english_mod.ff if we haven't didn't succeed reading localization.txt or we haven't found a corresponding localized_mod.ff file
	if (load_localized_mod == false)
	{
		locale = "english";
		if (FileExists(va("%s\\%s\\localized_%s_mod.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string, locale)))
		{
			load_localized_mod = true;
			totalZoneCount = totalZoneCount + 1;
		}
	}
 
	// in cod waw mods are loaded from appdata not base game
	if (FileExists(va("%s\\%s\\mod_ex.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modEx = true;
		totalZoneCount = totalZoneCount + 1;
	}
	if (FileExists(va("%s\\%s\\mod_patch.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modPatch = true;
		totalZoneCount = totalZoneCount + 1;

	}
	if (FileExists(va("%s\\%s\\mod_ex_patch.ff", (*fs_localAppData)->current.string, (*fs_game)->current.string)))
	{
		load_modEx_Patch = true;
		totalZoneCount = totalZoneCount + 1;

	}

	// LOAD LOCALIZED FILE BY
	// FIRST READING localiztion.txt
	// then load a .ff localized_languag_mod.ff

	if (!load_modPatch && !load_modEx && !load_modEx_Patch && !load_localized_mod)
	{
		DB_LoadXAssets(zoneInfo, zoneCount, sync);
	}
	else
	{
		modZoneInfo = new XZoneInfo[totalZoneCount];
		memset(modZoneInfo, 0, sizeof(XZoneInfo) * (totalZoneCount)); // is needed, causes mod_ex to freeze w/o it 
		for (int i = 0; i < zoneCount; ++i)
		{
			modZoneInfo[i].name = zoneInfo[i].name;
			modZoneInfo[i].allocFlags = zoneInfo[i].allocFlags;
			modZoneInfo[i].freeFlags = zoneInfo[i].freeFlags;
		}
		int currentIndex = zoneCount;
		if (load_modEx)
		{
			modZoneInfo[currentIndex].name = "mod_ex";
			modZoneInfo[currentIndex].allocFlags = 2048; // allocFlags indicate what loading method to go through, 2048 matches code_post_gfx/mod from 006D5672
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_modPatch)
		{
			modZoneInfo[currentIndex].name = "mod_patch";
			modZoneInfo[currentIndex].allocFlags = 2048; // allocFlags indicate what loading method to go through, 2048 matches code_post_gfx/mod from 006D5672
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_modEx_Patch)
		{
			modZoneInfo[currentIndex].name = "mod_ex_patch";
			modZoneInfo[currentIndex].allocFlags = 2048; // allocFlags indicate what loading method to go through, 2048 matches code_post_gfx/mod from 006D5672
			modZoneInfo[currentIndex].freeFlags = 0;
			currentIndex = currentIndex + 1;
		}
		if (load_localized_mod)
		{
			modZoneInfo[currentIndex].name = va("localized_%s_mod", locale.c_str());
			modZoneInfo[currentIndex].allocFlags = 2048; // allocFlags indicate what loading method to go through, 2048 matches code_post_gfx/mod from 006D5672
			modZoneInfo[currentIndex].freeFlags = 0;
		}
		DB_LoadXAssets(modZoneInfo, totalZoneCount, sync);
	}
}

void __cdecl CodePostGFXFFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	// Code Post gfx is loaded after reading player var, so we need to update them with the value of the .conf, we can do this only here, so let's hack this shit
	UINT enableVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	vulkan = Dvar_RegisterBool(0, "vulkan", DVAR_FLAG_ARCHIVE, "Use vulkan instead of DirectX 9.0c (only used for UI, if you want to change please use the change in the options or in the .conf file");
	vulkan->current.boolean = enableVulkan;

	DB_LoadXAssets(zoneInfo, zoneCount, sync);
}

void PatchT4_Load()
{
	// to be used?
	Detours::X86::DetourFunction((uintptr_t)0x006D5728, (uintptr_t)&CodePostGFXFFLoadHook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction((uintptr_t)0x006D5672, (uintptr_t)&ModFFLoadHook, Detours::X86Option::USE_CALL);
	//00644C5D, r_init
	//if ((*dedicated)->current.integer > 0)
	//nop(0x00644C5D, 5);
}