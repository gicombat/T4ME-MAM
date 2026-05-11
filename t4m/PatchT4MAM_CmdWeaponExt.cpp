// ==========================================================
// T4M project — PatchT4MAM_CmdWeaponExt.cpp
//
// Phase 1a — extension du field weapon de l'usercmd vanilla (byte-sized) à
// 16 bits via le padding `usercmd_s+0x1B` (ex-alignment slot, défini comme
// `weapon_high` dans cod/structs.hpp).
//
// Vanilla : CL_FinishMove (sub_63E4C0) écrit `cmd.weapon = byte ptr dword_307D674`
// (= byte truncation pour idx ≥ 256). Le full dword est intact dans
// `dword_307D674` (chemin nominal) ou `dword_307D6E8` (chemin alt-cmd injection).
//
// Phase 1a (ce fichier) installe 2 SafetyHook midhooks dans CL_FinishMove qui
// écrivent le high byte au padding `cmd.weapon_high = (full_idx >> 8) & 0xFF`
// après chaque byte write vanilla. Ça donne accès à idx 0..0xFFFF en local
// (SP / Host) lorsque combiné avec `cmd.weapon` low byte par le reader.
//
// Phase 1b (à venir) — détour MSG_WriteDeltaUsercmd / MSG_ReadDeltaUsercmd
// pour sérialiser/désérialiser le high byte sur le wire (= support Coop/Zombie
// clients connectés). Pas dans ce fichier pour l'instant.
//
// Long-term : voir plans/plan_usercmd_weapon_word_extension.md pour la refonte
// `usercmd_s.weapon` byte → word/dword.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <safetyhook.hpp>
#include <cstdint>

namespace T4M
{
    static void InstallCmdWeaponHighFiller()
    {
        // Path 1 (chemin nominal) — vanilla `mov [edx+14h], al` à 0x0063E4CB
        // (al = byte ptr dword_307D674). Notre midhook fire à l'instruction
        // suivante 0x0063E4CE, lit le full dword et écrit son high byte.
        static auto h1 = safetyhook::create_mid(0x0063E4CE, [](SafetyHookContext& ctx) {
            uint32_t fullIdx = *(uint32_t*)0x0307D674;
            *(uint8_t*)(ctx.edx + 0x1B) = (uint8_t)(fullIdx >> 8);
        });
        (void)h1;

        // Path 2 (alt-cmd injection — déclenché par `dword_307D6E0 != 0`).
        // Vanilla `mov [edx+14h], al` à 0x0063E575 (al = byte ptr dword_307D6E8,
        // déjà chargé en eax full dword juste avant). Midhook à 0x0063E578.
        static auto h2 = safetyhook::create_mid(0x0063E578, [](SafetyHookContext& ctx) {
            uint32_t fullIdx = *(uint32_t*)0x0307D6E8;
            *(uint8_t*)(ctx.edx + 0x1B) = (uint8_t)(fullIdx >> 8);
        });
        (void)h2;
    }
}

void PatchT4MAM_CmdWeaponExt()
{
    T4M::InstallCmdWeaponHighFiller();
}
