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

void PatchT4MAM_PickupDecoder()
{
    static auto detour_G_TouchItem = safetyhook::create_inline(
        (void*)0x004FD210,
        (void*)&T4M::G_TouchItem_Wrapper);
    (void)detour_G_TouchItem;

    static auto detour_BG_CanItem = safetyhook::create_inline(
        (void*)0x0040FCB0,
        (void*)&T4M::BG_CanItemBeGrabbed_Wrapper);
    (void)detour_BG_CanItem;

    static auto detour_Cmd_GiveF = safetyhook::create_inline(
        (void*)0x004F3D60,
        (void*)&T4M::Cmd_GiveF_Wrapper);
    (void)detour_Cmd_GiveF;
}
