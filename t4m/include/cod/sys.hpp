#pragma once

namespace T4
{
	namespace engine
	{
		// main hunk/memory reservation: picks the hunk size from dvars
		WEAK symbol<void()>Sys_AllocHunk{ "Sys_AllocHunk" };

		WEAK symbol<void*()>Sys_EnterRecursiveLock{ "Sys_EnterRecursiveLock" };
		WEAK symbol<int()>Sys_LeaveRecursiveLock{ "Sys_LeaveRecursiveLock" };
		WEAK symbol<char*()> Sys_BinaryPath{ "Sys_BinaryPath" };
		WEAK symbol<void()> Sys_SyncDatabase{ "Sys_SyncDatabase" };
		WEAK symbol<void()> Sys_WakeDatabase{ "Sys_WakeDatabase" };
		WEAK symbol<void()> Hunk_BeginRegistration{ "Hunk_BeginRegistration" };
		WEAK symbol<void(int)> Hunk_ClearTempMemory{ "Hunk_ClearTempMemory" };

		// Sys_Error (sub_5FE8C0, CSV key "Com_Fatal") is DETOURED to T4M::Sys_Error.
		// sub_5A33F0 - SuspendThread on every engine thread but the caller.
		WEAK symbol<void()> Sys_SuspendOtherThreads{ "Sys_SuspendOtherThreads" };
		// sub_5D9E90 - restores display mode, hides the caller's windows, resets gamma.
		WEAK symbol<void()> FixWindowsDesktop{ "FixWindowsDesktop" };
		// sub_6057F0 - creates the console window on the calling thread if absent, then shows it.
		WEAK symbol<void()> Sys_ShowConsole{ "Sys_ShowConsole" };
		// Statically linked CRT, called by VA like the rest of Sys_Error.
		WEAK symbol<int(char*, size_t, const char*, va_list)> crt_vsnprintf{ "_vsnprintf" };
		WEAK symbol<void(int)> crt_exit{ "exit" };

		// WaW sub_605870 - usercall(text@ecx) ; retn. EM_REPLACESEL on the console edit control.
		inline void Conbuf_AppendText(const char* text)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Conbuf_AppendText"));
			__asm
			{
				mov  ecx, text
				call fn
			}
		}

		// WaW sub_605960 - usercall(text@esi) ; retn. Stores the text, MessageBoxA "Error".
		inline void Sys_SetErrorText(const char* text)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sys_SetErrorText"));
			__asm
			{
				mov  esi, text
				call fn
			}
		}
	}
} // namespace T4::engine
