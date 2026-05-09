#include "StdInc.h"
#include "MemoryMgr.h"
#include "enums.hpp"
#include "structs.hpp"
#include "xasset.hpp"
#include "clientscript/clientscript_public.hpp"
using namespace T4;
#include <cstddef>
#include <cstring>

// =============================================================================
// (1) WeaponDef pool stride bump 0x9AC -> 0x9DC
// =============================================================================
//
// MUST run before PatchT4_MemoryLimits so DB_ReallocXAssetPool(WEAPON, ...)
// queries the patched size getter (sub_47AF00) and allocates 512 * 0x9DC bytes.
// Branch A of sub_486780 (mov ecx, 9ACh at 0x00486796 - .ff stream read via sub_47B0E0)
// is intentionally NOT patched: changing it would desync the .ff binary stream.

static_assert(sizeof(WeaponDef) == 0x9DC,
    "WeaponDef must be 0x9DC bytes after lowReady extension (vanilla = 0x9AC, +0x30)");


// =============================================================================
// (2) .weapon text parser field-def table extension
// =============================================================================
//
// Vanilla field-def table at off_8DDF48: 565 entries x 12 bytes.
// Each entry: { const char* name, uint32 weapondef_offset, uint32 type }.
//
// Type indicators:
//   Types 0..0x0B are generic parsers handled in sub_5F75D0's jumptable. Each
//   reads from the entry's `offset` field and writes to WeaponDef+offset:
//     0x00 -> string callback (calls arg_18 = sub_423DD0)
//     0x04 -> atoi generic int (digits only; atoi("0.5")=0, NOT useful for "time")
//     0x06 -> atof generic float
//     0x07 -> atof * 1000 stored as int (seconds -> milliseconds-as-int)
//     others -> bool, sized-string, asset lookups
//   Types 0x0C..0x23 are FIELD-SPECIFIC parsers in sub_423250 with HARDCODED
//   weapondef offsets — DO NOT use these for new fields.
//
// For our 12 lowReady fields:
//   iLowReadyInTime/LoopTime/OutTime -> 0x07 (seconds -> ms-as-int)
//   lowReadyOfs*/Rot*                -> 0x06 (atof generic float)
//   slowReady*Anim                   -> 0x00 (string callback)

namespace T4M
{
    constexpr uint32_t TYPE_STRING = 0x00;
    constexpr uint32_t TYPE_FLOAT  = 0x06;   // atof generic
    constexpr uint32_t TYPE_TIME   = 0x07;   // atof seconds * 1000 -> int ms

    struct FieldDef 
    {
        const char* name;
        uint32_t    offset;
        uint32_t    type;
    };

    constexpr uint32_t VANILLA_COUNT    = 0x235;
    constexpr uint32_t NEW_COUNT        = VANILLA_COUNT + 12;
    constexpr uint32_t VANILLA_TABLE_VA = 0x008DDF48;

    static const char k_lowReadyInTime[]   = "lowReadyInTime";
    static const char k_lowReadyLoopTime[] = "lowReadyLoopTime";
    static const char k_lowReadyOutTime[]  = "lowReadyOutTime";
    static const char k_lowReadyOfsF[]     = "lowReadyOfsF";
    static const char k_lowReadyOfsR[]     = "lowReadyOfsR";
    static const char k_lowReadyOfsU[]     = "lowReadyOfsU";
    static const char k_lowReadyRotP[]     = "lowReadyRotP";
    static const char k_lowReadyRotY[]     = "lowReadyRotY";
    static const char k_lowReadyRotR[]     = "lowReadyRotR";
    static const char k_lowReadyInAnim[]   = "lowReadyInAnim";
    static const char k_lowReadyLoopAnim[] = "lowReadyLoopAnim";
    static const char k_lowReadyOutAnim[]  = "lowReadyOutAnim";

