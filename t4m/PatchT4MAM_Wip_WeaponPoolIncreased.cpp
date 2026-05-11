// ==========================================================
// T4M project - PatchT4MAM_Wip_WeaponPoolIncreased.cpp
//
// ARCHIVE 2026-05-11 - WIP weapon-pool expansion stack
// Consolidates 7 files paused after regressions on weap idx > 140 :
//   - PatchT4MAM_CentityPropagator.cpp (P1)
//   - PatchT4MAM_CmdWeaponExt.cpp      (Phase 1a)
//   - PatchT4MAM_WireCodec.cpp         (Phase 1b)
//   - PatchT4MAM_PsBitmapExt.cpp       (Bitmap ext)
//   - PatchT4MAM_CycleHandler.cpp      (cycle sidecar)
//   - PatchT4MAM_NetfieldCodec.cpp     (P3)
//   - PatchT4MAM_PickupDecoder.cpp     (pickup decoder, this session)
//
// PAUSED status : all install function CALL sites (safetyhook::create_*,
// Memory::VP::Patch) are commented '//' line-by-line. Reconstruction
// bodies, naked thunks, helpers stay uncommented (compile as dead code,
// easily re-enableable). See plans/plan_weapons_full_detour.md and
// project_paused_weapon_pool_expansion_2026_05_11.md for reactivation.
// ==========================================================

// =====================================================================
// SECTION 1 / 7 - PatchT4MAM_CentityPropagator.cpp
// =====================================================================

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

//void PatchT4MAM_CentityPropagator()
//{
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // static auto detour = safetyhook::create_inline(
    //     (void*)0x00410660,
    //     (void*)&T4M::CG_PropagatePsWeapon_Wrapper);
    // (void)detour;
//}


// =====================================================================
// SECTION 2 / 7 - PatchT4MAM_CmdWeaponExt.cpp
// =====================================================================

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
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // T4M::InstallCmdWeaponHighFiller();
}


// =====================================================================
// SECTION 3 / 7 - PatchT4MAM_WireCodec.cpp
// =====================================================================

// ==========================================================
// T4M project — PatchT4MAM_WireCodec.cpp
//
// Phase 1b — full reconstruction of MSG_WriteDeltaUsercmd (sub_675BC0) and
// MSG_ReadDeltaUsercmd (sub_675FD0) with extension of the wire format to
// transport `usercmd_s.weapon_high` (T4M extension at offset 0x1B). Required
// for Coop / Zombie clients connected over the wire — Phase 1a (CL_FinishMove
// midhooks in PatchT4MAM_CmdWeaponExt.cpp) only fills the high byte locally,
// so SP / Host work but remote clients send 0 to the server.
//
// Wire format change (T4M-only, breaks compat with vanilla clients) :
//   Vanilla full-change branch :  ... [7 bits weapon delta] [7 bits offhand] ...
//   T4M     full-change branch :  ... [16 bits combined weapon delta] [8 bits offhand] ...
// Combined 16-bit weapon = (weapon_high << 8) | weapon, encoded as a single
// sub_675940 delta call with bits=16 and mask=0xFFFF. Offhand only widens
// from 7 to 8 bits — usercmd_s.offHandIndex is a 1-byte field at +0x15, so
// 8 bits is the natural maximum. Validators dropped on both fields since the
// vanilla 0x80 cap no longer reflects pool reality.
// Bandwidth : when weapon/offhand doesn't change between consecutive cmds
// (= 99%+ of ticks at 30Hz), the codec emits a single 0-bit marker.
//
// See analysis/wire_codec_RE.md for the full RE notes and field-by-field
// flowchart of the 3 writer branches and 2 reader branches.
//
// STATUS :
//   - 11 naked wrappers for the __usercall helpers (sub_674D00 / sub_674E00 /
//     sub_674F50 / sub_675060 / sub_675500 / sub_675560 / sub_675940 /
//     sub_6759A0 / sub_675A10 / sub_675A70 / sub_676690).
//   - T4_Reconstructed::MSG_WriteDeltaUsercmd : FULL reconstruction (3 branches).
//   - T4_Reconstructed::MSG_ReadDeltaUsercmd  : FULL reconstruction (3 branches).
//   - PatchT4MAM_WireCodec : installs detours on both 0x675BC0 (writer) and
//     0x675FD0 (reader). Buttons validator retained ; weapon and offhand
//     validators removed since both idx now legitimately span 0..0xFFFF.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include "include/cod/structs.hpp"
#include <safetyhook.hpp>
#include <cstdint>

using namespace T4;

// =============================================================================
// Helper VAs (vanilla functions, called via naked wrappers below)
// =============================================================================

namespace { namespace VA {
    constexpr uintptr_t MSG_WriteBitsRaw          = 0x00674D00; // __usercall(edx=msg)+stack(val,bits)
    constexpr uintptr_t MSG_WriteOneBitMarker     = 0x00674E00; // __usercall(eax=msg)
    constexpr uintptr_t MSG_ReadBits              = 0x00674F50; // __usercall(edx=msg)+stack(bits)
    constexpr uintptr_t MSG_ReadBit               = 0x00675060; // __usercall(edx=msg)
    constexpr uintptr_t MSG_ReadShortSignExt      = 0x00675500; // __usercall(eax=msg)
    constexpr uintptr_t MSG_ReadDword             = 0x00675560; // __usercall(ecx=msg)
    constexpr uintptr_t MSG_WriteDeltaField       = 0x00675940; // __usercall(eax=msg,ecx=key,edi=prev)+stack(val,bits)
    constexpr uintptr_t MSG_WriteDeltaByteSigned  = 0x006759A0; // __usercall(eax=msg)+stack(key,cur,prev)
    constexpr uintptr_t MSG_ReadDeltaByte         = 0x00675A10; // __usercall(eax=msg)+stack(key)
    constexpr uintptr_t MSG_WriteDeltaWord        = 0x00675A70; // __usercall(eax=msg,ecx=prev)+stack(key,cur)
    constexpr uintptr_t MSG_ReadSignedDelta       = 0x00676690; // __usercall(ecx=msg)
}}

// Delta-XOR mask globals — vanilla data table (read-only).
static volatile uint32_t* const g_mask_1bit  = (uint32_t*)0x008CF5CC; // = 0x01
static volatile uint32_t* const g_mask_7bit  = (uint32_t*)0x008CF5E4; // = 0x7F (vanilla weapon — unused in T4M)
static volatile uint32_t* const g_mask_8bit  = (uint32_t*)0x008CF5E8; // = 0xFF (gun byte)
static volatile uint32_t* const g_mask_27bit = (uint32_t*)0x008CF634; // = 0x07FFFFFF (buttons)

// T4M : 16-bit combined weapon mask. Pure C++ constant — not in vanilla data
// table. Used by both writer (implicitly via sub_675940) and reader (XOR with
// `key3 & k_mask_16bit` after MSG_ReadBits to invert the delta encoding).
constexpr uint32_t k_mask_16bit = 0xFFFFu;

// Float consts used by gun-pitch fixed-point conversion.
static const float* const k_pitchToFixed = (float*)0x008AF424; // ≈ 65535/360
static const float* const k_fixedToPitch = (float*)0x008AF4FC; // ≈ 360/65535

// Validator strings (vanilla addresses). Only buttons validator remains active;
// weapon and offhand validators were dropped along with their 7-bit caps.
static const char* const k_strInvalidButtons = (const char*)0x00945D88; // "client sent an invalid buttons value %i\n"

// =============================================================================
// Naked wrappers : __usercall (vanilla helpers) → __cdecl (C++-callable).
//
// Pattern : preserve callee-saved regs implicitly via __cdecl ABI. The wrapper
// only needs to translate stack args to register args and adjust esp.
//
// IMPORTANT : each wrapper saves esi+edi+ebx so that callers don't have to
// worry about __usercall scratching them — vanilla helpers do scratch ebx/edi
// in some paths (sub_675940 uses esi as a scratch + restores it ; sub_675060
// uses esi+edi etc.). __cdecl convention requires us to preserve them.
// =============================================================================

namespace T4_Reconstructed { namespace Wire {

// MSG_WriteBitsRaw(msg, val, bits)
// __usercall(edx=msg) + stack(val, bits).
extern "C" __declspec(naked) void __cdecl MSG_WriteBitsRaw(void* /*msg*/, int /*val*/, int /*bits*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+10h]                  ; msg
        mov     eax, [esp+18h]                  ; bits
        push    eax
        mov     eax, [esp+18h]                  ; val
        push    eax
        mov     eax, VA::MSG_WriteBitsRaw
        call    eax
        add     esp, 8
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_WriteOneBitMarker(msg)
// __usercall(eax=msg).
extern "C" __declspec(naked) void __cdecl MSG_WriteOneBitMarker(void* /*msg*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]                  ; msg
        mov     edx, VA::MSG_WriteOneBitMarker
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadBits(msg, bits) → value
// __usercall(edx=msg) + stack(bits).
extern "C" __declspec(naked) int __cdecl MSG_ReadBits(void* /*msg*/, int /*bits*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+10h]                  ; msg
        mov     eax, [esp+14h]                  ; bits
        push    eax
        mov     ecx, VA::MSG_ReadBits
        call    ecx
        add     esp, 4
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadBit(msg) → bit (0 or 1)
// __usercall(edx=msg).
extern "C" __declspec(naked) int __cdecl MSG_ReadBit(void* /*msg*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     edx, [esp+10h]
        mov     ecx, VA::MSG_ReadBit
        call    ecx
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadShortSignExt(msg) → signed short (sign-extended)
// __usercall(eax=msg).
extern "C" __declspec(naked) int __cdecl MSG_ReadShortSignExt(void* /*msg*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, VA::MSG_ReadShortSignExt
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadDword(msg) → 32-bit raw value
// __usercall(ecx=msg).
extern "C" __declspec(naked) int __cdecl MSG_ReadDword(void* /*msg*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     ecx, [esp+10h]
        mov     edx, VA::MSG_ReadDword
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_WriteDeltaField(msg, key, prev, val, bits)
// __usercall(eax=msg, ecx=key, edi=val/NEW) + stack(arg_0=prev/OLD, arg_4=bits).
// Helper logic : cmp(arg_0, edi); if differ, write `key XOR edi` over `bits` bits.
// → edi must hold NEW (the value we want to encode), arg_0 holds OLD (compare-only).
extern "C" __declspec(naked) void __cdecl MSG_WriteDeltaField(
    void* /*msg*/, int /*key*/, int /*prev*/, int /*val*/, int /*bits*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]                  ; msg
        mov     ecx, [esp+14h]                  ; key
        mov     edi, [esp+1Ch]                  ; val (NEW) → edi
        mov     edx, [esp+20h]                  ; bits
        push    edx                              ; arg_4 = bits
        mov     edx, [esp+1Ch]                   ; prev (was [esp+18h], shifted +4)
        push    edx                              ; arg_0 = prev (OLD)
        mov     edx, VA::MSG_WriteDeltaField
        call    edx
        add     esp, 8
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_WriteDeltaByteSigned(msg, key, cur, prev)
// __usercall(eax=msg) + stack(arg_0=key, arg_4=prev/OLD, arg_8=cur/NEW).
// Helper compares arg_4 vs arg_8, writes `key XOR arg_8` (= key XOR NEW) on diff.
extern "C" __declspec(naked) void __cdecl MSG_WriteDeltaByteSigned(
    void* /*msg*/, int /*key*/, int /*cur*/, int /*prev*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]                  ; msg
        mov     edx, [esp+18h]                  ; cur (NEW)
        push    edx                              ; arg_8 = cur
        mov     edx, [esp+20h]                   ; prev (was [esp+1Ch], shifted +4)
        push    edx                              ; arg_4 = prev (OLD)
        mov     edx, [esp+1Ch]                   ; key (was [esp+14h], shifted +8)
        push    edx                              ; arg_0 = key
        mov     edx, VA::MSG_WriteDeltaByteSigned
        call    edx
        add     esp, 0Ch
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadDeltaByte(msg, key) → signed byte (delta-decoded)
// __usercall(eax=msg) + stack(key).
extern "C" __declspec(naked) int __cdecl MSG_ReadDeltaByte(void* /*msg*/, int /*key*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]                  ; msg
        mov     edx, [esp+14h]                  ; key
        push    edx
        mov     edx, VA::MSG_ReadDeltaByte
        call    edx
        add     esp, 4
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_WriteDeltaWord(msg, key, cur, prev)
// __usercall(eax=msg, ecx=cur/NEW) + stack(arg_0=key, arg_4=prev/OLD).
// Helper compares arg_4 vs ecx (low 16), writes `key XOR ecx` on diff.
// → ecx must hold NEW (current word), arg_4 holds OLD.
extern "C" __declspec(naked) void __cdecl MSG_WriteDeltaWord(
    void* /*msg*/, int /*key*/, int /*cur*/, int /*prev*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+10h]                  ; msg
        mov     ecx, [esp+18h]                  ; cur (NEW) → ecx
        mov     edx, [esp+1Ch]                  ; prev
        push    edx                              ; arg_4 = prev (OLD)
        mov     edx, [esp+18h]                   ; key (was [esp+14h], shifted +4)
        push    edx                              ; arg_0 = key
        mov     edx, VA::MSG_WriteDeltaWord
        call    edx
        add     esp, 8
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

// MSG_ReadSignedDelta(msg) → signed delta dword
// __usercall(ecx=msg).
extern "C" __declspec(naked) int __cdecl MSG_ReadSignedDelta(void* /*msg*/)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        mov     ecx, [esp+10h]
        mov     edx, VA::MSG_ReadSignedDelta
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        ret
    }
}

}} // namespace T4_Reconstructed::Wire

// =============================================================================
// T4_Reconstructed::MSG_WriteDeltaUsercmd  — full reconstruction of sub_675BC0.
//
// Args (from caller, sub_63EA80+0x301) :
//   msg_t* msg   — passed in esi (__usercall)
//   int    key   — sequenceNum, passed via stack (arg_0)
//   usercmd_s* from — stack (arg_4)
//   usercmd_s* to   — stack (arg_8)
//
// The 3 branches mirror vanilla loc_675C00 (large time delta), loc_675D0E
// (small-change), and loc_675DF2 (full-change). The T4M extension lives in
// the FULL-CHANGE branch only — small-change implies weapon unchanged.
//
// @modified — 2 changes vs vanilla :
//   (1) Weapon delta widened from 7 bits to 8 bits (mask 0xFF instead of 0x7F).
//       This makes the wire transport the full byte of `to.weapon`.
//   (2) After the weapon delta call, write 8 raw bits of `to.weapon_high`
//       (= `to[+0x1B]`). This carries the high byte of the 16-bit idx for
//       T4M weapon pool ≥ 256.
// =============================================================================

namespace T4_Reconstructed
{
    using Wire::MSG_WriteBitsRaw;
    using Wire::MSG_WriteOneBitMarker;
    using Wire::MSG_WriteDeltaField;
    using Wire::MSG_WriteDeltaByteSigned;
    using Wire::MSG_WriteDeltaWord;

