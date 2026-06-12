// ==========================================================
// T4M project
// 
// Component: clientdll
// Purpose: Dvar modifications, from changing flags to 
//          restricting changes
//
// Initial author: Pigophone / NTAuthority (copied from 
//                 PatchMW2.cpp)
// Adapated: 2015-07-10
// Started: 2012-04-19
// ==========================================================

#include "StdInc.h"

float cgFov90 = 90.0f;

void PatchT4_Dvars()
{
	//DVAR: cg_fov
	//MODS: Clear cheat flag, set archive flag, set maximum to 90
	*(WORD*)T4M::GetAddress("cg_fov_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;
	*(float**)T4M::GetAddress("cg_fov_valuePtr_site") = &cgFov90;

	//DVAR: cg_fovMin
	//MODS: Clear cheat flag, set archive flag
	*(WORD*)T4M::GetAddress("cg_fovMin_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;

	//DVAR: cg_fovScale
	//MODS: Clear cheat flag, set archive flag
	*(WORD*)T4M::GetAddress("cg_fovScale_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;

	//DVAR: r_lodBiasRigid
	//MODS: Clear cheat flag, set archive flag
	*(WORD*)T4M::GetAddress("r_lodBiasRigid_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;

	//DVAR: r_lodBiasSkinned
	//MODS: Clear cheat flag, set archive flag
	*(WORD*)T4M::GetAddress("r_lodBiasSkinned_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;

	//DVAR: ui_hud_hardcore
	//MODS: Clear cheat flag, set archive flag
	*(WORD*)T4M::GetAddress("ui_hud_hardcore_flags_site") ^= DVAR_FLAG_CHEAT | DVAR_FLAG_ARCHIVE;

	//DVAR: fs_basegame
	//MODS: Clear cheat flag, set archive flag
	//*(WORD*)0x005DDEDD ^= DVAR_FLAG_INIT;
}