    static void ApplyWeaponDefSizePatches()
    {
        // sub_47AF00 - per-type sizeof getter (table off_8DCC18[0x18])
        Memory::VP::Patch<uint32_t>(0x0047AF01, 0x9DC);

        // sub_490030 - pool init for asset type 0x18 (weapon)
        Memory::VP::Patch<uint32_t>(0x00490055, 0x9DC);          // lea ecx, [eax+9ACh]    -> 9DCh
        Memory::VP::Patch<uint32_t>(0x00490061, 0x9DC);          // imul esi, 9ACh         -> 9DCh
        Memory::VP::Patch<int32_t>(0x00490068, -0x9D8);         // [esi+edi-9A8h]         -> -9D8h (= stride - 4)

        // sub_486780 - .ff weapon scratch setup (skip branch A which reads from .ff stream)
        Memory::VP::Patch<uint32_t>(0x004867B3, 0x9DC);          // push 9ACh (memset)     -> 9DCh
        Memory::VP::Patch<uint32_t>(0x004867D7, 0x9DC);          // mov [...]+0x4, 9ACh    -> 9DCh
        Memory::VP::Patch<uint32_t>(0x004867E9, 0x9DC);          // add dword_B548BC, 9ACh -> 9DCh

        // sub_424130 - BG_LoadWeaponDef (.iwd path, primary weapon loader)
        Memory::VP::Patch<uint32_t>(0x00424154, 0x9DC);          // push 9ACh (sub_5E4350) -> 9DCh
    }

    static void ApplyWeaponDefParserPatches()
    {
        static uint8_t* newTable = (uint8_t*)VirtualAlloc(
            NULL, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!newTable) 
            return;

        memcpy(newTable, (const void*)VANILLA_TABLE_VA, VANILLA_COUNT * sizeof(FieldDef));

        FieldDef* extra = reinterpret_cast<FieldDef*>(newTable + VANILLA_COUNT * sizeof(FieldDef));
        extra[ 0] = { k_lowReadyInTime,   offsetof(WeaponDef, iLowReadyInTime),   TYPE_TIME   };
        extra[ 1] = { k_lowReadyLoopTime, offsetof(WeaponDef, iLowReadyLoopTime), TYPE_TIME   };
        extra[ 2] = { k_lowReadyOutTime,  offsetof(WeaponDef, iLowReadyOutTime),  TYPE_TIME   };
        extra[ 3] = { k_lowReadyOfsF,     offsetof(WeaponDef, lowReadyOfsF),      TYPE_FLOAT  };
        extra[ 4] = { k_lowReadyOfsR,     offsetof(WeaponDef, lowReadyOfsR),      TYPE_FLOAT  };
        extra[ 5] = { k_lowReadyOfsU,     offsetof(WeaponDef, lowReadyOfsU),      TYPE_FLOAT  };
        extra[ 6] = { k_lowReadyRotP,     offsetof(WeaponDef, lowReadyRotP),      TYPE_FLOAT  };
        extra[ 7] = { k_lowReadyRotY,     offsetof(WeaponDef, lowReadyRotY),      TYPE_FLOAT  };
        extra[ 8] = { k_lowReadyRotR,     offsetof(WeaponDef, lowReadyRotR),      TYPE_FLOAT  };
        extra[ 9] = { k_lowReadyInAnim,   offsetof(WeaponDef, slowReadyInAnim),   TYPE_STRING };
        extra[10] = { k_lowReadyLoopAnim, offsetof(WeaponDef, slowReadyLoopAnim), TYPE_STRING };
        extra[11] = { k_lowReadyOutAnim,  offsetof(WeaponDef, slowReadyOutAnim),  TYPE_STRING };

        // Patch the two `push imm32` immediates in sub_424130.
        Memory::VP::Patch<uint32_t>(0x00424328, NEW_COUNT);             // count: 565 -> 577
        Memory::VP::Patch<uint32_t>(0x0042432D, (uint32_t)newTable);    // table base
    }

}
// =============================================================================
// (3) CG_RegisterWeapon iteration cap bump 0x80 -> NEW_MAX_WEAPONS
// =============================================================================
//
// sub_4659B0 is the per-frame iterator that calls sub_464BF0 (CG_RegisterWeapon)
// for each weapon idx 1..127. Vanilla cap is hardcoded as `cmp esi, 80h` at
// 0x00465A15 (6-byte form: 81 FE 80 00 00 00, immediate at +2 = 0x00465A17).
//
// Without this patch, weapons with idx >= 128 are NEVER registered. Their
// cg_weaponInfo[idx] entries stay at the BSS-zero state, and sub_464F90 (the
// attachment-meld builder) reads cg_weaponInfo[idx]+0x4 = NULL → meld crash.
//
// Confirmed via t4m_meld_diag.log: CG_RegWpn entries stop at idx=127, and the
// failing weapons (shotgun_1912_wet, springfield_scoped, sw1917_mw2,
// type100_smg_wet_s2, type100_smg_early_wet_s2, type11_lmg) all have idx >= 128.
//
// Side effect: with the cap raised, sub_464BF0 will be called for idx that may
// have bg_weaponDefs[idx] = NULL (= unallocated slots in the extended pool).
// sub_464BF0 dereferences `[ebx+0Ch]` where ebx = bg_weaponDefs[idx] without
// a NULL check, so we add a midhook guard at the function entry to early-out
// when the WeaponDef ptr is null.
// =============================================================================