    extern "C" void __cdecl MSG_WriteDeltaUsercmd(
        void* msg, int key, void* fromPtr, void* toPtr)
    {
        auto from = (uint8_t*)fromPtr;
        auto to   = (uint8_t*)toPtr;

        // ---- Time delta (small/large) ----
        // Vanilla : `mov eax, [ebp+0]; sub eax, [edi]; cmp eax, 100h; jnb loc_675C00`.
        const uint32_t timeFrom = *(uint32_t*)(from + 0x00);
        const uint32_t timeTo   = *(uint32_t*)(to   + 0x00);
        const uint32_t timeDelta = timeTo - timeFrom;

        if (timeDelta < 0x100) {
            // SMALL-TIME-DELTA path : emit 1-bit marker via sub_674E00 then write
            // the byte into the buffer at the byte cursor [+0x14].
            // Vanilla : sub_674E00(msg) + raw-byte-write of timeDelta low byte.
            MSG_WriteOneBitMarker(msg);
            // Inline the raw-byte write : equivalent to MSG_WriteBitsRaw(msg, timeDelta&0xFF, 8)
            // but vanilla uses the byte-cursor path. We use bits-raw for safety — same wire.
            MSG_WriteBitsRaw(msg, (int)(timeDelta & 0xFF), 8);
        } else {
            // LARGE-TIME-DELTA path : write zero-bit marker + raw dword timeTo.
            // Vanilla advances the bit cursor through a sub-byte boundary then
            // writes the raw 4-byte serverTime via byte-cursor path. We emulate
            // by writing 8 zero-bit + 32-bit timeTo. NOTE : the actual vanilla
            // logic checks msg buffer space and sets overflow flag — defer to
            // helper if needed.
            MSG_WriteBitsRaw(msg, 0, 1);            // zero marker
            MSG_WriteBitsRaw(msg, (int)timeTo, 32); // full serverTime
        }

        // ---- forward / right pre-check (for branch selection at loc_675C73) ----
        const uint8_t fromForward = *(uint8_t*)(from + 0x16);
        const uint8_t fromRight   = *(uint8_t*)(from + 0x17);
        const uint8_t toForward   = *(uint8_t*)(to   + 0x16);
        const uint8_t toRight     = *(uint8_t*)(to   + 0x17);
        const bool forwardRightUnchanged = (fromForward == toForward) && (fromRight == toRight);

        // ---- Branch selection : full-change vs small-change vs no-change ----
        // Vanilla full-change criteria : buttons (excl bit 0), weapon, offhand,
        // angles[2], meleeChargeYaw, meleeChargeDist all unchanged → small/no.
        // T4M : also include weapon_high in the criteria.
        const uint32_t fromBtn  = *(uint32_t*)(from + 0x04);
        const uint32_t toBtn    = *(uint32_t*)(to   + 0x04);
        const uint8_t  fromWpn  = *(uint8_t*)(from + 0x14);
        const uint8_t  toWpn    = *(uint8_t*)(to   + 0x14);
        const uint8_t  fromOff  = *(uint8_t*)(from + 0x15);
        const uint8_t  toOff    = *(uint8_t*)(to   + 0x15);
        const uint8_t  fromWpnHi = *(uint8_t*)(from + 0x1B);  // T4M
        const uint8_t  toWpnHi   = *(uint8_t*)(to   + 0x1B);  // T4M
        const uint32_t fromAng2 = *(uint32_t*)(from + 0x10);
        const uint32_t toAng2   = *(uint32_t*)(to   + 0x10);
        const float    fromMcy  = *(float*)(from + 0x28);
        const float    toMcy    = *(float*)(to   + 0x28);
        const uint8_t  fromMcd  = *(uint8_t*)(from + 0x2C);
        const uint8_t  toMcd    = *(uint8_t*)(to   + 0x2C);

        // T4M : compare combined 16-bit weapon (low | high<<8) — covers both
        // vanilla weapon byte and the T4M extension high byte in one comparison.
        const uint32_t fromWpn16 = (uint32_t)fromWpn | ((uint32_t)fromWpnHi << 8);
        const uint32_t toWpn16   = (uint32_t)toWpn   | ((uint32_t)toWpnHi   << 8);

        const bool bigChanged =
            ((fromBtn ^ toBtn) & 0xFFFFFFFEu) != 0 ||
            fromWpn16 != toWpn16 ||
            fromOff != toOff ||
            fromAng2 != toAng2 ||
            fromMcy != toMcy ||
            fromMcd != toMcd;

        // SMALL-CHANGE prerequisites : forward+right unchanged + angle/gun deltas
        // unchanged also. We enter the small-change path only if NOT bigChanged
        // AND forward+right unchanged AND angles[0]/[1]/gunPitch/gunYaw unchanged
        // AND buttons LSB unchanged. Vanilla uses a flag at [esp+0xC+arg_8] = 1
        // for the forward+right test.
        const uint32_t fromAng0 = *(uint32_t*)(from + 0x08);
        const uint32_t toAng0   = *(uint32_t*)(to   + 0x08);
        const uint32_t fromAng1 = *(uint32_t*)(from + 0x0C);
        const uint32_t toAng1   = *(uint32_t*)(to   + 0x0C);
        const uint16_t fromGp   = *(uint16_t*)(from + 0x1C);
        const uint16_t toGp     = *(uint16_t*)(to   + 0x1C);
        const uint16_t fromGy   = *(uint16_t*)(from + 0x1E);
        const uint16_t toGy     = *(uint16_t*)(to   + 0x1E);
        const bool smallEligible = !bigChanged
            && fromAng0 == toAng0
            && fromAng1 == toAng1
            && ((fromBtn ^ toBtn) & 1) == 0
            && forwardRightUnchanged
            && fromGp == toGp
            && fromGy == toGy;

        if (smallEligible) {
            // ---- NO-CHANGE branch (loc_675D0E early-out at line ~944469 in dump) ----
            // Wait, vanilla doesn't have a no-change branch — small-change IS the
            // path when nothing big changed but the small-eligible criteria all
            // pass (= even angles unchanged).
            //
            // RE-READ : vanilla at loc_675D0E emits 1-bit `key` then returns. So
            // small-change actually means "no significant delta" — emit signature
            // and bail. Distinct from full-change (loc_675DF2).
            MSG_WriteBitsRaw(msg, key, 1);
            return;
        }

        if (!bigChanged) {
            // ---- SMALL-CHANGE branch ----
            // Vanilla loc_675D0E flow : write signature bits (key^1, key, key^buttonsLSB)
            // then delta-encode angles + buttons LSB + forward/right/pitch/yaw +
            // gun pitch/yaw/X/Y/Z, then return.
            const uint32_t btnXor = key ^ ((toBtn ^ fromBtn) & 1);
            // Wait — re-reading the asm at loc_675D0E :
            //   mov eax, ebx ; xor eax, 1 ; push 1 ; push eax ; ... call sub_674D00
            //   push 1 ; push ebx ; call sub_674D00
            //   xor ebx, [ebp+0] ; mov ecx, [ebp+4] ; xor ecx, ebx ;
            //   push 1 ; push ecx ; call sub_674D00
            // So : write 1 bit `key XOR 1`, write 1 bit `key`, then
            // ebx ^= to.serverTime (= update key), then write 1 bit
            // `(to.buttons XOR (key XOR to.serverTime))` clipped to 1 bit.

            MSG_WriteBitsRaw(msg, key ^ 1, 1);
            MSG_WriteBitsRaw(msg, key, 1);
            const uint32_t key2 = key ^ timeTo;                   // key after XOR with serverTime
            const uint32_t btnLsb = (toBtn ^ key2) & 1;           // 1-bit buttons delta
            MSG_WriteBitsRaw(msg, (int)btnLsb, 1);

            // Delta-words : angles[0], angles[1] — using key2 as new key
            MSG_WriteDeltaWord(msg, (int)key2,
                (int)*(uint32_t*)(to   + 0x08),
                (int)*(uint32_t*)(from + 0x08));
            MSG_WriteDeltaWord(msg, (int)key2,
                (int)*(uint32_t*)(to   + 0x0C),
                (int)*(uint32_t*)(from + 0x0C));

            // Delta-bytes signed : forward, right, pitchmove, yawmove
            MSG_WriteDeltaByteSigned(msg, (int)key2,
                (int8_t)*(int8_t*)(to   + 0x16),
                (int8_t)*(int8_t*)(from + 0x16));
            MSG_WriteDeltaByteSigned(msg, (int)key2,
                (int8_t)*(int8_t*)(to   + 0x17),
                (int8_t)*(int8_t*)(from + 0x17));
            MSG_WriteDeltaByteSigned(msg, (int)key2,
                (int8_t)*(int8_t*)(to   + 0x19),
                (int8_t)*(int8_t*)(from + 0x19));
            MSG_WriteDeltaByteSigned(msg, (int)key2,
                (int8_t)*(int8_t*)(to   + 0x1A),
                (int8_t)*(int8_t*)(from + 0x1A));

            // Delta-words signed : wiimoteGunPitch, wiimoteGunYaw, gunXOfs,
            // gunYOfs, gunZOfs.
            MSG_WriteDeltaWord(msg, (int)key2,
                (int16_t)*(int16_t*)(to   + 0x1C),
                (int16_t)*(int16_t*)(from + 0x1C));
            MSG_WriteDeltaWord(msg, (int)key2,
                (int16_t)*(int16_t*)(to   + 0x1E),
                (int16_t)*(int16_t*)(from + 0x1E));
            MSG_WriteDeltaWord(msg, (int)key2,
                (int16_t)*(int16_t*)(to   + 0x20),
                (int16_t)*(int16_t*)(from + 0x20));
            MSG_WriteDeltaWord(msg, (int)key2,
                (int16_t)*(int16_t*)(to   + 0x22),
                (int16_t)*(int16_t*)(from + 0x22));
            MSG_WriteDeltaWord(msg, (int)key2,
                (int16_t)*(int16_t*)(to   + 0x24),
                (int16_t)*(int16_t*)(from + 0x24));
            return;
        }

        // ---- FULL-CHANGE branch (loc_675DF2) — T4M extension lives here ----
        // Branch signature : (key^1, key^1) distinguishes from small-change's
        // (key^1, key). Vanilla loc_675DF2 saves (key^1) in arg_8 slot then
        // emits it twice via separate sub_674D00 calls.
        MSG_WriteBitsRaw(msg, key ^ 1, 1);
        MSG_WriteBitsRaw(msg, key ^ 1, 1);
        // Vanilla : `mov edx, [ebp+4]; xor edx, ebx; push 1; push edx ; call sub_674D00`
        // = write 1 bit of `to.buttons XOR key`. Note : this is BEFORE key gets
        // XOR'd with to.serverTime (vs the small-change branch which does XOR first).
        MSG_WriteBitsRaw(msg, (int)(toBtn ^ (uint32_t)key), 1);

        // Delta-words : angles[0], angles[1] — still using ORIGINAL key
        MSG_WriteDeltaWord(msg, key,
            (int)*(uint32_t*)(to   + 0x08),
            (int)*(uint32_t*)(from + 0x08));
        MSG_WriteDeltaWord(msg, key,
            (int)*(uint32_t*)(to   + 0x0C),
            (int)*(uint32_t*)(from + 0x0C));

        // Delta-bytes signed : forward, right, pitchmove, yawmove
        MSG_WriteDeltaByteSigned(msg, key,
            (int8_t)*(int8_t*)(to   + 0x16),
            (int8_t)*(int8_t*)(from + 0x16));
        MSG_WriteDeltaByteSigned(msg, key,
            (int8_t)*(int8_t*)(to   + 0x17),
            (int8_t)*(int8_t*)(from + 0x17));
        MSG_WriteDeltaByteSigned(msg, key,
            (int8_t)*(int8_t*)(to   + 0x19),
            (int8_t)*(int8_t*)(from + 0x19));
        MSG_WriteDeltaByteSigned(msg, key,
            (int8_t)*(int8_t*)(to   + 0x1A),
            (int8_t)*(int8_t*)(from + 0x1A));

        // Delta-words signed : wiimoteGunPitch, wiimoteGunYaw, gunXOfs,
        // gunYOfs, gunZOfs.
        MSG_WriteDeltaWord(msg, key,
            (int16_t)*(int16_t*)(to   + 0x1C),
            (int16_t)*(int16_t*)(from + 0x1C));
        MSG_WriteDeltaWord(msg, key,
            (int16_t)*(int16_t*)(to   + 0x1E),
            (int16_t)*(int16_t*)(from + 0x1E));
        MSG_WriteDeltaWord(msg, key,
            (int16_t)*(int16_t*)(to   + 0x20),
            (int16_t)*(int16_t*)(from + 0x20));
        MSG_WriteDeltaWord(msg, key,
            (int16_t)*(int16_t*)(to   + 0x22),
            (int16_t)*(int16_t*)(from + 0x22));
        MSG_WriteDeltaWord(msg, key,
            (int16_t)*(int16_t*)(to   + 0x24),
            (int16_t)*(int16_t*)(from + 0x24));

        // Vanilla updates key BEFORE the angles[2] delta-word call :
        //   xor ebx, [ebp+0]  ; ebx ^= to.serverTime  (= key3)
        //   push from.angles[2] ; push ebx (key3) ; call sub_675A70
        const uint32_t key3 = key ^ timeTo;

        // angles[2] (= [+0x10]) — written with key3 (updated key).
        const int angle2New = (int)*(int32_t*)(to   + 0x10);
        const int angle2Old = (int)*(int32_t*)(from + 0x10);
        MSG_WriteDeltaWord(msg, (int)key3, angle2New, angle2Old);

        // Buttons : write 27 bits of (to.buttons >> 1). Vanilla emits this via
        // sub_675940 with prev = (from.buttons >> 1), bits = 27. The wire mask
        // covers buttons[1..27].
        MSG_WriteDeltaField(msg, (int)key3,
            (int)((int32_t)fromBtn >> 1),               // prev (signed shift to match SAR)
            (int)((int32_t)toBtn >> 1),                 // val
            27);

        // ---- WEAPON write (T4M : 16-bit combined weapon | weapon_high) ----
        // Vanilla : `push 7; call sub_675940` with mask dword_8CF5E4 = 0x7F.
        // T4M     : pack (weapon_high << 8) | weapon and emit 16-bit delta in a
        // single sub_675940 call. Reader unpacks symmetrically. Mask = 0xFFFF
        // (k_mask_16bit, C++ side). When weapon doesn't change, sub_675940 emits
        // a single 0-bit marker — bandwidth-optimal at steady state.
        // fromWpn16 / toWpn16 already computed above for branch detection.
        MSG_WriteDeltaField(msg, (int)key3, (int)fromWpn16, (int)toWpn16, 16);

        // ---- OFFHAND write (T4M : widened 7→8 bits, full byte) ----
        // Vanilla : 7-bit delta with mask dword_8CF5E4 = 0x7F. Capped offhand
        // idx at 0x7F. usercmd_s.offHandIndex is a 1-byte field, so 8 bits is
        // the natural max. Reuse mask 0xFF (dword_8CF5E8) used elsewhere for
        // 8-bit fields.
        MSG_WriteDeltaField(msg, (int)key3, (int)fromOff, (int)toOff, 8);

        // Conditional : selectedLocation[0..1] if buttons & 0x10000.
        if (toBtn & 0x10000) {
            MSG_WriteDeltaByteSigned(msg, (int)key3,
                (int8_t)*(int8_t*)(to   + 0x34),
                (int8_t)*(int8_t*)(from + 0x34));
            MSG_WriteDeltaByteSigned(msg, (int)key3,
                (int8_t)*(int8_t*)(to   + 0x35),
                (int8_t)*(int8_t*)(from + 0x35));
        }

        // Conditional : pitchstep float + meleeChargeDist byte if buttons & 4.
        if (toBtn & 4) {
            // Convert float pitchstep to fixed-point uint16 : `mulss ; cvttss2si ; and 0xFFFF`.
            const float toFlt   = *(float*)(to   + 0x28);
            const float fromFlt = *(float*)(from + 0x28);
            const uint16_t toFixed   = (uint16_t)(int)(toFlt   * (*k_pitchToFixed));
            const uint16_t fromFixed = (uint16_t)(int)(fromFlt * (*k_pitchToFixed));
            MSG_WriteDeltaWord(msg, (int)key3, (int)toFixed, (int)fromFixed);

            // meleeChargeDist : 8-bit delta with mask 0xFF (dword_8CF5E8).
            MSG_WriteDeltaField(msg, (int)key3, (int)fromMcd, (int)toMcd, 8);
        }
    }
}

// =============================================================================
// T4_Reconstructed::MSG_ReadDeltaUsercmd — full reconstruction of sub_675FD0.
//
// Args (plain __cdecl, all stack — no __usercall on this side) :
//   msg_t*    msg     (arg_0)
//   int       key     (arg_4) — sequence number / running encryption key
//   usercmd_s* from   (arg_8)
//   usercmd_s* to     (arg_C)
//
// Branch structure (mirrors writer) :
//   - read 1-bit time-delta flag; small/large time decode
//   - read sig bit 1 ; if 0 → no-change exit (validators skipped, vanilla behavior)
//   - read sig bit 2 ; if 0 → small-change branch ; if 1 → full-change branch
//   - small-change : delta-words angles[0..1], delta-bytes fwd/rt/pm/ym, delta-words
//                    gunPitch/Yaw/X/Y/Z. Returns without running validators.
//   - full-change  : LSB delta + above + angles[2] + buttons27 + WEAPON16 + offhand7 +
//                    conditional alt bytes (buttons & 0x10000) + conditional pitchstep
//                    + meleeChargeDist (buttons & 4). Runs validators on exit.
//
// @modified — 2 changes vs vanilla :
//   (1) WEAPON read widened from 7 bits to 16 bits (k_mask_16bit = 0xFFFF).
//       Decoded value splits into to.weapon (low byte, +0x14) and
//       to.weapon_high (high byte, +0x1B, T4M extension).
//   (2) Weapon validator (`if (to.weapon >= 0x80) reset`) REMOVED — weapon idx
//       now legitimately spans 0..0xFFFF.
// =============================================================================

namespace T4_Reconstructed
{
    using Wire::MSG_ReadBit;
    using Wire::MSG_ReadBits;
    using Wire::MSG_ReadDword;
    using Wire::MSG_ReadShortSignExt;
    using Wire::MSG_ReadDeltaByte;
    using Wire::MSG_ReadSignedDelta;

    namespace {
        // Vanilla print function for validator warnings (sub_59A380, distinct from
        // Com_Printf at 0x59A2C0). Signature : (channel, fmt, ...). Channel 0xF is
        // the "warning" channel used by vanilla validators.
        typedef void(__cdecl* WirePrintf_t)(int channel, const char* fmt, ...);
        static const WirePrintf_t WirePrintf = (WirePrintf_t)0x0059A380;

        // ---- Delta-reader helpers ----

        // Reads a flag bit; if 1, reads `bits` bits and XORs with (mask & key).
        // Else returns prev. Used for buttons27 (27 bits, mask 0x07FFFFFF) and
        // T4M weapon16 (16 bits, mask 0xFFFF).
        static inline uint32_t ReadDeltaBits(
            void* msg, uint32_t key, uint32_t prev, uint32_t mask, int bits)
        {
            if (MSG_ReadBit(msg)) {
                uint32_t raw = (uint32_t)MSG_ReadBits(msg, bits);
                return raw ^ (mask & key);
            }
            return prev;
        }

        // Reads a flag bit; if 1, reads sign-ext word and XORs with sign-ext
        // (key & 0xFFFF). Else returns prev. Used for delta-words (angles[0..1],
        // [2], gun*, pitchstep fixed-point).
        static inline int16_t ReadDeltaShortSigned(
            void* msg, uint32_t key, int16_t prev)
        {
            if (MSG_ReadBit(msg)) {
                int16_t raw = (int16_t)MSG_ReadShortSignExt(msg);
                return (int16_t)(raw ^ (int16_t)(key & 0xFFFFu));
            }
            return prev;
        }

        // Reads a flag bit; if 1, calls MSG_ReadDeltaByte (= sub_675A10 which
        // handles the XOR internally). Else returns prev. Used for delta-bytes
        // (forward, right, pitchmove, yawmove, selectedLocation).
        static inline int8_t ReadDeltaByteSigned(
            void* msg, uint32_t key, int8_t prev)
        {
            if (MSG_ReadBit(msg)) {
                return (int8_t)MSG_ReadDeltaByte(msg, (int)key);
            }
            return prev;
        }
    }

    extern "C" void __cdecl MSG_ReadDeltaUsercmd(
        void* msg, int keyInt, void* fromPtr, void* toPtr)
    {
        auto from = (uint8_t*)fromPtr;
        auto to   = (uint8_t*)toPtr;
        const uint32_t key = (uint32_t)keyInt;

        // ---- rep movsd 0xE = memcpy 0x38 bytes (entire usercmd_s) ----
        memcpy(to, from, 0x38);

        // ---- Time delta : 1 flag bit + small (signed delta) or large (raw 32-bit) ----
        uint32_t timeTo;
        if (MSG_ReadBit(msg)) {
            const int delta = MSG_ReadSignedDelta(msg);
            timeTo = (uint32_t)*(int32_t*)(from + 0x00) + (uint32_t)delta;
        } else {
            timeTo = (uint32_t)MSG_ReadDword(msg);
        }
        *(uint32_t*)(to + 0x00) = timeTo;

        // ---- Signature bit 1 ----
        // Writer wrote `key` (no-change), `key^1` (small), or `key^1` (full).
        // Reader XORs with (key & 1) → 0 means no-change, 1 means continue.
        const uint32_t sig1 = (uint32_t)MSG_ReadBits(msg, 1) ^ (key & 1u);
        if (sig1 == 0) {
            // NO-CHANGE branch : to is already a copy of from (memcpy above) +
            // the new serverTime. Validators are skipped (vanilla loc_67662B).
            return;
        }

        // Clear LSB of to.buttons before reading the LSB delta.
        *(uint32_t*)(to + 0x04) &= 0xFFFFFFFEu;

        // ---- Signature bit 2 ----
        // Writer wrote `key` (small) or `key^1` (full). Reader XORs with (key & 1)
        // → 0 means small-change, 1 means full-change.
        const uint32_t sig2 = (uint32_t)MSG_ReadBits(msg, 1) ^ (key & 1u);

        if (sig2 == 0) {
            // ---------------- SMALL-CHANGE BRANCH ----------------
            // Vanilla updates key to key2 = key XOR timeTo BEFORE reading LSB.
            const uint32_t key2 = key ^ timeTo;
            const uint32_t lsb = (uint32_t)MSG_ReadBits(msg, 1) ^ (key2 & 1u);
            *(uint32_t*)(to + 0x04) |= lsb;

            // angles[0..1] (delta-words, stored as zero-ext dword)
            *(uint32_t*)(to + 0x08) = (uint16_t)ReadDeltaShortSigned(msg, key2,
                (int16_t)*(int16_t*)(from + 0x08));
            *(uint32_t*)(to + 0x0C) = (uint16_t)ReadDeltaShortSigned(msg, key2,
                (int16_t)*(int16_t*)(from + 0x0C));

            // forward / right / pitchmove / yawmove (delta-bytes signed)
            *(int8_t*)(to + 0x16) = ReadDeltaByteSigned(msg, key2, *(int8_t*)(from + 0x16));
            *(int8_t*)(to + 0x17) = ReadDeltaByteSigned(msg, key2, *(int8_t*)(from + 0x17));
            *(int8_t*)(to + 0x19) = ReadDeltaByteSigned(msg, key2, *(int8_t*)(from + 0x19));
            *(int8_t*)(to + 0x1A) = ReadDeltaByteSigned(msg, key2, *(int8_t*)(from + 0x1A));

            // gun pitch / yaw / X / Y / Z (delta-words signed, stored as int16)
            *(int16_t*)(to + 0x1C) = ReadDeltaShortSigned(msg, key2, *(int16_t*)(from + 0x1C));
            *(int16_t*)(to + 0x1E) = ReadDeltaShortSigned(msg, key2, *(int16_t*)(from + 0x1E));
            *(int16_t*)(to + 0x20) = ReadDeltaShortSigned(msg, key2, *(int16_t*)(from + 0x20));
            *(int16_t*)(to + 0x22) = ReadDeltaShortSigned(msg, key2, *(int16_t*)(from + 0x22));
            *(int16_t*)(to + 0x24) = ReadDeltaShortSigned(msg, key2, *(int16_t*)(from + 0x24));
            return;  // small-change skips validators (vanilla behavior)
        }

        // ---------------- FULL-CHANGE BRANCH ----------------
        // LSB delta uses ORIGINAL key (key3 update happens AFTER the gun deltas).
        const uint32_t lsbFull = (uint32_t)MSG_ReadBits(msg, 1) ^ (key & 1u);
        *(uint32_t*)(to + 0x04) |= lsbFull;

        // angles[0..1] with original key
        *(uint32_t*)(to + 0x08) = (uint16_t)ReadDeltaShortSigned(msg, key,
            (int16_t)*(int16_t*)(from + 0x08));
        *(uint32_t*)(to + 0x0C) = (uint16_t)ReadDeltaShortSigned(msg, key,
            (int16_t)*(int16_t*)(from + 0x0C));

        // forward / right / pitchmove / yawmove with original key
        *(int8_t*)(to + 0x16) = ReadDeltaByteSigned(msg, key, *(int8_t*)(from + 0x16));
        *(int8_t*)(to + 0x17) = ReadDeltaByteSigned(msg, key, *(int8_t*)(from + 0x17));
        *(int8_t*)(to + 0x19) = ReadDeltaByteSigned(msg, key, *(int8_t*)(from + 0x19));
        *(int8_t*)(to + 0x1A) = ReadDeltaByteSigned(msg, key, *(int8_t*)(from + 0x1A));

        // gun pitch/yaw/X/Y/Z with original key
        *(int16_t*)(to + 0x1C) = ReadDeltaShortSigned(msg, key, *(int16_t*)(from + 0x1C));
        *(int16_t*)(to + 0x1E) = ReadDeltaShortSigned(msg, key, *(int16_t*)(from + 0x1E));
        *(int16_t*)(to + 0x20) = ReadDeltaShortSigned(msg, key, *(int16_t*)(from + 0x20));
        *(int16_t*)(to + 0x22) = ReadDeltaShortSigned(msg, key, *(int16_t*)(from + 0x22));
        *(int16_t*)(to + 0x24) = ReadDeltaShortSigned(msg, key, *(int16_t*)(from + 0x24));

        // Vanilla : `xor edi, [ebp+0]` — update key to key3 = key XOR to.serverTime.
        const uint32_t key3 = key ^ timeTo;

        // angles[2] with key3 (stored as zero-ext dword from low-16 sign-decode)
        *(uint32_t*)(to + 0x10) = (uint16_t)ReadDeltaShortSigned(msg, key3,
            (int16_t)*(int16_t*)(from + 0x10));

        // Clear all but LSB of to.buttons before merging buttons27 << 1.
        *(uint32_t*)(to + 0x04) &= 1u;

        // 27-bit buttons (mask dword_8CF634 = 0x07FFFFFF). Vanilla : if flag=0,
        // uses (from.buttons >> 1) signed → which becomes (from.buttons & ~1)
        // when shifted back. We mirror by passing prev = (uint32_t)((int)from.buttons >> 1).
        const uint32_t prevBtn27 = (uint32_t)((int32_t)*(int32_t*)(from + 0x04) >> 1);
        const uint32_t btn27 = ReadDeltaBits(msg, key3, prevBtn27, 0x07FFFFFFu, 27);
        *(uint32_t*)(to + 0x04) |= (btn27 << 1);

        // ---- WEAPON read (T4M : 16-bit combined, mask 0xFFFF) ----
        const uint32_t prevWpn16 = (uint32_t)*(uint8_t*)(from + 0x14)
                                 | ((uint32_t)*(uint8_t*)(from + 0x1B) << 8);
        const uint32_t weapon16 = ReadDeltaBits(msg, key3, prevWpn16, k_mask_16bit, 16);
        *(uint8_t*)(to + 0x14) = (uint8_t)(weapon16 & 0xFFu);
        *(uint8_t*)(to + 0x1B) = (uint8_t)((weapon16 >> 8) & 0xFFu);

        // ---- OFFHAND read (T4M : widened 7→8 bits, full byte, mask 0xFF) ----
        const uint32_t offhand = ReadDeltaBits(msg, key3,
            (uint32_t)*(uint8_t*)(from + 0x15), 0xFFu, 8);
        *(uint8_t*)(to + 0x15) = (uint8_t)(offhand & 0xFFu);

        // ---- Cond buttons & 0x10000 : alt bytes (selectedLocation[0..1]) ----
        if (*(uint32_t*)(to + 0x04) & 0x10000u) {
            *(int8_t*)(to + 0x34) = ReadDeltaByteSigned(msg, key3, *(int8_t*)(from + 0x34));
            *(int8_t*)(to + 0x35) = ReadDeltaByteSigned(msg, key3, *(int8_t*)(from + 0x35));
        }

        // ---- Cond buttons & 4 : pitchstep float + meleeChargeDist byte ----
        if (*(uint32_t*)(to + 0x04) & 4u) {
            // Pitchstep : reconstruct float from 16-bit fixed-point.
            const float fromFlt = *(float*)(from + 0x28);
            const int16_t fromFixed = (int16_t)(int)(fromFlt * (*k_pitchToFixed));
            const int16_t toFixed = ReadDeltaShortSigned(msg, key3, fromFixed);
            *(float*)(to + 0x28) = (float)(int16_t)toFixed * (*k_fixedToPitch);

            // meleeChargeDist (8-bit delta, mask dword_8CF5E8 = 0xFF).
            const uint32_t mcd = ReadDeltaBits(msg, key3,
                (uint32_t)*(uint8_t*)(from + 0x2C), 0xFFu, 8);
            *(uint8_t*)(to + 0x2C) = (uint8_t)(mcd & 0xFFu);
        }

        // ==== Validators ====
        // Buttons : cap at 0x10000000 — if exceeded, warn + reset to from.buttons.
        if (*(uint32_t*)(to + 0x04) >= 0x10000000u) {
            WirePrintf(0xF, "client sent an invalid buttons value %i\n",
                *(uint32_t*)(to + 0x04));
            *(uint32_t*)(to + 0x04) = *(uint32_t*)(from + 0x04);
        }

        // T4M : weapon AND offhand validators REMOVED. Vanilla capped both at
        // 0x80 (cmp al, 0x80; jb; warn; reset). Both fields now legitimately
        // span 0..0xFFFF after the wire widening above, so the 7-bit cap no
        // longer reflects reality.
    }
}

