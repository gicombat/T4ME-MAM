#pragma once

// Filesystem + hunk temp allocator. Added for the "Hunk_AllocateTempMemoryHigh:
// failed on N bytes" investigation — see
// "Claude R&D/T4M CoD WaW/analysis/hunk_temp_alloc_RE.md".

namespace T4
{
	namespace engine
	{
		// WaW sub_5DBD20 — thin shim over sub_5DB630 with thread = 0.
		// Returns the file length, or -1 / -2 on failure. The length comes from
		// FS_FileLength for a raw file, and from unz_file_info.uncompressed_size
		// copied out of the pak's shared master handle for an .iwd entry.
		WEAK symbol<int(const char* name, int* handleOut)> FS_FOpenFileRead{ "FS_FOpenFileRead" };

		// WaW sub_5DAC70 — usercall(handle@eax). Declared for documentation only:
		// call it through the wrapper below if it is ever needed.
		// WEAK symbol<int()> FS_FileLength{ "FS_FileLength" };

		// WaW sub_5E4220 — usercall(size@eax). Com_Error(ERR_DROP) when the
		// request does not fit, after hunk_high_temp was already advanced.
		// WEAK symbol<void*()> Hunk_AllocateTempMemoryHigh{ "Hunk_AllocateTempMemoryHigh" };

		WEAK symbol<FsHandleEntry> fs_handles{ "fs_handles" };   // 64 entries, stride 0x11C
		WEAK symbol<int> s_hunkTotal{ "s_hunkTotal" };
		WEAK symbol<int> hunk_high_temp{ "hunk_high_temp" };
		WEAK symbol<int> hunk_low_temp{ "hunk_low_temp" };
		WEAK symbol<dvar_t*> fs_debug{ "fs_debug" };

		// WaW sub_6233E0 — usercall(unz@esi, pos@stack) -> 0, or -0x66 if unz is null.
		// Writes unz->posInCentralDir, then refreshes unz->curFileInfo from the
		// central directory and sets unz->curFileInfoOk to the outcome. On failure
		// it leaves curFileInfo untouched, i.e. holding the previous entry's values.
		inline int unzSetOffset(UnzFile* unz, int pos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("unzSetOffset"));
			int result;
			__asm
			{
				push esi
				push pos
				mov  esi, unz
				call fn
				add  esp, 4
				pop  esi
				mov  result, eax
			}
			return result;
		}

		// WaW sub_6235D0 — usercall(unz@eax).
		inline int unzOpenCurrentFile(UnzFile* unz)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("unzOpenCurrentFile"));
			int result;
			__asm
			{
				mov  eax, unz
				call fn
				mov  result, eax
			}
			return result;
		}
	}
}
