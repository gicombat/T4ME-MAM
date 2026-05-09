#pragma once

namespace T4
{
	void TempMemorySetPos(char* pos);
	void TempMemoryReset(HunkUser* user);
	char* TempMalloc(int len);
}