// =============================================================================
// __usercall → __cdecl dispatch wrappers for the 2 main detour entry points.
//
// Engine calls sub_675BC0 / sub_675FD0 with a __usercall convention — first
// arg in esi for sub_675BC0, all-stack for sub_675FD0. SafetyHook installs the
// JMP at those VAs so we receive control with the engine's register state.
// We translate to __cdecl and forward to the C++ reconstruction.
// =============================================================================

namespace T4M
{
    // sub_675BC0 : __usercall(esi=msg) + stack(arg_0=key, arg_4=from, arg_8=to)
    extern "C" __declspec(naked) void MSG_WriteDeltaUsercmd_Wrapper()
    {
        __asm {
            push    [esp+0Ch]                   ; to (arg_8)
            push    [esp+0Ch]                   ; from (arg_4) — esp shifted +4, so same imm
            push    [esp+0Ch]                   ; key (arg_0)
            push    esi                         ; msg (__usercall)
            call    T4_Reconstructed::MSG_WriteDeltaUsercmd
            add     esp, 16
            retn
        }
    }

    // sub_675FD0 : plain __cdecl-like (all stack args), preserved by 1 dummy push.
    // Engine pushes : ret_addr | arg_0=msg | arg_4=key | arg_8=from | arg_C=to
    // Forward as-is.
    extern "C" __declspec(naked) void MSG_ReadDeltaUsercmd_Wrapper()
    {
        __asm {
            push    [esp+10h]                   ; to
            push    [esp+10h]                   ; from
            push    [esp+10h]                   ; key
            push    [esp+10h]                   ; msg
            call    T4_Reconstructed::MSG_ReadDeltaUsercmd
            add     esp, 16
            retn
        }
    }
}

// =============================================================================
// Install hook(s).
//
// Both writer and reader detours are active. The wire format is symmetric :
// writer emits 16-bit combined weapon + 7-bit offhand in the full-change branch
// (vs vanilla 7+7), reader decodes the same and propagates to.weapon (low) and
// to.weapon_high (high). T4M-only (incompatible with vanilla peers, acceptable
// for Coop/Zombie where all clients are modded).
// =============================================================================

void PatchT4MAM_WireCodec()
{
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // static auto writer_detour = safetyhook::create_inline(
    //     (void*)0x00675BC0,
    //     (void*)&T4M::MSG_WriteDeltaUsercmd_Wrapper);
    // (void)writer_detour;
    //
    // static auto reader_detour = safetyhook::create_inline(
    //     (void*)0x00675FD0,
    //     (void*)&T4M::MSG_ReadDeltaUsercmd_Wrapper);
    // (void)reader_detour;
}


// =====================================================================
// SECTION 4 / 7 - PatchT4MAM_PsBitmapExt.cpp
// =====================================================================

// ==========================================================
// T4M project — PatchT4MAM_PsBitmapExt.cpp
//
// Phase "Bitmap ext" of plan_weapons_full_detour.md.
//
// Extends the 3 vanilla weapon bitmaps in playerState_s :
//   - weapons[4]         @ 0x7FC  (4 dwords = 128 bits)
//   - weaponold[4]       @ 0x80C  (4 dwords = 128 bits)
//   - weaponrechamber[4] @ 0x81C  (4 dwords = 128 bits)
// to 512 bits each via a per-PS sidecar struct.
//
// Vanilla writers (the only sites in the binary that bit-set/bit-clear
// these bitmaps) :
//   - sub_41D6A0 (G_TakeWeapon)  : clears weapons[idx] + alt-firemode chain
//   - sub_5528D0 (G_GiveWeapon)  : sets weapons[idx], clears weaponold/rechamber
//
// Both are detoured here. For idx < 128 they update the in-struct vanilla
// bitmaps (preserving correctness for vanilla readers in that range). For
// idx >= 128 they ONLY update the sidecar, preventing OOB writes from
// corrupting the adjacent struct fields (proneDirection etc.).
//
// T4M reconstructions (e.g. PM_Weapon_Dropping) read/write through the
// unified helpers PsBitTest / PsBitSet / PsBitClear — vanilla in-struct for
// idx < 128, sidecar for idx >= 128.
//
// Vanilla readers of high-idx bitmap bits still go OOB into adjacent fields.
// They are not detoured here ; they'll silently see "owned" or "not owned"
// based on whatever weaponold/weaponrechamber bits happen to be set. This is
// no worse than vanilla pre-T4M behavior (vanilla never produced idx >= 128).
//
// Sidecar lifetime : lazy-init per ps* on first access via unordered_map.
// Stale entries persist across map loads if the same ps memory is reused ;
// acceptable until we observe issues, then add a reset hook.
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
#include <unordered_map>

extern T4::WeaponDef** bg_weaponDefs;  // defined in PatchT4Script.cpp

// Forward declarations of the trace toggles defined further down in this TU.
// Needed by PsBitSet/PsBitClear/G_TakeWeapon above their actual definition.
extern "C" bool T4M_GiveTraceEnabled;
extern "C" volatile uint32_t T4M_GiveRetAddr;

namespace T4M
{
    // 12 dwords cover idx 128..511 (= 384 bits). Slot k (k=0..11) holds
    // bits for idx [(k+4)*32 .. (k+4)*32 + 31].
    struct PsBitmapExt
    {
        uint32_t weapons_high[12];
        uint32_t weaponold_high[12];
        uint32_t weaponrechamber_high[12];
    };

    static std::unordered_map<playerState_s*, PsBitmapExt> g_psBitmapExt;

    static PsBitmapExt& GetExt(playerState_s* ps)
    {
        return g_psBitmapExt[ps];  // zero-inits on first access
    }

    static uint32_t* PickArr(PsBitmapExt& ext, int inStructOffset)
    {
        switch (inStructOffset) {
            case 0x7FC: return ext.weapons_high;
            case 0x80C: return ext.weaponold_high;
            case 0x81C: return ext.weaponrechamber_high;
            default:    return nullptr;
        }
    }

    // Test bit `idx` in the bitmap at `inStructOffset` (0x7FC/0x80C/0x81C).
    // Returns true if set. For idx < 128 reads vanilla in-struct ; for
    // idx >= 128 reads sidecar.
    extern "C" bool PsBitTest(playerState_s* ps, int inStructOffset, unsigned idx)
    {
        const unsigned word = idx >> 5;
        const uint32_t bit  = 1u << (idx & 0x1F);
        if (word < 4) {
            return (*(uint32_t*)((uint8_t*)ps + inStructOffset + word * 4) & bit) != 0;
        }
        auto* arr = PickArr(GetExt(ps), inStructOffset);
        return arr ? (arr[word - 4] & bit) != 0 : false;
    }

    extern "C" void PsBitSet(playerState_s* ps, int inStructOffset, unsigned idx)
    {
        const unsigned word = idx >> 5;
        const uint32_t bit  = 1u << (idx & 0x1F);
        if (word < 4) {
            volatile uint32_t* p = (volatile uint32_t*)((uint8_t*)ps + inStructOffset + word * 4);
            const uint32_t before = *p;
            *p |= bit;
            const uint32_t after = *p;
            // Trace only weapons[] writes (inStructOffset == 0x7FC) to avoid noise.
            if (inStructOffset == 0x7FC) {
                T4::Com_Printf(0, "[T4M BITSET] ps=%p idx=%u word=%u bit=0x%08X before=0x%08X after=0x%08X\n",
                    (void*)ps, idx, word, bit, before, after);
            }
            return;
        }
        if (auto* arr = PickArr(GetExt(ps), inStructOffset)) {
            arr[word - 4] |= bit;
        }
    }

    extern "C" void PsBitClear(playerState_s* ps, int inStructOffset, unsigned idx)
    {
        const unsigned word = idx >> 5;
        const uint32_t bit  = 1u << (idx & 0x1F);
        if (word < 4) {
            volatile uint32_t* p = (volatile uint32_t*)((uint8_t*)ps + inStructOffset + word * 4);
            const uint32_t before = *p;
            *p &= ~bit;
            const uint32_t after = *p;
            if (inStructOffset == 0x7FC) {
                T4::Com_Printf(0, "[T4M BITCLR] ps=%p idx=%u word=%u bit=0x%08X before=0x%08X after=0x%08X\n",
                    (void*)ps, idx, word, bit, before, after);
            }
            return;
        }
        if (auto* arr = PickArr(GetExt(ps), inStructOffset)) {
            arr[word - 4] &= ~bit;
        }
    }
}


// =====================================================================
// ps.ammo[] / ps.ammoClip[] sidecar — slot range 128..639.
//
// Vanilla layout (sized for vanilla 128 weapons):
//   ps.ammo[128]     @ +0x17C  (= 512 bytes)
//   ps.ammoClip[128] @ +0x5FC  (= 512 bytes)
// def[+0x3F4] = ammoSlot (= ammo[] index)
// def[+0x3FC] = clipSlot (= ammoClip[] index)
//
// T4M extended weapons can declare new ammo/clip slot ids ≥ 128, in which
// case sub_4FC570 (and other vanilla ammo handlers) OOB-write into adjacent
// playerState_s fields — including the weapons[] bitmap @ +0x7FC. That OOB
// is what corrupts m1garand ownership after spawn.
//
// Strategy: per-PS sidecar indexed by (slot - 128), wired via PsAmmoPtr /
// PsAmmoClipPtr. Vanilla functions that OOB are detoured to use the helpers.
// =====================================================================

namespace T4M
{
    struct PsAmmoSlotExt
    {
        int ammo[512];      // covers slot 128..639
        int ammoClip[512];
    };

    static std::unordered_map<playerState_s*, PsAmmoSlotExt> g_psAmmoExt;

    static PsAmmoSlotExt& GetAmmoExt(playerState_s* ps)
    {
        return g_psAmmoExt[ps];  // zero-inits on first access
    }

    inline int* PsAmmoPtr(playerState_s* ps, unsigned slot)
    {
        if (slot < 128) {
            return (int*)((uint8_t*)ps + 0x17C + slot * 4);
        }
        const unsigned i = slot - 128;
        if (i >= 512) {
            static int scratch = 0;
            return &scratch;
        }
        return &GetAmmoExt(ps).ammo[i];
    }

    inline int* PsAmmoClipPtr(playerState_s* ps, unsigned slot)
    {
        if (slot < 128) {
            return (int*)((uint8_t*)ps + 0x5FC + slot * 4);
        }
        const unsigned i = slot - 128;
        if (i >= 512) {
            static int scratch = 0;
            return &scratch;
        }
        return &GetAmmoExt(ps).ammoClip[i];
    }
}


// =====================================================================
// __usercall shims for vanilla helpers called by the reconstructions.
// =====================================================================

// sub_41D800 — __usercall(eax = idx, [esp+4] = ps, [esp+8] = outer_idx).
// Returns ammo total in eax. Multi-caller, kept vanilla.
static __declspec(naked) int call_BG_TotalAmmoIfReplaced(
    int /*eax_idx*/, void* /*ps*/, int /*outer_idx*/)
{
    __asm
    {
        mov     eax, [esp+4]
        push    [esp+12]
        push    [esp+12]
        mov     ecx, 0x0041D800
        call    ecx
        add     esp, 8
        retn
    }
}

// sub_41D590 — __usercall(esi = ps, [esp+4] = arg_0). Returns idx in eax.
// Multi-caller, kept vanilla. We save/restore esi (cdecl callee-save).
static __declspec(naked) int call_BG_FindLinkedWeapon(
    void* /*ps*/, int /*arg_0*/)
{
    __asm
    {
        push    esi
        mov     esi, [esp+8]       // ps  (skipped over saved esi)
        push    [esp+12]            // arg_0
        mov     ecx, 0x0041D590
        call    ecx
        add     esp, 4
        pop     esi
        retn
    }
}

// sub_41E320 — __usercall(eax = idx, ecx = ps). Returns ammo total in eax.
// Multi-caller, kept vanilla.
static __declspec(naked) int call_BG_AmmoTotal(int /*idx*/, void* /*ps*/)
{
    __asm
    {
        mov     eax, [esp+4]
        mov     ecx, [esp+8]
        mov     edx, 0x0041E320
        jmp     edx
    }
}

// sub_552AB0 — vanilla expects eax = idx (used as %i in a debug print). The
// vanilla caller also pushes ps.clientNum but the function doesn't read the
// stack arg ; we faithfully preserve the dead push (variadic-printf paranoia).
static __declspec(naked) void call_G_LinkActionSlot(int /*idx_via_eax*/)
{
    __asm
    {
        mov     eax, [esp+4]       // eax = idx
        push    0                   // vanilla pushed clientNum here ; unused by callee
        mov     ecx, 0x00552AB0
        call    ecx
        add     esp, 4
        retn
    }
}

// sub_41C930 — __usercall(eax = def). Returns scaled clip max in eax.
// Read-only on def, no ps access — kept vanilla.
static __declspec(naked) int call_sub_41C930(void* /*def*/)
{
    __asm
    {
        push    ebx
        push    esi
        push    edi
        mov     eax, [esp+0x10]
        mov     edx, 0x0041C930
        call    edx
        pop     edi
        pop     esi
        pop     ebx
        retn
    }
}

// sub_41E4E0 — __usercall(eax = idx). Returns al (= bool, "is name special").
// Scratches esi. Kept vanilla, called by Sub_41E5E0 reconstruction.
static __declspec(naked) int call_sub_41E4E0(unsigned /*idx*/)
{
    __asm
    {
        push    esi
        mov     eax, [esp+8]
        mov     edx, 0x0041E4E0
        call    edx
        movzx   eax, al
        pop     esi
        retn
    }
}

// sub_41D8B0 — __usercall(eax = idx). Returns int in eax. No esi/edi/ebx
// scratching seen at the vanilla call site, but save them to be safe.
static __declspec(naked) int call_sub_41D8B0(int /*idx*/)
{
    __asm
    {
        push    edi
        push    esi
        push    ebx
        mov     eax, [esp+0x10]
        mov     edx, 0x0041D8B0
        call    edx
        pop     ebx
        pop     esi
        pop     edi
        retn
    }
}

// Sidecar-aware replacement for sub_422D80 — "any weapon with same ammoSlot
// owned by ps ?" Walks the full weapon table testing PsBitTest (which knows
// about high-idx sidecar) instead of vanilla's inline OOB-prone bitmap read.
static bool TestAnyWeaponSharesAmmoSlot(unsigned testIdx, playerState_s* ps)
{
    uint8_t* testDef = (uint8_t*)bg_weaponDefs[testIdx];
    if (!testDef) return false;
    const uint32_t testAmmoSlot = *(uint32_t*)(testDef + 0x3F4);
    const unsigned totalWeapons = *(uint32_t*)(0x046DE3BC) + 1;
    for (unsigned i = 1; i < totalWeapons && i < 600; ++i) {
        uint8_t* def_i = (uint8_t*)bg_weaponDefs[i];
        if (!def_i) continue;
        if (*(uint32_t*)(def_i + 0x3F4) != testAmmoSlot) continue;
        if (i == testIdx) continue;  // vanilla's `cmp [esp+10h+arg_0], edx ; jz skip`
        if (T4M::PsBitTest(ps, 0x7FC, i)) return true;
    }
    return false;
}


// =====================================================================
// Reconstruction of sub_41D6A0 — "G_TakeWeapon" (clears weapons[idx]
// bit + alt-firemode chain ; optionally clears ammo).
//
// Vanilla ABI : __usercall(esi = ps, edi = idx, [esp+4] = arg_0 dword
// "drop_ammo" flag). Returns 0 if not owned, 1 if removed.
// =====================================================================

namespace T4_Reconstructed
{
    extern "C" int __cdecl G_TakeWeapon(playerState_s* ps, unsigned idx, int drop_ammo)
    {
        if (T4M_GiveTraceEnabled) {
            const char* name = "(null def)";
            if (idx < 600) {
                auto* d = (uint8_t*)bg_weaponDefs[idx];
                if (d) {
                    const char* n = *(const char**)d;
                    if (n) name = n;
                }
            }
            T4::Com_Printf(0, "[T4M TAKE] ps=%p idx=%u name=%s drop_ammo=%d\n",
                (void*)ps, idx, name, drop_ammo);
        }

        // Test ownership ; if not owned, return 0.
        if (!T4M::PsBitTest(ps, 0x7FC, idx)) {
            return 0;
        }

        // Clear the bit.
        T4M::PsBitClear(ps, 0x7FC, idx);

        // Vanilla : if drop_ammo != 0, normalize ammo for this weapon's slot.
        if (drop_ammo != 0) {
            int total = call_BG_TotalAmmoIfReplaced((int)idx, ps, (int)idx);
            auto def = (uint8_t*)bg_weaponDefs[idx];
            uint32_t ammoSlot      = *(uint32_t*)(def + 0x3F4);
            uint32_t ammoClipSlot  = *(uint32_t*)(def + 0x3FC);
            int ps_ammo            = *(int*)((uint8_t*)ps + ammoSlot * 4 + 0x17C);
            int clamped            = (total != 0) ? ((ps_ammo > total) ? total : ps_ammo) : 0;
            *(int*)((uint8_t*)ps + ammoSlot * 4 + 0x17C)     = clamped;
            *(int*)((uint8_t*)ps + ammoClipSlot * 4 + 0x5FC) = 0;
        }

        // Walk alt-firemode chain from this weapon's def. For each linked alt
        // that's owned, clear its bit (+ optionally its ammo).
        auto def0 = (uint8_t*)bg_weaponDefs[idx];
        uint32_t altIdx = *(uint32_t*)(def0 + 0x63C);
        while (altIdx != 0) {
            if (!T4M::PsBitTest(ps, 0x7FC, altIdx)) break;
            if (drop_ammo != 0) {
                auto altDef = (uint8_t*)bg_weaponDefs[altIdx];
                uint32_t altAmmoSlot     = *(uint32_t*)(altDef + 0x3F4);
                uint32_t altAmmoClipSlot = *(uint32_t*)(altDef + 0x3FC);
                *(int*)((uint8_t*)ps + altAmmoSlot * 4 + 0x17C)     = 0;
                *(int*)((uint8_t*)ps + altAmmoClipSlot * 4 + 0x5FC) = 0;
            }
            T4M::PsBitClear(ps, 0x7FC, altIdx);
            auto altDef = (uint8_t*)bg_weaponDefs[altIdx];
            altIdx = *(uint32_t*)(altDef + 0x63C);
        }

        // Vanilla : if weaponstate in {0x1A, 0x1B, 0x1C} clear some flags.
        uint32_t weaponstate = *(uint32_t*)((uint8_t*)ps + 0x108);
        if (weaponstate == 0x1A || weaponstate == 0x1B || weaponstate == 0x1C) {
            *(uint32_t*)((uint8_t*)ps + 0xCC) &= 0xFFFFFCFFu;
            *(uint32_t*)((uint8_t*)ps + 0x10) &= 0xFFFFFFFDu;
            *(uint32_t*)((uint8_t*)ps + 0x0C) &= 0xFFFFFDFFu;
            *(uint32_t*)((uint8_t*)ps + 0x40) = 0;
            *(uint32_t*)((uint8_t*)ps + 0x44) = 0;
            *(uint32_t*)((uint8_t*)ps + 0x108) = 0;

            // Vanilla : if pm_type < 8 toggle the 0x200 bit at +0x910.
            if (*(uint32_t*)((uint8_t*)ps + 4) < 8) {
                uint32_t v = *(uint32_t*)((uint8_t*)ps + 0x910);
                *(uint32_t*)((uint8_t*)ps + 0x910) = (~v) & 0x200;
            }
        }

        // If the removed weapon was the current ps.weapon, reset to 0.
        if (*(uint32_t*)((uint8_t*)ps + 0x104) == idx) {
            *(uint32_t*)((uint8_t*)ps + 0x104) = 0;
        }

        return 1;
    }
}

namespace T4M
{
    // __usercall wrapper : sub_41D6A0 receives ps in esi, idx in edi, drop_ammo
    // pushed at [esp+4]. Translate to __cdecl (ps, idx, drop_ammo).
    extern "C" __declspec(naked) void G_TakeWeapon_Wrapper()
    {
        __asm {
            push    [esp+4]                 // arg_8 = drop_ammo
            push    edi                     // arg_4 = idx
            push    esi                     // arg_0 = ps
            call    T4_Reconstructed::G_TakeWeapon
            add     esp, 12
            retn                            // caller cleans the 1 stack arg
        }
    }
}


// =====================================================================
// Reconstruction of sub_5528D0 — "G_GiveWeapon" (sets weapons[idx] bit,
// clears weaponold/weaponrechamber, walks alt-firemode chain).
//
// Vanilla ABI : __usercall(ecx = ps, eax = idx, [esp+4] = arg_0 byte "weaponmodel").
// Returns 0 if not given (already owned, gated, etc.) or 1 if newly granted.
// =====================================================================

