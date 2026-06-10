#pragma once

namespace T4
{
	namespace engine
	{
		// main hunk/memory reservation: picks the hunk size from dvars
		WEAK symbol<void()>Sys_AllocHunk{ "Sys_AllocHunk" };
	}
} // namespace T4::engine
