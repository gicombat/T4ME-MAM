// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Full detour of the WaW memory subsystem — flat hunk, HunkUser, PMem.
//          Phase 1: faithful reconstruction of the flat-hunk allocators, detoured
//          at their vanilla VAs, operating on the SHARED vanilla globals in place.
//
// Plan : "Claude R&D/T4M CoD WaW/plans/plan_memory_full_detour.md"
// RE   : "Claude R&D/T4M CoD WaW/analysis/memory_subsystem_RE.md"
//
// The flat hunk is 1:1 with CoD4 (universal/com_memory.cpp). These are ports of
// that source, adapted to the WaW globals (via cod/mem.hpp symbols) and the exact
// WaW commit/decommit flags observed at 0x5E4220 / 0x5E4300 / 0x5E4350 / 0x5E4450 /
// 0x5E4580. Behaviour is meant to be byte-for-byte equivalent to vanilla (@faithful);
// the resize + validation land in Phase 4, gated by dvars, so this phase is a pure
// A/B fidelity test.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "Hooking.h"
#include <Windows.h>
#include <stdlib.h>
#include <string.h>

using namespace T4::engine;

namespace
{
	// WaW commits reserved pages with MEM_COMMIT, plus MEM_TOP_DOWN for small
	// spans (<= 0x20000). TOP_DOWN is ignored when committing already-reserved
	// pages, but we replicate the exact flag the vanilla code computes.
	inline void HunkCommit(void* base, unsigned int size)
	{
		DWORD fl = MEM_COMMIT | (size <= 0x20000 ? MEM_TOP_DOWN : 0);
		if (!VirtualAlloc(base, size, fl, PAGE_READWRITE))
			Sys_OutOfMemError(".\\universal\\com_memory.cpp", 0);
	}

	inline void HunkDecommit(void* base, unsigned int size)
	{
		VirtualFree(base, size, MEM_DECOMMIT);
	}

	inline unsigned int PageDown(unsigned char* p) { return (unsigned int)p & 0xFFFFF000u; }
	inline unsigned int PageUp(unsigned char* p)   { return ((unsigned int)p + 0xFFF) & 0xFFFFF000u; }

	// Vanilla file string passed to Sys_OutOfMemError from the PMem allocator.
	const char* const kPMemFile = "C:\\cod5\\cod\\codsrc\\src\\universal\\physicalmemory.cpp";

	// PMem region size — C++ is now the authority on it (was the 0x12C00000 immediate
	// inside Hunk_InitMemory). Keep it at the vanilla 300 MB for the faithful phase;
	// bump here to grow the zone budget without touching any .text immediate.
	const unsigned int kPMemSize = 0x12C00000; // 300 MB
}

namespace T4_Reconstructed
{
	// WaW sub_5E4220 — usercall(size@eax). Temp allocation on the HIGH side.
	// Check: hunk_high.temp + hunk_low.temp > s_hunkTotal -> Com_Error(ERR_DROP).
	extern "C" void* __cdecl Hunk_AllocateTempMemoryHigh(int size)
	{
		const int            total = *s_hunkTotal;
		unsigned char* const base  = *s_hunkData;

		unsigned int endBuf = PageDown(base + total - hunk_high->temp);

		int hi = hunk_high->temp + size;
		hi = (hi + 15) & ~0xF;
		hunk_high->temp = hi;

		if (hi + hunk_low->temp > total)
			Com_Error(ERR_DROP,
				"Hunk_AllocateTempMemoryHigh: failed on %i bytes (total %i MB, low %i MB, high %i MB)",
				size, total / 0x100000, hunk_low->temp / 0x100000, hi / 0x100000);

		unsigned char* buf = base + total - hi;
		unsigned int   pageBase = PageDown(buf);
		if (endBuf != pageBase)
			HunkCommit((void*)pageBase, endBuf - pageBase);

		return buf;
	}

	// WaW sub_5E4300 — cdecl(). Rolls the HIGH temp cursor back to permanent and
	// decommits the pages it freed.
	extern "C" void __cdecl Hunk_ClearTempMemoryHigh()
	{
		const int            total = *s_hunkTotal;
		unsigned char* const base  = *s_hunkData;

		unsigned int beginBuf = PageDown(base + total - hunk_high->temp);
		hunk_high->temp = hunk_high->permanent;
		unsigned int target = PageDown(base + total - hunk_high->permanent);
		if (target != beginBuf)
			HunkDecommit((void*)beginBuf, target - beginBuf);
	}