// Runtime trace toggle for G_GiveWeapon. Set to false to silence.
// Used to diagnose unexpected weapon grants at spawn / pickup. C-linkage so
// the naked wrapper below can reference it without name-mangling friction.
extern "C" bool T4M_GiveTraceEnabled = true;

// Engine return-address captured by G_GiveWeapon_Wrapper before the C++ call.
// Lets the trace report which engine site triggered the give. Single-threaded
// game logic, so a plain global is fine.
extern "C" volatile uint32_t T4M_GiveRetAddr = 0;

namespace T4_Reconstructed
{
    extern "C" int __cdecl G_GiveWeapon(playerState_s* ps, unsigned idx, int weaponmodel)
    {
        const uint8_t bl = (uint8_t)weaponmodel;

        if (T4M_GiveTraceEnabled) {
            const char* name = "(null def)";
            if (idx < 600) {
                auto* d = (uint8_t*)bg_weaponDefs[idx];
                if (d) {
                    const char* n = *(const char**)d;
                    if (n) name = n;
                }
            }
            // Dump ps.weapons[idx>>5] BEFORE we touch it, so we can correlate
            // bit-mutation events against the cycle dump even when no BITSET
            // fires (e.g. high idx routed to sidecar, or skipped via gate).
            const unsigned w = idx >> 5;
            const uint32_t live = (w < 4)
                ? *(uint32_t*)((uint8_t*)ps + 0x7FC + w * 4)
                : 0;
            T4::Com_Printf(0, "[T4M GIVE] ps=%p idx=%u name=%s model=%d ret=0x%08X live.w%u=0x%08X\n",
                (void*)ps, idx, name, weaponmodel, T4M_GiveRetAddr, w, live);
        }

        // Already owned ? Bail with 0.
        if (T4M::PsBitTest(ps, 0x7FC, idx)) {
            if (T4M_GiveTraceEnabled) T4::Com_Printf(0, "[T4M GIVE]   -> already owned, skip\n");
            return 0;
        }

        auto def = (uint8_t*)bg_weaponDefs[idx];
        if (!def) return 0;

        // Class gate : type 7 / 8 are non-grantable here.
        uint32_t cls = *(uint32_t*)(def + 0x148);
        if (cls == 7 || cls == 8) {
            if (T4M_GiveTraceEnabled) T4::Com_Printf(0, "[T4M GIVE]   -> cls=%u gated, skip\n", cls);
            return 0;
        }

        // Per-weaponmodel start-ammo gate : def[+0xC + bl*4] != 0.
        if (*(uint32_t*)(def + bl * 4 + 0x0C) == 0) {
            if (T4M_GiveTraceEnabled) T4::Com_Printf(0, "[T4M GIVE]   -> startammo[%u]=0, skip\n", bl);
            return 0;
        }

        // Set weapons[idx] ; clear weaponold[idx] and weaponrechamber[idx].
        T4M::PsBitSet  (ps, 0x7FC, idx);
        T4M::PsBitClear(ps, 0x80C, idx);
        T4M::PsBitClear(ps, 0x81C, idx);

        // Vanilla wrote ps.weaponmodels[idx] = bl. weaponmodels[128] @ 0xB50.
        // For idx >= 128 this OOB-writes. We bound to 127 to avoid corruption ;
        // a future sidecar for weaponmodels can lift this if needed.
        if (idx < 128) {
            *((uint8_t*)ps + idx + 0xB50) = bl;
        }

        // class == 10 : offhand-class — return 1 immediately.
        if (cls == 0x0A) return 1;

        // No linked-weapon (def[+0x174] == 0) : walk alt-firemode chain.
        if (*(uint32_t*)(def + 0x174) == 0) {
            uint32_t altIdx = *(uint32_t*)(def + 0x63C);
            while (altIdx != 0) {
                if (T4M::PsBitTest(ps, 0x7FC, altIdx)) break;
                if (T4M_GiveTraceEnabled) {
                    const char* aname = "(null)";
                    if (altIdx < 600) {
                        auto* ad = (uint8_t*)bg_weaponDefs[altIdx];
                        if (ad) { const char* n = *(const char**)ad; if (n) aname = n; }
                    }
                    T4::Com_Printf(0, "[T4M GIVE]   -> alt-chain altIdx=%u name=%s\n", altIdx, aname);
                }
                T4M::PsBitSet  (ps, 0x7FC, altIdx);
                // Vanilla quirk : clears weaponrechamber at the OUTER idx's word,
                // not at altIdx's. Faithful copy.
                T4M::PsBitClear(ps, 0x81C, idx);
                if (altIdx < 128) {
                    *((uint8_t*)ps + altIdx + 0xB50) = bl;
                }
                auto altDef = (uint8_t*)bg_weaponDefs[altIdx];
                if (!altDef) break;
                altIdx = *(uint32_t*)(altDef + 0x63C);
            }
            return 1;
        }

        // Has linked weapon : pickup-link logic.
        uint32_t offhandIdx = *(uint32_t*)((uint8_t*)ps + 0xFC);
        if (offhandIdx == 0) {
            // No current offhand : assign this idx as offhand and link.
            *(uint32_t*)((uint8_t*)ps + 0xFC) = idx;
            call_G_LinkActionSlot((int)idx);    // vanilla : eax = edi = outer idx
            return 1;
        }

        // Have an offhand : check if its ammo total > 0.
        int ammoTotal = call_BG_AmmoTotal((int)offhandIdx, ps);
        if (ammoTotal > 0) return 1;

        // Try to find a linked weapon for the existing offhand's link-hash.
        auto oldOffhandDef = (uint8_t*)bg_weaponDefs[offhandIdx];
        uint32_t linkHash  = oldOffhandDef ? *(uint32_t*)(oldOffhandDef + 0x174) : 0;
        int found = call_BG_FindLinkedWeapon(ps, (int)linkHash);
        if (found != 0) {
            *(uint32_t*)((uint8_t*)ps + 0xFC) = (uint32_t)found;
        } else {
            *(uint32_t*)((uint8_t*)ps + 0xFC) = idx;
        }
        uint32_t newOffhand = *(uint32_t*)((uint8_t*)ps + 0xFC);
        call_G_LinkActionSlot((int)newOffhand); // vanilla : eax = ps.offHandIndex
        return 1;
    }
}

namespace T4M
{
    // __usercall wrapper : sub_5528D0 receives ps in ecx, idx in eax,
    // weaponmodel byte at [esp+4]. Translate to __cdecl (ps, idx, model).
    // Also capture the engine return-address into T4M_GiveRetAddr so the trace
    // (when enabled) can identify the calling site.
    extern "C" __declspec(naked) void G_GiveWeapon_Wrapper()
    {
        __asm {
            push    edx                     // scratch save
            mov     edx, [esp+4]            // engine return-address
            mov     T4M_GiveRetAddr, edx
            pop     edx
            push    [esp+4]                 // arg_8 = weaponmodel byte (in low byte)
            push    eax                     // arg_4 = idx
            push    ecx                     // arg_0 = ps
            call    T4_Reconstructed::G_GiveWeapon
            add     esp, 12
            retn                            // caller cleans the 1 stack arg
        }
    }
}


// =====================================================================
// Watchdog : poll sv.ps.weapons[2] (= word containing m1garand bit) at
// several boundary points. The bit mutates from 0x80 to 0x04 between our
// G_GiveWeapon and the cycle dump, but no PsBitSet / PsBitClear / TAKE
// fires in between. Goal : narrow down which boundary the mutation crosses.
// =====================================================================

namespace
{
    // Watch the 16-dword strip covering weapons[0..3] + weaponold[0..3] +
    // weaponrechamber[0..3] + proneDir/proneDirPitch/proneTorsoPitch/viewlocked
    // (= +0x7FC..+0x83B). Any OOB write from per-slot indexing into ammoClip[]
    // or ammo[] (slot ≥ 128) lands somewhere in this strip.
    constexpr unsigned k_watchStripDwords = 16;
    static volatile uint32_t g_lastStrip[k_watchStripDwords] = {
        0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu,
        0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu,
        0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu,
        0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu,
    };

    static const char* StripFieldName(unsigned i)
    {
        // Each entry is the dword at +0x7FC + i*4.
        switch (i) {
            case 0: case 1: case 2: case 3:    return "weapons";
            case 4: case 5: case 6: case 7:    return "weaponold";
            case 8: case 9: case 10: case 11:  return "weaponrechamber";
            case 12: return "proneDirection";
            case 13: return "proneDirectionPitch";
            case 14: return "proneTorsoPitch";
            case 15: return "viewlocked";
            default: return "unknown";
        }
    }

    void PollServerW2(const char* label)
    {
        uint8_t* ent0 = (uint8_t*)0x0176C6F0;
        playerState_s* sps = *(playerState_s**)(ent0 + 0x180);
        if (!sps) return;
        const uint32_t* strip = (const uint32_t*)((uint8_t*)sps + 0x7FC);
        for (unsigned i = 0; i < k_watchStripDwords; ++i) {
            const uint32_t v = strip[i];
            if (v != g_lastStrip[i]) {
                const unsigned offset = 0x7FC + i * 4;
                T4::Com_Printf(0,
                    "[T4M WATCH/%s] ps=%p %s[%u] (+0x%X) changed: 0x%08X -> 0x%08X\n",
                    label, (void*)sps, StripFieldName(i), i & 3, offset,
                    g_lastStrip[i], v);
                g_lastStrip[i] = v;
            }
        }
    }
}


// =====================================================================
// Diagnostic : on each call to sub_46A090 (CG cycle handler used by
// `weapnext` / `weapprev`), dump every idx 0..511 that the engine's inline
// bitmap test would consider "owned" via cg_predictedPlayerState.weapons[]
// (= dword_351E74C). Reveals phantom idx caused by OOB-aliasing into the
// adjacent CG-state dwords (weaponold/rechamber/proneDirection/etc.).
// =====================================================================

namespace
{
    constexpr uintptr_t k_cgWeaponsBase = 0x0351E74C; // = cg_predictedPlayerState.weapons[0]

    // Server-side gentity_s[0].playerState_s pointer (= local player ps in SP/host).
    // gentity table base = dword_176C6F0, entity stride = 0x378, ps ptr field = +0x180.
    static playerState_s* GetServerPs()
    {
        uint8_t* ent0 = (uint8_t*)0x0176C6F0;
        return *(playerState_s**)(ent0 + 0x180);
    }

    void DumpCycleOwned()
    {
        T4::Com_Printf(0, "[T4M CYCLE] ---- cg_predictedPS.weapons[] dump ----\n");
        // First, raw dump of cg.weapons[0..3] dwords as hex for direct compare.
        {
            const uint32_t* w = (uint32_t*)k_cgWeaponsBase;
            T4::Com_Printf(0, "[T4M CYCLE] cg.weapons[0..3] = %08X %08X %08X %08X\n",
                w[0], w[1], w[2], w[3]);
        }
        // Server-side ps for compare.
        playerState_s* sps = GetServerPs();
        T4::Com_Printf(0, "[T4M CYCLE] GetServerPs() = %p\n", (void*)sps);
        if (sps) {
            const uint32_t* sw = (uint32_t*)((uint8_t*)sps + 0x7FC);
            T4::Com_Printf(0, "[T4M CYCLE] sv.weapons[0..3] = %08X %08X %08X %08X\n",
                sw[0], sw[1], sw[2], sw[3]);
            const uint32_t* swo = (uint32_t*)((uint8_t*)sps + 0x80C);
            T4::Com_Printf(0, "[T4M CYCLE] sv.weaponold[0..3] = %08X %08X %08X %08X\n",
                swo[0], swo[1], swo[2], swo[3]);
        } else {
            T4::Com_Printf(0, "[T4M CYCLE] sv.ps = NULL\n");
        }
        // Sidecar high bits (idx 128..511).
        if (sps) {
            auto it = T4M::g_psBitmapExt.find(sps);
            if (it != T4M::g_psBitmapExt.end()) {
                const auto& ext = it->second;
                T4::Com_Printf(0,
                    "[T4M CYCLE] sidecar.weapons_high[0..11] = %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n",
                    ext.weapons_high[0], ext.weapons_high[1], ext.weapons_high[2], ext.weapons_high[3],
                    ext.weapons_high[4], ext.weapons_high[5], ext.weapons_high[6], ext.weapons_high[7],
                    ext.weapons_high[8], ext.weapons_high[9], ext.weapons_high[10], ext.weapons_high[11]);
            } else {
                T4::Com_Printf(0, "[T4M CYCLE] sidecar = absent (lazy-init)\n");
            }
        }
        // Per-idx detected dump (= inline test).
        for (unsigned idx = 0; idx < 512; ++idx) {
            const unsigned word = idx >> 5;
            const uint32_t bit  = 1u << (idx & 0x1F);
            const uint32_t val  = *(uint32_t*)(k_cgWeaponsBase + word * 4);
            if (val & bit) {
                const char* name = "(null def)";
                if (idx < 600) {
                    auto* d = (uint8_t*)bg_weaponDefs[idx];
                    if (d) {
                        const char* n = *(const char**)d;
                        if (n) name = n;
                    }
                }
                T4::Com_Printf(0, "[T4M CYCLE]   inline-owned idx=%u name=%s word=%u bit=0x%08X\n",
                    idx, name, word, bit);
            }
        }
        T4::Com_Printf(0, "[T4M CYCLE] ---- end dump ----\n");
    }
}


// =====================================================================
// Reconstruction of sub_4FC510 — "refill clip from reserve" ammo helper.
//
// Vanilla ABI : __usercall(ecx = idx, esi = ps). No stack args, returns nothing.
//
// Same per-slot OOB issue as sub_4FC570 : writes to ps.ammo[ammoSlot] (+0x17C)
// and ps.ammoClip[clipSlot] (+0x5FC) for slot ≥ 128 corrupt adjacent fields.
// Called directly from sub_4ED310 (chain processor, in the !ebp branch) as
// well as from sub_4FC570, so a standalone detour is needed.
// =====================================================================

namespace T4_Reconstructed
{
    extern "C" void __cdecl Sub_4FC510(unsigned idx, playerState_s* ps)
    {
        if (idx == 0) return;
        const unsigned totalWeapons = *(uint32_t*)(0x046DE3BC) + 1;
        if (idx >= totalWeapons) return;

        uint8_t* def = (uint8_t*)bg_weaponDefs[idx];
        if (!def) return;

        const unsigned ammoSlot = *(uint32_t*)(def + 0x3F4);
        const unsigned clipSlot = *(uint32_t*)(def + 0x3FC);

        int* const pClip = T4M::PsAmmoClipPtr(ps, clipSlot);
        int* const pAmmo = T4M::PsAmmoPtr(ps, ammoSlot);

        const int oldClip = *pClip;
        const int maxClip = call_sub_41C930(def);

        int transferable = maxClip - oldClip;
        if (transferable > *pAmmo) transferable = *pAmmo;
        if (transferable <= 0) return;

        *pAmmo -= transferable;
        *pClip += transferable;
    }
}

namespace T4M
{
    // __usercall wrapper : sub_4FC510 receives idx in ecx, ps in esi.
    extern "C" __declspec(naked) void Sub_4FC510_Wrapper()
    {
        __asm {
            push    esi                     // ps (cdecl arg_1)
            push    ecx                     // idx (cdecl arg_0)
            call    T4_Reconstructed::Sub_4FC510
            add     esp, 8
            retn
        }
    }
}


// =====================================================================
// Reconstruction of sub_4FC570 — "give-ammo / pickup chain helper".
//
// Vanilla ABI : __usercall(eax = gentity_owner) + stack(arg_0 = idx,
// arg_4 = passed-through to nested G_GiveWeapon, arg_8 = ammoToAdd,
// arg_C = "force ammo recompute" flag). Caller cleans stack. Returns int
// (= delta clipAfter + ammoAfter - clipBefore - ammoBefore) in eax.
//
// Why detoured (= the bug we observed) : the vanilla function reads/writes
//   ps.ammo[ammoSlot]      @ +0x17C + ammoSlot*4
//   ps.ammoClip[clipSlot]  @ +0x5FC + clipSlot*4
// Arrays sized for vanilla 128 weapons. For T4M-extended weapons with
// slot ≥ 128, the OOB write at loc_4FC6F8 (`mov [esi+ebx*4+5FCh], eax`)
// lands in adjacent fields — including the weapons[] bitmap at +0x7FC.
// That bit-7 wipe is what caused m1garand to disappear from inventory
// at spawn (cycle dump showed sv.weapons[2] = 0x04 instead of 0x80).
//
// Reconstruction routes ammo/clip accesses through PsAmmoPtr /
// PsAmmoClipPtr (in-struct for slot < 128, sidecar for slot ≥ 128).
// sub_422D80 is replaced by TestAnyWeaponSharesAmmoSlot, and sub_4FC510
// is called via its own C++ reconstruction (same OOB issue there).
// =====================================================================

namespace T4_Reconstructed
{
    extern "C" int __cdecl Sub_4FC570(
        void* ent, unsigned idx, int arg_4, int ammoToAdd, int flag)
    {
        playerState_s* ps = *(playerState_s**)((uint8_t*)ent + 0x180);
        uint8_t* def = (uint8_t*)bg_weaponDefs[idx];
        if (!def) return 0;

        // Gate: owned ? or some other weapon shares ammoSlot ? or def has
        // chain marker = 1 ? Otherwise early-return 0.
        const bool owned = T4M::PsBitTest(ps, 0x7FC, idx);
        bool sharedAmmo = false;
        if (!owned) {
            sharedAmmo = TestAnyWeaponSharesAmmoSlot(idx, ps);
            if (!sharedAmmo) {
                if (*(uint32_t*)(def + 0x174) != 1) {
                    return 0;
                }
            }
        }

        // Ammo path (loc_4FC5C0).
        const unsigned ammoSlot = *(uint32_t*)(def + 0x3F4);
        const unsigned clipSlot = *(uint32_t*)(def + 0x3FC);
        int* const pAmmo = T4M::PsAmmoPtr(ps, ammoSlot);
        int* const pClip = T4M::PsAmmoClipPtr(ps, clipSlot);

        const int oldClip = *pClip;
        const int oldAmmo = *pAmmo;

        const int total = call_BG_TotalAmmoIfReplaced((int)idx, ps, 0);
        *pAmmo += ammoToAdd;

        bool didGive = false;
        if (*(uint32_t*)(def + 0x5EC) != 0) {
            T4_Reconstructed::G_GiveWeapon(ps, idx, arg_4);
            didGive = true;
        }

        // loc_4FC62B branch
        if (flag != 0 || didGive) {
            T4_Reconstructed::Sub_4FC510(idx, ps);
        }
        if (didGive) {
            *pAmmo = 0;
        } else {
            if (*pAmmo > total) *pAmmo = total;
        }

        // loc_4FC662 — clip scaling (def[+0x408] is the scaling factor)
        const int defScale = *(int*)(def + 0x408);
        int newClipMax = 1;
        if (defScale > 1) {
            const float mult = *(float*)((*(uint32_t*)0x008F187C) + 0x10);
            const double halfRoundConst = *(double*)0x008AF568;
            const double scaled = (double)((float)defScale * mult);
            const int rounded = (int)(scaled + halfRoundConst);
            if (rounded > 0) newClipMax = rounded;
        }

        // loc_4FC6AC : cap current ammoClip to scaled max (vanilla recomputes
        // an identical scaling here, kept as a single computation).
        if (*pClip > newClipMax) {
            *pClip = newClipMax;
        }

        // loc_4FC6FF : optional take-if-empty path.
        if (*(int*)(def + 0x414) >= 0) {
            const int helperRet = call_sub_41D8B0((int)idx);
            if (helperRet < 0) {
                if (*(uint32_t*)(def + 0x5EC) != 0) {
                    *pClip += helperRet;
                    if (*pClip <= 0) {
                        T4_Reconstructed::G_TakeWeapon(ps, idx, 1);
                        return 0;  // vanilla falls through to loc_4FC747 (= ret 0)
                    }
                } else {
                    // loc_4FC751
                    *pAmmo += helperRet;
                    if (*pAmmo < 0) *pAmmo = 0;
                }
            }
        }

        // loc_4FC765 — return delta
        return (*pClip - oldClip) + (*pAmmo - oldAmmo);
    }
}

namespace T4M
{
    // __usercall wrapper : sub_4FC570 receives ent in eax, idx/arg_4/arg_8/arg_C
    // on stack (caller-cleaned). Forward 5 args to the cdecl reconstruction.
    extern "C" __declspec(naked) void Sub_4FC570_Wrapper()
    {
        __asm {
            push    [esp+10h]               // arg_C
            push    [esp+10h]               // arg_8 (shifted +4 due to prev push)
            push    [esp+10h]               // arg_4
            push    [esp+10h]               // arg_0 = idx
            push    eax                     // ent (was in eax)
            call    T4_Reconstructed::Sub_4FC570
            add     esp, 14h                // pop our 5 pushes
            retn                            // caller cleans the 4 stack args
        }
    }
}


// =====================================================================
// Reconstruction of sub_41E5E0 — "subtract clip ammo for weapon idx".
//
// Vanilla ABI : __usercall(esi = amount, edi = ps) + stack(arg_0 = idx).
// Caller cleans the 1 stack arg. Returns nothing.
//
// Logic : guarded by dword_8F449C[+0x10] / sub_41E4E0 (special-name skip).
// If gate passes, decrements ps.ammoClip[def.clipSlot] by amount, clamped
// to 0 minimum. Vanilla writes `*pClip = clip - min(clip, amount)`.
//
// Why detoured : the `mov [edi+ecx*4+5FCh], eax` write at 0x0041E619 OOB
// for clipSlot ≥ 128 (T4M-extended weapons). Same pattern as sub_4FC510.
// =====================================================================

