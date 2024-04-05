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
			Com_Printf(0, "localization.txt exist but could not be open, please reload mod to try again loading config\n");
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
					locale = line.c_str();
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

void __cdecl FFLoadHook(XZoneInfo *zoneInfo, int zoneCount, int sync)
{
	static XZoneInfo *ffZoneInfo;

	Com_Printf(0, "FF HOOK READ");
	for (int i = 0; i < zoneCount; ++i)
	{
		Com_Printf(0, zoneInfo[i].name);
		Com_Printf(0, "\n");
		Com_Printf(0, std::to_string(zoneInfo[i].allocFlags).c_str());
		Com_Printf(0, "\n");
		Com_Printf(0, std::to_string(zoneInfo[i].freeFlags).c_str());
		Com_Printf(0, "\n");
	}
	// in t4 mods are loaded from appdata not base game
	//if (FileExists(va("%s\\zone\\t4m_patch.ff", (*fs_basepath)->current.string)))
	//{
	//	ffZoneInfo = new XZoneInfo[zoneCount];
	//	memset(ffZoneInfo, 0, sizeof(XZoneInfo) * (zoneCount)); // is needed, causes mod_ex to freeze w/o it 
	//	for (int i = 0; i < zoneCount; ++i)
	//	{
	//		ffZoneInfo[i].name = zoneInfo[i].name;
	//		ffZoneInfo[i].allocFlags = zoneInfo[i].allocFlags;
	//		ffZoneInfo[i].freeFlags = zoneInfo[i].freeFlags;
	//	}
	//	// if game freezes on startup mod_ex might be bad.
	//	ffZoneInfo[zoneCount].name = "t4m_patch";
	//	ffZoneInfo[zoneCount].allocFlags = 2048; // allocFlags indicate what loading method to go through, 2048 matches code_post_gfx/mod from 006D5672
	//	ffZoneInfo[zoneCount].freeFlags = 0;

	//	DB_LoadXAssets(ffZoneInfo, zoneCount + 1, sync); // +1 to count
	//}
	//else
	//{
		DB_LoadXAssets(zoneInfo, zoneCount, sync);
	//}
}

void PatchT4_Load()
{
	// to be used?
	//Detours::X86::DetourFunction((PBYTE)0x006D5728, (PBYTE)&FFLoadHook, Detours::X86Option::USE_CALL);
	Detours::X86::DetourFunction((PBYTE)0x006D5672, (PBYTE)&ModFFLoadHook, Detours::X86Option::USE_CALL);
	//00644C5D, r_init
	//if ((*dedicated)->current.integer > 0)
	//nop(0x00644C5D, 5);
}