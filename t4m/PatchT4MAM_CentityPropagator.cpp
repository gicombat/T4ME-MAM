// ==========================================================
// T4M project — PatchT4MAM_CentityPropagator.cpp
//
// Detour of sub_410660 — the function that propagates fields from a
// playerState_s (eax) to a centity_t-style struct (ecx) once per frame.
//
// Bug : vanilla reads ps.weapon as BYTE at 0x004106B6
//   movzx edx, byte ptr [eax+104h]   ; edx = ps.weapon & 0xFF
//   mov   [ecx+0E0h], edx             ; centity[+0xE0] = truncated idx
// → idx ≥ 256 truncated. With T4M extending the WEAPON pool to 512, idx
// 256..511 wrap to 0..255 here, then the rendering pipeline picks the wrong
// weapon. DR0 watchpoint at centity+0x1B0 (= unk_351FFFC + 0x1B0) confirmed
// this is the truncation site (cf. plan_weapons_full_detour.md §4 P1).
//
// Fix : reconstruct the full function body in C++ with `mov edx, [eax+0x104]`
// (full 32-bit read) instead of `movzx edx, byte ptr`. All other reads stay
// faithful to vanilla.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>
#include <cstdint>

namespace T4_Reconstructed
{
    // @faithful — matches sub_410660 vanilla 1:1 except the marked dword read.
    extern "C" void __cdecl CG_PropagatePsWeapon(void* ps, void* ce)
    {
        auto p_ps = (uint8_t*)ps;
        auto p_ce = (uint8_t*)ce;

        *(uint32_t*)(p_ce + 0xF0) = *(uint32_t*)(p_ps + 0xA4);
        *(uint32_t*)(p_ce + 0xF4) = *(uint32_t*)(p_ps + 0xAC);
        *(float*)   (p_ce + 0x54) = *(float*)   (p_ps + 0x74);

        if (*(uint32_t*)(p_ps + 0xCC) & 0x300) {
            *(uint32_t*)(p_ce + 0x98) = *(uint32_t*)(p_ps + 0x83C);
        }

        // Vanilla : `mov dl, [eax+14h]; and dl, 6; neg dl; sbb edx, edx;
        //           and edx, FFFFFFFCh; add edx, 5` → 1 if (flag14 & 6) else 5.
        uint8_t flag14 = *(uint8_t*)(p_ps + 0x14);
        *(uint32_t*)(p_ce + 4) = (flag14 & 6) ? 1 : 5;

        *(uint32_t*)(p_ce + 0xAC) = *(uint32_t*)(p_ps + 0xF8);

        // @modified — was `movzx edx, byte ptr [eax+0x104]` (truncates idx ≥ 256).
        // Read full 32-bit ps.weapon. Required for T4M weapon pool ≥ 256.
        uint32_t weaponIdx = *(uint32_t*)(p_ps + 0x104);
        *(uint32_t*)(p_ce + 0xE0) = weaponIdx;

        // Indexed byte read at ps[ps.weapon + 0xB50]. Vanilla already uses the
        // full dword for this offset calc; we just reuse weaponIdx.
        *(uint32_t*)(p_ce + 0xE4) = *(uint8_t*)(p_ps + weaponIdx + 0xB50);

        *(uint32_t*)(p_ce + 0x9C) = *(uint16_t*)(p_ps + 0x88);
    }
}

namespace T4M
{
    // __usercall wrapper : sub_410660 receives ps in eax, ce in ecx ; no stack
    // args. Push them as __cdecl args, call the reconstruction, then retn.
    extern "C" __declspec(naked) void CG_PropagatePsWeapon_Wrapper()
    {
        __asm {
            push    ecx                                 // arg_4 = ce
            push    eax                                 // arg_0 = ps
            call    T4_Reconstructed::CG_PropagatePsWeapon
            add     esp, 8
            retn
        }
    }
}

void PatchT4MAM_CentityPropagator()
{
    static auto detour = safetyhook::create_inline(
        (void*)0x00410660,
        (void*)&T4M::CG_PropagatePsWeapon_Wrapper);
    (void)detour;
}