namespace T4_Reconstructed
{
    extern "C" void __cdecl Sub_41E5E0(int amount, playerState_s* ps, unsigned idx)
    {
        // Gate : dword_8F449C is a global ptr ; if [+0x10] != 0, special-name
        // weapons skip this whole function via sub_41E4E0.
        uint8_t* global = *(uint8_t**)0x008F449C;
        if (global && *(global + 0x10) != 0) {
            if (call_sub_41E4E0(idx) != 0) return;
        }

        uint8_t* def = (uint8_t*)bg_weaponDefs[idx];
        if (!def) return;

        const unsigned clipSlot = *(uint32_t*)(def + 0x3FC);
        int* const pClip = T4M::PsAmmoClipPtr(ps, clipSlot);
        const int clip = *pClip;
        const int subAmount = (clip < amount) ? clip : amount;
        *pClip = clip - subAmount;
    }
}

namespace T4M
{
    // __usercall wrapper : esi = amount, edi = ps, stack(arg_0 = idx).
    extern "C" __declspec(naked) void Sub_41E5E0_Wrapper()
    {
        __asm {
            push    [esp+4]                 // idx (cdecl arg 2)
            push    edi                     // ps (cdecl arg 1)
            push    esi                     // amount (cdecl arg 0)
            call    T4_Reconstructed::Sub_41E5E0
            add     esp, 12                 // pop our 3 cdecl args
            retn                            // caller cleans the 1 stack arg
        }
    }
}


// =====================================================================
// Reconstruction of sub_41E350 — "reload current weapon clip from ammo".
//
// Vanilla ABI : __usercall(esi = ps). No stack args, no return.
//
// Reads ps.weapon (+0x104) for current weapon idx, looks up def, optionally
// computes a scaled clip max via def[+0x408] × global multiplier, and
// transfers ammo into clip up to that max (or altCap def[+0x634] /
// def[+0x630] for special weapon states). Emits a HUD reload event at the
// ring buffer at ps[+0xD0].
//
// Why detoured : the writes at 0x0041E424 / 0x0041E42B
//   `sub [esi+ebp*4+17Ch], edi ; add [esi+ebx*4+5FCh], edi`
// OOB for ammoSlot ≥ 128 or clipSlot ≥ 128 (T4M-extended weapons).
// =====================================================================

namespace T4_Reconstructed
{
    namespace {
        // Scaled clip max computation : (scaleFactor * mult + halfRound) → int,
        // clamped to 1 minimum. Replicates the FPU rounding pattern in vanilla.
        static int ComputeScaledClipMax(int scaleFactor)
        {
            if (scaleFactor <= 1) return 1;
            const float mult = *(float*)((*(uint32_t*)0x008F187C) + 0x10);
            const double halfRoundConst = *(double*)0x008AF568;
            const double scaled = (double)((float)scaleFactor * mult);
            const int rounded = (int)(scaled + halfRoundConst);
            return (rounded > 0) ? rounded : 1;
        }
    }

    extern "C" void __cdecl Sub_41E350(playerState_s* ps)
    {
        const uint32_t weapState = *(uint32_t*)((uint8_t*)ps + 0x108);
        const uint32_t weapIdx   = *(uint32_t*)((uint8_t*)ps + 0x104);
        uint8_t* def = (uint8_t*)bg_weaponDefs[weapIdx];
        if (!def) return;

        const bool stateGate = (weapState == 9 || weapState == 0xA);
        if (stateGate && *(uint32_t*)(def + 0x634) == 0) return;

        const int scaleFactor   = *(int*)(def + 0x408);
        const unsigned clipSlot = *(uint32_t*)(def + 0x3FC);
        const unsigned ammoSlot = *(uint32_t*)(def + 0x3F4);
        int* const pAmmo = T4M::PsAmmoPtr(ps, ammoSlot);
        int* const pClip = T4M::PsAmmoClipPtr(ps, clipSlot);
        const int ammo = *pAmmo;
        const int clip = *pClip;

        const int clipMax = ComputeScaledClipMax(scaleFactor);
        int toReload = clipMax - clip;
        if (toReload > ammo) toReload = ammo;

        if (stateGate) {
            // loc_41E476 path — state 9/A : cap to def[+0x634] if smaller
            // than clipMax (vanilla recomputes clipMax, kept simple).
            const int altCap   = *(int*)(def + 0x634);
            const int clipMax2 = ComputeScaledClipMax(scaleFactor);
            if (altCap < clipMax2 && toReload > altCap) {
                toReload = altCap;
            }
        } else {
            // Normal path — if def[+0x630] != 0, cap to altReload when smaller
            // than scaledMax of def itself (= sub_41C930(def)).
            const int altReload = *(int*)(def + 0x630);
            if (altReload != 0) {
                const int scaledMax = call_sub_41C930(def);
                if (altReload < scaledMax && toReload > altReload) {
                    toReload = altReload;
                }
            }
        }

        if (toReload == 0) return;

        *pAmmo -= toReload;
        *pClip += toReload;

        // HUD reload-event ring buffer (vanilla quirk)
        uint32_t hudIdx = *(uint32_t*)((uint8_t*)ps + 0xD0) & 3;
        *(uint32_t*)((uint8_t*)ps + 0xD4 + hudIdx * 4) = 0x16;     // event tag 0x16 = reload
        *(uint32_t*)((uint8_t*)ps + 0xE4 + hudIdx * 4) = 0;
        uint32_t newCounter = (*(uint32_t*)((uint8_t*)ps + 0xD0) + 1) & 0xFF;
        *(uint32_t*)((uint8_t*)ps + 0xD0) = newCounter;
    }
}

namespace T4M
{
    // __usercall wrapper : sub_41E350 takes ps in esi, no stack args.
    extern "C" __declspec(naked) void Sub_41E350_Wrapper()
    {
        __asm {
            push    esi                     // ps
            call    T4_Reconstructed::Sub_41E350
            add     esp, 4
            retn
        }
    }
}


// =====================================================================
// Install
// =====================================================================

void PatchT4MAM_PsBitmapExt()
{
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // static auto take_detour = safetyhook::create_inline(
    //     (void*)0x0041D6A0, (void*)&T4M::G_TakeWeapon_Wrapper);
    // static auto give_detour = safetyhook::create_inline(
    //     (void*)0x005528D0, (void*)&T4M::G_GiveWeapon_Wrapper);
    // static auto fc570_detour = safetyhook::create_inline(
    //     (void*)0x004FC570, (void*)&T4M::Sub_4FC570_Wrapper);
    // static auto fc510_detour = safetyhook::create_inline(
    //     (void*)0x004FC510, (void*)&T4M::Sub_4FC510_Wrapper);
    // static auto e5e0_detour = safetyhook::create_inline(
    //     (void*)0x0041E5E0, (void*)&T4M::Sub_41E5E0_Wrapper);
    // static auto e350_detour = safetyhook::create_inline(
    //     (void*)0x0041E350, (void*)&T4M::Sub_41E350_Wrapper);
    // (void)fc570_detour;
    // (void)fc510_detour;
    // (void)e5e0_detour;
    // (void)e350_detour;
    // static auto cycle_dump_hook = safetyhook::create_mid(
    //     (void*)0x0046A090,
    //     [](SafetyHookContext& /*ctx*/) { DumpCycleOwned(); });
    //
    // // Watchdog hooks : poll sv.weapons[2] at several boundaries to localize
    // // which subsystem clears bit 7 and sets bit 2 between our G_GiveWeapon
    // // (which writes 0x80) and the cycle dump (which sees 0x04).
    // static auto watch_4ED3D0 = safetyhook::create_mid(
    //     (void*)0x004ED3D0,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_GScrGiveW"); });
    // static auto watch_4ED310 = safetyhook::create_mid(
    //     (void*)0x004ED310,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_ChainProc"); });
    // // watch_4FC570 disabled — would conflict with the create_inline detour
    // // installed below at the same VA.
    // static auto watch_4FC7F0 = safetyhook::create_mid(
    //     (void*)0x004FC7F0,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_FC7F0"); });
    // static auto watch_4FCEB0 = safetyhook::create_mid(
    //     (void*)0x004FCEB0,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_FCEB0"); });
    // // Hook the very instruction after `call sub_5528D0` inside sub_4ED3D0 :
    // // 0x004ED4EF = retaddr seen in the give trace. Fires right after our
    // // G_GiveWeapon returns, before sub_4ED310 (ChainProc) is reached.
    // static auto watch_post_give = safetyhook::create_mid(
    //     (void*)0x004ED4EF,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("POST_Give"); });
    // // Right after `call sub_4ED310` (line 346830 = `add esp, 0Ch` at 0x004ED4FA).
    // // Bisects between sub_4ED310 returning and sub_4ED3D0 returning.
    // static auto watch_post_chain = safetyhook::create_mid(
    //     (void*)0x004ED4FA,
    //     [](SafetyHookContext& /*ctx*/) { PollServerW2("POST_ChainProc"); });
    //
    // (void)take_detour;
    // (void)give_detour;
    // (void)cycle_dump_hook;
    // (void)watch_4ED3D0;
    // (void)watch_4ED310;
    // (void)watch_4FC7F0;
    // (void)watch_4FCEB0;
    // (void)watch_post_give;
    // (void)watch_post_chain;
}


// =====================================================================
// SECTION 5 / 7 - PatchT4MAM_CycleHandler.cpp
// =====================================================================

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
// Note 2026-05-11 : duplicate of the GetServerPs() defined in the PsBitmapExt
// section above (anonymous namespace). Commented out to avoid C2668 ambiguous
// call in the merged WIP archive. The call below resolves to the PsBitmapExt
// version (same body, same translation unit).
//
// static playerState_s* GetServerPs()
// {
//     uint8_t* ent0 = (uint8_t*)0x0176C6F0;
//     return *(playerState_s**)(ent0 + 0x180);
// }

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
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // static auto cycle_detour = safetyhook::create_inline(
    //     (void*)0x0046A090,
    //     (void*)&T4_Reconstructed::Sub_46A090);
    // (void)cycle_detour;
}


// =====================================================================
// SECTION 6 / 7 - PatchT4MAM_NetfieldCodec.cpp
// =====================================================================

// ==========================================================
// T4M project — PatchT4MAM_NetfieldCodec.cpp
//
// Phase P3 of plan_weapons_full_detour.md — snapshot netfield codec for
// playerState_s.weapon (16-bit on the wire).
//
// The vanilla netfield decoder `sub_676A50` (VA 0x676A50) dispatches per-field
// based on the `bits` value at off_836B30[entry].bits (= [entry+8]). For the
// `ps.weapon` entry (entry 54 @ VA 0x836E90), vanilla `bits == 7` truncates
// idx >= 128. The codec is bidirectional (encoder reads the same table), so
// patching the table's bits field 7 -> 16 makes both encoder and decoder use
// the 16-bit path symmetrically.
//
// IMPLEMENTATION :
//   1. Byte patch at VA 0x836E98 : 0x07 -> 0x10 (= 16 bits, mask 0xFFFF) for
//      ps.weapon entry. Encoder (sub_67B0C0) reads bits dynamically from the
//      same global table so the change propagates symmetrically.
//
//   2. Detour sub_676A50 with a naked wrapper. Dispatch :
//      - entry_ptr in [0x836B30, 0x837BD0) AND entry.bits > 0 -> C++ generic
//        positive-bits decoder (MSG_ReadDeltaField_PositiveBits) handling all
//        playerstate netfields (main table + origin/color.rgba sub-tables).
//      - else -> jmp trampoline (vanilla sub_676A50 unchanged), covering
//        entityState entries AND playerstate entries with bits <= 0
//        (negative special codes : -88 32-bit XOR, -89 5-bit signed delta,
//         vec3 components, etc.).
//
// Pass 4 (2026-05-10) completes the reconstruction. ALL 26 dispatch branches
// of sub_676A50 are now routed through C++ for playerstate entries (bits ∈
// {0, > 0, -77..-79, -82..-89, -91..-100} — covers every value observed in
// off_836B30 / origin / color.rgba sub-tables). Vanilla helpers (sub_676820,
// sub_676640, sub_676750, sub_6767C0) are shimmed via __usercall naked
// wrappers ; their FPU/float-math is reused as-is. Trampoline fall-through
// remains only for entityState entries (out of playerstate range).
//
// See analysis/netfield_codec_RE.md for the full branch flowchart.
//
// SAFETY NOTES :
// - `sub_676A50` is also called for entityState fields via sub_6770C0 (a
//   distinct iterator over a different netfield table at higher VA). The
//   range check on entry_ptr distinguishes playerstate from entityState.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include "include/cod/structs.hpp"
#include <safetyhook.hpp>
#include <cstdint>

using namespace T4;

namespace {

// Vanilla helper VAs.
namespace VA {
    // Note 2026-05-11 : MSG_ReadBit / MSG_ReadBits / MSG_ReadDword /
    // MSG_ReadShortSignExt already defined in the WireCodec section's VA
    // namespace above (same translation unit, same anonymous-namespace::VA).
    // Removed here to avoid C2374/C2086 redefinition errors in the merged WIP
    // archive. asm refs resolve to the WireCodec definitions (same values).
    constexpr uintptr_t MSG_ReadByte          = 0x00676690;  // __usercall(ecx=msg) -> eax (1 byte)
    constexpr uintptr_t MSG_ReadDeltaSpecial  = 0x006766D0;  // __usercall(edx=msg)+stack(arg_0) -> eax
    constexpr uintptr_t MSG_ReadDelta676640   = 0x00676640;  // __usercall(esi=arg_0,edx=msg) -> eax  (for bits == -78)
    constexpr uintptr_t MSG_ReadFloatVec1     = 0x00676750;  // __usercall(edx=msg)+stack(arg_0,arg_4) -> xmm0 (for bits == -91/-92)
    constexpr uintptr_t MSG_ReadFloatScale    = 0x006767C0;  // __usercall(edx=msg)+stack(arg_0) -> xmm0 (for bits == -90)
    constexpr uintptr_t MSG_ReadVec3Comp      = 0x00676820;  // __usercall(esi=msg,xmm0=hi)+stack(arg_0,arg_4,arg_8,arg_C) -> xmm0 (for vec3 family)
    constexpr uintptr_t SubMSG_676A50         = 0x00676A50;  // detour target
}

// Float scale constants referenced by various branches (vanilla .rdata).
constexpr uintptr_t k_dword_826A4C = 0x00826A4C;  // float : multi-use const (addend, hi)
constexpr uintptr_t k_dword_84D664 = 0x0084D664;  // float : multiplier for bits == -86
constexpr uintptr_t k_dword_8AF4FC = 0x008AF4FC;  // float : multiplier for bits == -100/-87
constexpr uintptr_t k_dword_8AF214 = 0x008AF214;  // float : lo for bits == -77
constexpr uintptr_t k_dword_8E46E4 = 0x008E46E4;  // float : hi for bits == -83
constexpr uintptr_t k_dword_8E4638 = 0x008E4638;  // float : hi for bits == -82

constexpr uintptr_t k_NetfieldTable_PsWeaponBits  = 0x00836E98; // = entry+8

} // namespace

// Trampoline pointer (set after install). Declared at global scope to ensure
// MSVC inline asm can resolve its name unambiguously (anonymous-namespace
// statics with `volatile` qualifier have caused asm linkage issues on this
// compiler version). Read via `mov ecx, [g_xxx]` in the wrapper.
extern "C" volatile uintptr_t g_msg_read_field_trampoline = 0;

namespace {

// =============================================================================
// __usercall shims for the helpers used by the ps.weapon decode path.
// =============================================================================

// sub_675060 — read 1 bit. __usercall(edx = msg) -> eax (0 or 1).
__declspec(naked) int call_MSG_ReadBit(void* /*msg*/)
{
    __asm
    {
        mov     edx, [esp+4]
        mov     ecx, VA::MSG_ReadBit
        jmp     ecx
    }
}

// sub_674F50 — read N bits raw. __usercall(edx = msg) + stack(bits) -> eax.
__declspec(naked) unsigned call_MSG_ReadBits(void* /*msg*/, unsigned /*bits*/)
{
    __asm
    {
        mov     edx, [esp+4]
        push    [esp+8]
        mov     ecx, VA::MSG_ReadBits
        call    ecx
        add     esp, 4
        retn
    }
}

// sub_675560 — read 32-bit signed value. __usercall(ecx = msg) -> eax.
__declspec(naked) int call_MSG_ReadDword(void* /*msg*/)
{
    __asm
    {
        mov     ecx, [esp+4]
        mov     edx, VA::MSG_ReadDword
        jmp     edx
    }
}

// sub_675500 — read 16-bit value, sign-extended to 32. __usercall(eax = msg) -> eax.
__declspec(naked) int call_MSG_ReadShortSignExt(void* /*msg*/)
{
    __asm
    {
        mov     eax, [esp+4]
        mov     edx, VA::MSG_ReadShortSignExt
        jmp     edx
    }
}

// sub_676690 — read 1 byte from msg buffer. __usercall(ecx = msg) -> eax (zero-extended byte).
__declspec(naked) int call_MSG_ReadByte(void* /*msg*/)
{
    __asm
    {
        mov     ecx, [esp+4]
        mov     edx, VA::MSG_ReadByte
        jmp     edx
    }
}

// sub_6766D0 — delta-special reader. __usercall(edx = msg) + stack(arg_0 = old) -> eax.
// Internally : reads 1 bit. If 1 -> reads 24 bits raw. If 0 -> reads 5 bits (= bit
// position N) and returns arg_0 ^ (1 << N) (= toggle bit N of old).
__declspec(naked) int call_MSG_ReadDeltaSpecial(void* /*msg*/, uint32_t /*old_val*/)
{
    __asm
    {
        mov     edx, [esp+4]
        push    [esp+8]
        mov     ecx, VA::MSG_ReadDeltaSpecial
        call    ecx
        add     esp, 4
        retn
    }
}

// sub_676640 — special path reader for bits == -78. __usercall(esi = arg_0,
// edx = msg) -> eax. Internally : 1-bit cascade dispatching to readbits(3)*50 /
// readbits(8)*50 / sub_675560 (32-bit raw).
__declspec(naked) int call_MSG_ReadDelta676640(void* /*msg*/, int /*esi_value*/)
{
    __asm
    {
        push    esi
        mov     edx, [esp+8]
        mov     esi, [esp+12]
        mov     eax, VA::MSG_ReadDelta676640
        call    eax
        pop     esi
        retn
    }
}

// sub_676750 — float-vec1 reader for bits == -91/-92. __usercall(edx = msg) +
// stack(arg_0 = bits_value, arg_4 = old_float) -> xmm0. Returns float bits via eax.
__declspec(naked) uint32_t call_MSG_ReadFloatVec1(
    void* /*msg*/, int /*bits_value*/, uint32_t /*old_float_bits*/)
{
    __asm
    {
        mov     edx, [esp+4]
        push    [esp+12]                // arg_4 = old_float_bits
        push    [esp+12]                // arg_0 = bits_value
        mov     eax, VA::MSG_ReadFloatVec1
        call    eax
        add     esp, 8
        // Convert xmm0 to eax for cdecl uint32_t return.
        movd    eax, xmm0
        retn
    }
}

// sub_6767C0 — float-scale reader for bits == -90. __usercall(edx = msg) +
// stack(arg_0 = old_float) -> xmm0.
__declspec(naked) uint32_t call_MSG_ReadFloatScale(
    void* /*msg*/, uint32_t /*old_float_bits*/)
{
    __asm
    {
        mov     edx, [esp+4]
        push    [esp+8]                 // arg_0 = old_float_bits
        mov     eax, VA::MSG_ReadFloatScale
        call    eax
        add     esp, 4
        movd    eax, xmm0
        retn
    }
}

// sub_676820 — vec3 component reader for bits == -77/-80/-81/-82/-83/-84.
// __usercall(esi = msg, xmm0 = hi_float) + stack(arg_0 = old_float, arg_4 = lo_float,
//                                                  arg_8 = range_int, arg_C = flag_int) -> xmm0.
__declspec(naked) uint32_t call_MSG_ReadVec3Comp(
    void*    /*msg*/,
    uint32_t /*old_float_bits*/,
    uint32_t /*lo_float_bits*/,
    int      /*range*/,
    int      /*flag*/,
    uint32_t /*hi_float_bits*/)
{
    __asm
    {
        push    esi
        mov     esi, [esp+8]            // msg
        movss   xmm0, dword ptr [esp+28] // hi_float (loaded as float bits)
        push    [esp+24]                 // arg_C = flag (after 1 push, slot at +24)
        push    [esp+24]                 // arg_8 = range
        push    [esp+24]                 // arg_4 = lo_float
        push    [esp+24]                 // arg_0 = old_float
        mov     eax, VA::MSG_ReadVec3Comp
        call    eax
        add     esp, 16
        movd    eax, xmm0
        pop     esi
        retn
    }
}

} // namespace


// =============================================================================
// C++ reconstruction of sub_676A50 -- generic positive-bits-int decoder
// covering ALL playerstate netfield entries with bits > 0. Routes to the
// vanilla loc_676F86 path with all upstream gates (delta-flag, zero-flag,
// type-5 "is-2" check). Dispatched by the wrapper based on a range check
// (entry in [off_836B30, off_837BD0)) and bits > 0.
//
// Vanilla wire format for any positive-bits entry :
//   bit 1 : delta-flag (loc_676A86)
//     0  -> field unchanged (copy old, return)
//     1  -> continue
//   (special-bits cascade : no match for positive bits, falls to loc_676F47)
//   bit 2 : zero-flag (loc_676F47, only when type != 4)
//     0  -> field is zero (store 0, return)
//     1  -> continue
//   bit 3 : type-5 "is-2" check (loc_676F68, only when type == 5)
//     1  -> field is 2 (store 2, return)
//     0  -> continue
//   bits N : raw value (loc_676F86), delta-XOR with (old & mask)
//
// Type-byte semantics observed in vanilla :
//   type == 0 (default)   -> delta-flag + zero-flag + N-bit value
//   type == 2 (always-on) -> skip delta-flag, then zero-flag + N-bit value
//   type == 4 (no-zero)   -> delta-flag + skip zero-flag + N-bit value
//   type == 5 (allow-2)   -> delta-flag + zero-flag + "is-2" bit + N-bit value
//
// EARLIER BUG (fixed 2026-05-10) : C++ skipped the zero-flag read entirely,
// consuming N+1 bits where vanilla consumes N+2 bits. The 1-bit wire desync
// shifted ALL subsequent netfield reads by 1 bit, corrupting the entire
// snapshot stream and crashing CL_GetSnapshot's memcpy with a garbage size.
// =============================================================================

