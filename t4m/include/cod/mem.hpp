#pragma once

// Memory subsystem — flat hunk + HunkUser + PMem.
// Foundation for the full memory-manager detour (plans/plan_memory_full_detour.md).
// RE + address map: analysis/memory_subsystem_RE.md. Layouts live in structs.hpp
// (hunkUsed_t, hunkHeader_t, HunkUser, PhysicalMemory, PhysicalMemoryPrim,
// PhysicalMemoryAllocation). Vanilla globals are SHARED IN PLACE — never copied
// to a private region — so non-detoured vanilla readers stay coherent.

namespace T4
{
	namespace engine
	{
		// ── Flat hunk globals ──────────────────────────────────────────────
		// Data VAs are identical SP <-> German (project rule). s_hunkTotal /
		// hunk_high_temp / hunk_low_temp are declared in fs.hpp; here we add the
		// base cursors and the backing store.
		WEAK symbol<unsigned char*> s_hunkData{ "s_hunkData" };       // 0x46E5058 VirtualAlloc base
		WEAK symbol<unsigned char*> s_origHunkData{ "s_origHunkData" }; // 0x46E505C
		WEAK symbol<hunkUsed_t>     hunk_high{ "hunk_high" };         // 0x21AB2FC {permanent, temp}
		WEAK symbol<hunkUsed_t>     hunk_low{ "hunk_low" };           // 0x21AB304 {permanent, temp}

		// ── PMem globals ───────────────────────────────────────────────────
		WEAK symbol<PhysicalMemory>  g_mem{ "g_mem" };                 // 0x224F9D0
		WEAK symbol<int>             g_overAllocatedSize{ "g_overAllocatedSize" }; // 0x220D34C
		WEAK symbol<unsigned char>   g_pmemInited{ "g_pmemInited" };   // 0x46E5079 one-shot init guard

		// ── Vanilla helpers the reconstructions call ───────────────────────
		// Sys_OutOfMemErrorInternal(file, line) — WaW 0x5FE760, terminal OOM popup.
		WEAK symbol<void(const char* file, int line)> Sys_OutOfMemError{ "Sys_OutOfMemError" };
		// PMem_Init (Hunk_InitMemory) — WaW 0x5F5480, cdecl(), guarded lazy 300 MB init.
		WEAK symbol<void()> Hunk_InitMemory{ "Hunk_InitMemory" };

		// ── Function VA map (detoured in later phases; keys live in the CSV) ─
		// Flat hunk:
		//   Sys_AllocHunk (Com_InitHunkMemory)      0x5E3CA0   cdecl
		//   Hunk_Clear                              0x5E3F60   cdecl
		//   Hunk_AllocAlign      (perm high)        ~0x5E41xx  usercall(size@eax)   [start TBD Phase 1]
		//   Hunk_AllocateTempMemoryHigh             0x5E4220   usercall(size@eax)
		//   Hunk_ClearTempMemoryHigh                0x5E4300   cdecl
		//   Hunk_AllocLowAlign   (perm low)         0x5E4350   usercall(size@eax)
		//   Hunk_AllocateTempMemory (temp low)      0x5E4450   usercall(size@eax)   magic 0x89537892
		//   Hunk_FreeTempMemory                     0x5E4580   cdecl(buf)
		//   Hunk_ClearTempMemory                    0x4B2F80   cdecl
		// HunkUser:
		//   Hunk_UserCreate                         0x5E46E0   cdecl
		//   Hunk_UserAlloc                          0x5E47B0   cdecl(user, size, align)
		// PMem:
		//   PMem_Init (Hunk_InitMemory)             0x5F5480   cdecl  (300 MB commit)
		//   PMem_Alloc (DB_MemAlloc)                0x5F5590   usercall(size@eax, allocType@stack)
		//   PMem_Free (DB_FreeXZoneMemory)          0x5F5540   usercall(allocType@eax)
		//   PMem_BeginAlloc / PMem_EndAlloc         TBD Phase 3
		//   DB_AllocXZoneMemory                     0x48CED0   cdecl  (7 blocks)
		// Popup path (documentation): Sys_Error 0x5FE8C0 -> MessageBoxA @ 0x60599F.
	}
}
