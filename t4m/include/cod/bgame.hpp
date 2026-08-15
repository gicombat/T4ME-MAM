#pragma once

// BG_* / G_* gameplay functions. T4::game per the namespace bucketing rule.

namespace T4
{
	namespace game
	{
		// Resolves a weapon name to its bg_weaponDefs index.
		WEAK engine::symbol<int(const char* name)>BG_FindWeaponIndex{ "BG_FindWeaponIndex" };

		// Same, but takes the loader used when precaching is still open (g_precacheOpen != 0).
		WEAK engine::symbol<int(const char* name, void* loader)>BG_FindWeaponIndex_Internal{ "BG_FindWeaponIndex_Internal" };

		// Passed as the loader callback above; never called directly.
		WEAK engine::symbol<void(int index)>BG_LoadWeaponByIndex{ "BG_LoadWeaponByIndex" };
	}
}
