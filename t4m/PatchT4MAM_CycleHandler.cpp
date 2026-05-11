// ==========================================================
// T4M project — PatchT4MAM_CycleHandler.cpp
//
// Reconstruction of sub_46A090 — the CG-side "cycle to next/prev owned
// weapon" handler used by the `weapnext` / `weapprev` console commands
// (callers : sub_469DE0 / sub_469E40).
//
// Why detoured : vanilla iterates `idx = 1..total` and tests ownership
// inline via `test dword_351E74C[idx>>5*4], (1 << (idx&31))`. The base
// dword_351E74C is `cg_predictedPlayerState.weapons[0]` (4 dwords for
// idx 0..127). For idx ≥ 128, the read OOB-aliases into `weaponold[]`,
// `weaponrechamber[]`, `proneDirection`, `viewlocked` etc., picking up
// LEGITIMATE bits set by vanilla game logic and falsely reporting
// high-idx weapons as owned. The cycle then tries to switch to the
// phantom weapon — sometimes crashing on model load (eg. nagant1895_s2,
// type96_lmg_wet).
//
// Each false-positive cascades : switching to the phantom puts the
// previous weapon in `weaponold`, setting a new bit that aliases yet
// another high-idx phantom on the next scroll.
//
// This reconstruction routes the ownership test through
// T4M::PsBitTest(serverPs, 0x7FC, idx), which knows about the per-PS
// sidecar (PatchT4MAM_PsBitmapExt.cpp) for idx ≥ 128. CG-side state
// for high idx is mirrored from the server-side ps obtained via
// gentity[0]+0x180 — same source the cycle dump uses.
// ==========================================================

#include "enums.hpp"
#include "structs.hpp"
#include "xasset.hpp"
using namespace T4;
#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>
#include <cstdint>

extern T4::WeaponDef** bg_weaponDefs;

// Sidecar-aware ownership test, defined in PatchT4MAM_PsBitmapExt.cpp.
namespace T4M {
    extern "C" bool PsBitTest(playerState_s* ps, int inStructOffset, unsigned idx);
}

// =====================================================================
// Vanilla helper wrappers (kept vanilla, called from the recon).
// =====================================================================

// sub_469E90 — __usercall(edx = def) + stack(arg_0). Returns al (bool).
// Checks if current weapon's alt-fire idx is owned + applies side effects.
static __declspec(naked) int call_sub_469E90(int /*arg_0*/, void* /*def*/)
{
    __asm
    {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+0x14]    // def (arg 2)
        mov     ecx, [esp+0x10]    // arg_0 (arg 1)
        push    ecx
        mov     eax, 0x00469E90
        call    eax
        add     esp, 4
        movzx   eax, al
        pop     ebx
        pop     esi
        pop     edi
        retn
    }
}

// sub_46BDB0 — __usercall(edx = idx) + stack(arg_0). Performs the actual
// weapon switch. Returns void.
static __declspec(naked) void call_sub_46BDB0(int /*arg_0*/, unsigned /*idx*/)
{
    __asm
    {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+0x14]    // idx (arg 2)
        mov     ecx, [esp+0x10]    // arg_0 (arg 1)
        push    ecx
        mov     eax, 0x0046BDB0
        call    eax
        add     esp, 4
        pop     ebx
        pop     esi
        pop     edi
        retn
    }
}

// sub_40DCF0 — __usercall(edx = ptr) + stack(arg_0 = offset ptr).
// Used in the "var58C exists" path. Returns int (bool-ish).
static __declspec(naked) int call_sub_40DCF0(void* /*edx_ptr*/, void* /*arg_0*/)
{
    __asm
    {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+0x10]    // edx_ptr (arg 1)
        mov     eax, [esp+0x14]    // arg_0 (arg 2)
        push    eax
        mov     ecx, 0x0040DCF0
        call    ecx
        add     esp, 4
        pop     ebx
        pop     esi
        pop     edi
        retn
    }
}

// sub_459A70 — __usercall(eax = case, ecx = arg_0). Case-switch helper.
static __declspec(naked) void call_sub_459A70(int /*caseSel*/, int /*arg_0*/)
{
    __asm
    {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+0x10]    // case (arg 1)
        mov     ecx, [esp+0x14]    // arg_0 (arg 2)
        mov     edx, 0x00459A70
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        retn
    }
}

// =====================================================================
// Server-side ps* lookup — same as the cycle dump and bitmap sidecar.
// =====================================================================

static playerState_s* GetServerPs()
{
    uint8_t* ent0 = (uint8_t*)0x0176C6F0;
    return *(playerState_s**)(ent0 + 0x180);
}

// Sidecar-aware ownership test for the CG cycle iterator. Idx < 128 reads
// cg.predictedPS.weapons[] (vanilla bitmap, fastest); idx ≥ 128 falls back
// to the per-PS sidecar via server-side ps.
static bool CycleIsOwned(unsigned idx)
{
    if (idx < 128) {
        const uint32_t* cgWeapons = (const uint32_t*)0x0351E74C; // = cg_predictedPS.weapons[0]
        return (cgWeapons[idx >> 5] & (1u << (idx & 31))) != 0;
    }
    playerState_s* sps = GetServerPs();
    return sps ? T4M::PsBitTest(sps, 0x7FC, idx) : false;
}

