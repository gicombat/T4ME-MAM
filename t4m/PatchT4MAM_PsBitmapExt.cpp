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
    static auto take_detour = safetyhook::create_inline(
        (void*)0x0041D6A0, (void*)&T4M::G_TakeWeapon_Wrapper);
    static auto give_detour = safetyhook::create_inline(
        (void*)0x005528D0, (void*)&T4M::G_GiveWeapon_Wrapper);
    static auto fc570_detour = safetyhook::create_inline(
        (void*)0x004FC570, (void*)&T4M::Sub_4FC570_Wrapper);
    static auto fc510_detour = safetyhook::create_inline(
        (void*)0x004FC510, (void*)&T4M::Sub_4FC510_Wrapper);
    static auto e5e0_detour = safetyhook::create_inline(
        (void*)0x0041E5E0, (void*)&T4M::Sub_41E5E0_Wrapper);
    static auto e350_detour = safetyhook::create_inline(
        (void*)0x0041E350, (void*)&T4M::Sub_41E350_Wrapper);
    (void)fc570_detour;
    (void)fc510_detour;
    (void)e5e0_detour;
    (void)e350_detour;
    static auto cycle_dump_hook = safetyhook::create_mid(
        (void*)0x0046A090,
        [](SafetyHookContext& /*ctx*/) { DumpCycleOwned(); });

    // Watchdog hooks : poll sv.weapons[2] at several boundaries to localize
    // which subsystem clears bit 7 and sets bit 2 between our G_GiveWeapon
    // (which writes 0x80) and the cycle dump (which sees 0x04).
    static auto watch_4ED3D0 = safetyhook::create_mid(
        (void*)0x004ED3D0,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_GScrGiveW"); });
    static auto watch_4ED310 = safetyhook::create_mid(
        (void*)0x004ED310,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_ChainProc"); });
    // watch_4FC570 disabled — would conflict with the create_inline detour
    // installed below at the same VA.
    static auto watch_4FC7F0 = safetyhook::create_mid(
        (void*)0x004FC7F0,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_FC7F0"); });
    static auto watch_4FCEB0 = safetyhook::create_mid(
        (void*)0x004FCEB0,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("ENTER_FCEB0"); });
    // Hook the very instruction after `call sub_5528D0` inside sub_4ED3D0 :
    // 0x004ED4EF = retaddr seen in the give trace. Fires right after our
    // G_GiveWeapon returns, before sub_4ED310 (ChainProc) is reached.
    static auto watch_post_give = safetyhook::create_mid(
        (void*)0x004ED4EF,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("POST_Give"); });
    // Right after `call sub_4ED310` (line 346830 = `add esp, 0Ch` at 0x004ED4FA).
    // Bisects between sub_4ED310 returning and sub_4ED3D0 returning.
    static auto watch_post_chain = safetyhook::create_mid(
        (void*)0x004ED4FA,
        [](SafetyHookContext& /*ctx*/) { PollServerW2("POST_ChainProc"); });

    (void)take_detour;
    (void)give_detour;
    (void)cycle_dump_hook;
    (void)watch_4ED3D0;
    (void)watch_4ED310;
    (void)watch_4FC7F0;
    (void)watch_4FCEB0;
    (void)watch_post_give;
    (void)watch_post_chain;
}
