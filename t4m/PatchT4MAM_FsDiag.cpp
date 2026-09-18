// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Observation-only instrumentation for the intermittent
//          "Hunk_AllocateTempMemoryHigh: failed on N bytes" drop and for
//          the silent zone drops that end in a map-load freeze.
//
// RE : "Claude R&D/T4M CoD WaW/analysis/hunk_temp_alloc_RE.md"
//
// Five vanilla sites do `len = FS_FOpenFileRead(name, &h);` then
// `Hunk_AllocateTempMemoryHigh(len + 1)` with `len < 0` as their only check.
// For an .iwd entry that len is unz_file_info.uncompressed_size, memcpy'd out
// of the pak's shared master unzFile right after unzSetOffset wrote it.
//
// Neither fault is reproducible on demand, so the triggers below watch for the
// fault itself rather than for its most extreme outcome. unzSetOffset leaves
// two witnesses in the 0x80 bytes that get copied into our handle:
//
//   unz[0x14] = the central-directory offset it was asked to seek to
//   unz[0x18] = 1 if unzlocal_GetCurrentFileInfo refreshed cur_file_info, else 0
//
// Watching the witnesses instead of the number matters: observed bogus lengths
// span 1.3 GB to 2 GB, so any hand-picked threshold only catches the last case
// seen. Both checks below are size-independent.
//
// So a returned length is untrustworthy whenever unz[0x18] == 0 (the info is
// whatever the previous read left there) or unz[0x14] != the offset we asked
// for (another thread moved the shared master between our seek and our copy —
// the length then belongs to that thread's file, and usually looks perfectly
// plausible). Either one is logged, whatever the number looks like.
//
// Everything lands in t4m_fsdiag.log, appended and flushed per line: the run
// that matters may end in a freeze, where nothing is ever drawn again.
//
// Nothing here changes behaviour: the hooks only read registers and print.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <safetyhook.hpp>
#include <stdio.h>
#include <stdarg.h>

namespace
{
	// unz_s field offsets, all inside the 0x80 bytes memcpy'd from the master.
	const int kUnzRequestedPos = 0x14 / 4;
	const int kUnzInfoValid    = 0x18 / 4;
	const int kUnzCompressed   = 0x40 / 4;   // cur_file_info.compressed_size
	const int kUnzUncompressed = 0x44 / 4;   // cur_file_info.uncompressed_size — the returned length

	// Backstop for the raw path and for anything the two witnesses above miss.
	// Deliberately NOT a constant: observed bogus lengths range from 1.3 GB to
	// 2 GB, and a threshold picked by hand would only ever be the last one seen.
	// The boundary that actually matters is the one the engine itself enforces.
	bool FsDiag_ExceedsHunk(unsigned int len)
	{
		const unsigned int total = (unsigned int)*T4::engine::s_hunkTotal;
		return total != 0 && len > total;
	}

	const char* const kLogFile = "t4m_fsdiag.log";

	struct FsOpenRecord
	{
		char         name[128];
		unsigned int len;
		int          handle;
		int          fromPak;
		int          suspect;
		DWORD        threadId;
		DWORD        tick;
	};

	// Wide enough to still hold the map-load burst that precedes an incident.
	FsOpenRecord  s_ring[64];
	volatile LONG s_ringSeq = 0;

	// Two FS threads can be inside a suspect open at the same time — that is the
	// bug itself — so the log has to serialize, or their lines shred each other.
	// A capture on 2026-08-28 lost half a line exactly this way.
	CRITICAL_SECTION s_logLock;
	bool             s_logLockReady = false;

	void FsDiag_Write(const char* text)
	{
		if (s_logLockReady)
			EnterCriticalSection(&s_logLock);

		FILE* f = fopen(kLogFile, "ab");
		if (f)
		{
			fwrite(text, 1, strlen(text), f);
			fclose(f);   // closed per event: a freeze or a hard kill must not lose it
		}

		if (s_logLockReady)
			LeaveCriticalSection(&s_logLock);
	}

	void FsDiag_Vlogf(const char* fmt, va_list ap)
	{
		char buf[1024];
		int n = _snprintf(buf, sizeof(buf) - 1, "[%08X] ", GetTickCount());
		if (n < 0)
			n = 0;
		_vsnprintf(buf + n, sizeof(buf) - 1 - n, fmt, ap);
		buf[sizeof(buf) - 1] = '\0';
		FsDiag_Write(buf);
	}

	void FsDiag_Logf(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		FsDiag_Vlogf(fmt, ap);
		va_end(ap);
	}