namespace T4_Reconstructed
{
    extern "C" void __cdecl MSG_ReadDeltaField_PositiveBits(
        void*  msg,            // arg in edi (vanilla); passed via stack here
        int    arg_0,          // unused for positive-bits branch
        void*  oldStruct,      // arg_4
        void*  newStruct,      // arg_8
        void*  entry,          // arg_C : the table entry ptr (in off_836B30 range)
        int    debug_print,    // arg_10
        int    zero_old_byte)  // arg_14 (low byte = "use zero as old")
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);           // = 0x104
        int     entry_bits   = *(int*)(entry_b + 8);           // = 7 vanilla / 16 patched
        uint8_t entry_type   = *(entry_b + 0xC);               // byte read (faithful)

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        // -- Entry init (faithful to vanilla lines 945777-945794) --
        // arg_14 != 0 -> use zero as old. Else -> *(oldStruct + offset).
        uint32_t old_val;
        if ((zero_old_byte & 0xFF) != 0) {
            old_val = 0;
        } else {
            old_val = *(uint32_t*)((uint8_t*)oldStruct + entry_offset);
        }

        // -- (1) Delta-flag (lines 945795-945807) --
        // type != 2 -> read 1-bit delta-flag ; if 0 -> copy old, return.
        if (entry_type != 2) {
            if (call_MSG_ReadBit(msg) == 0) {
                *(uint32_t*)dest = old_val;
                return;
            }
        }

        // ebx = bits dispatch (lines 945810+). For ps.weapon (bits=7, positive),
        // none of the special-bits codes match ; the cascade falls through to
        // loc_676F47.

        // -- (2) Zero-flag (loc_676F47, lines 946381-946395) --
        // type != 4 -> read 1-bit zero-flag ; if 0 -> store 0, return.
        if (entry_type != 4) {
            if (call_MSG_ReadBit(msg) == 0) {
                *(uint32_t*)dest = 0;
                return;
            }
        }

        // -- (3) Type-5 special "is-2" check (loc_676F68, lines 946402-946417) --
        // type == 5 -> read 1 bit ; if 1 -> store 2, return ; else fall to
        // loc_676F86. For ps.weapon (type=0) this is a no-op pass-through.
        if (entry_type == 5) {
            if (call_MSG_ReadBit(msg) == 1) {
                *(uint32_t*)dest = 2;
                return;
            }
        }

        // -- (4) Positive-bits-int generic path (loc_676F86) --
        // For ps.weapon entry_bits = 7 (vanilla) or 16 (post P3 byte patch).
        // No sign-extension needed (bits is positive).
        uint32_t bits = (uint32_t)entry_bits;
        uint32_t raw  = call_MSG_ReadBits(msg, bits);
        uint32_t mask = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
        uint32_t new_val = (old_val & mask) ^ raw;

        *(uint32_t*)dest = new_val;
    }


    // =========================================================================
    // Helper : standard delta-flag read at function start.
    // Returns true if the field changed (= continue decode), false if it was
    // copied from old (= function should return). Replicates the delta-flag
    // logic shared by all negative-bits-dispatch branches.
    // =========================================================================
    static bool ReadDeltaFlagOrCopyOld(
        void*    msg,
        uint8_t  entry_type,
        void*    oldStruct,
        void*    newStruct,
        int      entry_offset,
        int      zero_old_byte,
        uint32_t* out_old_val)
    {
        uint32_t old_val;
        if ((zero_old_byte & 0xFF) != 0) {
            old_val = 0;
        } else {
            old_val = *(uint32_t*)((uint8_t*)oldStruct + entry_offset);
        }
        *out_old_val = old_val;

        if (entry_type != 2) {
            if (call_MSG_ReadBit(msg) == 0) {
                *(uint32_t*)((uint8_t*)newStruct + entry_offset) = old_val;
                return false;
            }
        }
        return true;
    }


    // =========================================================================
    // bits == -88 (0xFFFFFFA8) -- 32-bit XOR delta
    //
    // Vanilla flow (loc_676ABC, lines 945830-945855) :
    //   raw = MSG_ReadDword(msg)            ; sub_675560, 32-bit signed
    //   new = old ^ raw
    //
    // Total wire bits : 1 (delta-flag) + 32 (raw) = 33 bits when changed,
    // or 1 bit when unchanged.
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_XorDelta32(
        void*  msg,
        int    arg_0,
        void*  oldStruct,
        void*  newStruct,
        void*  entry,
        int    debug_print,
        int    zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        // bits dispatch -> bits == -88 -> loc_676ABC : 32-bit XOR delta.
        // No further control bits, no zero-flag.
        uint32_t raw     = (uint32_t)call_MSG_ReadDword(msg);
        uint32_t new_val = old_val ^ raw;

        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = new_val;
    }


    // =========================================================================
    // bits == -93 (0xFFFFFFA3) -- 16-bit signed read direct
    //
    // Vanilla flow (loc_676D60-loc_676D73, lines 946144-946158) :
    //   raw = MSG_ReadShortSignExt(msg)     ; sub_675500, 16-bit sign-extended
    //   new = raw                            ; direct store, no XOR with old
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits93(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        int32_t raw = call_MSG_ReadShortSignExt(msg);
        *(int32_t*)((uint8_t*)newStruct + entry_offset) = raw;
    }


    // =========================================================================
    // bits == -94 (0xFFFFFFA2) -- byte read direct
    //
    // Vanilla flow (loc_676D4C-loc_676D5F, lines 946131-946141) :
    //   raw = MSG_ReadByte(msg)              ; sub_676690, raw byte
    //   new = raw                             ; direct store
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits94(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t raw = (uint32_t)call_MSG_ReadByte(msg);
        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = raw;
    }


    // =========================================================================
    // bits == -95 (0xFFFFFFA1) -- readbits(7) * 100 (centi-second timing field)
    //
    // Vanilla flow (loc_676D74-loc_676D8F, lines 946161-946174) :
    //   raw = MSG_ReadBits(msg, 7) * 100
    //   new = raw                             ; direct store, no XOR
    //
    // Likely encodes a timer or similar small-range field with 100-unit step.
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits95(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t raw = call_MSG_ReadBits(msg, 7) * 100u;
        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = raw;
    }


    // =========================================================================
    // Helper : loc_676B0A body (shared between bits == -89 and bits == 0 with
    // bit_a == 1).
    //
    // Vanilla flow (lines 945863-945895) :
    //   bit_b = MSG_ReadBit(msg)
    //   if (bit_b == 1) :
    //     -> loc_676ABC : 32-bit XOR delta
    //   else :
    //     bits5    = MSG_ReadBits(msg, 5)
    //     byte_val = MSG_ReadByte(msg)             ; sub_676690
    //     old_int  = (int32_t)*(float*)&old_val    ; cvttss2si reinterpret
    //     combined = (byte_val << 5) | bits5
    //     xored    = combined ^ (old_int + 0x1000)
    //     new_int  = xored - 0x1000
    //     new_float= (float)new_int                ; cvtsi2ss
    //     store new_float
    // =========================================================================
    static void Loc_676B0A_Body(void* msg, uint32_t old_val, void* dest)
    {
        int control = call_MSG_ReadBit(msg);
        if (control != 0) {
            uint32_t raw     = (uint32_t)call_MSG_ReadDword(msg);
            uint32_t new_val = old_val ^ raw;
            *(uint32_t*)dest = new_val;
        } else {
            uint32_t bits5    = call_MSG_ReadBits(msg, 5);
            int32_t  byte_val = call_MSG_ReadByte(msg);
            int32_t  old_int  = (int32_t)*(float*)&old_val;     // cvttss2si reinterpret
            int32_t  combined = (byte_val << 5) | (int32_t)bits5;
            int32_t  xored    = combined ^ (old_int + 0x1000);
            int32_t  new_int  = xored - 0x1000;
            *(float*)dest     = (float)new_int;
        }
    }


    // =========================================================================
    // bits == -89 (0xFFFFFFA7) -- pseudo-int delta (loc_676B03 -> loc_676B0A)
    //
    // Vanilla flow : delta-flag, then loc_676B0A body.
    // Total wire bits (when changed) : 1 + 1 + 32 = 34 bits (XOR path)
    //                                  1 + 1 + 5 + 8 = 15 bits (5-bit signed path)
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits89(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        Loc_676B0A_Body(msg, old_val, (uint8_t*)newStruct + entry_offset);
    }


    // =========================================================================
    // bits == 0 -- bool / mixed (loc_676A96 fall-through after `test ebx, ebx`)
    //
    // Vanilla flow :
    //   delta-flag.
    //   bit_a = MSG_ReadBit(msg)
    //   if (bit_a == 0) :
    //     bit_b = MSG_ReadBit(msg)
    //     store (bit_b << 31)                  ; either 0 or 0x80000000
    //   else :                                  ; bit_a == 1
    //     loc_676B0A body                       ; same as bits == -89 non-delta path
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_Bits0(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        int bit_a = call_MSG_ReadBit(msg);
        if (bit_a == 0) {
            // bool path : read bit_b, store (bit_b << 31)
            int bit_b = call_MSG_ReadBit(msg);
            *(uint32_t*)dest = (uint32_t)bit_b << 31;
        } else {
            // bit_a == 1 -> loc_676B0A body (shared with bits == -89)
            Loc_676B0A_Body(msg, old_val, dest);
        }
    }


    // =========================================================================
    // bits == -97 (0xFFFFFF9F) -- 8-bit subtract-from-arg_0 OR 32-bit raw
    //
    // Vanilla flow (loc_676CB7-loc_676CF0) :
    //   delta-flag.
    //   bit = MSG_ReadBit(msg)
    //   if (bit == 0) :
    //     raw = MSG_ReadBits(msg, 8)
    //     store arg_0 - raw
    //   else :
    //     raw = MSG_ReadDword(msg)
    //     store raw                             ; NOTE: store directly, no XOR
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits97(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        int bit = call_MSG_ReadBit(msg);
        if (bit == 0) {
            uint32_t raw     = call_MSG_ReadBits(msg, 8);
            int32_t  result  = arg_0 - (int32_t)raw;
            *(int32_t*)dest  = result;
        } else {
            uint32_t raw     = (uint32_t)call_MSG_ReadDword(msg);
            *(uint32_t*)dest = raw;
        }
    }


    // =========================================================================
    // bits == -98 (0xFFFFFF9E) -- delta-special via sub_6766D0
    //
    // Vanilla flow (loc_676C9D-loc_676CB6) :
    //   delta-flag.
    //   new = MSG_ReadDeltaSpecial(msg, old)     ; sub_6766D0(arg_0=old, edx=msg)
    //   store new                                 ; direct, no XOR
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits98(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        int new_val = call_MSG_ReadDeltaSpecial(msg, old_val);
        *(int*)((uint8_t*)newStruct + entry_offset) = new_val;
    }


    // =========================================================================
    // Helper : loc_676DDE body (16-bit signed * scale, shared between -100/-87)
    // =========================================================================
    static void Loc_676DDE_Body(void* msg, void* dest)
    {
        int32_t int16_value = call_MSG_ReadShortSignExt(msg);
        float   scale       = *(float*)k_dword_8AF4FC;
        float   new_val     = (float)int16_value * scale;
        *(float*)dest = new_val;
    }


    // =========================================================================
    // bits == -87 (0xFFFFFFA9) -- 16-bit signed * scale (radians/degrees)
    //
    // Vanilla flow (loc_676DD9 -> loc_676DDE) :
    //   delta-flag.
    //   loc_676DDE body : (float)readbits16_signed * dword_8AF4FC -> store as float
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits87(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        Loc_676DDE_Body(msg, (uint8_t*)newStruct + entry_offset);
    }


    // =========================================================================
    // bits == -100 (0xFFFFFF9C) -- 1-bit gate + (0.0f shortcut OR loc_676DDE)
    //
    // Vanilla flow (loc_676DBC) :
    //   delta-flag.
    //   bit = MSG_ReadBit(msg)
    //   if (bit == 0) : store 0.0f
    //   else          : loc_676DDE body
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits100(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        int bit = call_MSG_ReadBit(msg);
        if (bit == 0) {
            *(float*)dest = 0.0f;
        } else {
            Loc_676DDE_Body(msg, dest);
        }
    }


    // =========================================================================
    // bits == -86 (0xFFFFFFAA) -- readbits(6) * scale + offset (small-range float)
    //
    // Vanilla flow (loc_676DFB-loc_676E29) :
    //   delta-flag.
    //   raw = MSG_ReadBits(msg, 6)
    //   val = (float)raw * dword_84D664 + dword_826A4C
    //   store val as float
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits86(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t raw = call_MSG_ReadBits(msg, 6);
        float    val = (float)(int32_t)raw;
        val *= *(float*)k_dword_84D664;
        val += *(float*)k_dword_826A4C;
        *(float*)((uint8_t*)newStruct + entry_offset) = val;
    }


    // =========================================================================
    // bits == -99 (0xFFFFFF9D) -- 4-bit signed delta float OR 32-bit XOR
    //
    // Vanilla flow (loc_676B6C tail -> loc_676BEF / 4-bit-signed-path) :
    //   delta-flag.
    //   bit_a = MSG_ReadBit(msg)
    //   if (bit_a == 0) : store 0
    //   else :
    //     bit_b = MSG_ReadBit(msg)
    //     if (bit_b == 1) : 32-bit XOR delta with old (loc_676BEF)
    //     else            : 4-bit signed delta + sub_676690 + math (shl 4, 0x800)
    //                       converted to float via cvtsi2ss
    //
    // Similar structure to bits == -89 but with shl 4 and 0x800 magic constant.
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits99(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        int bit_a = call_MSG_ReadBit(msg);
        if (bit_a == 0) {
            *(uint32_t*)dest = 0;
            return;
        }

        int bit_b = call_MSG_ReadBit(msg);
        if (bit_b != 0) {
            // 32-bit XOR delta (loc_676BEF)
            uint32_t raw     = (uint32_t)call_MSG_ReadDword(msg);
            *(uint32_t*)dest = old_val ^ raw;
        } else {
            // 4-bit signed delta with sub_676690 + math
            uint32_t bits4    = call_MSG_ReadBits(msg, 4);
            int32_t  byte_val = call_MSG_ReadByte(msg);
            int32_t  old_int  = (int32_t)*(float*)&old_val;
            int32_t  combined = (byte_val << 4) | (int32_t)bits4;
            int32_t  xored    = combined ^ (old_int + 0x800);
            int32_t  new_int  = xored - 0x800;
            *(float*)dest     = (float)new_int;
        }
    }


    // =========================================================================
    // bits == -85 (0xFFFFFFAB) -- byte-component delta (RGBA-like)
    //
    // Vanilla flow (loc_676E2A-loc_676E9A) :
    //   delta-flag.
    //   bit_a = MSG_ReadBit(msg)
    //   if (bit_a == 1) :
    //     copy old (4 bytes) to new
    //     new[3] = (old[3] != 0) ? 0 : 0xFF              ; toggle alpha
    //   else :
    //     bit_b = MSG_ReadBit(msg)
    //     if (bit_b == 0) :
    //       new[0..2] = byte * 3 via sub_676690         ; RGB
    //     bits5    = MSG_ReadBits(msg, 5)
    //     shifted  = bits5 << 3                          ; 5-to-8-bit expansion
    //     new[3]   = shifted | (shifted >> 5)            ; replicate high bits
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits85(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest_b = (uint8_t*)newStruct + entry_offset;

        int bit_a = call_MSG_ReadBit(msg);
        if (bit_a != 0) {
            // Copy old (4 bytes), then toggle byte 3
            *(uint32_t*)dest_b = old_val;
            uint8_t old_byte_3 = (uint8_t)((old_val >> 24) & 0xFF);
            dest_b[3] = (old_byte_3 != 0) ? 0u : 0xFFu;
            return;
        }
        // bit_a == 0
        int bit_b = call_MSG_ReadBit(msg);
        if (bit_b == 0) {
            dest_b[0] = (uint8_t)call_MSG_ReadByte(msg);
            dest_b[1] = (uint8_t)call_MSG_ReadByte(msg);
            dest_b[2] = (uint8_t)call_MSG_ReadByte(msg);
        }
        // common : read 5 bits, expand to 8 bits, store byte 3
        uint32_t bits5   = call_MSG_ReadBits(msg, 5);
        uint8_t  shifted = (uint8_t)((bits5 & 0x1F) << 3);
        uint8_t  high    = (uint8_t)(shifted >> 5);
        dest_b[3] = shifted | high;
    }


    // =========================================================================
    // bits == -79 (0xFFFFFFB1) -- 2-bit switch with delta accumulators
    //
    // Vanilla flow (loc_676E9B-loc_676F46) :
    //   delta-flag.
    //   sw = MSG_ReadBits(msg, 2)
    //   switch (sw) :
    //     0 : raw = MSG_ReadBits(msg, 7)  ; sign-extend if bit 6 set
    //     1 : raw = MSG_ReadBits(msg, 13) ; sign-extend if bit 12 set
    //     2 : raw = byte | (16-bit << 8)  ; 24-bit signed, via sub_676690+sub_675500
    //                                      ; sign-extend if bit 23 set
    //     3 : raw = MSG_ReadDword(msg)    ; 32-bit raw
    //   store old + raw_signed                ; accumulator delta
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits79(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t sw = call_MSG_ReadBits(msg, 2);
        int32_t  delta;
        switch (sw) {
            case 0: {
                uint32_t raw = call_MSG_ReadBits(msg, 7);
                delta = (raw & 0x40u) ? (int32_t)(raw | 0xFFFFFF80u) : (int32_t)raw;
                break;
            }
            case 1: {
                uint32_t raw = call_MSG_ReadBits(msg, 13);
                delta = (raw & 0x1000u) ? (int32_t)(raw | 0xFFFFE000u) : (int32_t)raw;
                break;
            }
            case 2: {
                uint32_t lo  = (uint32_t)call_MSG_ReadByte(msg);
                uint32_t hi  = (uint32_t)call_MSG_ReadShortSignExt(msg);
                uint32_t comb = lo | (hi << 8);
                delta = (comb & 0x800000u) ? (int32_t)(comb | 0xFF000000u) : (int32_t)comb;
                break;
            }
            default: { // sw == 3
                uint32_t raw = (uint32_t)call_MSG_ReadDword(msg);
                delta = (int32_t)raw;
                break;
            }
        }

        *(int32_t*)((uint8_t*)newStruct + entry_offset) = (int32_t)old_val + delta;
    }


    // =========================================================================
    // bits == -78 (0xFFFFFFB2) -- delta via sub_676640 with arg_0
    //
    // Vanilla flow (loc_676CF0-loc_676D07) :
    //   delta-flag.
    //   esi = arg_0
    //   eax = sub_676640(esi, msg)         ; 1-bit + variable readbits cascade
    //   store eax
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits78(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        int new_val = call_MSG_ReadDelta676640(msg, arg_0);
        *(int*)((uint8_t*)newStruct + entry_offset) = new_val;
    }


    // =========================================================================
    // bits == -90 (0xFFFFFFA6) -- float scale via sub_6767C0
    //
    // Vanilla flow (loc_676D90 fall-through after -90 cmp) :
    //   delta-flag.
    //   xmm0 = sub_6767C0(msg, old_float)
    //   store xmm0 as float
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits90(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t new_bits = call_MSG_ReadFloatScale(msg, old_val);
        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = new_bits;
    }


    // =========================================================================
    // bits == -91 (0xFFFFFFA5) / -92 (0xFFFFFFA4) -- float-vec1 via sub_676750
    //
    // Vanilla flow (loc_676D90 -> loc_677000) :
    //   delta-flag.
    //   xmm0 = sub_676750(msg, bits_value, old_float)
    //   store xmm0 as float
    //
    // sub_676750 internally tests arg_0 == -92 to select lookup-table index.
    // Both -91 and -92 share this single function ; we route through a helper
    // that passes the matching bits value.
    // =========================================================================
    static void Vec1FloatBranch(
        void* msg, void* oldStruct, void* newStruct, int entry_offset,
        uint8_t entry_type, int zero_old_byte, int bits_value)
    {
        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t new_bits = call_MSG_ReadFloatVec1(msg, bits_value, old_val);
        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = new_bits;
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits91(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;
        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);
        Vec1FloatBranch(msg, oldStruct, newStruct, entry_offset, entry_type,
                        zero_old_byte, /*bits_value=*/(int)0xFFFFFFA5);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits92(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;
        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);
        Vec1FloatBranch(msg, oldStruct, newStruct, entry_offset, entry_type,
                        zero_old_byte, /*bits_value=*/(int)0xFFFFFFA4);
    }


    // =========================================================================
    // bits == -77/-80/-81/-82/-83/-84 -- vec3-component family via sub_676820
    //
    // Each branch sets up specific (lo_float, range, flag, hi_float) params,
    // calls sub_676820, stores xmm0 as float.
    //
    // Param table (extracted from vanilla loc_676C13..loc_676C71) :
    //   bits | lo float          | range | flag | hi float
    //   -84  | 0.0               | 1000  | 1    | dword_826A4C
    //   -83  | 0.0               | 10    | 0    | dword_8E46E4
    //   -82  | 0.0               | 100   | 1    | dword_8E4638
    //   -81  | 0.0               | 100   | 0    | dword_826A4C
    //   -80  | 0.0               | 1000  | 0    | dword_826A4C
    //   -77  | dword_8AF214      | 100   | 0    | dword_826A4C
    // =========================================================================
    static void Vec3Branch(
        void* msg, void* oldStruct, void* newStruct, int entry_offset,
        uint8_t entry_type, int zero_old_byte,
        uintptr_t lo_va, int range, int flag, uintptr_t hi_va)
    {
        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint32_t lo_bits = (lo_va == 0) ? 0u : *(uint32_t*)lo_va;  // 0.0f bits = 0
        uint32_t hi_bits = *(uint32_t*)hi_va;
        uint32_t new_bits = call_MSG_ReadVec3Comp(
            msg, old_val, lo_bits, range, flag, hi_bits);
        *(uint32_t*)((uint8_t*)newStruct + entry_offset) = new_bits;
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits84(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/0, /*range*/1000, /*flag*/1, /*hi*/k_dword_826A4C);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits83(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/0, /*range*/10, /*flag*/0, /*hi*/k_dword_8E46E4);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits82(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/0, /*range*/100, /*flag*/1, /*hi*/k_dword_8E4638);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits81(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/0, /*range*/100, /*flag*/0, /*hi*/k_dword_826A4C);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits80(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/0, /*range*/1000, /*flag*/0, /*hi*/k_dword_826A4C);
    }

    extern "C" void __cdecl MSG_ReadDeltaField_NegBits77(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0; (void)debug_print;
        auto entry_b = (uint8_t*)entry;
        Vec3Branch(msg, oldStruct, newStruct, *(int*)(entry_b + 4),
                   *(entry_b + 0xC), zero_old_byte,
                   /*lo*/k_dword_8AF214, /*range*/100, /*flag*/0,
                   /*hi*/k_dword_826A4C);
    }


    // =========================================================================
    // bits == -96 (0xFFFFFFA0) -- 3-state special (1-bit + 1-bit + readbits(10))
    //
    // Vanilla flow (loc_676D08-loc_676D4B) :
    //   delta-flag.
    //   bit_a = MSG_ReadBit(msg)
    //   if (bit_a == 1) : store 0x3FE             ; sentinel "max"
    //   else :
    //     bit_b = MSG_ReadBit(msg)
    //     if (bit_b == 1) : store 0               ; sentinel "zero"
    //     else            : store MSG_ReadBits(msg, 10)
    // =========================================================================
    extern "C" void __cdecl MSG_ReadDeltaField_NegBits96(
        void* msg, int arg_0, void* oldStruct, void* newStruct, void* entry,
        int debug_print, int zero_old_byte)
    {
        (void)arg_0;
        (void)debug_print;

        auto    entry_b      = (uint8_t*)entry;
        int     entry_offset = *(int*)(entry_b + 4);
        uint8_t entry_type   = *(entry_b + 0xC);

        uint32_t old_val;
        if (!ReadDeltaFlagOrCopyOld(msg, entry_type, oldStruct, newStruct,
                                    entry_offset, zero_old_byte, &old_val)) {
            return;
        }

        uint8_t* dest = (uint8_t*)newStruct + entry_offset;

        int bit_a = call_MSG_ReadBit(msg);
        if (bit_a == 1) {
            *(uint32_t*)dest = 0x3FE;
        } else {
            int bit_b = call_MSG_ReadBit(msg);
            if (bit_b == 1) {
                *(uint32_t*)dest = 0;
            } else {
                uint32_t raw     = call_MSG_ReadBits(msg, 10);
                *(uint32_t*)dest = raw;
            }
        }
    }
}


