#pragma once

namespace T4
{
	namespace engine
	{
		void TempMemorySetPos(char* pos);
		void TempMemoryReset(HunkUser* user);
		char* TempMalloc(int len);
	}
} // namespace T4::engine