	// WaW sub_5E4450 — usercall(size@eax). Temp allocation on the LOW side; each
	// block carries a hunkHeader_t {magic 0x89537892, size} at buf-0x10.
	extern "C" void* __cdecl Hunk_AllocateTempMemory(int size)
	{
		unsigned char* const base = *s_hunkData;
		if (!base)
		{
			// Pre-hunk fallback (vanilla: Z_Malloc). Zero-filled like malloc+memset.
			void* p = malloc(size);
			if (p)
				memset(p, 0, size);
			return p;
		}

		const int    total   = *s_hunkTotal;
		const int    prev    = hunk_low->temp;
		unsigned int beginBuf = PageUp(base + prev);

		const int      aligned = (hunk_low->temp + 15) & ~0xF;
		unsigned char* hdr     = base + aligned;
		const int      newTemp = aligned + size + 0x10;
		hunk_low->temp = newTemp;

		if (hunk_high->temp + newTemp > total)
			Com_Error(ERR_DROP,
				"Hunk_AllocateTempMemory: failed on %i bytes (total %i MB, low %i MB, high %i MB), needs %i more hunk bytes",
				size + 0x10, total / 0x100000, newTemp / 0x100000, hunk_high->temp / 0x100000,
				hunk_high->temp + newTemp - total);

		unsigned int endBuf = PageUp(base + newTemp);
		if (endBuf != beginBuf)
			HunkCommit((void*)beginBuf, endBuf - beginBuf);

		*(unsigned int*)hdr    = 0x89537892;      // hunkHeader_t.magic
		*(int*)(hdr + 4)       = newTemp - prev;  // hunkHeader_t.size (whole block)
		return hdr + 0x10;
	}

	// WaW sub_5E4580 — usercall(buf@esi). LIFO free of a temp-low block; the magic
	// check is the corruption guard, and a mismatch is FATAL (popup via Sys_Error).
	extern "C" void __cdecl Hunk_FreeTempMemory(char* buf)
	{
		unsigned char* const base = *s_hunkData;
		if (!base)
		{
			free(buf);
			return;
		}

		unsigned int* hdr = (unsigned int*)(buf - 0x10);
		if (hdr[0] != 0x89537892)
			Com_Error(ERR_FATAL, "Hunk_FreeTempMemory: bad magic");

		hdr[0] = 0x89537893;                 // mark freed
		const int blockSize = (int)hdr[1];   // hunkHeader_t.size

		unsigned int endBuf = PageUp(base + hunk_low->temp);
		hunk_low->temp -= blockSize;
		unsigned int beginBuf = PageUp(base + hunk_low->temp);
		if (endBuf != beginBuf)
			HunkDecommit((void*)beginBuf, endBuf - beginBuf);
	}

	// ── PMem — physical-memory stack for zone data (Phase 3) ────────────────────
	// The FREE path is already owned by T4M (T4M::DB_FreeXZoneMemory in
	// PatchT4Load.cpp, called from the reconstructed DB_UnloadAllZones, with the
	// strict-LIFO "free does not match allocation" guard). Here we take the ALLOC
	// path to match. prim[0] = LOW (grows up), prim[1] = HIGH (grows down); each
	// alloc checks against the opposite cursor and hits Sys_OutOfMemError on cross.
	//
	// WaW sub_5F5480 (Hunk_InitMemory) — cdecl(). One-shot lazy init of the PMem
	// region. The size is now a C++ constant (kPMemSize) instead of the .text
	// immediate — this is where we grow the zone budget natively.
	extern "C" void __cdecl PMem_Init()
	{
		if (*g_pmemInited)
			return;
		*g_pmemInited = 1;

		unsigned char* buf = (unsigned char*)VirtualAlloc(0, kPMemSize, MEM_COMMIT, PAGE_READWRITE);

		PhysicalMemory* mem = &*g_mem;
		memset(mem, 0, sizeof(PhysicalMemory));
		mem->buf          = buf;
		mem->name         = (char*)"main";
		mem->prim[1].pos  = kPMemSize; // HIGH cursor starts at the top
		mem->size         = kPMemSize;
	}

