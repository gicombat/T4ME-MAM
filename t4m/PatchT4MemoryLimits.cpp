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
#define NEW_SORTED_MATERIALS_SIZE 8192   // >= ASSET_TYPE_MATERIAL pool size (4096)
#define NEW_MAX_GENTITIES   2048
#define ENTITY_SIZE         0x378        // 888 bytes per gentity_s
#define OLD_ENTITY_BASE     T4M::GetAddress("g_entities")
#define GSPAWN_CMP_IMM      T4M::GetAddress("GSpawn_cmp_patch")   // immediate of "cmp ecx, 3FEh" in G_Spawn
#define ENTITY_BASE_DWORD   T4M::GetAddress("g_entityBasePtr")  // dword_18F5D8C: runtime entity-base ptr
#define NEW_MAX_LOCALIZED   4096

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
			T4::engine::Com_Printf(0, "^1[T4M] FATAL: entity pool VirtualAlloc failed\n");
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
			} else if (v == T4M::GetAddress("entityArray_end") || v == T4M::GetAddress("entityInit_loopBound")) {
				*(DWORD*)bp = newEnd;   ++patched;
			}
		}

		VirtualProtect((LPVOID)TEXT_START, TEXT_END - TEXT_START,
					   PAGE_EXECUTE_READ, &oldProt);

		T4::engine::Com_Printf(0, "[T4M] Entity pool: base=0x%08X, %d refs patched\n",
				   newBase, patched);
	}

	static SafetyHookMid GEntityPool_hook;

	static void PatchT4_GEntityPool()
	{
		// Hook at 0x62B969: first instruction after "call sub_5AA5F0; add esp, 0Ch"
		// in sub_62B7C0 (sub_62B7C0+1A1 calls sub_5AA5F0, +1A6 is add esp; +1A9 is here).
		GEntityPool_hook = safetyhook::create_mid(T4M::GetAddress("GEntityPool_hook"), [](SafetyHookContext&) {
			if (!g_newEntityPool) {
				SetupEntityPool(); // allocate + patch .text (one-shot)
			}
			// Always restore dword_18F5D8C — sub_502020 resets it to 0x176C6F0 each map load.
			if (g_newEntityPool) {
				*(DWORD*)ENTITY_BASE_DWORD = (DWORD)g_newEntityPool;
			}
		});
	}

	// =====================================================================
	// Relocation verifier — @new
	//
	// Every relocation below rewrites a fixed set of immediates in .text.
	// Nothing used to prove the set was complete, which is how the free-list
	// terminator at 0x48D398 stayed unpatched (it only worked because
	// VirtualAlloc zero-fills). This counts occurrences of each pre-relocation
	// value in .text before and after patching: the count must drop by exactly
	// the number of sites we claim to rewrite.
	//
	// Counting the delta rather than requiring zero survivors is deliberate —
	// some references legitimately remain (0x6D69EB still memsets the real rgp)
	// and .text also yields incidental 4-byte windows straddling instructions.
	// Those are identical in both passes and cancel out.
	// =====================================================================
	struct RelocCheck
	{
		const char* label;
		DWORD       oldValue;
		int         expectedSites;   // immediates PatchT4_MemoryLimits rewrites
		int         expectedBefore;  // occurrences in a pristine .text, counted on the exe
		int         before;
		int         after;
	};

	static RelocCheck g_relocChecks[4];
	static bool       g_relocScanned = false;

	// Real .text bounds from the PE headers — the old hardcoded 0x401000-0x800000
	// overruns into .rdata (.text actually ends at 0x7EB000).
	static void GetTextSection(BYTE** start, size_t* size)
	{
		*start = nullptr; *size = 0;
		BYTE* base = (BYTE*)GetModuleHandleA(nullptr);
		IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
		IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
		for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
			if (memcmp(sec[i].Name, ".text", 5) == 0) {
				*start = base + sec[i].VirtualAddress;
				*size  = sec[i].Misc.VirtualSize;
				return;
			}
		}
	}

	// One pass over .text for all four values at once — this runs twice during
	// init, and .text is ~4 MB.
	static void CountInText(int* counts)
	{
		for (int i = 0; i < 4; ++i)
			counts[i] = -1;

		BYTE* start; size_t size;
		GetTextSection(&start, &size);
		if (!start || size < 4)
			return;

		int n[4] = { 0, 0, 0, 0 };
		const DWORD v0 = g_relocChecks[0].oldValue, v1 = g_relocChecks[1].oldValue;
		const DWORD v2 = g_relocChecks[2].oldValue, v3 = g_relocChecks[3].oldValue;

		for (BYTE* p = start; p <= start + size - 4; ++p) {
			const DWORD v = *(DWORD*)p;
			if (v == v0) ++n[0];
			else if (v == v1) ++n[1];
			else if (v == v2) ++n[2];
			else if (v == v3) ++n[3];
		}

		for (int i = 0; i < 4; ++i)
			counts[i] = n[i];
	}

	static void RelocScanBefore()
	{
		const DWORD pool = (DWORD)T4M::GetAddress("g_assetEntryPool");
		const DWORD rgp  = (DWORD)T4M::GetAddress("rgp");

		// expectedBefore = occurrences counted in a pristine .text of the shipped
		// exe. Checking the delta alone is not enough: if something modified .text
		// before we scanned, the delta could still look right while the baseline
		// is wrong, and the check would pass on a false premise.
		g_relocChecks[0] = { "g_assetEntryPool",            pool,             22, 22, 0, 0 };
		g_relocChecks[1] = { "g_assetEntryPool[1]",         pool + 0x10,       2,  2, 0, 0 };
		g_relocChecks[2] = { "g_assetEntryPool free-list terminator", pool + 0x7FFF0, 1, 1, 0, 0 };
		// 12 = the 11 we rewrite + the 0x6D69EB memset we deliberately leave alone.
		g_relocChecks[3] = { "rgp.sortedMaterials",         rgp,              11, 12, 0, 0 };

		int counts[4];
		CountInText(counts);
		for (int i = 0; i < 4; ++i)
			g_relocChecks[i].before = counts[i];
	}

	// Init-safe: no Com_Printf here (PatchT4_* runs before the console exists).
	static void RelocScanAfter()
	{
		int counts[4];
		CountInText(counts);
		for (int i = 0; i < 4; ++i)
			g_relocChecks[i].after = counts[i];
		g_relocScanned = true;

		for (const RelocCheck& c : g_relocChecks) {
			if (c.before < 0 || c.before - c.after == c.expectedSites)
				continue;
			char msg[256];
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"[T4M reloc] %s (0x%08X): expected %d sites, %d disappeared (%d -> %d)\n",
				c.label, c.oldValue, c.expectedSites, c.before - c.after, c.before, c.after);
			OutputDebugStringA(msg);
		}
	}

	// Relocated rgp.sortedMaterials — owned by PatchT4_MemoryLimits, cleared by
	// the R_InitGlobalStructs reconstruction below.
	static DWORD* g_sortedMaterials = nullptr;

	// Highest rgp.materialCount seen so far. Sampled at the per-map material
	// reset (sub_6E9D10, "mov dword_3BF3884, 0" at 0x6E9D3A) — the last moment
	// the outgoing map's count is still readable. Answers "does anything real
	// exceed 2048 materials?" without hooking the draw path.
	//
	// NOT sampled in R_InitGlobalStructs: that one runs from R_Init (renderer
	// startup / vid_restart), not per map, so it would almost always read 0.
	static DWORD g_materialCountPeak = 0;

	// rgp + 0x2004 (dword_3BF3884). Derived, so it follows the `rgp` CSV key.
	static const DWORD kRgpMaterialCountOffset = 0x2004;

	static DWORD CurrentMaterialCount()
	{
		return *(DWORD*)((BYTE*)T4::engine::rgp + kRgpMaterialCountOffset);
	}

	// =====================================================================
	// P2 — `validateassetpool`: walk the XAssetEntry free list.
	//
	// The list is built by DB_Init and consumed/returned by DB_LinkXAssetEntry
	// and DB_UnloadZoneAssets. A relocation bug shows up here long before it
	// shows up as a crash: a node pointing outside the pool, a misaligned node,
	// or a list that never terminates.
	// =====================================================================
	void ValidateAssetEntryPool()
	{
		XAssetEntryPoolEntry* pool = T4::engine::g_assetEntryPool;
		const size_t capacity = NEW_ASSET_ENTRY_POOL_SIZE + 1;
		XAssetEntryPoolEntry* poolEnd = pool + capacity;

		XAssetEntryPoolEntry* node = *T4::engine::g_freeAssetEntries;
		size_t   count      = 0;
		int      outOfPool  = 0;
		int      misaligned = 0;
		bool     looped     = false;
		// An empty list is a legitimate terminal state: the pool is exhausted.
		// That is exactly the condition this command exists to diagnose, so it
		// must not be reported as corruption.
		bool     terminated = (node == nullptr);

		// Bounded by capacity: a cycle cannot produce more steps than nodes.
		while (node)
		{
			if (node < pool || node >= poolEnd) { ++outOfPool; break; }
			if (((BYTE*)node - (BYTE*)pool) % sizeof(XAssetEntryPoolEntry)) { ++misaligned; break; }
			if (++count > capacity) { looped = true; break; }
			node = node->next;
			if (!node) terminated = true;
		}

		T4::engine::Com_Printf(0, "[T4M] asset entry pool 0x%08X, %u entries of 0x%X\n",
			(DWORD)pool, (unsigned)capacity, (unsigned)sizeof(XAssetEntryPoolEntry));

		if (terminated && count == 0)
			T4::engine::Com_Printf(0, "^3  free list: empty — the pool is exhausted\n");
		else
			T4::engine::Com_Printf(0, "  free list: %u nodes, %s\n", (unsigned)count,
				terminated ? "^2terminated^7" : "^1NOT terminated^7");

		if (outOfPool)
			T4::engine::Com_Printf(0, "^1  node outside the pool after %u steps\n", (unsigned)count);
		if (misaligned)
			T4::engine::Com_Printf(0, "^1  misaligned node after %u steps\n", (unsigned)count);
		if (looped)
			T4::engine::Com_Printf(0, "^1  walk exceeded pool capacity — the list loops\n");
	}

	// =====================================================================
	// P4 — `listassetcaps`: requested pool size vs the cap actually reachable.
	//
	// DB_ReallocXAssetPool sizes a pool, but for some types a second limit binds
	// first and the extra entries are unreachable. Only types with a known
	// tighter limit carry a note; for the rest requested == effective.
	// =====================================================================
	void ListAssetCaps()
	{
		// effective == 0 means "known to be lower than requested, exact value
		// not established" — do not guess one here.
		struct KnownCap { int type; unsigned effective; const char* cause; };
		static const KnownCap knownCaps[] = {
			{ T4::engine::ASSET_TYPE_MATERIAL, 2048,
			  "Material_SetSortedIndex (sub_6E9900) packs the index in 11 bits" },
			{ T4::engine::ASSET_TYPE_WEAPON, 0,
			  "pool expansion paused; the binding limit is the weapon arrays, not this pool" },
		};

		// "elem size" = DB_GetXAssetTypeSize(type), i.e. sizeof the asset struct
		// (this is where the patched 0x9DC weapon stride shows up).
		T4::engine::Com_Printf(0, "%-24s %8s %9s %11s  %s\n",
			"asset type", "entries", "elem size", "pool bytes", "effective cap");

		for (int type = 0; type < T4::engine::ASSET_TYPE_MAX; ++type)
		{
			const unsigned requested = T4::engine::g_poolSize[type];
			if (!requested)
				continue;

			const int elemSize = T4M::DB_GetXAssetTypeSize(type);
			const KnownCap* cap = nullptr;
			for (const KnownCap& k : knownCaps)
				if (k.type == type) { cap = &k; break; }

			T4::engine::Com_Printf(0, "%-24s %8u %9d %11u  ",
				T4M::DB_GetXAssetTypeName(type), requested, elemSize,
				requested * (unsigned)elemSize);

			if (cap && cap->effective == 0)
				T4::engine::Com_Printf(0, "^3unknown^7 — %s\n", cap->cause);
			else if (cap && cap->effective < requested)
				T4::engine::Com_Printf(0, "^3%u^7 — %s\n", cap->effective, cap->cause);
			else
				T4::engine::Com_Printf(0, "^2%u^7\n", requested);
		}

		T4::engine::Com_Printf(0, "shared XAssetEntry pool: %u entries\n",
			(unsigned)NEW_ASSET_ENTRY_POOL_SIZE);

		// The measurement F4 is waiting on: is 2048 actually reached in practice?
		// live  = the map currently loaded; peak = highest seen since startup,
		// sampled by the R_InitGlobalStructs detour just before it clears rgp.
		// Fold the live count into the peak before reporting. The peak hook only
		// fires on the per-map material reset, and sub_6D6B80 guards that on
		// byte_3BF6961 — so on the first map since launch it has never fired and
		// a bare "peak 0" next to a live 1951 reads as a bug.
		const DWORD live = T4M::CurrentMaterialCount();
		if (live > T4M::g_materialCountPeak)
			T4M::g_materialCountPeak = live;
		const DWORD peak = T4M::g_materialCountPeak;

		T4::engine::Com_Printf(0, "rgp.materialCount: live %u, peak %u, addressable %u\n",
			live, peak, 2048u);
		T4::engine::Com_Printf(0,
			"  (materials registered with the renderer, one sorted index each --\n"
			"   NOT the same population as `listassetcounts`, which counts pool\n"
			"   entries and counts an overridden name once per version)\n");

		const DWORD high = peak;
		if (high > 2048)
			T4::engine::Com_Printf(0,
				"^1  OVER THE 11-BIT INDEX CEILING by %u — sorted indices wrap silently\n", high - 2048);
		else if (high > 1843)   // 90%
			T4::engine::Com_Printf(0, "^3  within 10%% of the ceiling\n");
		else
			T4::engine::Com_Printf(0, "^2  clear of the ceiling\n");
	}

	// Console command backing (`verifyrelocs`) — runtime, console is up here.
	void VerifyRelocations()
	{
		if (!g_relocScanned) {
			T4::engine::Com_Printf(0, "^3[T4M] relocation scan never ran\n");
			return;
		}

		BYTE* start; size_t size;
		GetTextSection(&start, &size);
		T4::engine::Com_Printf(0, "[T4M] relocation check (.text 0x%08X + 0x%X)\n", (DWORD)start, (DWORD)size);

		for (const RelocCheck& c : g_relocChecks) {
			const int gone = c.before - c.after;
			const bool ok  = (gone == c.expectedSites) && (c.before == c.expectedBefore);
			T4::engine::Com_Printf(0, "%s  %-40s 0x%08X  %2d/%2d rewritten, %d left\n",
				ok ? "^2 OK ^7" : "^1FAIL^7", c.label, c.oldValue, gone, c.expectedSites, c.after);
			if (c.before != c.expectedBefore)
				T4::engine::Com_Printf(0,
					"^1      baseline %d, expected %d — .text was already modified before the scan\n",
					c.before, c.expectedBefore);
		}
	}
}
// =====================================================================
// T4_Reconstructed::R_InitGlobalStructs — sub_6D69D0
// @modified — 1:1 with vanilla (CoD4 gfx_d3d/r_init.cpp:4176) plus the
//   sortedMaterials clear, which vanilla got for free from the rgp memset
//   until T4M relocated that array out of rgp.
//   DETOURED — do not call directly.
// =====================================================================
void T4_Reconstructed::R_InitGlobalStructs()
{
	BYTE* rg  = T4::engine::rg;
	BYTE* rgp = T4::engine::rgp;

	T4::engine::Mem_Memset(rg,  0, 0x2430);   // sizeof(rg)
	T4::engine::Mem_Memset(rgp, 0, 0x2280);   // sizeof(rgp)

	// @modified — rgp.sortedMaterials[] was relocated out of rgp, so the memset
	// above no longer reaches it. Without this the array keeps stale Material*
	// across a renderer reset and the sort comparator faults on freed pointers.
	if (T4M::g_sortedMaterials)
		T4::engine::Mem_Memset(T4M::g_sortedMaterials, 0, NEW_SORTED_MATERIALS_SIZE * sizeof(DWORD));

	// WaW-only block, no CoD4 equivalent: two qwords cleared, one copied.
	DWORD* src = T4::engine::r_globals_3BED830;
	DWORD* a   = T4::engine::r_globals_3DCB4D0;
	DWORD* b   = T4::engine::r_globals_463E3C8;
	a[0] = 0;      a[1] = 0;        // qword_3DCB4D0
	a[2] = 0;      a[3] = 0;        // qword_3DCB4D8
	b[0] = 0;      b[1] = 0;        // qword_463E3C8
	b[2] = src[0]; b[3] = src[1];   // qword_463E3D0 = qword_3BED830

	T4::engine::RB_InitBackendGlobalStructs();

	// rg.identityPlacement: quat = {0,0,0,1}, origin = {0,0,0}, scale = 1.0
	float* placement = T4::engine::rg_identityPlacement;
	placement[0] = 0.0f; placement[1] = 0.0f; placement[2] = 0.0f; placement[3] = 1.0f;
	placement[4] = 0.0f; placement[5] = 0.0f; placement[6] = 0.0f; placement[7] = 1.0f;

	// rg.identityViewParms: view / projection / viewProjection / inverseViewProjection
	const float* identity = T4::engine::identityMatrix44;
	for (int i = 0; i < 4; ++i)
		memcpy(rg + i * 0x40, identity, 0x40);
}

	void PatchT4_MemoryLimits()
	{
		// Baseline occurrence counts, taken before any immediate is rewritten.
		T4M::RelocScanBefore();

		// increase pool sizes to similar (or greater) t5 sizes.
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_FX, 2048);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_IMAGE, 8192);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_LOADED_SOUND, 4096);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_MATERIAL, 4096);
		// WEAPON: the 256 buys no usable weapon slots on its own — the count is
		// capped by the paused pool-expansion work, not by this pool. What the
		// call is actually for is the realloc itself, which re-sizes the pool at
		// the patched 0x9DC stride (PatchT4MAM_WeaponDef.cpp).
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_WEAPON, 256);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_XMODEL, 4096);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_RAWFILE, 2048);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_PHYSCONSTRAINTS, 256);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_PHYSPRESET, 256);
		T4M::DB_ReallocXAssetPool(T4::engine::ASSET_TYPE_XMODELPIECES, 256);

		// change the size of g_mem from 0x12C00000 to 0x19600000, UGX-Mod v1.1 is pretty fucking huge
		// had to increase due to it crashing in Com_BeginParseSession
		*(DWORD*)T4M::GetAddress("g_mem_5F5492") = 0x40000000; //0x14800000
		*(DWORD*)T4M::GetAddress("g_mem_5F54D1") = 0x40000000; //0x14800000
		*(DWORD*)T4M::GetAddress("g_mem_5F54DB") = 0x40000000; //0x14800000

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
		// Cannot extend in-place: dword_AD1C40 (= pool_base + 0x7FFF0, i.e.
		// g_assetEntryPool[0x7FFF].next — the free-list terminator, NOT
		// g_usageFrame) sits right after the pool, and other variables follow
		// immediately. Must allocate a new pool and patch all 25 code references.
		//
		// DB_Init (sub_48D340) builds the free list as:
		//   g_freeAssetEntries = &pool[1]                    (0x48D371)
		//   for (eax = 0x10; eax < LIMIT; eax += 0x10)       (0x48D390)
		//       *(pool + eax) = pool + 0x10 + eax            (0x48D382 / 0x48D388)
		//   *(DWORD*)(pool + 0x7FFF0) = 0                    (0x48D398)
		// so index 0 is the null sentinel and the last entry is LIMIT/0x10,
		// which must exist: allocate NEW_ASSET_ENTRY_POOL_SIZE + 1 entries.
		// =====================================================================


		// Allocate new pool (persists for lifetime of process).
		// +1 entry: index NEW_ASSET_ENTRY_POOL_SIZE is the free-list terminator.
		static XAssetEntryPoolEntry* newPool = (XAssetEntryPoolEntry*)VirtualAlloc(
			NULL,
			(NEW_ASSET_ENTRY_POOL_SIZE + 1) * sizeof(XAssetEntryPoolEntry),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!newPool) {
			T4::engine::Com_Printf(0, "^1ERROR: Failed to allocate expanded asset entry pool\n");
			return;
		}

		DWORD newPoolAddr = (DWORD)newPool;
		DWORD newPoolAddr10 = (DWORD)&newPool[1]; // pool + 0x10 (unk_A51C60 equivalent)

		// Update T4M's C pointer so DB_ListAssetPool and other T4M code use the new pool
		T4::engine::g_assetEntryPool = newPool;

		// Unprotect .text section pages covering all patch addresses (0x48D340 – 0x48FA30)
		DWORD oldProtect;
		VirtualProtect((LPVOID)T4M::GetAddress("DB_InitAssetEntryPool"), T4M::GetAddress("assetPool_patch_rangeEnd") - T4M::GetAddress("DB_InitAssetEntryPool"), PAGE_EXECUTE_READWRITE, &oldProtect);

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