// =====================================================================
// Reconstruction of sub_46A090.
//
// ABI : __cdecl(int arg_0, int arg_4, int arg_8) → int (bool success).
//   arg_0 = gentity / context pointer passed through to sub_46BDB0.
//   arg_4 = direction flag (0 = prev, non-0 = next).
//   arg_8 = "check ammo" flag (0 = no check, non-0 = require ammo).
// =====================================================================

namespace T4_Reconstructed
{
    extern "C" int __cdecl Sub_46A090(int arg_0, int arg_4, int arg_8)
    {
        // ---- Gate 1 : snap is valid ----
        uint8_t* snap = *(uint8_t**)0x034732E0;
        if (!snap) return 0;
        if (!(*(uint8_t*)(snap + 0x20) & 4)) return 0;
        if (*(int32_t*)0x0351DF54 >= 8) return 0;

        const unsigned total = *(uint32_t*)0x046DE3BC;
        const unsigned current = *(uint32_t*)0x0352B584;
        if (total + 1 < 2) return 0;
        uint8_t* curDef = (uint8_t*)bg_weaponDefs[current];

        // ---- Path A : current weapon is "alt-fire chain root" (mode 3) ----
        // Vanilla unconditionally reads curDef[+0x154] — we mirror that, since
        // bg_weaponDefs[0] is always a non-null default entry.
        const int curMode = *(int*)(curDef + 0x154);
        if (curMode == 3) {
            if (!call_sub_469E90(arg_0, curDef)) return 0;
            const unsigned altIdx = *(uint32_t*)(curDef + 0x63C);
            call_sub_46BDB0(arg_0, altIdx);
            return 1;
        }

        // ---- Path B : mode != 0 (and != 3) + cached dword_352B58C ref ----
        // Vanilla : `test eax, eax ; jz loc_46A146` — i.e. if mode == 0 SKIP
        // path B and go straight to the cycle loop. So Path B is entered only
        // when mode != 0 (= mode in {1, 2, 4, …}, since 3 was handled above).
        if (curMode != 0) {
            void* var58C = (void*)*(uint32_t*)0x0352B58C;
            if (var58C) {
                const int result = call_sub_40DCF0(var58C, (void*)0x0351DF50);
                if (result != 0) {
                    call_sub_46BDB0(arg_0, (unsigned)(uintptr_t)var58C);
                    return 1;
                }
            }
        }

        // ---- Path C : main cycle loop ----
        // Vanilla : `ebp = direction + total`, `esi += direction (mod total)`.
        // We replicate the modular arithmetic but with sidecar-aware ownership.
        const int direction = (arg_4 != 0) ? 1 : -1;
        const unsigned startIdx = (current != 0) ? current : 1;
        unsigned idx = startIdx;

        for (;;) {
            // Step : new_idx = ((idx + direction + total - 1) mod total) + 1
            // For direction +1 : 1→2, 2→3, …, total→1
            // For direction -1 : 2→1, 1→total, …
            const unsigned num = idx + (unsigned)(direction + (int)total - 1);
            idx = (num % total) + 1;

            // Wrapped back to start → wrap-cleanup path
            if (idx == startIdx) {
                if (!CycleIsOwned(current)) {
                    *(uint32_t*)0x0352B588 = *(uint32_t*)0x0351DF34;
                    if (current != 0) {
                        uint8_t* def0 = (uint8_t*)bg_weaponDefs[0];
                        if (def0 && *(uint32_t*)(def0 + 0x174) == 0) {
                            *(uint32_t*)0x0352B584 = 0;
                            call_sub_459A70(1, arg_0);
                            *(uint8_t*)0x03058528 = 0;
                        }
                    }
                }
                return 0;
            }

            // ---- Sidecar-aware ownership test (= the bug fix) ----
            if (!CycleIsOwned(idx)) continue;

            uint8_t* def = (uint8_t*)bg_weaponDefs[idx];
            if (!def) continue;

            // ---- Ammo gate (if arg_8 set) ----
            if (arg_8 != 0) {
                const unsigned clipSlot = *(uint32_t*)(def + 0x3FC);
                const unsigned ammoSlot = *(uint32_t*)(def + 0x3F4);
                // CG side ammo/clip read : in-struct for slot < 128, 0 for
                // sidecar (CG mirror isn't synced yet — high-idx ammo is
                // server-side only). This is acceptable since the gate
                // skips weapons with no ammo, and high-idx weapons will
                // simply be checked against 0.
                const int clip = (clipSlot < 128)
                    ? *(int*)(0x0351E54C + clipSlot * 4)
                    : 0;
                const int ammo = (ammoSlot < 128)
                    ? *(int*)(0x0351E0CC + ammoSlot * 4)
                    : 0;
                if (clip + ammo == 0 && *(int*)(def + 0x41C) == 0) continue;
            }

            const int mode = *(int*)(def + 0x154);
            if (mode == 1 || mode == 2 || mode == 3) continue;

            // Found a real owned weapon — switch and return success.
            call_sub_46BDB0(arg_0, idx);
            return 1;
        }
    }
}

// =====================================================================
// Install — single create_inline detour at sub_46A090 entry.
// =====================================================================

void PatchT4MAM_CycleHandler()
{
    static auto cycle_detour = safetyhook::create_inline(
        (void*)0x0046A090,
        (void*)&T4_Reconstructed::Sub_46A090);
    (void)cycle_detour;
}