#include <safetyhook.hpp>
extern T4::WeaponDef** bg_weaponDefs;

namespace T4M
{
    // Naked thunk: pure `retn` to bail out of sub_464BF0 cleanly. cdecl, caller
    // cleans the 2 stack args, so we just pop the saved ret addr.
    __declspec(naked) static void Sub_464BF0_RetnThunk()
    {
        __asm { retn }
    }

    // =====================================================================
    // byte_351EAA0 (per-weapon "current attachment slot" byte table) relocation.
    //
    // Vanilla layout: a 128-byte BSS region at 0x351EAA0, indexed by weapon idx.
    // Read sites:
    //   sub_465270+0x32 (mov cl, byte_351EAA0[edi])
    //   sub_469AB0+0x4D-ish (movzx eax, byte_351EAA0[esi])
    //
    // For idx >= 128, vanilla reads adjacent BSS data that's used by OTHER
    // game state — so the "byte" is whatever happens to be written there
    // (observed: 0xFF = 255 for weapon idx 148 = springfield_scoped). cl=255
    // then makes sub_464F90 read [WeaponDef + 0x408] as a model pointer (= a
    // small-int field of the WeaponDef interpreted as a pointer) → meld crash.
    //
    // Fix: VirtualAlloc 512 bytes, zero-init, .text-scan-replace the two
    // immediate `0x0351EAA0` occurrences. New buffer is BSS-zero so cl reads
    // always return 0 unless explicitly set by per-weapon attach selection
    // code (which we don't see writing to byte_351EAA0 — it's read-only here).
    // =====================================================================
    static BYTE* g_newCurrentAttachSlot = nullptr;

    // sub_465200 reads cl from `[dword_34732E0 + edi + 0xB5C]` — a per-weapon
    // attachment-slot byte inside a dynamic client struct (pointed to by
    // dword_34732E0). The struct's field is vanilla-sized for 128 weapons. For
    // weapon idx >= 128, the read returns adjacent struct bytes (= e.g. 0xFF
    // observed for idx=148) that get fed into sub_464F90 as cl, then used as
    // gunXModel[] index → reads beyond the array → meld crash.
    //
    // Can't relocate: the byte table is a field inside a dynamically-allocated
    // struct, not a fixed BSS address. Instead, midhook RIGHT AFTER the read
    // and override cl=0 (= default attach slot) when edi >= 128.
    //
    // Hook site: 0x0046522D = `cmp [esi+4], cl` (3 bytes). At this point the
    // mov has already executed (cl = OOB byte). Lambda fires before cmp, edits
    // ctx.ecx low byte to 0 if edi indicates OOB.
    static void Apply_Sub_465200_ClClamp()
    {
        static auto cl_clamp_hook = safetyhook::create_mid(0x0046522D, [](SafetyHookContext& ctx) {
            if (ctx.edi >= 128) {
                ctx.ecx = ctx.ecx & 0xFFFFFF00;  // clear low byte (= cl = 0)
            }
        });
        (void)cl_clamp_hook;
    }

    // =====================================================================
    // usercmd delta wire bit-count 7 → 9 for weapon AND offhand.
    //
    // Two paired patches that MUST stay in sync:
    //
    // (a) Writer in sub_675BC0 (delta encoder): `push 7; call sub_675940 (=
    //     MSG_WriteBits)`. Sends only 7 bits → wire truncates idx > 127.
    //       weapon  imm at 0x00675F0E
    //       offhand imm at 0x00675F26
    //
    // (b) Reader in sub_675FD0 (delta decoder + validator): `push 7; call
    //     sub_674F50 (= MSG_ReadBits)` followed by `mov [ebp+14h], al` (or
    //     [ebp+15h] for offhand). Reads 7 bits → cmd.weapon byte gets the
    //     truncated value. The 0x80 cap then rejects+resets via the path we
    //     already patched (jb→jmp at +0x620/+0x640).
    //       weapon  imm at 0x006764A7
    //       offhand imm at 0x006764D7
    //
    // After this patch, wire transports 9 bits (= 0..511 range) for both
    // fields. Combined with Apply_UsercmdValidationFix (= no reset on > 127),
    // weapon idx up to 255 (= byte field max) propagates correctly through
    // the wire encode/decode cycle.
    // =====================================================================
    static void Apply_UsercmdWireBitsExtension()
    {
        // Writer (sub_675BC0)
        Memory::VP::Patch<uint8_t>(0x00675F0E, 9);
        Memory::VP::Patch<uint8_t>(0x00675F26, 9);
        // Reader (sub_675FD0)
        Memory::VP::Patch<uint8_t>(0x006764A7, 9);
        Memory::VP::Patch<uint8_t>(0x006764D7, 9);
    }