// =============================================================================
// Naked detour wrapper for sub_676A50 (entry @ VA 0x676A50).
//
// At entry the stack is :
//   [esp+0x00] = ret_addr
//   [esp+0x04] = arg_0
//   [esp+0x08] = arg_4 (oldStruct)
//   [esp+0x0C] = arg_8 (newStruct)
//   [esp+0x10] = arg_C (entry_ptr)
//   [esp+0x14] = arg_10 (debug)
//   [esp+0x18] = arg_14 (zero_old, low byte read)
// And edi = msg (per vanilla __usercall convention).
//
// Dispatch (P3 Pass 2 + Pass 3 incremental) :
//   - entry_ptr in [0x836B30, 0x837BD0) (= playerstate netfield range) AND
//     entry.bits > 0           -> C++ MSG_ReadDeltaField_PositiveBits
//   - entry.bits == 0          -> C++ MSG_ReadDeltaField_Bits0 (bool/mixed)
//   - entry.bits == -88 (-0xA8)-> C++ MSG_ReadDeltaField_XorDelta32
//   - entry.bits == -89 (-0xA7)-> C++ MSG_ReadDeltaField_NegBits89 (pseudo-int delta)
//   - entry.bits == -86 (-0xAA)-> C++ MSG_ReadDeltaField_NegBits86 (6-bit float)
//   - entry.bits == -87 (-0xA9)-> C++ MSG_ReadDeltaField_NegBits87 (16-bit * scale)
//   - entry.bits == -93 (-0xA3)-> C++ MSG_ReadDeltaField_NegBits93 (16-bit signed)
//   - entry.bits == -94 (-0xA2)-> C++ MSG_ReadDeltaField_NegBits94 (byte read)
//   - entry.bits == -95 (-0xA1)-> C++ MSG_ReadDeltaField_NegBits95 (7-bit * 100)
//   - entry.bits == -96 (-0xA0)-> C++ MSG_ReadDeltaField_NegBits96 (3-state)
//   - entry.bits == -97 (-0x9F)-> C++ MSG_ReadDeltaField_NegBits97 (8/32-bit)
//   - entry.bits == -98 (-0x9E)-> C++ MSG_ReadDeltaField_NegBits98 (delta-special)
//   - entry.bits == -100(-0x9C)-> C++ MSG_ReadDeltaField_NegBits100 (1-bit + 16-bit)
//   - entry.bits == -99 (-0x9D)-> C++ MSG_ReadDeltaField_NegBits99  (4-bit signed float)
//   - entry.bits == -85 (-0xAB)-> C++ MSG_ReadDeltaField_NegBits85  (RGBA byte delta)
//   - entry.bits == -79 (-0xB1)-> C++ MSG_ReadDeltaField_NegBits79  (2-bit switch delta)
//   - entry.bits == -78 (-0xB2)-> C++ MSG_ReadDeltaField_NegBits78  (sub_676640 path)
//   - entry.bits == -90 (-0xA6)-> C++ MSG_ReadDeltaField_NegBits90  (sub_6767C0 float scale)
//   - entry.bits == -91 (-0xA5)-> C++ MSG_ReadDeltaField_NegBits91  (sub_676750 vec1)
//   - entry.bits == -92 (-0xA4)-> C++ MSG_ReadDeltaField_NegBits92  (sub_676750 vec1 var)
//   - entry.bits == -84 (-0xAC)-> C++ MSG_ReadDeltaField_NegBits84  (vec3 component, range 1000)
//   - entry.bits == -83 (-0xAD)-> C++ MSG_ReadDeltaField_NegBits83  (vec3 component, range 10)
//   - entry.bits == -82 (-0xAE)-> C++ MSG_ReadDeltaField_NegBits82  (vec3 component, range 100)
//   - entry.bits == -81 (-0xAF)-> C++ MSG_ReadDeltaField_NegBits81  (vec3 component, range 100 v2)
//   - entry.bits == -80 (-0xB0)-> C++ MSG_ReadDeltaField_NegBits80  (vec3 component, range 1000 v2)
//   - entry.bits == -77 (-0xB3)-> C++ MSG_ReadDeltaField_NegBits77  (vec3 component, dword_8AF214 lo)
//   - else                     -> jmp trampoline (vanilla unchanged ; only entityState entries hit this now)
//
// Pass 4 (2026-05-10) completes the dispatch table for ALL playerstate
// netfield entries (bits values ∈ {0, > 0, -77..-79, -82..-89, -91..-100}).
// All branches dispatch through C++. Vanilla helpers (sub_676820, sub_676640,
// sub_676750, sub_6767C0) remain vanilla — shimmed via naked __usercall
// wrappers ; their FPU/float-math is reused as-is.
// =============================================================================

extern "C" volatile uintptr_t g_msg_read_field_trampoline;  // forward (defined above at global scope)

namespace T4M
{
    extern "C" __declspec(naked) void MSG_ReadDeltaField_Detour()
    {
        __asm
        {
            mov     eax, [esp+0x10]                            // entry_ptr
            cmp     eax, 0x00836B30                            // entry < off_836B30 ?
            jb      fall_through
            cmp     eax, 0x00837BD0                            // entry >= off_837BD0 ?
            jae     fall_through

            mov     ecx, [eax+8]                                // entry.bits

            // bits > 0 -> positive-bits path
            test    ecx, ecx
            jg      cpp_positive
            // bits == 0 -> bool/mixed path
            jz      cpp_bits0

            // Negative-bits dispatch (cascaded cmp)
            cmp     ecx, 0xFFFFFFA8                             // -88
            je      cpp_xor32
            cmp     ecx, 0xFFFFFFA7                             // -89
            je      cpp_neg89
            cmp     ecx, 0xFFFFFFAA                             // -86
            je      cpp_neg86
            cmp     ecx, 0xFFFFFFA9                             // -87
            je      cpp_neg87
            cmp     ecx, 0xFFFFFFA3                             // -93
            je      cpp_neg93
            cmp     ecx, 0xFFFFFFA2                             // -94
            je      cpp_neg94
            cmp     ecx, 0xFFFFFFA1                             // -95
            je      cpp_neg95
            cmp     ecx, 0xFFFFFFA0                             // -96
            je      cpp_neg96
            cmp     ecx, 0xFFFFFF9F                             // -97
            je      cpp_neg97
            cmp     ecx, 0xFFFFFF9E                             // -98
            je      cpp_neg98
            cmp     ecx, 0xFFFFFF9C                             // -100
            je      cpp_neg100
            cmp     ecx, 0xFFFFFF9D                             // -99
            je      cpp_neg99
            cmp     ecx, 0xFFFFFFAB                             // -85
            je      cpp_neg85
            cmp     ecx, 0xFFFFFFB1                             // -79
            je      cpp_neg79
            cmp     ecx, 0xFFFFFFB2                             // -78
            je      cpp_neg78
            cmp     ecx, 0xFFFFFFA6                             // -90
            je      cpp_neg90
            cmp     ecx, 0xFFFFFFA5                             // -91
            je      cpp_neg91
            cmp     ecx, 0xFFFFFFA4                             // -92
            je      cpp_neg92
            cmp     ecx, 0xFFFFFFAC                             // -84
            je      cpp_neg84
            cmp     ecx, 0xFFFFFFAD                             // -83
            je      cpp_neg83
            cmp     ecx, 0xFFFFFFAE                             // -82
            je      cpp_neg82
            cmp     ecx, 0xFFFFFFAF                             // -81
            je      cpp_neg81
            cmp     ecx, 0xFFFFFFB0                             // -80
            je      cpp_neg80
            cmp     ecx, 0xFFFFFFB3                             // -77
            je      cpp_neg77

            // Fall through to trampoline for any unrecognized bits value
            // (shouldn't happen for known playerstate entries, but safe).
            jmp     fall_through

        cpp_positive:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_PositiveBits
            add     esp, 28
            retn

        cpp_bits0:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_Bits0
            add     esp, 28
            retn

        cpp_xor32:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_XorDelta32
            add     esp, 28
            retn

        cpp_neg89:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits89
            add     esp, 28
            retn

        cpp_neg86:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits86
            add     esp, 28
            retn

        cpp_neg87:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits87
            add     esp, 28
            retn

        cpp_neg93:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits93
            add     esp, 28
            retn

        cpp_neg94:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits94
            add     esp, 28
            retn

        cpp_neg95:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits95
            add     esp, 28
            retn

        cpp_neg96:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits96
            add     esp, 28
            retn

        cpp_neg97:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits97
            add     esp, 28
            retn

        cpp_neg98:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits98
            add     esp, 28
            retn

        cpp_neg100:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits100
            add     esp, 28
            retn

        cpp_neg99:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits99
            add     esp, 28
            retn

        cpp_neg85:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits85
            add     esp, 28
            retn

        cpp_neg79:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits79
            add     esp, 28
            retn

        cpp_neg78:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits78
            add     esp, 28
            retn

        cpp_neg90:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits90
            add     esp, 28
            retn

        cpp_neg91:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits91
            add     esp, 28
            retn

        cpp_neg92:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits92
            add     esp, 28
            retn

        cpp_neg84:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits84
            add     esp, 28
            retn

        cpp_neg83:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits83
            add     esp, 28
            retn

        cpp_neg82:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits82
            add     esp, 28
            retn

        cpp_neg81:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits81
            add     esp, 28
            retn

        cpp_neg80:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits80
            add     esp, 28
            retn

        cpp_neg77:
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    [esp+0x18]
            push    edi
            call    T4_Reconstructed::MSG_ReadDeltaField_NegBits77
            add     esp, 28
            retn

        fall_through:
            mov     ecx, dword ptr [g_msg_read_field_trampoline]
            jmp     ecx
        }
    }
}


// =============================================================================
// Install
// =============================================================================

void PatchT4MAM_NetfieldCodec()
{
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // // (1) Byte patch : netfield table entry 54 (ps.weapon) bits 7 -> 16.
    // //
    // //     Re-enabled 2026-05-10 after fixing the C++ decoder to match
    // //     vanilla's wire format (delta-flag + zero-flag, see
    // //     MSG_ReadDeltaField_PsWeapon). Previous attempt failed because the
    // //     decoder skipped the zero-flag bit, desyncing the snapshot stream.
    // //
    // //     Symmetric encoder/decoder : the snapshot encoder (sub_67B0C0 @
    // //     0x67B0C0) reads `bits` from the same off_836B30 table dynamically,
    // //     so this 1-byte change propagates to both sides without further
    // //     work. Wire transports 16-bit ps.weapon (idx 0..0xFFFF).
    // Memory::VP::Patch<uint8_t>(k_NetfieldTable_PsWeaponBits, 0x10);
    //
    // // (2) Detour sub_676A50. The naked wrapper either runs our ps.weapon
    // //     reconstruction (post-patch entry_bits=16) or jumps to the trampoline
    // //     (vanilla function for all other entries).
    // static auto hook = safetyhook::create_inline(
    //     (void*)VA::SubMSG_676A50,
    //     (void*)&T4M::MSG_ReadDeltaField_Detour);
    //
    // g_msg_read_field_trampoline = (uintptr_t)hook.original<void*>();
    // (void)hook;
}


// =====================================================================
// SECTION 7 / 7 - PatchT4MAM_PickupDecoder.cpp
// =====================================================================

// ==========================================================
// T4M project — PatchT4MAM_PickupDecoder.cpp
//
// Pickup chain reconstruction. Fixes the 7-bit `item_id` decoder in 3
// functions that read [ent+0xA8] (or its mirrors) and mask with the
// vanilla `and reg, 8000007Fh` pattern. With T4M's WEAPON pool raised to
// NEW_MAX_WEAPONS = 512, valid weap_idx is 0..511 (9 bits) ; vanilla's
// 7-bit decode wraps any idx >= 128 to wrong weapons during pickup.
//
// Reconstructions (@faithful, except the marked 9-bit decode) :
//   sub_4FD210  → T4_Reconstructed::G_TouchItem        (pickup entry)
//   sub_40FCB0  → T4_Reconstructed::BG_CanItemBeGrabbed (pickup gate)
//   sub_4F3D60  → T4_Reconstructed::Cmd_GiveF          (cheat "give X")
//
// All other callees (sub_4FD080 / sub_4FD150 / sub_54F350 / sub_54F3A0 /
// sub_4FC570 / sub_40FAA0 / sub_54EAB0 / sub_4FD500 / sub_4FEAB0 /
// sub_4FBE0 / sub_546EF0 / sub_54EDC0 / sub_5528D0 / sub_41D6A0 /
// sub_5F69E0 / sub_5F6D80 / sub_5F6CA0 / sub_7AA9C0 / sub_7AA796 /
// sub_5A9F30 / sub_67D3D0 / sub_59A440 / sub_59AC50 / sub_4F39A0 /
// sub_4F3A30 / sub_515BB0) are called through naked __cdecl thunks
// that load the vanilla __usercall ABI from cdecl args, mirroring the
// pattern in PatchT4MAM_WireCodec.cpp.
//
// sub_4FD080's decoder is already patched 9-bit in PatchT4_Temp.cpp.
// sub_5528D0 / sub_41D6A0 are already detoured by PatchT4MAM_PsBitmapExt ;
// our naked thunks call the vanilla VA so the existing detour fires.
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


// Runtime-resolved relocated bg_weaponDefs base (set by PatchT4MemoryLimits's
// SetupBgWeaponDefsTable). Hardcoding 0x8F6770 reads the OLD (now empty) vanilla
// BSS region — the global must be read via this extern.
extern T4::WeaponDef** bg_weaponDefs;


namespace T4_Reconstructed
{
    extern "C"
    {
        // sub_4FD210 — pickup entry. __usercall: eax=item_ent, arg_0=player_ent,
        // arg_4=touch_data. No return value.
        void __cdecl G_TouchItem(void* item_ent, void* player_ent, void* touch_data);

        // sub_40FCB0 — pickup gate. __usercall: esi=item_ent, edi=ps_ptr,
        // arg_0=touch_data. Returns bool in al.
        unsigned char __cdecl BG_CanItemBeGrabbed(void* item_ent, void* ps_ptr, void* touch_data);

        // sub_4F3D60 — "give X" cheat handler. __cdecl(arg_0=player_ent).
        void __cdecl Cmd_GiveF(void* player_ent);
    }
}


namespace T4M
{
    extern "C"
    {
        // Naked wrappers : translate vanilla __usercall ABI → __cdecl reconstruction
        // for the 3 detoured entry points. (`__declspec(naked)` is on the
        // definition further down — MSVC rejects it on declarations.)
        void G_TouchItem_Wrapper();
        void BG_CanItemBeGrabbed_Wrapper();
        void Cmd_GiveF_Wrapper();
    }
}


void PatchT4MAM_PickupDecoder();


// =====================================================================
// Naked __cdecl thunks for the vanilla callees. Each one preserves
// callee-saved registers and translates cdecl stack args back to the
// vanilla __usercall ABI before calling the VA.
// =====================================================================

namespace
{
    // sub_40FBE0 __usercall(@<eax> int weap_idx, @<ecx> void* item_ent, arg_0=ps, arg_4=player_ent) -> bool al
    extern "C" __declspec(naked)
    unsigned char __cdecl Call_sub_40FBE0(int /*weap_idx*/, void* /*item_ent*/, void* /*ps*/, void* /*player_ent*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; weap_idx
            mov     ecx, [esp+14h]                  ; item_ent
            push    [esp+1Ch]                       ; arg_4 = player_ent
            push    [esp+1Ch]                       ; arg_0 = ps (now at +0x1C after first push)
            mov     edx, 0x0040FBE0
            call    edx
            add     esp, 8
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4FD150 __usercall(@<eax> int weap_idx, @<ecx> void* item_ent, arg_0=player_ent)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_4FD150(int /*weap_idx*/, void* /*item_ent*/, void* /*player_ent*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; weap_idx
            mov     ecx, [esp+14h]                  ; item_ent
            push    [esp+18h]                       ; arg_0 = player_ent
            mov     edx, 0x004FD150
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4FD080 __usercall(@<eax> item_ent, @<edi> player_ent, arg_0=int* out, arg_4=touch_data) -> int
    extern "C" __declspec(naked)
    int __cdecl Call_sub_4FD080(void* /*item_ent*/, void* /*player_ent*/, int* /*out*/, void* /*touch_data*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; item_ent
            mov     edi, [esp+14h]                  ; player_ent
            push    [esp+1Ch]                       ; arg_4 = touch_data
            push    [esp+1Ch]                       ; arg_0 = out  (now at +0x1C)
            mov     edx, 0x004FD080
            call    edx
            add     esp, 8
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_54F350 __usercall(@<eax> player_ent, @<ecx> int weap_idx, arg_0=int slot)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_54F350(void* /*player_ent*/, int /*weap_idx*/, int /*slot*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; player_ent
            mov     ecx, [esp+14h]                  ; weap_idx
            push    [esp+18h]                       ; arg_0 = slot
            mov     edx, 0x0054F350
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_54F3A0 __usercall(@<eax> player_ent, @<ecx> int weap_idx, arg_0=int slot)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_54F3A0(void* /*player_ent*/, int /*weap_idx*/, int /*slot*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; player_ent
            mov     ecx, [esp+14h]                  ; weap_idx
            push    [esp+18h]                       ; arg_0 = slot
            mov     edx, 0x0054F3A0
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_546EF0 __usercall(@<eax> int kind, @<cx> word val_lo, arg_0=item_ent)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_546EF0(int /*kind*/, unsigned short /*val_lo*/, void* /*item_ent*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; kind
            mov     cx,  word ptr [esp+14h]         ; val_lo (low word)
            push    [esp+18h]                       ; arg_0 = item_ent
            mov     edx, 0x00546EF0
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4F39A0 __usercall(@<eax> player_ent) -> int eax
    extern "C" __declspec(naked)
    int __cdecl Call_sub_4F39A0(void* /*player_ent*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]
            mov     edx, 0x004F39A0
            call    edx
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_5F69E0 __usercall(@<eax> int max_len, @<edx> char* b, arg_0=char* a) -> int eax
    extern "C" __declspec(naked)
    int __cdecl Call_sub_5F69E0(int /*max_len*/, const char* /*b*/, const char* /*a*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; max_len
            mov     edx, [esp+14h]                  ; b
            push    [esp+18h]                       ; arg_0 = a
            mov     ecx, 0x005F69E0
            call    ecx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4FC570 __usercall(@<eax> player_ent, arg_0=int slot, arg_4=int unk0, arg_8=int amount, arg_C=int kind)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_4FC570(void* /*player_ent*/, int /*slot*/, int /*unk0*/, int /*amount*/, int /*kind*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; player_ent
            push    [esp+20h]                       ; arg_C = kind
            push    [esp+20h]                       ; arg_8 = amount (shifted +4 after first push)
            push    [esp+20h]                       ; arg_4 = unk0
            push    [esp+20h]                       ; arg_0 = slot
            mov     edx, 0x004FC570
            call    edx
            add     esp, 10h
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_5528D0 (G_GiveWeapon, detoured by PatchT4MAM_PsBitmapExt).
    // __usercall(@<eax> int weap_idx, @<ecx> ps, arg_0=int alt_idx).
    extern "C" __declspec(naked)
    void __cdecl Call_sub_5528D0(int /*weap_idx*/, void* /*ps*/, int /*alt_idx*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; weap_idx
            mov     ecx, [esp+14h]                  ; ps
            push    [esp+18h]                       ; arg_0 = alt_idx
            mov     edx, 0x005528D0
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_41D6A0 (G_TakeWeapon, detoured by PatchT4MAM_PsBitmapExt).
    // __usercall(@<esi> ps, arg_0=int weap_idx).
    extern "C" __declspec(naked)
    void __cdecl Call_sub_41D6A0(void* /*ps*/, int /*weap_idx*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     esi, [esp+10h]                  ; ps
            push    [esp+14h]                       ; arg_0 = weap_idx
            mov     edx, 0x0041D6A0
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_40FAA0 __usercall(@<eax> char* name, @<ecx> int flag) -> void* eax
    extern "C" __declspec(naked)
    void* __cdecl Call_sub_40FAA0(const char* /*name*/, int /*flag*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]                  ; name
            mov     ecx, [esp+14h]                  ; flag
            mov     edx, 0x0040FAA0
            call    edx
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4FD500 __usercall(@<eax> int eax_arg, arg_0=void* dst)
    extern "C" __declspec(naked)
    void __cdecl Call_sub_4FD500(int /*eax_arg*/, void* /*dst*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]
            push    [esp+14h]
            mov     edx, 0x004FD500
            call    edx
            add     esp, 4
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // sub_4FEAB0 __usercall(@<eax> int eax_arg, @<esi> item_ent)
    //
    // sub_4FEAB0 uses ESI = item_ent as ambient pointer — it WRITES
    // [esi+0x260], [esi+0xA8], [esi+4] (classname=3), [esi+0x12C..0x140]
    // (orientation), [esi+0x144], [esi+0xAC], [esi+0x16C..]/[esi+0x3C..]
    // (visual fields). Without esi = item_ent these writes corrupt random
    // memory and the item_ent stays uninitialized (= [+0xA8] reads 0 →
    // BG_CanItemBeGrabbed fatal "index out of range" error in Cmd_GiveF).
    extern "C" __declspec(naked)
    void __cdecl Call_sub_4FEAB0(int /*eax_arg*/, void* /*item_ent*/)
    {
        __asm {
            push    edi
            push    esi
            push    ebx
            mov     eax, [esp+10h]
            mov     esi, [esp+14h]
            mov     edx, 0x004FEAB0
            call    edx
            pop     ebx
            pop     esi
            pop     edi
            retn
        }
    }

