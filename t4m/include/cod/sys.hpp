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
	}
} // namespace T4::engine