    // =====================================================================
    // sub_675FD0 (usercmd_t validation, runs every tick even in SP):
    //   if (cmd.weapon  >= 0x80) { print "invalid weapon number"  ; cmd.weapon  = old.weapon;  }
    //   if (cmd.offhand >= 0x80) { print "invalid offhand index"  ; cmd.offhand = old.offhand; }
    //
    // Symptom: equip thompson_m1_wet (idx=165) → reset to colt (37 = previous
    // valid value), viewmodel/HUD show colt. Console spammed "invalid offhand
    // index 221" when offhand grenade > 127.
    //
    // The validation enforces a 7-bit cap independent of any netfield table —
    // applies in SP too because usercmd is generated locally each tick.
    //
    // Field is byte-sized (cmd.weapon / cmd.offhand are u8) → max 255. With
    // T4M's pool extension to 512, the byte field still only holds 0..255.
    // 226+ weapons fit (= user's current count). For full 512 we'd need to
    // expand the field to 16-bit, which touches every usercmd consumer.
    //
    // Cleanest patch: turn the `jb short` into `jmp short` (same 2-byte form,
    // 72 XX → EB XX) so the error/reset block is always skipped, no matter
    // what the byte value is. Allows full 0..255 range.
    //
    //   weapon  jb at 0x006765F0 → jmp (byte 72 → EB)
    //   offhand jb at 0x00676610 → jmp (byte 72 → EB)
    // =====================================================================
    static void Apply_UsercmdValidationFix()
    {
        Memory::VP::Patch<uint8_t>(0x006765F0, 0xEB);
        Memory::VP::Patch<uint8_t>(0x00676610, 0xEB);
    }

    // =====================================================================
    // playerState_s.weapon netfield bit-count: 7 → 9.
    //
    // Snapshot deltas serialize ps.weapon (offset 0x104) using the netfield
    // table at off_836B30 (entries are { name_ptr, offset, bits, type } × 16
    // bytes, 156 entries total, consumed by sub_676A50 via sub_6775F0).
    //
    // Vanilla bits = 7 → max weapon idx = 127. With T4M's pool extension to
    // 512, idx ≥ 128 wraps via 7-bit truncation: e.g. user equips
    // springfield_scoped (idx=148), client receives idx=20 (= 148 & 0x7F),
    // viewmodel/HUD show browninghp_ge_aw instead.
    //
    // Other weapon-related netfields verified correct:
    //   viewmodelIndex (0x120): bits=9 already (= 512 max, safe)
    //   weaponstate    (0x108): bits=5 (state machine, not idx)
    //   weaponShotCount(0x10C): bits=3 (counter)
    //   weaponold      (0x80C): bits=32 (full int)
    //   weaponrechamber(0x81C): bits=32
    //
    // Weapon entry virtual address = 0x836B30 (table base) + 54*16 (entry idx)
    //   bits field at +8 = 0x836E98.
    // =====================================================================
    static void Apply_PsWeaponBitsExtension()
    {
        Memory::VP::Patch<uint32_t>(0x00836E98, 9);
    }

    static void RelocateByte351EAA0()
    {
        if (g_newCurrentAttachSlot) return;
        const SIZE_T NEW_SIZE = 512; // matches NEW_MAX_WEAPONS

        g_newCurrentAttachSlot = (BYTE*)VirtualAlloc(NULL, NEW_SIZE,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_newCurrentAttachSlot) return;
        memset(g_newCurrentAttachSlot, 0, NEW_SIZE);

        const DWORD OLD_BASE = 0x0351EAA0;
        const DWORD NEW_BASE = (DWORD)g_newCurrentAttachSlot;
        const DWORD TEXT_START = 0x00401000U;
        const DWORD TEXT_END   = 0x00800000U;
        DWORD oldProt;
        VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
            PAGE_EXECUTE_READWRITE, &oldProt);

