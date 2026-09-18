// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Fix the shared-master race in the .iwd branch of
//          FS_FOpenFileReadForThread (sub_5DB630).
//
// RE : "Claude R&D/T4M CoD WaW/analysis/hunk_temp_alloc_RE.md" §§ 8, 14, 16
//
// Vanilla, once a pak entry has been located and a handle allocated, does its
// central-directory read on the pak's SHARED master unzFile and then memcpy's
// the result into the caller's own handle:
//
//     esi = pak->master
//     ebp = fsh[h].file                  ; a private clone, unless we hold the master
//     unzSetOffset(esi /* master */, pos)
//     memcpy(ebp, esi /* master */, 0x80)
//     unzOpenCurrentFile(ebp)
//     return ebp->curFileInfo.uncompressedSize
//
// Two opens on the same pak from different threads therefore fight over one
// FILE*. Observed in game on 2026-08-28: one thread asked for the entry at
// 0x30973967 and its copy came back holding 0x309866E2, the other thread's
// offset, with curFileInfoOk == 0 on BOTH sides — each had moved the master's
// FILE* out from under the other, so neither read the 0x02014B50 signature
// where it expected it. Vanilla tests neither unzSetOffset's return value nor
// curFileInfoOk, so the previous entry's size is returned as if it were this
// file's. It is usually a plausible number (6560 bytes for a .iwi, in that
// capture), which is why this has been silently corrupting reads rather than
// announcing itself. The extreme tails are what got noticed: a size beyond the
// hunk (the "failed on N bytes" drop) or one with bit 31 set, which the .gsc
// reader turns into a permanently memoized "this script does not exist".
//
// The fix seeks and parses on OUR OWN handle. The clone already owns a private
// FILE* and a copy of the archive header — that is what unzReOpen is for — so
// nothing is shared any more and the memcpy disappears with the race. When we
// hold the master itself, our handle IS the master and behaviour is identical
// to vanilla minus a self-copy.
//
// Everything else in the block is reproduced verbatim, in order.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "Hooking.h"
#include <stdio.h>

using namespace T4::engine;

namespace
{
	// How often to re-seek before giving up. The seek is idempotent, and with
	// the master no longer shared a repeat failure means a genuinely unreadable
	// central directory rather than contention.
	const int kSeekAttempts = 3;
}

// @modified — replaces the pak branch of sub_5DB630, 0x005DBA62 … 0x005DBB54.
// Called only by the naked trampoline below, which supplies the register-borne
// values the vanilla block had in edi/ebx and the locals it read off the stack.
extern "C" int __cdecl T4_Reconstructed_FS_OpenPakEntry(
	void* pak, const int* pakEntry, int* handleOut, int thread, const char* localName)
{
	const int handle = *handleOut;
	FsHandleEntry* fh = &fs_handles[handle];

	// Vanilla: strncpy into fsh[h].name, 0xFF bytes, then force-terminate.
	strncpy(fh->name, localName, 0xFF);
	fh->name[0xFF] = '\0';

	fh->pak = pak;

	UnzFile* unz = (UnzFile*)fh->file;
	const int pos = pakEntry[0];

	// ── the fix ──────────────────────────────────────────────────────────────
	// Seek and parse on our own handle. Vanilla did this on pak->master and then
	// copied 0x80 bytes back over ours; that copy is what made the operation
	// racy, and it is not needed once the parse writes straight into ours.
	int attempt = 0;
	for (;;)
	{
		unzSetOffset(unz, pos);
		if (unz->curFileInfoOk != 0)
			break;

		if (++attempt >= kSeekAttempts)
		{
			// Never fall through with an unparsed curFileInfo. Vanilla does, and
			// that is the whole bug: the caller gets the previous entry's size,
			// or -- worse -- a value with bit 31 set, which Scr_ReadFile memoizes
			// as "this file does not exist" for the rest of the session.
			T4M::FsDiag_Note("PAKOPEN giving up on '%s' after %d seeks (pos 0x%08X, pak %p)\n",
				localName, attempt, (unsigned int)pos, pak);
			Com_Error(ERR_DROP, "FS_FOpenFileRead: unreadable archive entry for '%s'", localName);
			return -1;
		}
	}

	unzOpenCurrentFile(unz);

	fh->posInCentralDir = pos;

	if ((*fs_debug)->current.integer && thread == 0)
		Com_Printf(10, "FS_FOpenFileRead: %s (found in '%s')\n", localName, pak);

	return (int)unz->curFileInfo.uncompressedSize;
}

// @wrapper — entered by a JMP planted over the block's first instruction, so it
// owns the block's exit too. On entry esp is E-0x320 (E = esp at the entry of
// sub_5DB630): edi holds the pak, ebx the central-directory entry, the resolved
// path is the local at esp+0x20, and the callee's own arguments sit at esp+0x324
// (name), +0x328 (&handle) and +0x32C (thread). The epilogue reproduces
// 0x005DBB4A … 0x005DBB54 byte for byte.
__declspec(naked) void FS_OpenPakEntry_Trampoline()
{
	__asm
	{
		lea  eax, [esp + 0x20]        // localName
		mov  ecx, [esp + 0x32C]       // thread
		mov  edx, [esp + 0x328]       // &handle

		push eax
		push ecx
		push edx
		push ebx                      // pakEntry
		push edi                      // pak
		call T4_Reconstructed_FS_OpenPakEntry
		add  esp, 20

		pop  edi
		pop  esi
		pop  ebp
		pop  ebx
		add  esp, 0x310
		ret
	}
}

void PatchT4MAM_FsPakOpen()
{
	// USE_JUMP: we never return into vanilla, the trampoline runs the block to
	// its ret. The overwritten instruction (mov ebp,[esp+0x328], 7 bytes) is the
	// block's only entry point -- verified: the sole inbound branches, from
	// 0x005DB9FE and 0x005DBA33, both target its first byte.
	Detours::X86::DetourFunction(T4M::GetAddress("FS_OpenPakEntry_site"),
		(uintptr_t)&FS_OpenPakEntry_Trampoline, Detours::X86Option::USE_JUMP);
}