#if 0 // === MIGRATED TO C++ === these 12 functions are now detoured in
	  // PatchT4MAM_AssetPool.cpp (they read the C++ pointer g_assetEntryPool), so
	  // their vanilla bodies — and these byte-patches — are unreachable. The pool
	  // is still allocated + g_assetEntryPool set above; only the .text patches go.
	  // sub_48F9B0 (below) is NOT yet detoured, so its two patches stay live.
	  // Re-enable this block if any of those detours is reverted.
		// ---- sub_48D340 (DB_InitAssetEntryPool) ----
		// C7 05 84 78 95 00 [60 1C A5 00] → mov dword_957884, offset unk_A51C60
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D371") = newPoolAddr10;
		// 8D 88 [60 1C A5 00] → lea ecx, [eax + unk_A51C60]
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D382") = newPoolAddr10;
		// 89 88 [50 1C A5 00] → mov [eax + dword_A51C50], ecx
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D388") = newPoolAddr;
		// 3D [F0 FF 07 00] → cmp eax, 7FFF0h  →  change limit to 0xFFFF0
		*(DWORD*)T4M::GetAddress("assetPool_limit_48D390") = NEW_ASSET_ENTRY_POOL_SIZE * 0x10; // 65535 * 0x10 = 0xFFFF0
		// C7 05 [40 1C AD 00] [00 00 00 00] → mov dword_AD1C40, 0
		// Free-list terminator: relocate the destination to newPool[NEW_ASSET_ENTRY_POOL_SIZE].next.
		// Left unpatched it wrote into the old BSS and the new pool's terminator
		// only held because VirtualAlloc zero-fills.
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D398") = newPoolAddr + NEW_ASSET_ENTRY_POOL_SIZE * 0x10;

		// ---- sub_48D560 (DB_EnumXAssets) ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D5B8") = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D5E5") = newPoolAddr;

		// ---- sub_48D760 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D784") = newPoolAddr;

		// ---- sub_48D7D0 ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D7F5") = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D848") = newPoolAddr;

		// ---- sub_48D860 (DB_AddXAsset / link entry) ----
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48D90F") = newPoolAddr;

		// ---- sub_48DEA0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48DEF4") = newPoolAddr;

		// ---- sub_48DFB0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48DFB4") = newPoolAddr;

		// ---- sub_48DFF0 (DB_UnloadXAssets) ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E059") = newPoolAddr;
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E115") = newPoolAddr;
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E1C2") = newPoolAddr;
		// 81 EF [50 1C A5 00] → sub edi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E1E8") = newPoolAddr;
		// 81 EA [50 1C A5 00] → sub edx, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E292") = newPoolAddr;

		// ---- sub_48E370 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48E3A6") = newPoolAddr;

		// ---- sub_48F340 (DB_PostLoadXZone) ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F378") = newPoolAddr;
		// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F4D4") = newPoolAddr;
		// 81 C7 [50 1C A5 00] → add edi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F558") = newPoolAddr;

		// ---- sub_48F670 ----
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F68C") = newPoolAddr;

		// ---- sub_48F6E0 ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F704") = newPoolAddr;

		// ---- sub_48F9B0 (now detoured too — DB_PostUnloadCleanup) ----
		// 05 [50 1C A5 00] → add eax, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48F9D4") = newPoolAddr;
		// 81 C6 [50 1C A5 00] → add esi, offset dword_A51C50
		*(DWORD*)T4M::GetAddress("assetPool_reloc_48FA28") = newPoolAddr;