    // ---- Plain cdecl callees — no register translation needed ----

    using sub_54EDC0_t = void(__cdecl*)(void*);
    inline void Call_sub_54EDC0(void* item_ent) { ((sub_54EDC0_t)0x0054EDC0)(item_ent); }

    using sub_7AA9C0_t = void(__cdecl*)(char*, const char*, int);
    inline void Call_sub_7AA9C0(char* d, const char* s, int n) { ((sub_7AA9C0_t)0x007AA9C0)(d, s, n); }

    using sub_5F6CA0_t = void(__cdecl*)(char*);
    inline void Call_sub_5F6CA0(char* s) { ((sub_5F6CA0_t)0x005F6CA0)(s); }

    using sub_5A9F30_t = const char*(__cdecl*)(int, const char*, const char*);
    inline const char* Call_sub_5A9F30(int a, const char* b, const char* c) { return ((sub_5A9F30_t)0x005A9F30)(a, b, c); }

    using sub_67D3D0_t = void(__cdecl*)(const char*, ...);
    inline void Call_sub_67D3D0(const char* fmt, const char* arg) { ((sub_67D3D0_t)0x0067D3D0)(fmt, arg); }

    using sub_59AC50_t = void(__cdecl*)(int, const char*);
    inline void Call_sub_59AC50(int level, const char* msg) { ((sub_59AC50_t)0x0059AC50)(level, msg); }

    using sub_5F6D80_t = const char*(__cdecl*)(const char*, int, int);
    inline const char* Call_sub_5F6D80(const char* fmt, int a, int b) { return ((sub_5F6D80_t)0x005F6D80)(fmt, a, b); }

    using sub_4F3A30_t = char*(__cdecl*)(int);
    inline char* Call_sub_4F3A30(int n) { return ((sub_4F3A30_t)0x004F3A30)(n); }

    using sub_7AA796_t = int(__cdecl*)(const char*);
    inline int Call_sub_7AA796(const char* s) { return ((sub_7AA796_t)0x007AA796)(s); }

    using sub_54EAB0_t = void*(__cdecl*)();
    inline void* Call_sub_54EAB0() { return ((sub_54EAB0_t)0x0054EAB0)(); }

    // ---- Global VAs ----
    constexpr uintptr_t kGlobal_18F6DBC = 0x018F6DBC;
    constexpr uintptr_t kGlobal_18F6DB8 = 0x018F6DB8;
    constexpr uintptr_t kGlobal_185581C = 0x0185581C;
    constexpr uintptr_t kGlobal_46DE3BC = 0x046DE3BC;
    constexpr uintptr_t kGlobal_1F33BBE = 0x01F33BBE;

    // String pointers in vanilla .rdata.
    constexpr uintptr_t kStr_unk_844FC8 = 0x00844FC8;
    constexpr uintptr_t kStr_off_818594 = 0x00818594;
}


// =====================================================================
// Helper : resolve the weap_idx for an item entity.
//
// Vanilla decoders read `[ent + packed_offset]` (= 0xA8, 0xE0-mirror at
// 0x260, or 0x178) and apply `and reg, 8000007Fh` to extract a 7-bit
// weap_idx. Vanilla encoder sub_509890 packs as `(model_idx << 7) + weap_idx`.
//
// Problem with naive 9-bit re-decode : if model_idx != 0 in the encoder,
// our 9-bit mask reads `model_idx << 7 + weap_idx` as a single number,
// giving wrong weap_idx.
//
// Solution : `[ent + 0xE0]` stores the raw dword weap_idx (the encoder reads
// it as input ; sub_50AE60 also reads it directly to look up bg_weaponDefs).
// This bypasses the packed-field ambiguity entirely.
//
// Strategy : prefer [+0xE0] if valid (in range + bg_weaponDefs slot non-NULL),
// else fall back to 9-bit decode of the packed field. The fallback handles
// entity types where [+0xE0] isn't set (or is 0) but the packed field is.
// =====================================================================
static inline int ResolveWeapIdx(void* ent, int packed_offset)
{
    // Primary : the raw dword weap_idx at [+0xE0].
    int32_t e0 = *(int32_t*)((unsigned char*)ent + 0xE0);
    if (e0 > 0 && e0 < 512 && bg_weaponDefs != nullptr && bg_weaponDefs[e0] != nullptr) {
        return e0;
    }

    // Fallback : 9-bit decode of the packed field.
    int32_t raw    = *(int32_t*)((unsigned char*)ent + packed_offset);
    int32_t masked = raw & 0x800001FF;
    if (masked < 0) {
        masked -= 1;
        masked |= 0xFFFFFE00;
        masked += 1;
    }
    return masked;
}


// =====================================================================
// sub_40FCB0 → T4_Reconstructed::BG_CanItemBeGrabbed
// =====================================================================
namespace T4_Reconstructed
{
    extern "C" unsigned char __cdecl BG_CanItemBeGrabbed(void* item_ent, void* ps_ptr, void* touch_data)
    {
        auto p_item = (unsigned char*)item_ent;
        auto p_ps   = (unsigned char*)ps_ptr;

        // Vanilla : `test byte ptr [edi+10h], 80h ; jz loc_40FCBF`.
        if (p_ps[0x10] & 0x80) {
            return 0;
        }

        // Range check : warn if item_id raw not in [1, 0x800).
        int32_t item_id_raw = *(int32_t*)(p_item + 0xA8);
        if (item_id_raw < 1 || item_id_raw >= 0x800) {
            int classname = *(int32_t*)(p_item + 4);
            const char* msg = Call_sub_5F6D80((const char*)kStr_unk_844FC8, item_id_raw, classname);
            Call_sub_59AC50(1, msg);
        }

        // @modified — 9-bit decode replaces vanilla 7-bit.
        int weap_idx = ResolveWeapIdx(item_ent, 0xA8);

        // @new — NULL guard. With 9-bit decode, weap_idx can land on an
        // unallocated pool slot (e.g. when vanilla content stored an item_id
        // = (model<<7 | weap) that we now interpret as a bare 9-bit weap_idx).
        // Vanilla 7-bit decode kept idx in [0, 127] where all slots were filled
        // so vanilla never had to NULL-check. sub_40FBE0 itself derefs
        // bg_weaponDefs[idx] internally and would AV — guard here.
        WeaponDef* wd = (weap_idx >= 0 && weap_idx < 512) ? bg_weaponDefs[weap_idx] : nullptr;
        if (wd == nullptr) return 0;

        // Primary check.
        if (Call_sub_40FBE0(weap_idx, item_ent, ps_ptr, touch_data)) {
            return 1;
        }

        int32_t alt_data = *(int32_t*)((unsigned char*)wd + 0x63C);
        if (alt_data == 0) {
            return 0;
        }

        // Alt check : pass `alt_data` (not weap_idx) as the eax arg.
        if (!Call_sub_40FBE0(alt_data, item_ent, ps_ptr, touch_data)) {
            return 0;
        }
        return 1;
    }
}


// =====================================================================
// sub_4FD210 → T4_Reconstructed::G_TouchItem
// =====================================================================
namespace T4_Reconstructed
{
    extern "C" void __cdecl G_TouchItem(void* item_ent, void* player_ent, void* touch_data)
    {
        auto p_item   = (unsigned char*)item_ent;
        auto p_player = (unsigned char*)player_ent;

        if (p_item[0x19C] == 0) return;
        p_item[0x19C] = 0;

        unsigned char* ps = *(unsigned char**)(p_player + 0x180);
        if (ps == nullptr) return;
        if (*(int32_t*)(p_player + 0x1C8) < 1) return;
        if (*(int32_t*)kGlobal_18F6DBC != 0) return;
        if (ps[0x14] & 0x40) return;

        // @modified — 9-bit decode replaces vanilla 7-bit.
        int weap_idx = ResolveWeapIdx(item_ent, 0xA8);

        // Pickup gate. Call vanilla sub_40FCB0 VA so the SafetyHook detour
        // routes to T4_Reconstructed::BG_CanItemBeGrabbed.
        unsigned char canTake;
        {
            void* item_local = item_ent;
            void* ps_local   = ps;
            void* td_local   = touch_data;
            __asm {
                push    edi
                push    esi
                push    ebx
                mov     esi, item_local
                mov     edi, ps_local
                push    td_local
                mov     eax, 0x0040FCB0
                call    eax
                add     esp, 4
                mov     canTake, al
                pop     ebx
                pop     esi
                pop     edi
            }
        }

        if (!canTake) {
            if (touch_data != nullptr) return;
            // @new — NULL guard before sub_4FD150 (vanilla derefs weaponDef internally).
            if (weap_idx >= 0 && weap_idx < 512 && bg_weaponDefs[weap_idx] != nullptr) {
                Call_sub_4FD150(weap_idx, item_ent, player_ent);
            }
            return;
        }

        // Build pickup log string from ps[+0x21F0] (player name).
        char buf[64] = {0};
        Call_sub_7AA9C0(buf, (const char*)(ps + 0x21F0), 63);
        buf[63] = 0;
        Call_sub_5F6CA0(buf);

        // @new — NULL guard (see BG_CanItemBeGrabbed for rationale).
        WeaponDef* wd = (weap_idx >= 0 && weap_idx < 512) ? bg_weaponDefs[weap_idx] : nullptr;
        if (wd != nullptr) {
            const char* weap_name  = *(const char**)wd;
            int32_t player_field0  = *(int32_t*)(p_player + 0);
            const char* formatted = Call_sub_5A9F30(player_field0, buf, weap_name);
            Call_sub_67D3D0("Weapon;%d;%d;%s;%s\n", formatted);
        }

        int slot_out = 0;
        int picked = Call_sub_4FD080(item_ent, player_ent, &slot_out, touch_data);

        if (slot_out != 0) {
            if (*(int32_t*)(ps + 0x2158) != 0) {
                Call_sub_54F350(player_ent, weap_idx, slot_out);
            } else {
                Call_sub_54F3A0(player_ent, weap_idx, slot_out);
            }
        }

        if (picked) {
            if (*(int32_t*)(p_item + 4) == 4) {
                unsigned short word_val = *(unsigned short*)kGlobal_1F33BBE;
                Call_sub_546EF0(0, word_val, item_ent);
            }
            Call_sub_54EDC0(item_ent);
        }
    }
}


// =====================================================================
// sub_4F3D60 → T4_Reconstructed::Cmd_GiveF
// =====================================================================
//
// Cascading dispatch (var_8 = "keep cascading" flag, set only by off_818594) :
//   off_818594        → HP add → weapons loop → ammo path → allammo
//   "health"          → HP add → return
//   "weapons"         → weapons loop → return
//   "ammo"            → ammo path → return
//   "allammo" + amt   → allammo loop → return
//   <weapon name>     → spawn item_ent + touch dispatch  ← BUG fixed here
namespace T4_Reconstructed
{
    extern "C" void __cdecl Cmd_GiveF(void* player_ent)
    {
        auto p_player = (unsigned char*)player_ent;

        if (Call_sub_4F39A0(player_ent) == 0) return;

        char* arg2 = Call_sub_4F3A30(2);
        int amount = Call_sub_7AA796(arg2);

        char* keyword = Call_sub_4F3A30(1);
        if (keyword == nullptr) return;
        const char* p = keyword;
        while (*p != 0) ++p;
        if (p == keyword) return;

        int var_8 = 0;
        bool matched_off818594 = (Call_sub_5F69E0(0x7FFFFFFF, (const char*)kStr_off_818594, keyword) == 0);
        bool matched_health    = false;
        if (matched_off818594) {
            var_8 = 1;
        } else {
            matched_health = (Call_sub_5F69E0(6, "health", keyword) == 0);
        }

        if (matched_off818594 || matched_health) {
            if (amount) {
                *(int32_t*)(p_player + 0x1C8) += amount;
            } else {
                int32_t* g_185581C = *(int32_t**)kGlobal_185581C;
                int32_t v = g_185581C[0x10 / 4];
                int32_t* ps = *(int32_t**)(p_player + 0x180);
                ps[0x16C / 4] = v;
                *(int32_t*)(p_player + 0x1C8) = ps[0x16C / 4];
            }
            if (var_8 == 0) return;
            goto weapons_loop;
        }

        if (Call_sub_5F69E0(0x7FFFFFFF, "weapons", keyword) == 0) {
            goto weapons_loop;
        }
        if (Call_sub_5F69E0(4, "ammo", keyword) == 0) {
            goto ammo_block;
        }
        goto allammo_block;


    weapons_loop:
        {
            int32_t maxw  = *(int32_t*)kGlobal_46DE3BC;
            int32_t total = maxw + 1;
            if (total > 1) {
                for (int i = 1; i < total; ++i) {
                    WeaponDef* wd = bg_weaponDefs[i];
                    // @new — NULL guard. Vanilla `add eax, 0Ch ; jz` is dead code
                    // because vanilla slots [1, 128) were always filled. With T4M's
                    // 512-slot pool, many slots are NULL — skip those.
                    if (wd == nullptr) continue;
                    int32_t* ps  = *(int32_t**)(p_player + 0x180);
                    Call_sub_41D6A0(ps, 1);
                    int32_t* ps2 = *(int32_t**)(p_player + 0x180);
                    Call_sub_5528D0(i, ps2, 0);
                }
            }
            if (var_8 == 0) return;
        }

    ammo_block:
        {
            if (amount) {
                int32_t* ps = *(int32_t**)(p_player + 0x180);
                int32_t current_weap = ps[0x104 / 4];
                if (current_weap != 0) {
                    Call_sub_4FC570(player_ent, current_weap, 0, amount, 1);
                }
            } else {
                int32_t maxw  = *(int32_t*)kGlobal_46DE3BC;
                int32_t total = maxw + 1;
                for (int i = 1; i < total; ++i) {
                    Call_sub_4FC570(player_ent, i, 0, 0x3E6, 1);
                }
            }
            if (var_8 == 0) return;
        }

    allammo_block:
        {
            bool matched_allammo = (Call_sub_5F69E0(7, "allammo", keyword) == 0);
            if (matched_allammo && amount) {
                int32_t maxw  = *(int32_t*)kGlobal_46DE3BC;
                int32_t total = maxw + 1;
                for (int i = 1; i < total; ++i) {
                    Call_sub_4FC570(player_ent, i, 0, amount, 1);
                }
                return;
            }
        }

        // ---- weapon-name branch (loc_4F3F8C) — the buggy decoder path ----

        if (var_8 != 0) return;     // off_818594 cascade ends here (no spawn)

        *(int32_t*)kGlobal_18F6DB8 = 1;

        void* weap_ref = Call_sub_40FAA0(keyword, 0);
        if (weap_ref == nullptr) {
            *(int32_t*)kGlobal_18F6DB8 = 0;
            return;
        }

        void* item_ent = Call_sub_54EAB0();
        auto p_item = (unsigned char*)item_ent;

        *(float*)(p_item + 0x160) = *(float*)(p_player + 0x160);
        *(float*)(p_item + 0x164) = *(float*)(p_player + 0x164);
        *(float*)(p_item + 0x168) = *(float*)(p_player + 0x168);

        Call_sub_4FD500((int)(intptr_t)weap_ref, p_item + 0x1A0);
        Call_sub_4FEAB0((int)(intptr_t)weap_ref, item_ent);

        p_item[0x19C] = 1;

        // BUG branch.
        if (*(int32_t*)weap_ref == 1) {
            // @modified — 9-bit decode replaces vanilla 7-bit.
            int idx = ResolveWeapIdx(item_ent, 0x260);
            // @new — NULL guard.
            WeaponDef* wd_at_idx = (idx >= 0 && idx < 512) ? bg_weaponDefs[idx] : nullptr;
            if (wd_at_idx != nullptr) {
                int32_t wd_type = *(int32_t*)((unsigned char*)wd_at_idx + 0x174);
                int32_t* ps = *(int32_t**)(p_player + 0x180);
                if (wd_type == 3) {
                    ps[0x100 / 4] = 1;
                } else if (wd_type == 2) {
                    ps[0x100 / 4] = 0;
                }
            }
        }

        // Dispatch through G_TouchItem (VA → SafetyHook detour).
        {
            void* item_local   = item_ent;
            void* player_local = player_ent;
            __asm {
                push    edi
                push    esi
                push    ebx
                push    0                          ; touch_data = 0
                push    player_local
                mov     eax, item_local
                mov     edx, 0x004FD210
                call    edx
                add     esp, 8
                pop     ebx
                pop     esi
                pop     edi
            }
        }

        p_item[0x19C] = 0;
        if (p_item[0x11D] != 0) {
            Call_sub_54EDC0(item_ent);
        }

        *(int32_t*)kGlobal_18F6DB8 = 0;
    }
}


// =====================================================================
// Naked wrappers — __usercall ABI translation for SafetyHook detours
// =====================================================================

namespace T4M
{
    // sub_4FD210 vanilla ABI : eax=item_ent, [esp+4]=player_ent, [esp+8]=touch_data.
    extern "C" __declspec(naked) void G_TouchItem_Wrapper()
    {
        __asm {
            push    [esp+8]                  ; touch_data
            push    [esp+8]                  ; player_ent (still at +8 after first push)
            push    eax                      ; item_ent
            call    T4_Reconstructed::G_TouchItem
            add     esp, 0Ch
            retn
        }
    }

    // sub_40FCB0 vanilla ABI : esi=item_ent, edi=ps_ptr, [esp+4]=touch_data.
    extern "C" __declspec(naked) void BG_CanItemBeGrabbed_Wrapper()
    {
        __asm {
            push    [esp+4]                  ; touch_data
            push    edi                      ; ps_ptr
            push    esi                      ; item_ent
            call    T4_Reconstructed::BG_CanItemBeGrabbed
            add     esp, 0Ch
            retn
        }
    }

    // sub_4F3D60 vanilla ABI : __cdecl, single stack arg. Caller does not clean up.
    extern "C" __declspec(naked) void Cmd_GiveF_Wrapper()
    {
        __asm {
            push    [esp+4]                  ; player_ent
            call    T4_Reconstructed::Cmd_GiveF
            add     esp, 4
            retn
        }
    }
}


// =====================================================================
// Install the 3 detours.
// =====================================================================

//void PatchT4MAM_PickupDecoder()
//{
    // PAUSED 2026-05-11 — see plan_weapons_full_detour.md
    // static auto detour_G_TouchItem = safetyhook::create_inline(
    //     (void*)0x004FD210,
    //     (void*)&T4M::G_TouchItem_Wrapper);
    // (void)detour_G_TouchItem;
    //
    // static auto detour_BG_CanItem = safetyhook::create_inline(
    //     (void*)0x0040FCB0,
    //     (void*)&T4M::BG_CanItemBeGrabbed_Wrapper);
    // (void)detour_BG_CanItem;
    //
    // static auto detour_Cmd_GiveF = safetyhook::create_inline(
    //     (void*)0x004F3D60,
    //     (void*)&T4M::Cmd_GiveF_Wrapper);
    // (void)detour_Cmd_GiveF;
//}


// =====================================================================
// ARCHIVED - dword_8F44A8 9-bit patches (originally in PatchT4MemoryLimits.cpp
//            SetupDword8F44A8Table, blocks 3-7 commented out 2026-05-11)
// =====================================================================
//
// // (3) Init count - rep stosd 32 KB - 4
// *(DWORD*)0x0041D2F1 = 0x00001FFF;
//
// // (4) Init stride - sub_41D310 loop
// *(DWORD*)0x0041D347 = 0x00000800;
//
// // (5) Composite-encoder shifts shl 7->9
// for (DWORD va : k_dword8F44A8_Shl7Sites) {
//     *(BYTE*)va = 0x09;
// }
//
// // (6) Reverse-mapper masks 0x8000007F -> 0x800001FF
// for (DWORD va : k_dword8F44A8_AndMaskSites) {
//     *(DWORD*)va = 0x800001FF;
// }
//
// // (7) Slot-extract shifts sar 7->9
// for (DWORD va : k_dword8F44A8_Sar7Sites) {
//     *(BYTE*)va = 0x09;
// }
