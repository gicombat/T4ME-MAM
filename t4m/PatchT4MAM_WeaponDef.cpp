#include "StdInc.h"
#include "t4_headers.h"
#include "MemoryMgr.h"
#include <cstddef>
#include <cstring>

// =============================================================================
// (1) WeaponDef pool stride bump 0x9AC -> sizeof(WeaponDef)
// =============================================================================
//
// MUST run before PatchT4_MemoryLimits so DB_ReallocXAssetPool(WEAPON, ...)
// queries the patched size getter (sub_47AF00) and allocates 512 * stride bytes.
// Branch A of sub_486780 (mov ecx, 9ACh at 0x00486796 - .ff stream read via sub_47B0E0)
// is intentionally NOT patched: changing it would desync the .ff binary stream.

static_assert(sizeof(WeaponDef) == 0x9FC,
    "WeaponDef: vanilla 0x9AC + lowReady 0x30 + reload extensions 0x20");


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
//
// Reload extension types match their vanilla counterparts in the table
// (segmentedReload 0x05, reloadAmmoAdd 0x04, reloadStartTime 0x07,
// reloadStartAnim 0x00, reloadStartSound 0x0B).

namespace T4M
{
    constexpr uint32_t TYPE_STRING = 0x00;
    constexpr uint32_t TYPE_INT    = 0x04;   // atoi generic
    constexpr uint32_t TYPE_BOOL   = 0x05;
    constexpr uint32_t TYPE_FLOAT  = 0x06;   // atof generic
    constexpr uint32_t TYPE_TIME   = 0x07;   // atof seconds * 1000 -> int ms
    constexpr uint32_t TYPE_SOUND  = 0x0B;   // snd_alias_list_t*

    struct FieldDef 
    {
        const char* name;
        uint32_t    offset;
        uint32_t    type;
    };

    constexpr uint32_t VANILLA_COUNT    = 0x235;
    constexpr uint32_t EXTRA_COUNT      = 12 + 8;
    constexpr uint32_t NEW_COUNT        = VANILLA_COUNT + EXTRA_COUNT;

    constexpr uint32_t WEAPONDEF_STRIDE = sizeof(WeaponDef);

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

    static const char k_closedBolt[]                  = "closedBolt";
    static const char k_reloadAmmoAddEmpty[]          = "reloadAmmoAddEmpty";
    static const char k_reloadStartEmptyTime[]        = "reloadStartEmptyTime";
    static const char k_reloadStartEmptyAddTime[]     = "reloadStartEmptyAddTime";
    static const char k_reloadStartEmptyAdd[]         = "reloadStartEmptyAdd";
    static const char k_reloadStartEmptyAnim[]        = "reloadStartEmptyAnim";
    static const char k_reloadStartEmptySound[]       = "reloadStartEmptySound";
    static const char k_reloadStartEmptySoundPlayer[] = "reloadStartEmptySoundPlayer";

    static void ApplyWeaponDefSizePatches()
    {
        // sub_47AF00 - per-type sizeof getter (table off_8DCC18[0x18])
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefSizeGetter_stride_site"), WEAPONDEF_STRIDE);

        // sub_490030 - pool init for asset type 0x18 (weapon)
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefPoolInit_lea_site"), WEAPONDEF_STRIDE);          // lea ecx, [eax+9ACh]
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefPoolInit_imul_site"), WEAPONDEF_STRIDE);         // imul esi, 9ACh
        Memory::VP::Patch<int32_t>(T4M::GetAddress("WeaponDefPoolInit_negOfs_site"), -(int32_t)(WEAPONDEF_STRIDE - 4)); // [esi+edi-9A8h] = -(stride - 4)

        // sub_486780 - .ff weapon scratch setup (skip branch A which reads from .ff stream)
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefFfScratch_memset_site"), WEAPONDEF_STRIDE);      // push 9ACh (memset)
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefFfScratch_field_site"), WEAPONDEF_STRIDE);       // mov [...]+0x4, 9ACh
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("WeaponDefFfScratch_accum_site"), WEAPONDEF_STRIDE);       // add dword_B548BC, 9ACh

        // sub_424130 - BG_LoadWeaponDef (.iwd path, primary weapon loader)
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("BG_LoadWeaponDef_stride_site"), WEAPONDEF_STRIDE);        // push 9ACh (sub_5E4350)
    }

    static void ApplyWeaponDefParserPatches()
    {
        static uint8_t* newTable = (uint8_t*)VirtualAlloc(
            NULL, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!newTable)
            return;

        // resolved at runtime (GetAddress reads the embedded CSV) — not constexpr
        const uint32_t VANILLA_TABLE_VA = (uint32_t)T4M::GetAddress("weaponDefParseFieldTable");

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

        extra[12] = { k_closedBolt,                  offsetof(WeaponDef, closedBolt),                  TYPE_BOOL   };
        extra[13] = { k_reloadAmmoAddEmpty,          offsetof(WeaponDef, iReloadAmmoAddEmpty),         TYPE_INT    };
        extra[14] = { k_reloadStartEmptyTime,        offsetof(WeaponDef, iReloadStartEmptyTime),       TYPE_TIME   };
        extra[15] = { k_reloadStartEmptyAddTime,     offsetof(WeaponDef, iReloadStartEmptyAddTime),    TYPE_TIME   };
        extra[16] = { k_reloadStartEmptyAdd,         offsetof(WeaponDef, iReloadStartEmptyAdd),        TYPE_INT    };
        extra[17] = { k_reloadStartEmptyAnim,        offsetof(WeaponDef, sreloadStartEmptyAnim),       TYPE_STRING };
        extra[18] = { k_reloadStartEmptySound,       offsetof(WeaponDef, reloadStartEmptySound),       TYPE_SOUND  };
        extra[19] = { k_reloadStartEmptySoundPlayer, offsetof(WeaponDef, reloadStartEmptySoundPlayer), TYPE_SOUND  };

        // Patch the two `push imm32` immediates in sub_424130.
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("BG_LoadWeaponDef_fieldCount_site"), NEW_COUNT);             // count: 565 -> 585
        Memory::VP::Patch<uint32_t>(T4M::GetAddress("BG_LoadWeaponDef_fieldTable_site"), (uint32_t)newTable);    // table base
    }

}
// =============================================================================
// Single entry-point — replaces PatchT4M_WeaponDefSize() + PatchT4M_WeaponDefParser()
// =============================================================================

void PatchT4MAM_WeaponDef()
{
    T4M::ApplyWeaponDefSizePatches();
    T4M::ApplyWeaponDefParserPatches();
}