#endif // === MIGRATED TO C++ (all 13 g_assetEntryPool functions detoured) ===

		// Restore whatever protection was there before. Note this is currently a
		// no-op: Main_UnprotectModule (Main.cpp) already turns the whole PE image
		// RWX at startup, so oldProtect is PAGE_EXECUTE_READWRITE. Kept because
		// pairing the calls is correct regardless of what the image-wide state is.
		VirtualProtect((LPVOID)T4M::GetAddress("DB_InitAssetEntryPool"), T4M::GetAddress("assetPool_patch_rangeEnd") - T4M::GetAddress("DB_InitAssetEntryPool"), oldProtect, &oldProtect);

		// =====================================================================
		// Fix rgp.sortedMaterials overflow
		//
		// dword_3BF1880 is the base of rgp (r_global_permanent_t, 0x2280 bytes),
		// whose first member is Material* sortedMaterials[2048]. Related globals:
		//   dword_3BF3884 = rgp + 0x2004 = rgp.materialCount
		//   dword_3BF392C = rgp + 0x20AC = rgp.world
		//
		// R_LoadWorld (sub_705070) and sub_719F40 call sub_48DF60
		// (DB_EnumXAssets_FastFile_Array) with ASSET_TYPE_MATERIAL to fill
		// sortedMaterials[], then sort it. sub_48DF60 ignores its third argument
		// (the 800h "max" push is never read) and writes ALL matching assets, so
		// with the material pool raised to 4096 it overruns the array and
		// clobbers materialCount / needSortMaterials / world. Observed crashes:
		//   0x719A2E: sort comparator gets garbage → access violation
		//   0x491500: corrupted rgp.world → bitfield access violation
		//
		// Fix: move sortedMaterials[] out of rgp into a larger buffer and patch
		// the 11 instructions that reference the array base.
		//
		// NOT patched on purpose:
		//   0x6D69EB — "push offset dword_3BF1880" in sub_6D69D0 is the dest of
		//              memset(rgp, 0, 0x2280), i.e. the whole-struct clear, not
		//              an array access. Redirecting it leaves the real rgp
		//              (default materials, images, world, saved screens) dirty.
		//              Consequence: the relocated array is no longer zeroed on a
		//              renderer reset — restored by the R_InitGlobalStructs
		//              detour below (T4_Reconstructed::R_InitGlobalStructs).
		//   dword_3BF3884 — materialCount never overflowed, it was corrupted by
		//              the array overrun. Leaving it inside rgp keeps it in sync
		//              with both vanilla reset paths (the memset above and
		//              "mov dword_3BF3884, 0" in sub_6E9D10).
		//
		// KNOWN LIMIT: Material_SetSortedIndex (sub_6E9900) packs the sorted
		// index into an 11-bit field of material->info.drawSurf ("and eax, 7FFh",
		// mask 0FFFFF001h), so only 2048 distinct indices are addressable. Past
		// 2048 materials the index wraps silently and the "Too many unique
		// materials" Com_Error does not fire (it tests equality with 800h, and
		// the R_LoadWorld enum writes materialCount directly). Going beyond 2048
		// requires widening that bitfield, not a bigger array.
		// =====================================================================


		DWORD* newSortedMaterials = (DWORD*)VirtualAlloc(
			NULL,
			NEW_SORTED_MATERIALS_SIZE * sizeof(DWORD),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);

		if (!newSortedMaterials) {
			T4::engine::Com_Printf(0, "^1ERROR: Failed to allocate expanded sortedMaterials array\n");
			return;
		}

		T4M::g_sortedMaterials = newSortedMaterials;   // R_InitGlobalStructs recon clears it
		DWORD newMaterialsAddr = (DWORD)newSortedMaterials;

		// Unprotect renderer .text pages covering patch addresses.
		// Range starts at the lowest patched site (0x6DC964) — it used to start
		// at 0x6D69E0, which needlessly covered the sub_6D69D0 memset.
		DWORD oldProtect2;
		VirtualProtect((LPVOID)T4M::GetAddress("sortedMaterials_patch_rangeStart"), T4M::GetAddress("sortedMaterials_patch_rangeEnd") - T4M::GetAddress("sortedMaterials_patch_rangeStart"), PAGE_EXECUTE_READWRITE, &oldProtect2);

		// --- Patch 11 references to rgp.sortedMaterials ---
		// Binary scan found pattern 80 18 BF 03 at the exact VA of the immediate.
		// Patch address = VA directly (no offset needed).

		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_6DC964") = newMaterialsAddr;
		// 8B 14 95 [80 18 BF 03] → mov edx, dword_3BF1880[edx*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_6DCA8C") = newMaterialsAddr;
		// 89 0C 85 [80 18 BF 03] → mov dword_3BF1880[eax*4], ecx
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_6E993D") = newMaterialsAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld)
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_705784") = newMaterialsAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (R_LoadWorld sort call)
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_70579F") = newMaterialsAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40)
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_719F52") = newMaterialsAddr;
		// 68 [80 18 BF 03] → push offset dword_3BF1880  (sub_719F40 sort call)
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_719F6D") = newMaterialsAddr;
		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_741C11") = newMaterialsAddr;
		// 8B 1C 85 [80 18 BF 03] → mov ebx, dword_3BF1880[eax*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_741C98") = newMaterialsAddr;
		// 8B 04 85 [80 18 BF 03] → mov eax, dword_3BF1880[eax*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_741EB7") = newMaterialsAddr;
		// 8B 2C 85 [80 18 BF 03] → mov ebp, dword_3BF1880[eax*4]
		*(DWORD*)T4M::GetAddress("sortedMaterials_reloc_74200E") = newMaterialsAddr;

		// Same pairing as the asset-pool block above, same no-op caveat.
		VirtualProtect((LPVOID)T4M::GetAddress("sortedMaterials_patch_rangeStart"), T4M::GetAddress("sortedMaterials_patch_rangeEnd") - T4M::GetAddress("sortedMaterials_patch_rangeStart"), oldProtect2, &oldProtect2);

		// All relocations done — verify every claimed site actually moved.
		T4M::RelocScanAfter();

		// Observation only (no behaviour change): sample the outgoing map's
		// rgp.materialCount at 0x6E9D3A, the "mov dword_3BF3884, 0" that ends
		// sub_6E9D10's per-map material reset. Feeds `listassetcaps`, which is
		// the measurement the F4 decision is waiting on.
		static auto materialCount_reset_hook = safetyhook::create_mid(
			T4M::GetAddress("materialCount_reset_hook"), [](SafetyHookContext&) {
				const DWORD count = T4M::CurrentMaterialCount();
				if (count > T4M::g_materialCountPeak)
					T4M::g_materialCountPeak = count;
			});

		// Own R_InitGlobalStructs so the relocated sortedMaterials is cleared on
		// every renderer reset, the way the vanilla rgp memset used to do it.
		Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("R_InitGlobalStructs"),
			(uintptr_t)&T4_Reconstructed::R_InitGlobalStructs, Detours::X86Option::USE_JUMP);

		// rgp.materialCount (dword_3BF3884) is deliberately left in place — see
		// the "NOT patched on purpose" note above. The 800h pushed at 0x70577F
		// and 0x719F4D is dead: sub_48DF60 never reads its third argument.

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
		static auto GSpawn_limit_hook = safetyhook::create_mid(T4M::GetAddress("GSpawn_limit_hook"), [](SafetyHookContext& ctx) {
			// ctx.ecx = numGEntities. Redirect eip to skip or enter the error path.
			if (ctx.ecx < (DWORD)(NEW_MAX_GENTITIES - 2)) {
				ctx.eip = T4M::GetAddress("GSpawn_alloc_eip"); // allocation path (loc_54EAF2)
			} else {
				ctx.eip = T4M::GetAddress("GSpawn_error_eip"); // error path ("G_Spawn: no free entities")
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
			*(DWORD*)T4M::GetAddress("strTable_base_54A20C") = newStrBase;

			// Patch first "push 3FFh" (limit arg to sub_54A1A0)
			// Encoding: 68 [imm32]; immediate at 0x54A2F3
			*(DWORD*)T4M::GetAddress("strTable_bound_54A2F3") = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

			// Patch second "push 3FFh" (limit arg to sub_54A1A0)
			// Encoding: 68 [imm32]; immediate at 0x54A330
			*(DWORD*)T4M::GetAddress("strTable_bound_54A330") = (DWORD)(NEW_MAX_LOCALIZED - 1); // 4095 = 0xFFF

			Com_Printf(0, "[T4M] Localized string table expanded: new limit=%d\n",
					   NEW_MAX_LOCALIZED - 1);
		} else {
			Com_Printf(0, "^1ERROR: Failed to allocate expanded localized string table\n");
		}
			*/
	}