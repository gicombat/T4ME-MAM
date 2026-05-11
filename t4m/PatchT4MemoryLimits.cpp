// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Increasing memory pool sizes
//
// Initial author: TheApadayo
//
// Started: 2015-07-18
// ==========================================================

#include "StdInc.h"
#include <safetyhook.hpp>

#define NEW_ASSET_ENTRY_POOL_SIZE 65535
#define NEW_IMAGE_SORT_BUFFER_SIZE 8192
#define NEW_MAX_GENTITIES   2048
#define ENTITY_SIZE         0x378        // 888 bytes per gentity_s
#define OLD_ENTITY_BASE     0x0176C6F0U
#define GSPAWN_CMP_IMM      0x54EAC3U   // immediate of "cmp ecx, 3FEh" in G_Spawn
#define ENTITY_BASE_DWORD   0x18F5D8CU  // dword_18F5D8C: runtime entity-base ptr
#define NEW_MAX_LOCALIZED   4096

// Shared with the WEAPON asset pool reallocation. cg_weaponInfo MUST be
// at least this large or the loop in sub_465200/sub_465160/sub_465270
// will OOB-read into the dvar registration globals at 0x3466040+.
#define NEW_MAX_WEAPONS     148

// External T4M-side globals that hardcode the OLD vanilla table addresses.
// We update these to the new heap base after relocation so T4M reconstructions
// (PM_Weapon, BG_GetWeaponDef, viewmodel pose, etc.) read from the same table
// vanilla .text writes to. Without this fix-up, T4M code reads the now-empty
// OLD BSS region → null pointers → crashes in PM_Weapon and friends.
extern T4::WeaponDef** bg_weaponDefs;  // defined in PatchT4Script.cpp

// Defined in this file (below). Other files extern-declare it to access the
// runtime-resolved cg_weaponInfo base after SetupCgWeaponInfoTable runs.
namespace T4M { BYTE* cg_weaponInfo = (BYTE*)0x03463C40; }

// Per-weapon companion tables — vanilla 128-sized OOB targets relocated by
// SetupBgWeaponDefsTable. T4M code that hardcoded the OLD bases must read these
// runtime pointers after PatchT4_MemoryLimits has run.
namespace T4M {
    void** g_dword8F44A8  = (void**)0x008F44A8;   // per-weapon item-slot table
    void** g_dword35D0DC8 = (void**)0x035D0DC8;   // per-weapon display name cache
}

// =====================================================================
// G_Entity pool expansion
//
// sub_502020 (entity system init) is called via sub_5AA5F0 from
// sub_62B7C0+1A1 every map start. It sets:
//   dword_18F5D8C = 0x176C6F0   (runtime entity-base ptr → old 1024-entry BSS array)
//
// We hook at 0x62B969 (first instruction after "call sub_5AA5F0; add esp, 0Ch"
// in sub_62B7C0). At that point sub_502020 has already initialized the entity
// system and written 0x176C6F0 to dword_18F5D8C.
//
// First invocation: allocate a 2048-entry array, copy the initialized BSS
// state into it, then scan .text with ONE VirtualProtect call and replace
// all three reference patterns:
//   0x0176C6F0  (array base)           → new_base
//   0x0184A6F0  (array end, sub_62B7C0 loop bound) → new_base + 2048*0x378
//   0x0184A780  (init loop bound in sub_502020, 0x90 past end) → same new_end
//
// Every invocation (including subsequent map loads where sub_502020 resets
// dword_18F5D8C to 0x176C6F0): redirect dword_18F5D8C to the new array.
// =====================================================================
namespace T4M
{
	static BYTE* g_newEntityPool = nullptr;

	static void SetupEntityPool()
	{
		// Allocate 2048-entry entity array (2048 * 0x378 = ~1.7 MB)
		g_newEntityPool = (BYTE*)VirtualAlloc(
			NULL,
			(SIZE_T)NEW_MAX_GENTITIES * ENTITY_SIZE,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!g_newEntityPool) {
			T4::Com_Printf(0, "^1[T4M] FATAL: entity pool VirtualAlloc failed\n");
			return;
		}

		// Copy initialized state from old 1024-entry BSS array
		memcpy(g_newEntityPool, (void*)OLD_ENTITY_BASE, 1024 * ENTITY_SIZE);

		DWORD newBase = (DWORD)g_newEntityPool;
		// "one past last entity" — upper bound for entity iteration and init loops
		DWORD newEnd  = newBase + (DWORD)NEW_MAX_GENTITIES * ENTITY_SIZE;

		// Patch all .text references in one VirtualProtect call.
		// Three patterns:
		//   0x0176C6F0 (OLD_ENTITY_BASE)  → newBase
		//   0x0184A6F0 (array end)        → newEnd  (sub_62B7C0 loop bound, line 833611)
		//   0x0184A780 (init loop bound)  → newEnd  (sub_502020 init loop, line 376007)
		// Scan is byte-granular to catch any alignment within instructions.
		const DWORD TEXT_START = 0x00401000U;
		const DWORD TEXT_END   = 0x00800000U;
		DWORD oldProt;
		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
					   PAGE_EXECUTE_READWRITE, &oldProt);

