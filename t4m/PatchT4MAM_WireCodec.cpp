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
    static auto writer_detour = safetyhook::create_inline(
        (void*)0x00675BC0,
        (void*)&T4M::MSG_WriteDeltaUsercmd_Wrapper);
    (void)writer_detour;

    static auto reader_detour = safetyhook::create_inline(
        (void*)0x00675FD0,
        (void*)&T4M::MSG_ReadDeltaUsercmd_Wrapper);
    (void)reader_detour;
}