        for (BYTE* bp = (BYTE*)TEXT_START; bp < (BYTE*)(TEXT_END - 3); ++bp) {
            if (*(DWORD*)bp == OLD_BASE) {
                *(DWORD*)bp = NEW_BASE;
            }
        }
        // Leave .text writable per project policy.
    }

    static void ApplyCgRegisterWeaponCapPatch()
    {
        // (a) Loop cap: cmp esi, 80h → cmp esi, 200h (= 512 = NEW_MAX_WEAPONS).
        // The 32-bit immediate of the 6-byte cmp lives at 0x465A17.
        Memory::VP::Patch<uint32_t>(0x00465A17, 0x200);

        // (b) Bypass the bitmap test that filters which weapons to register.
        //
        // Vanilla reads a hex-encoded bitmap string at byte_305D5FC[dword_305D344]
        // (32 chars = 128 weapons × 4 bits) into a local 128-byte buffer at the
        // start of sub_4659B0. The loop body at loc_4659E1 reads one byte per
        // 4 weapons, parses it as a hex digit, and tests `1 << (esi & 3)` to
        // decide whether to call sub_464BF0(arg_0, esi).
        //
        // For esi >= 128 (= reading past the bitmap's null terminator), we get
        // arbitrary stack garbage interpreted as hex digits → registration
        // becomes a non-deterministic SPARSE pattern (e.g. logged 146, 147,
        // 150, 151, 153, 154 but NOT 142, 148, 149, 152 — all because garbage
        // byte X bit (esi & 3) happened to be 1 vs 0).
        //
        // Fix: NOP the `jz short loc_465A12` at sub_4659B0+0x56 = 0x00465A06
        // so sub_464BF0 is unconditionally called for every idx 1..0x1FF. The
        // NULL guard below filters out unallocated slots.
        //
        // Original bytes: 74 0A    (jz +0xA)
        // Patched bytes:  90 90    (nop, nop)
        Memory::VP::Patch(0x00465A06, { 0x90, 0x90 });

        // (c) Guard sub_464BF0 against NULL bg_weaponDefs[idx]. Vanilla never
        // had to handle this since the loop stopped before any unallocated slot
        // and the bitmap filter excluded missing weapons; post-bypass we hit
        // idx 128..511 where most slots are empty for any single map. Without
        // this guard, sub_464BF0 NULL-derefs `[ebx+0Ch]` immediately.
        static auto sub_464BF0_null_def_guard = safetyhook::create_mid(0x00464BF0, [](SafetyHookContext& ctx) {
            int weapIdx = *(int*)(ctx.esp + 8);  // arg_4
            if (weapIdx <= 0 || weapIdx >= 512) {
                ctx.eip = (uintptr_t)&Sub_464BF0_RetnThunk;
                return;
            }
            if (!::bg_weaponDefs || !::bg_weaponDefs[weapIdx]) {
                ctx.eip = (uintptr_t)&Sub_464BF0_RetnThunk;
                return;
            }
        });
        (void)sub_464BF0_null_def_guard;
    }
}

// =============================================================================
// Single entry-point — replaces PatchT4M_WeaponDefSize() + PatchT4M_WeaponDefParser()
// =============================================================================

void PatchT4MAM_WeaponDef()
{
    T4M::ApplyWeaponDefSizePatches();
    T4M::ApplyWeaponDefParserPatches();
    T4M::RelocateByte351EAA0();
    T4M::Apply_Sub_465200_ClClamp();
    T4M::ApplyCgRegisterWeaponCapPatch();
    // Bit-width patches DISABLED 2026-05-09 — superseded by plan_weapons_full_detour.md.
    // We pivot to full C++ detours of the writers/readers (sub_410660 / sub_41F010 /
    // sub_676A50) which carry idx as int32_t natively. Byte-level imm patches were
    // unreliable (DR0 confirmed truncation persisted on netfield path despite bits=9)
    // and forced trade-offs (wire bits desync delta XOR with dword_8CF5E4).
    // T4M::Apply_PsWeaponBitsExtension();    // netfield ps.weapon bits 7→9 (off_836B30 entry 54)
    // T4M::Apply_UsercmdValidationFix();     // usercmd jb→jmp 7-bit cap bypass
    // T4M::Apply_UsercmdWireBitsExtension(); // usercmd wire bits 7→9 (writer + reader)
}
