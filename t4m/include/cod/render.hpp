#pragma once

namespace T4
{
	namespace engine
	{
		// sub_6D6AD0 — renderer init. Called from Com_PostInit_Video (sub_644BE0+0x7D).
		WEAK symbol<void()>R_Init{ "R_Init" };
		WEAK symbol<void()> R_BeginRegistration{ "R_BeginRegistration" };
		WEAK symbol<void()> R_ClearScene{ "R_ClearScene" };
		WEAK symbol<void()> CL_BeginRegistration{ "CL_BeginRegistration" };
		WEAK symbol<void()> CL_ClearState{ "CL_ClearState" };

		// sub_6FC7A0 — called by R_InitGlobalStructs, between the two memsets
		// and the identity setup (CoD4 gfx_d3d/r_init.cpp:4179).
		WEAK symbol<void()> RB_InitBackendGlobalStructs{ "RB_InitBackendGlobalStructs" };

		// Renderer global structs. sizeof(rg) = 0x2430, sizeof(rgp) = 0x2280.
		//   rg  + 0x00 .. 0xFF   identityViewParms: 4 consecutive mat4x4
		//   rg  + 0x164          materialHashTable[2048]
		//   rg  + 0x234C         identityPlacement: quat[4], origin[3], scale
		//   rgp + 0x00           sortedMaterials[2048]  (RELOCATED, see PatchT4MemoryLimits.cpp)
		//   rgp + 0x2004         materialCount
		WEAK symbol<BYTE> rg{ "rg" };
		WEAK symbol<BYTE> rgp{ "rgp" };
		WEAK symbol<float> rg_identityPlacement{ "rg_identityPlacement" };
		WEAK symbol<const float> identityMatrix44{ "identityMatrix44" };

		// Set by R_InitGlobalStructs; no equivalent in the CoD4 body — WaW-only
		// state, subsystem not identified.
		WEAK symbol<DWORD> r_globals_3BED830{ "r_globals_3BED830" };
		WEAK symbol<DWORD> r_globals_3DCB4D0{ "r_globals_3DCB4D0" };
		WEAK symbol<DWORD> r_globals_463E3C8{ "r_globals_463E3C8" };
	}
} // namespace T4::engine
