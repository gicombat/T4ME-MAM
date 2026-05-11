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
    constexpr uintptr_t MSG_ReadBit           = 0x00675060;  // __usercall(edx=msg)
    constexpr uintptr_t MSG_ReadBits          = 0x00674F50;  // __usercall(edx=msg) + stack(bits)
    constexpr uintptr_t MSG_ReadDword         = 0x00675560;  // __usercall(ecx=msg) -> eax (32-bit signed)
    constexpr uintptr_t MSG_ReadShortSignExt  = 0x00675500;  // __usercall(eax=msg) -> eax (16-bit sign-ext)
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
    // (1) Byte patch : netfield table entry 54 (ps.weapon) bits 7 -> 16.
    //
    //     Re-enabled 2026-05-10 after fixing the C++ decoder to match
    //     vanilla's wire format (delta-flag + zero-flag, see
    //     MSG_ReadDeltaField_PsWeapon). Previous attempt failed because the
    //     decoder skipped the zero-flag bit, desyncing the snapshot stream.
    //
    //     Symmetric encoder/decoder : the snapshot encoder (sub_67B0C0 @
    //     0x67B0C0) reads `bits` from the same off_836B30 table dynamically,
    //     so this 1-byte change propagates to both sides without further
    //     work. Wire transports 16-bit ps.weapon (idx 0..0xFFFF).
    Memory::VP::Patch<uint8_t>(k_NetfieldTable_PsWeaponBits, 0x10);

    // (2) Detour sub_676A50. The naked wrapper either runs our ps.weapon
    //     reconstruction (post-patch entry_bits=16) or jumps to the trampoline
    //     (vanilla function for all other entries).
    static auto hook = safetyhook::create_inline(
        (void*)VA::SubMSG_676A50,
        (void*)&T4M::MSG_ReadDeltaField_Detour);

    g_msg_read_field_trampoline = (uintptr_t)hook.original<void*>();
    (void)hook;
}