	// WaW sub_5F5590 — usercall(alignment@eax, size@edi, allocType@stack).
	extern "C" void* __cdecl PMem_Alloc(int alignment, int size, int allocType)
	{
		PMem_Init(); // guarded lazy init of the PMem region (our reconstruction)

		PhysicalMemoryPrim* prim = &g_mem->prim[allocType];
		const unsigned int  mask = (unsigned int)alignment - 1;

		if (allocType == 0) // LOW, grows up
		{
			unsigned int alignedPos = (prim->pos + mask) & ~mask;
			unsigned int newPos     = alignedPos + (unsigned int)size;
			int over = (int)(newPos - g_mem->prim[1].pos);
			*g_overAllocatedSize = over;
			if (over > 0)
			{
				Com_PrintError(16, "Need %i more bytes of '%s' physical ram for alloc to succeed\n",
					over, g_mem->name);
				Sys_OutOfMemError(kPMemFile, 966);
			}
			prim->pos = newPos;
			return g_mem->buf + alignedPos;
		}
		else // HIGH, grows down (allocType == 1)
		{
			unsigned int alignedPos = ((unsigned int)prim->pos - (unsigned int)size) & ~mask;
			int over = (int)(g_mem->prim[0].pos - alignedPos);
			*g_overAllocatedSize = over;
			if (over > 0)
				Sys_OutOfMemError(kPMemFile, 1000);
			prim->pos = alignedPos;
			return g_mem->buf + alignedPos;
		}
	}
}

// @wrapper — usercall(size@eax) -> cdecl. Detoured over sub_5E4220.
__declspec(naked) void W_Hunk_AllocateTempMemoryHigh()
{
	__asm
	{
		push eax
		call T4_Reconstructed::Hunk_AllocateTempMemoryHigh
		add  esp, 4
		ret
	}
}

// @wrapper — usercall(size@eax) -> cdecl. Detoured over sub_5E4450.
__declspec(naked) void W_Hunk_AllocateTempMemory()
{
	__asm
	{
		push eax
		call T4_Reconstructed::Hunk_AllocateTempMemory
		add  esp, 4
		ret
	}
}

// @wrapper — usercall(buf@esi) -> cdecl. Detoured over sub_5E4580.
__declspec(naked) void W_Hunk_FreeTempMemory()
{
	__asm
	{
		push esi
		call T4_Reconstructed::Hunk_FreeTempMemory
		add  esp, 4
		ret
	}
}

// @wrapper — usercall(alignment@eax, size@edi, allocType@stack) -> cdecl.
// Detoured over sub_5F5590. Vanilla returns with a plain ret, so the stack arg is
// caller-cleaned; we only balance our own three pushes.
__declspec(naked) void W_PMem_Alloc()
{
	__asm
	{
		mov  ecx, [esp + 4]   // allocType (caller's stack arg)
		push ecx
		push edi              // size
		push eax              // alignment
		call T4_Reconstructed::PMem_Alloc
		add  esp, 0xC
		ret
	}
}

void PatchT4_Mem()
{
	// Phase 1 — flat-hunk TEMP family (faithful). USE_JUMP: each reconstruction
	// owns the whole function; nothing returns into vanilla. usercall entries go
	// through a naked wrapper that spills the register-borne arg onto the stack.
	Detours::X86::DetourFunction(T4M::GetAddress("Hunk_AllocateTempMemoryHigh"),
		(uintptr_t)&W_Hunk_AllocateTempMemoryHigh, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Hunk_ClearTempMemoryHigh"),
		(uintptr_t)&T4_Reconstructed::Hunk_ClearTempMemoryHigh, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Hunk_AllocateTempMemory"),
		(uintptr_t)&W_Hunk_AllocateTempMemory, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Hunk_FreeTempMemory"),
		(uintptr_t)&W_Hunk_FreeTempMemory, Detours::X86Option::USE_JUMP);

	// Phase 3 — PMem zone allocator. (The free path is already owned in
	// PatchT4Load.cpp; this takes the alloc + init paths to match.)
	Detours::X86::DetourFunction(T4M::GetAddress("Hunk_InitMemory"),
		(uintptr_t)&T4_Reconstructed::PMem_Init, Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("PMem_Alloc"),
		(uintptr_t)&W_PMem_Alloc, Detours::X86Option::USE_JUMP);
}