	// esp at both return sites is E-0x320, so arg1/arg2 sit at fixed offsets.
	// unz is our handle's unzFile (pak path only, still live in ebp), reqPos the
	// offset this open asked unzSetOffset for (still live in ebx).
	void FsDiag_RecordReturn(SafetyHookContext& ctx, int fromPak, const unsigned int* unz, const int* reqPos)
	{
		const char*  name   = *(const char**)(ctx.esp + 0x324);
		int*         handle = *(int**)(ctx.esp + 0x328);
		unsigned int len    = (unsigned int)ctx.eax;

		const char* why = 0;
		if (fromPak && unz)
		{
			if (unz[kUnzInfoValid] == 0)
				why = "unzSetOffset failed, cur_file_info is stale";
			else if (reqPos && (int)unz[kUnzRequestedPos] != *reqPos)
				why = "shared master moved between seek and copy, length is another file's";
		}
		if (!why && FsDiag_ExceedsHunk(len))
			why = "length exceeds the whole hunk, the alloc that follows cannot succeed";

		LONG slot = (InterlockedIncrement(&s_ringSeq) - 1) % (LONG)(sizeof(s_ring) / sizeof(s_ring[0]));
		FsOpenRecord& r = s_ring[slot];
		r.len      = len;
		r.handle   = handle ? *handle : -1;
		r.fromPak  = fromPak;
		r.suspect  = (why != 0);
		r.threadId = GetCurrentThreadId();
		r.tick     = GetTickCount();
		if (name)
		{
			strncpy(r.name, name, sizeof(r.name) - 1);
			r.name[sizeof(r.name) - 1] = '\0';
		}
		else
		{
			r.name[0] = '\0';
		}

		if (!why)
			return;

		// One buffer, one write: the two threads that trip this are concurrent by
		// construction, and two writes per event interleave into unreadable lines.
		char msg[768];
		int n = _snprintf(msg, sizeof(msg) - 1,
			"[%08X] SUSPECT FS_FOpenFileRead(\"%s\") -> %u (0x%08X) src=%s handle=%d tid=%u : %s\n",
			r.tick, r.name, len, len, fromPak ? "pak" : "raw", r.handle, r.threadId, why);
		if (n < 0)
			n = 0;

		if (fromPak && unz)
		{
			_snprintf(msg + n, sizeof(msg) - 1 - n,
				"            tid=%u unz: infoValid=%u posAsked=0x%08X posInCopy=0x%08X "
				"compressed=%u uncompressed=%u\n",
				r.threadId, unz[kUnzInfoValid], reqPos ? (unsigned int)*reqPos : 0u,
				unz[kUnzRequestedPos], unz[kUnzCompressed], unz[kUnzUncompressed]);
		}
		msg[sizeof(msg) - 1] = '\0';
		FsDiag_Write(msg);
	}

	void FsDiag_DumpRing()
	{
		const LONG count = (LONG)(sizeof(s_ring) / sizeof(s_ring[0]));
		LONG seq = s_ringSeq;
		FsDiag_Logf("  last %ld FS opens (oldest first):\n", seq < count ? seq : count);
		for (LONG i = (seq > count ? seq - count : 0); i < seq; i++)
		{
			const FsOpenRecord& r = s_ring[i % count];
			FsDiag_Logf("  %s #%ld %-3s len=%u (0x%08X) handle=%d tid=%u t=%08X  %s\n",
				r.suspect ? "!!" : "  ", i, r.fromPak ? "pak" : "raw",
				r.len, r.len, r.handle, r.threadId, r.tick, r.name);
		}
	}
}

// Shared with PatchT4Load.cpp so map-load boundaries and dropped zones leave a
// trail in the same file — a freeze writes nothing else.
void __cdecl T4M::FsDiag_Note(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	FsDiag_Vlogf(fmt, ap);
	va_end(ap);
}

// Registered from PatchT4(); the hooks themselves fire at runtime only.
void PatchT4MAM_FsDiag()
{
	InitializeCriticalSection(&s_logLock);
	s_logLockReady = true;

	SYSTEMTIME now;
	GetLocalTime(&now);
	FsDiag_Logf("=== session start %04u-%02u-%02u %02u:%02u:%02u — t4m %s ===\n",
		now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
		T4M::IsMpVersion() ? "mp" : "sp");

	// --- FS side: catch the bogus length at the source -----------------------
	// ebp = our unzFile, ebx = the pak entry whose first dword is the offset
	// this open asked unzSetOffset for. Both are still live at 0x5DBB4A.
	static auto fs_ret_pak_hook = safetyhook::create_mid(
		T4M::GetAddress("FS_FOpenFileRead_ret_pak"), [](SafetyHookContext& ctx) {
			FsDiag_RecordReturn(ctx, /*fromPak=*/1,
				(const unsigned int*)ctx.ebp, (const int*)ctx.ebx);
		});

	static auto fs_ret_raw_hook = safetyhook::create_mid(
		T4M::GetAddress("FS_FOpenFileRead_ret_raw"), [](SafetyHookContext& ctx) {
			FsDiag_RecordReturn(ctx, /*fromPak=*/0, 0, 0);
		});

	// --- Hunk side: the failing request, plus which of the 5 sites asked -----
	static auto hunk_alloc_hook = safetyhook::create_mid(
		T4M::GetAddress("Hunk_AllocateTempMemoryHigh"), [](SafetyHookContext& ctx) {
			const unsigned int size  = (unsigned int)ctx.eax;
			const unsigned int total = (unsigned int)*T4::engine::s_hunkTotal;
			if (size <= total)
				return;

			FsDiag_Logf("DROP Hunk_AllocateTempMemoryHigh(%u / 0x%08X) > total %u  "
				"caller=0x%08X high=%d low=%d tid=%u\n",
				size, size, total, *(unsigned int*)ctx.esp,
				*T4::engine::hunk_high_temp, *T4::engine::hunk_low_temp,
				GetCurrentThreadId());
			FsDiag_DumpRing();
		});
}