		int patched = 0;
		for (BYTE* bp = (BYTE*)TEXT_START; bp < (BYTE*)(TEXT_END - 3); ++bp) {
			DWORD v = *(DWORD*)bp;
			if (v == OLD_ENTITY_BASE) {
				*(DWORD*)bp = newBase;  ++patched;
			} else if (v == 0x0184A6F0U || v == 0x0184A780U) {
				*(DWORD*)bp = newEnd;   ++patched;
			}
		}

		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
					   PAGE_EXECUTE_READ, &oldProt);

		T4::Com_Printf(0, "[T4M] Entity pool: base=0x%08X, %d refs patched\n",
				   newBase, patched);
	}

	// =====================================================================
	// cg_weaponInfo table relocation (vanilla 0x3463C40, 128 entries, stride 0x48).
	//
	// Vanilla sized for BG_MAX_WEAPONS = 128. With T4M raising the WEAPON
	// asset pool to NEW_MAX_WEAPONS (= 512), maps with > 128 weapons hit
	// catastrophic OOB into the dvar globals at 0x3466040+ (written by sub_65ED80).
	// Result: dvar_t* gets read as a model, eventually crashes in sub_608D80 meld.
	//
	// Fix: VirtualAlloc 512 entries, then patch every .text occurrence of the
	// 8 IDA-labeled addresses inside the table (the only values that appear as
	// 4-byte immediates in instruction streams). Range scanning is unsafe — too
	// many false positives (random instruction byte sequences can fall inside
	// the 0x2400-byte range and get incorrectly rewritten).
	// =====================================================================
	static BYTE* g_newCgWeaponInfo = nullptr;

	// Distinct addresses inside cg_weaponInfo[] that appear as immediates in
	// .text. List from grep on the IDA dump (`^(dword|unk)_3463C[0-9A-F]+`):
	//   0x3463C40 (base)              0x3463C88 (entry 1 + 0x00 = base+0x48)
	//   0x3463C70 (entry 0 + 0x30)    0x3463C98 (entry 1 + 0x10 = base+0x58)
	//   0x3463C74 (entry 0 + 0x34)    0x3463CB4 (entry 1 + 0x6C? — base+0x74)
	//   0x3463C7C (entry 0 + 0x3C)    0x3463CB8 (entry 1 + 0x70? — base+0x78)
	// Each one gets shifted by the same delta so that all field offsets stay
	// consistent with the new base.
	static const DWORD k_cgWeaponInfoAddrs[] = {
		0x3463C40, 0x3463C70, 0x3463C74, 0x3463C7C,
		0x3463C88, 0x3463C98, 0x3463CB4, 0x3463CB8,
	};

	static void SetupCgWeaponInfoTable()
	{
		if (g_newCgWeaponInfo) return;

		const DWORD OLD_TABLE_BASE = 0x03463C40;
		const DWORD ENTRY_STRIDE   = 0x48;

		g_newCgWeaponInfo = (BYTE*)VirtualAlloc(
			NULL,
			(SIZE_T)NEW_MAX_WEAPONS * ENTRY_STRIDE,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!g_newCgWeaponInfo) return;

		// BSS-equivalent zero init.
		memset(g_newCgWeaponInfo, 0, (SIZE_T)NEW_MAX_WEAPONS * ENTRY_STRIDE);

		const DWORD newBase = (DWORD)g_newCgWeaponInfo;
		const DWORD delta   = newBase - OLD_TABLE_BASE;

		const DWORD TEXT_START = 0x00401000U;
		const DWORD TEXT_END   = 0x00800000U;
		DWORD oldProt;
		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
		               PAGE_EXECUTE_READWRITE, &oldProt);

		// Exact-match scan: only shift values that exactly equal one of the
		// 8 known table addresses. Full 32-bit constants are extremely unlikely
		// to appear as random instruction bytes by coincidence.
		for (BYTE* bp = (BYTE*)TEXT_START; bp < (BYTE*)(TEXT_END - 3); ++bp) {
			DWORD v = *(DWORD*)bp;
			for (DWORD known : k_cgWeaponInfoAddrs) {
				if (v == known) {
					*(DWORD*)bp = v + delta;
					break;
				}
			}
		}

		// Update T4M's runtime pointer for code paths that hardcoded the OLD
		// base inline (e.g., PatchT4MAM_LowReady.cpp).
		T4M::cg_weaponInfo = g_newCgWeaponInfo;

		// Intentionally leave .text writable — see comment in main body.
	}

	// =====================================================================
	// Per-weapon pointer tables relocation (7 tables, vanilla 128 × 4 bytes each).
	//
	// `sub_41D2A0` (BG init) populates 5 separate per-weapon arrays at indices 0
	// with the same default weapon pointer:
	//   0x8F6570 (count: 0x8F64FC)  — purpose TBD
	//   0x8F6770 (count: 0x8F6B74)  — bg_weaponDefs (primary def pointers)
	//   0x8F6970 (count: 0x8F6B70)  — alt def variant
	//   0x8F6B78 (count: 0x8F6FA8)  — alt def variant
	//   0x8F6FB0 (count: ?)          — alt def variant
	// Two more 128×4 companions are populated by CG_RegisterWeapon (sub_464BF0):
	//   0x8F44A8                    — per-weapon item-slot ptr (cgw[+0x38])
	//   0x35D0DC8                   — per-weapon display name cache
	//
	// All are sized for vanilla BG_MAX_WEAPONS = 128. With T4M raising WEAPON
	// pool, indices ≥ 128 OOB into adjacent tables/structs, causing weapons to
	// read each other's data (wrong names, missing ammo, "(NULL)" weapon names),
	// and clobbering adjacent globals in the case of 0x8F44A8 / 0x35D0DC8 — the
	// suspected source of HUD/stance/hitmarker image corruption per
	// plan_weapons_full_detour.md §3.A.
	//
	// 0x8F6770 has an extra alias 0x8F6758 (= base - 0x18) used in 2 sites
	// (sub_451180, sub_451310) where the weapon index is pre-shifted by +6.
	// The 2 new tables (8F44A8, 35D0DC8) have no known shifted alias.
	// =====================================================================
	struct PerWeaponTable {
		DWORD oldBase;
		BYTE** newBasePtr;       // out: where to store the heap allocation
		bool hasAliasMinus18;    // true for 0x8F6770 only
	};

	static BYTE* g_newTable8F6570  = nullptr;
	static BYTE* g_newBgWeaponDefs = nullptr;  // 0x8F6770
	static BYTE* g_newTable8F6970  = nullptr;
	static BYTE* g_newTable8F6B78  = nullptr;
	static BYTE* g_newTable8F6FB0  = nullptr;
	static BYTE* g_newTable8F44A8  = nullptr;  // per-weapon item-slot ptr
	static BYTE* g_newTable35D0DC8 = nullptr;  // per-weapon display name cache

	static PerWeaponTable g_perWeaponTables[] = {
		// All 5 per-weapon tables initialized by sub_41D2A0. With 200+ weapons,
		// every one OOBs into adjacent BSS — and crucially, dword_8F6570 ends
		// EXACTLY at bg_weaponDefs (0x8F6770), so its OOB writes corrupt
		// bg_weaponDefs[0..]. dword_8F6970 ends at the next table, etc.
		// Confirmed crash 2026-05-08 in sub_41CEF0 at offset 0x57:
		//   "mov edx, [edx+410h]" with edx=1 from dword_8F6570[ebx*4] OOB read.
		{ 0x008F6770, &g_newBgWeaponDefs, true  },
		{ 0x008F6570, &g_newTable8F6570,  false },
		{ 0x008F6970, &g_newTable8F6970,  false },
		{ 0x008F6B78, &g_newTable8F6B78,  false },
		{ 0x008F6FB0, &g_newTable8F6FB0,  false },
		// 2 OOB companions (dword_8F44A8, dword_35D0DC8) DISABLED 2026-05-09 —
		// brute-force .text scan caused a false-positive replacement that
		// crashed sub_41D310 stride-0x200 init loop on a vanilla idx<128 map.
		// Re-introduce only with an explicit known-address list (see
		// SetupCgWeaponInfoTable's k_cgWeaponInfoAddrs[] for the pattern), once
		// the actual ref sites have been enumerated from the IDA dump.
		// { 0x008F44A8, &g_newTable8F44A8,  false },
		// { 0x035D0DC8, &g_newTable35D0DC8, false },
	};

	static void SetupBgWeaponDefsTable()
	{
		if (g_newBgWeaponDefs) return;

		const DWORD ENTRY_STRIDE = 0x4;
		const SIZE_T ALLOC_SIZE  = (SIZE_T)NEW_MAX_WEAPONS * ENTRY_STRIDE;

		// 1. Allocate a fresh heap buffer for each table.
		for (auto& t : g_perWeaponTables) {
			BYTE* buf = (BYTE*)VirtualAlloc(NULL, ALLOC_SIZE,
			                                MEM_COMMIT | MEM_RESERVE,
			                                PAGE_READWRITE);
			if (!buf) return;  // partial init is OK — leftover tables stay vanilla
			memset(buf, 0, ALLOC_SIZE);
			*t.newBasePtr = buf;
		}

		// 2. Single .text scan that handles all 5 tables + the one alias.
		const DWORD TEXT_START = 0x00401000U;
		const DWORD TEXT_END   = 0x00800000U;
		DWORD oldProt;
		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
		               PAGE_EXECUTE_READWRITE, &oldProt);

		for (BYTE* bp = (BYTE*)TEXT_START; bp < (BYTE*)(TEXT_END - 3); ++bp) {
			DWORD v = *(DWORD*)bp;
			bool matched = false;
			for (auto& t : g_perWeaponTables) {
				if (v == t.oldBase) {
					*(DWORD*)bp = (DWORD)*t.newBasePtr;
					matched = true;
					break;
				}
				if (t.hasAliasMinus18 && v == t.oldBase - 0x18) {
					*(DWORD*)bp = (DWORD)*t.newBasePtr - 0x18;
					matched = true;
					break;
				}
			}
			(void)matched;
		}
		// Leave .text writable — see SetupCgWeaponInfoTable for rationale.

		// 3. Update T4M-side runtime pointers for hardcoded inline casts.
		::bg_weaponDefs    = (T4::WeaponDef**)g_newBgWeaponDefs;
		// dword_8F44A8 / dword_35D0DC8 are NOT in this brute-scan because they
		// have collateral patches (stride 0x200, shl 7, mask 0x7F) that need
		// coordinated edits — handled by their own dedicated setup functions.
	}

	// =====================================================================
	// dword_8F44A8 — vanilla 16 sub-tables × 128 weapons × 4 bytes = 8 KB struct.
	// (NOT a simple per-weapon table — see analysis/dword_8F44A8_and_35D0DC8_RE.md.)
	// Sub-table 0 = "weapon registered" bitmap (cmp at sub_51A4BD, write at sub_41D310).
	// Sub-tables 1..15 = misc per-weapon flags, written by sub_41D310 init loop with
	// stride 0x200 = 128 entries × 4 bytes per sub-table.
	//
	// To support 512 weapons we extend the layout to 16 × 512 × 4 = 32 KB. The
	// composite encoding becomes `slot*512 + weapon_idx` (was `slot*128 + weapon_idx`).
	// Sites to patch (PHASE A — byte-level immediates):
	//   13 base addresses (`lea/cmp/sub` references to 0x008F44A8) → newBase
	//   2 rep-stosd targets (`mov edi, 0x008F44AC`) → newBase + 4
	//   1 init count (`mov ecx, 0x7FF` in sub_41D2A0) → 0x1FFF
	//   1 init stride (`add eax, 0x200` in sub_41D310 loop) → 0x800
	//   3 composite indexers (`shl reg, 7` in sub_40FAA0, sub_4FDF90, sub_50E3A0) → shl 9
	//
	// PHASE B (function-level detours, NOT in this file): sub_4FD500, sub_4FD590,
	// sub_4FEAB0 reverse-mappers — they sign-extend from 7 bits using `or reg, 0xFFFFFF80`
	// in 3-byte short form `83 CX 80`, which can't expand to 9-bit form `81 CX 00 FE FF FF`
	// in place. Reconstructed in C++ — see PatchT4MAM_Dword8F44A8_Detours.cpp.
	// =====================================================================
	static BYTE* g_newDword8F44A8 = nullptr;

	// 13 base addresses where 0x008F44A8 appears as a 4-byte immediate / disp32.
	// Each VA points at the first byte of the 4-byte field within the instruction.
	// Source : PE binary scan + IDA xref cross-check (audit doc §1.9).
	static const DWORD k_dword8F44A8_BaseSites[] = {
		0x0040FAD1,  // sub_40FAA0 — composite indexer (after shl esi,7)
		0x0041D331,  // sub_41D310 — init loop entry
		0x00464C4B,  // sub_464BF0 (CG_RegisterWeapon) — write itemSlotPtr
		0x004F2B2C,  // sub_4F2B25 — direct subtable_0 lookup
		0x004FD50F,  // sub_4FD500 — reverse-map (Phase B detours this whole fn)
		0x004FD5A0,  // sub_4FD590 — reverse-map (Phase B detours this whole fn)
		0x004FDFEB,  // sub_4FDF90 — composite indexer (after shl ebp,7)
		0x004FEAB1,  // sub_4FEAB0 — reverse-map (Phase B detours this whole fn)
		0x0050E43D,  // sub_50E3A0 — composite indexer
		0x0050E446,  // sub_50E3A0 — pointer→idx (returns composite, no mask)
		0x0051A4E2,  // sub_51A4BD — "Item entity is not a weapon" check
		0x00522D76,  // sub_522D6F — direct subtable_0 lookup
		0x005460F0,  // sub_5460B0 — direct subtable_0 lookup
	};

	// 2 sites with 0x008F44AC immediate (rep stosd target = base + 4).
	static const DWORD k_dword8F44A8_RepStosdSites[] = {
		0x0041D2F6,  // sub_41D2A0 — main BG init zero-pass
		0x0041D3C7,  // sub_41D3A0 — secondary init
	};

	// 3 sites with `shl reg, 7` (composite encoding * 128). Each VA points at the
	// imm8 (third byte of `C1 EX 07` encoding). Patched 0x07 → 0x09 (= shift by 9 = * 512).
	static const DWORD k_dword8F44A8_Shl7Sites[] = {
		0x0040FACB,  // sub_40FAA0 — `shl esi, 7`
		0x004FDFE3,  // sub_4FDF90 — `shl ebp, 7`
		0x0050E437,  // sub_50E3A0 — `shl esi, 7`
	};

	// 3 sites with `and reg, 0x8000007F` (low-7-bit mask + sign bit) paired with
	// the sub-form base patch in reverse-mappers. Each VA points at the imm32.
	// Patched 0x8000007F → 0x800001FF (low 9 bits + sign bit, supports idx 0..511).
	// The follow-on `or reg, 0xFFFFFF80` sign-ext block is DEAD CODE in our setup
	// (composite_idx is always ≥ 0 for ptrs within our buffer → JNS taken → block
	// skipped) so it does NOT need patching despite being encoded in 3-byte short form.
	static const DWORD k_dword8F44A8_AndMaskSites[] = {
		0x004FD517,  // sub_4FD500 — `25 [imm32]` form (and eax)
		0x004FD64B,  // sub_4FD590 — `81 E7 [imm32]` form (and edi)
		0x004FEABE,  // sub_4FEAB0 — `81 E5 [imm32]` form (and ebp)
	};

	// 2 sites with `sar eax, 7` (composite >> 7 = slot extraction in 16x128 layout)
	// in reverse-mappers that need slot indexing. Patched 7 → 9 (= composite >> 9
	// for 16x512 layout). imm8 at +2 of `C1 F8 07`.
	// Note: the prior `and edx, 0x7Fh` is dead code (cdq produces edx=0 for non-
	// negative composite, masking 0 yields 0, no correction needed) — skipped.
	static const DWORD k_dword8F44A8_Sar7Sites[] = {
		0x004FD648,  // sub_4FD590 — slot extract
		0x004FEADF,  // sub_4FEAB0 — slot extract
	};

	static void SetupDword8F44A8Table()
	{
		// 16 sub-tables × NEW_MAX_WEAPONS × 4 bytes = 32 KB for NEW_MAX_WEAPONS=512.
		const SIZE_T ALLOC_SIZE = 16 * (SIZE_T)NEW_MAX_WEAPONS * 4;
		g_newDword8F44A8 = (BYTE*)VirtualAlloc(NULL, ALLOC_SIZE,
		                                       MEM_COMMIT | MEM_RESERVE,
		                                       PAGE_READWRITE);
		if (!g_newDword8F44A8) return;
		memset(g_newDword8F44A8, 0, ALLOC_SIZE);

		const DWORD newBase = (DWORD)g_newDword8F44A8;

		// All immediate patches need .text writable. We unprotect the smallest
		// span covering every site, in one shot, and leave it writable (project
		// convention — see SetupCgWeaponInfoTable).
		const DWORD TEXT_START = 0x00401000U;
		const DWORD TEXT_END   = 0x00800000U;
		DWORD oldProt;
		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
		               PAGE_EXECUTE_READWRITE, &oldProt);

		// (1) 13 base patches — replace 0x008F44A8 immediate with newBase.
		for (DWORD va : k_dword8F44A8_BaseSites) {
			*(DWORD*)va = newBase;
		}

		// (2) 2 rep-stosd target patches — replace 0x008F44AC with newBase + 4.
		for (DWORD va : k_dword8F44A8_RepStosdSites) {
			*(DWORD*)va = newBase + 4;
		}

		// PAUSED 2026-05-11 — dword_8F44A8 9-bit composite encoding patches
		// archived in PatchT4MAM_Wip_WeaponPoolIncreased.cpp. Vanilla 7-bit
		// composite encoding (= sub-tables of 128 entries each) used instead.
		// For NEW_MAX_WEAPONS=148, addressable range stays at 8 KB (vanilla),
		// allocation is 9472 bytes (16 * 148 * 4) — overprovisioned but safe.
		//
		// // (3) Init count — `mov ecx, 0x7FF` → `mov ecx, 0x1FFF` (rep stosd zeros 32 KB - 4).
		// *(DWORD*)0x0041D2F1 = 0x00001FFF;
		//
		// // (4) Init stride — `add eax, 0x200` → `add eax, 0x800` (sub_41D310 loop).
		// *(DWORD*)0x0041D347 = 0x00000800;
		//
		// // (5) 3 composite-encoder shifts — `shl reg, 7` → `shl reg, 9`.
		// for (DWORD va : k_dword8F44A8_Shl7Sites) {
		// 	*(BYTE*)va = 0x09;
		// }
		//
		// // (6) 3 reverse-mapper masks — `and reg, 0x8000007F` → `0x800001FF`.
		// for (DWORD va : k_dword8F44A8_AndMaskSites) {
		// 	*(DWORD*)va = 0x800001FF;
		// }
		//
		// // (7) 2 slot-extract shifts — `sar eax, 7` → `sar eax, 9`.
		// for (DWORD va : k_dword8F44A8_Sar7Sites) {
		// 	*(BYTE*)va = 0x09;
		// }

		// .text intentionally left writable — see SetupCgWeaponInfoTable comment.

		T4M::g_dword8F44A8 = (void**)g_newDword8F44A8;
	}

	// =====================================================================
	// dword_35D0DC8 — per-weapon localized display name cache (vanilla 128 × 4 = 512 B).
	// Two .text sites :
	//   (a) VA 0x00464E52 — writer in sub_464BF0 / CG_RegisterWeapon (loc_464E4F):
	//       89 04 B5 C8 0D 5D 03   mov [esi*4 + 0x035D0DC8], eax
	//   (b) VA 0x004514AF — reader via alias `dword_35D0DB0 = dword_35D0DC8 - 0x18`
	//       in sub_451310 (pickup hint dispatcher) : the alias is paired with
	//       a `weapon_idx + 6` pre-shift pattern (same trick as bg_weaponDefs's
	//       0x8F6758 alias). Without patching this site, the pickup hint reader
	//       targets dead vanilla BSS → no hint shown for picked-up weapons.
	// Adjacent .data at 0x35D0FCC..0x35D0FFC stores HUD material/font handles
	// registered by sub_6621B0 and read by sub_44FC10 (HUD render). At idx≥129
	// the OOB write clobbers those handles → image corruption (HUD/stance/
	// hitmarker show wrong sprites). Reloc target = NEW_MAX_WEAPONS × 4 = 2 KB.
	// See analysis/dword_8F44A8_and_35D0DC8_RE.md §2.
	// =====================================================================
	static void SetupDword35D0DC8Table()
	{
		const SIZE_T ALLOC_SIZE = (SIZE_T)NEW_MAX_WEAPONS * 4;
		BYTE* buf = (BYTE*)VirtualAlloc(NULL, ALLOC_SIZE,
		                                MEM_COMMIT | MEM_RESERVE,
		                                PAGE_READWRITE);
		if (!buf) return;
		memset(buf, 0, ALLOC_SIZE);

		const DWORD newBase = (DWORD)buf;

		// Two .text sites covering the table base + the -0x18 alias.
		const DWORD TEXT_START = 0x00401000U;
		const DWORD TEXT_END   = 0x00800000U;
		DWORD oldProt;
		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
		               PAGE_EXECUTE_READWRITE, &oldProt);

		// (a) Writer site 0x00464E52 — patch base address.
		*(DWORD*)0x00464E52 = newBase;
		// (b) Alias reader site 0x004514AF — patch with newBase - 0x18 so that
		//     `dword_35D0DB0_alias[(idx+6)*4]` lands at newBase + idx*4.
		*(DWORD*)0x004514AF = newBase - 0x18;
		// Leave .text writable — see SetupCgWeaponInfoTable for rationale.

		T4M::g_dword35D0DC8 = (void**)buf;
	}

	static SafetyHookMid GEntityPool_hook;

	static void PatchT4_GEntityPool()
	{
		// Hook at 0x62B969: first instruction after "call sub_5AA5F0; add esp, 0Ch"
		// in sub_62B7C0 (sub_62B7C0+1A1 calls sub_5AA5F0, +1A6 is add esp; +1A9 is here).
		GEntityPool_hook = safetyhook::create_mid(0x62B969, [](SafetyHookContext&) {
			if (!g_newEntityPool) {
				SetupEntityPool(); // allocate + patch .text (one-shot)
			}
			// Always restore dword_18F5D8C — sub_502020 resets it to 0x176C6F0 each map load.
			if (g_newEntityPool) {
				*(DWORD*)ENTITY_BASE_DWORD = (DWORD)g_newEntityPool;
			}
		});
	}
}
	void PatchT4_MemoryLimits()
	{
		// increase pool sizes to similar (or greater) t5 sizes.
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_FX, 2048);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_IMAGE, 8192);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_LOADED_SOUND, 4096);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_MATERIAL, 4096);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_WEAPON, NEW_MAX_WEAPONS);
		// CG-side per-weapon table (cg_weaponInfo[128] vanilla) must match the
		// raised WEAPON pool size or sub_465200's iterator OOBs into adjacent
		// dvar registration globals at 0x3466040+. See SetupCgWeaponInfoTable().
		T4M::SetupCgWeaponInfoTable();
		// dword_35D0DC8 — per-weapon localized display name cache (1 .text site).
		// Adjacent HUD material handles get clobbered for idx≥129 → image
		// corruption. Trivial single-immediate patch.
		T4M::SetupDword35D0DC8Table();
		// dword_8F44A8 — vanilla 8 KB struct (16 sub-tables × 128 × 4) extended
		// to 32 KB (16 × 512 × 4). 25 byte-level patches in-place — see
		// SetupDword8F44A8Table() and analysis/dword_8F44A8_and_35D0DC8_RE.md.
		T4M::SetupDword8F44A8Table();
		// bg_weaponDefs[128] vanilla — pointer table read by sub_402940 etc.
		// Must match NEW_MAX_WEAPONS too. See SetupBgWeaponDefsTable().
		T4M::SetupBgWeaponDefsTable();
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_XMODEL, 4096);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_RAWFILE, 2048);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_PHYSCONSTRAINTS, 256);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_PHYSPRESET, 256);
		T4M::DB_ReallocXAssetPool(ASSET_TYPE_XMODELPIECES, 256);

		// change the size of g_mem from 0x12C00000 to 0x19600000, UGX-Mod v1.1 is pretty fucking huge
		// had to increase due to it crashing in Com_BeginParseSession
		*(DWORD*)0x5F5492 = 0x40000000; //0x14800000
		*(DWORD*)0x5F54D1 = 0x40000000; //0x14800000
		*(DWORD*)0x5F54DB = 0x40000000; //0x14800000

		//*(DWORD*)0x5F5492 = 0x26100000; //0x14800000
		//*(DWORD*)0x5F54D1 = 0x26100000; //0x14800000
		//*(DWORD*)0x5F54DB = 0x26100000; //0x14800000

		// change the num of entities available to be spawned in G_Spawn from 1022 to 1500
		// still a W.I.P. is missing array and hash table(?) changes
		//PatchMemory(0x0054EAC3, (PBYTE)"\xDC\x05", 2);

		// =====================================================================
		// Increase XASSET_ENTRY_POOL_SIZE from 32767 to 65535 (max uint16)
		//
		// The vanilla g_assetEntryPool at 0xA51C50 is a static 32767-entry pool
		// (0x80000 bytes) shared by ALL asset types. T4M's DB_ReallocXAssetPool
		// increases per-type pools but not this global pool, causing crashes
		// ("Could not allocate asset") when total loaded assets exceed ~32K.
		//
		// Cannot extend in-place: g_usageFrame (dword_AD1C40) sits right at
		// pool_base + 0x7FFF0, and other variables follow immediately after.
		// Must allocate a new pool and patch all 24 code references.
		// =====================================================================

		// Allocate new pool (persists for lifetime of process)
		static XAssetEntryPoolEntry* newPool = (XAssetEntryPoolEntry*)VirtualAlloc(
			NULL,
			NEW_ASSET_ENTRY_POOL_SIZE * sizeof(XAssetEntryPoolEntry),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!newPool) {
			T4::Com_Printf(0, "^1ERROR: Failed to allocate expanded asset entry pool\n");
			return;
		}

		DWORD newPoolAddr = (DWORD)newPool;
		DWORD newPoolAddr10 = (DWORD)&newPool[1]; // pool + 0x10 (unk_A51C60 equivalent)

		// Update T4M's C pointer so DB_ListAssetPool and other T4M code use the new pool
		T4::g_assetEntryPool = newPool;

		// Unprotect .text section pages covering all patch addresses (0x48D340 – 0x48FA30)
		DWORD oldProtect;
		VirtualProtect((LPVOID)0x48D340, 0x48FA30 - 0x48D340, PAGE_EXECUTE_READWRITE, &oldProtect);

		// All addresses below verified by scanning the binary for byte patterns
		// 0x00A51C50 (LE: 50 1C A5 00) and 0x00A51C60 (LE: 60 1C A5 00).
		// Encoding rules:
		//   add eax, imm32 = 05 [imm32]       → immediate at instr+1
		//   add esi, imm32 = 81 C6 [imm32]    → immediate at instr+2
		//   add edi, imm32 = 81 C7 [imm32]    → immediate at instr+2
		//   sub edx, imm32 = 81 EA [imm32]    → immediate at instr+2
		//   sub edi, imm32 = 81 EF [imm32]    → immediate at instr+2
		//   lea reg,[reg+imm32] = 8D XX [imm32] → immediate at instr+2
		//   mov [reg+imm32],reg = 89 XX [imm32] → immediate at instr+2
		//   cmp eax, imm32 = 3D [imm32]       → immediate at instr+1
		//   mov [imm32],imm32 = C7 05 [a4][v4] → value at instr+6

		// ---- sub_48D340 (DB_InitAssetEntryPool) ----
		// C7 05 84 78 95 00 [60 1C A5 00] → mov dword_957884, offset unk_A51C60
		*(DWORD*)0x48D371 = newPoolAddr10;
		// 8D 88 [60 1C A5 00] → lea ecx, [eax + unk_A51C60]
		*(DWORD*)0x48D382 = newPoolAddr10;
		// 89 88 [50 1C A5 00] → mov [eax + dword_A51C50], ecx
		*(DWORD*)0x48D388 = newPoolAddr;
		// 3D [F0 FF 07 00] → cmp eax, 7FFF0h  →  change limit to 0xFFFF0
		*(DWORD*)0x48D390 = NEW_ASSET_ENTRY_POOL_SIZE * 0x10; // 65535 * 0x10 = 0xFFFF0

		// ---- sub_48D560 (DB_EnumXAssets) ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48D5B8 = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48D5E5 = newPoolAddr;

		// ---- sub_48D760 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48D784 = newPoolAddr;

		// ---- sub_48D7D0 ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48D7F5 = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48D848 = newPoolAddr;

		// ---- sub_48D860 (DB_AddXAsset / link entry) ----
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)0x48D90F = newPoolAddr;

		// ---- sub_48DEA0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48DEF4 = newPoolAddr;

		// ---- sub_48DFB0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48DFB4 = newPoolAddr;

		// ---- sub_48DFF0 (DB_UnloadXAssets) ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48E059 = newPoolAddr;
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)0x48E115 = newPoolAddr;
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48E1C2 = newPoolAddr;
		// 81 EF [50 1C A5 00] → sub edi, offset dword_A51C50
		*(DWORD*)0x48E1E8 = newPoolAddr;
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)0x48E292 = newPoolAddr;

		// ---- sub_48E370 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48E3A6 = newPoolAddr;

		// ---- sub_48F340 (DB_PostLoadXZone) ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48F378 = newPoolAddr;
		// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
		*(DWORD*)0x48F4D4 = newPoolAddr;
		// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
		*(DWORD*)0x48F558 = newPoolAddr;

		// ---- sub_48F670 ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48F68C = newPoolAddr;

		// ---- sub_48F6E0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48F704 = newPoolAddr;

		// ---- sub_48F9B0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)0x48F9D4 = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)0x48FA28 = newPoolAddr;

		// =====================================================================
		// Fix renderer image sort array overflow
		//
		// R_LoadWorld and sub_719F40 call sub_48DF60 (DB_EnumXAssets_FastFile_Array)
		// to fill dword_3BF1880[] with image asset headers, then sort them.
		// The array is hardcoded for 0x800 (2048) entries, but T4M increases the
		// image pool to 8192. sub_48DF60 has NO bounds check — it writes ALL
		// matching assets, overflowing the buffer and corrupting:
		//   - dword_3BF3884 (image count, at array + 0x2004)
		//   - dword_3BF392C (scene structure pointer, at array + 0x20AC)
		// This causes both observed crashes:
		//   0x719A2E: sort comparator gets garbage → access violation
		//   0x491500: corrupted scene pointer → bitfield access violation
		//
		// Fix: allocate a larger buffer (8192 entries) and patch all 21 references.
		// =====================================================================


		// Allocate new image sort buffer + count variable (contiguous)
		static DWORD* newImageBuffer = (DWORD*)VirtualAlloc(
			NULL,
			NEW_IMAGE_SORT_BUFFER_SIZE * sizeof(DWORD) + sizeof(DWORD), // array + count
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!newImageBuffer) {
			T4::Com_Printf(0, "^1ERROR: Failed to allocate expanded image sort buffer\n");
			return;
		}

		DWORD newImgBufAddr = (DWORD)newImageBuffer;
		// Place the count variable right after the array, mirroring original layout
		// Original: array at 0x3BF1880, count at 0x3BF3884 (offset +0x2004 from array, but
		// we just need a separate DWORD for the count — any stable address works)
		DWORD* newImageCount = &newImageBuffer[NEW_IMAGE_SORT_BUFFER_SIZE];
		DWORD newImgCntAddr = (DWORD)newImageCount;

		// Unprotect renderer .text pages covering patch addresses
		// Range: 0x6D69EB to 0x74200E+4
		DWORD oldProtect2;
		VirtualProtect((LPVOID)0x6D69E0, 0x742012 - 0x6D69E0, PAGE_EXECUTE_READWRITE, &oldProtect2);

		// --- Patch 12 references to dword_3BF1880 (image sort array) ---
		// Binary scan found pattern 80 18 BF 03 at the exact VA of the immediate.
		// Patch address = VA directly (no offset needed).

		// 68 [80 18 BF 03] → push offset dword_3BF1880
		*(DWORD*)0x6D69EB = newImgBufAddr;
		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)0x6DC964 = newImgBufAddr;
		// 8B 14 95 [80 18 BF 03] → mov edx, dword_3BF1880[edx*4]
		*(DWORD*)0x6DCA8C = newImgBufAddr;
		// 89 0C 85 [80 18 BF 03] → mov dword_3BF1880[eax*4], ecx
		*(DWORD*)0x6E993D = newImgBufAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld)
		*(DWORD*)0x705784 = newImgBufAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld sort call)
		*(DWORD*)0x70579F = newImgBufAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40)
		*(DWORD*)0x719F52 = newImgBufAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40 sort call)
		*(DWORD*)0x719F6D = newImgBufAddr;
		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)0x741C11 = newImgBufAddr;
		// 8B 1C 85 [80 18 BF 03] → mov ebx, dword_3BF1880[eax*4]
		*(DWORD*)0x741C98 = newImgBufAddr;
		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)0x741EB7 = newImgBufAddr;
		// 8B 2C 85 [80 18 BF 03] → mov ebp, dword_3BF1880[eax*4]
		*(DWORD*)0x74200E = newImgBufAddr;

		// --- Patch 9 references to dword_3BF3884 (image count) ---
		// Binary scan found pattern 84 38 BF 03 at the exact VA of the immediate.

		// A1 [84 38 BF 03] → mov eax, dword_3BF3884
		*(DWORD*)0x6E990F = newImgCntAddr;
		// A1 [84 38 BF 03] → mov eax, dword_3BF3884
		*(DWORD*)0x6E9936 = newImgCntAddr;
		// A1 [84 38 BF 03] → mov eax, dword_3BF3884
		*(DWORD*)0x6E9942 = newImgCntAddr;
		// A3 [84 38 BF 03] → mov dword_3BF3884, eax
		*(DWORD*)0x6E995A = newImgCntAddr;
		// C7 05 [84 38 BF 03] 00000000 → mov dword_3BF3884, 0
		*(DWORD*)0x6E9D3C = newImgCntAddr;
		// A3 [84 38 BF 03] → mov dword_3BF3884, eax  (R_LoadWorld)
		*(DWORD*)0x705793 = newImgCntAddr;
		// 8B 0D [84 38 BF 03] → mov ecx, dword_3BF3884
		*(DWORD*)0x705799 = newImgCntAddr;
		// A3 [84 38 BF 03] → mov dword_3BF3884, eax  (sub_719F40)
		*(DWORD*)0x719F61 = newImgCntAddr;
		// 8B 0D [84 38 BF 03] → mov ecx, dword_3BF3884
		*(DWORD*)0x719F67 = newImgCntAddr;

		// Also patch the max count passed to sub_48DF60 (0x800 → 0x2000)
		// At 0x70577F: push 800h → change immediate to 8192
		// At 0x719F4D: push 800h → change immediate to 8192
		// These are ignored by the function, but patch them for correctness

		// =====================================================================
		// G_Spawn entity limit increase
		//
		// G_Spawn (sub_54EAB0) at loc_54EAF2 allocates new entities as:
		//   entity_ptr = dword_18F5D8C + numGEntities * 0x378
		// The check "cmp ecx, 3FEh; jnz" at 0x54EAC1 errors when numGEntities
		// reaches exactly 1022, capping usable entity indices at 0-1021.
		//
		// This patch raises the limit to NEW_MAX_GENTITIES - 2.
		// The actual entity array relocation (VirtualAlloc + .text scan + dword_18F5D8C
		// redirect) is handled by PatchT4_GEntityPool() / SetupEntityPool() above,
		// which fires on the first map load via a hook at 0x62B969.
		// =====================================================================

		// Mid-hook at 0x54EAC1 replaces the "cmp ecx, 3FEh; jnz loc_54EAF2" pair entirely.
		// At hook point: ecx = numGEntities (set by "mov ecx, dword_18F5D94" just before).
		// Original behaviour: error when ecx == 0x3FE (1022), allocate otherwise.
		// New behaviour: error when ecx >= NEW_MAX_GENTITIES - 2, allocate otherwise.
		//
		// Entity array note: dword_18F5D8C (runtime entity-base ptr) currently points to the
		// 1024-entry BSS array at 0x176C6F0. Entities at index >= 1024 will land past that
		// array until a full pool relocation (VirtualAlloc + ~498 .text patches) is done.
		//
		// Hash table note: dword_18F7910 and dword_18F794C (BSS, 0x18F7910 / 0x18F794C) are
		// set to 0x3FF (1023) at init by sub_502020. They act as sentinel/comparison values
		// in sub_54DDC0 and sub_54EDC0 for entity hash-slot tracking. When entity index 1023
		// is freed those functions compare against 0x3FF and perform a no-op reset — harmless.
		// These do not need updating for the spawn-limit increase alone.
		/*
		static auto GSpawn_limit_hook = safetyhook::create_mid(0x54EAC1, [](SafetyHookContext& ctx) {
			// ctx.ecx = numGEntities. Redirect eip to skip or enter the error path.
			if (ctx.ecx < (DWORD)(NEW_MAX_GENTITIES - 2)) {
				ctx.eip = 0x54EAF2; // allocation path (loc_54EAF2)
			} else {
				ctx.eip = 0x54EAC9; // error path ("G_Spawn: no free entities")
			}
		});
		Com_Printf(0, "[T4M] G_Spawn limit patched to %d (mid-hook)\n", NEW_MAX_GENTITIES - 2);
		T4M::PatchT4_GEntityPool();
		*/
		// =====================================================================
		// Localized string hash table limit increase
		//
		// sub_54A1A0 (generic asset lookup) uses a 1023-entry WORD hash table
		// at BSS address 0x2350428. The table address is encoded as a 4-byte
		// immediate in a single lea instruction at 0x54A20C, and the lookup
		// bound 0x3FF (1023) appears as push imm32 immediates at 0x54A2F3 and
		// 0x54A330. Redirecting all three gives the new limit.
		// =====================================================================

		/*
		static WORD* newStringTable = (WORD*)VirtualAlloc(
			NULL,
			(DWORD)NEW_MAX_LOCALIZED * sizeof(WORD),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (newStringTable) {
			DWORD newStrBase = (DWORD)newStringTable;

			// Patch "lea ecx, ds:2350428h[ecx*2]" → redirect to new table
			// Encoding: 8D 0C 4D [imm32] at 0x54A209; address immediate at 0x54A20C
			*(DWORD*)0x54A20C = newStrBase;

			// Patch first "push 3FFh" (limit arg to sub_54A1A0)
			// Encoding: 68 [imm32]; immediate at 0x54A2F3
			*(DWORD*)0x54A2F3 = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

			// Patch second "push 3FFh" (limit arg to sub_54A1A0)
			// Encoding: 68 [imm32]; immediate at 0x54A330
			*(DWORD*)0x54A330 = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

			Com_Printf(0, "[T4M] Localized string table expanded: new limit=%d\n",
					   NEW_MAX_LOCALIZED - 1);
		} else {
			Com_Printf(0, "^1ERROR: Failed to allocate expanded localized string table\n");
		}
			*/
	}