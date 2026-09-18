#pragma once

namespace T4
{
	namespace engine
	{

		typedef void(__cdecl* Com_PrintMessage_t)(int channel, const char* fmt, int error);
		typedef void(__cdecl* Com_PrintError_t)(int channel, const char* fmt, ...);
		typedef int(__cdecl* Com_sscanf_t)(const char* src, const char* fmt, ...);

		WEAK symbol<void(int type, const char* message, ...)>Com_Error{ "Com_Error" };
		WEAK symbol<void(int channel, const char* format, ...)>Com_Printf{ "Com_Printf" };
		WEAK symbol<void(int channel, const char* format, ...)>Com_DPrintf{ "Com_DPrintf" };   // sub_59A310 (developer-gated)
		WEAK symbol<void(int channel, const char* fmt, int error)>Com_PrintMessage{ "Com_PrintMessage" };
		WEAK symbol<void(int channel, const char* fmt, ...)>Com_PrintError{ "Com_PrintError" };   // sub_59A380 ("^1Error: ", type 3)
		WEAK symbol<void(int channel, const char* fmt, ...)>Com_PrintWarning{ "Com_PrintWarning" }; // sub_59A440 ("^3", type 2)
		WEAK symbol<int(const char* src, const char* fmt, ...)>Com_sscanf{ "Com_sscanf" };
		// CRT sprintf (sub_7AA926, unbounded: dst+fmt+varargs). Distinct from Com_sprintf (0x5F6D00, bounded dst+size).
		// Named crt_sprintf (not "sprintf") to avoid ambiguity with the C runtime ::sprintf under `using namespace`.
		WEAK symbol<int(char* dst, const char* fmt, ...)>crt_sprintf{ "sprintf" };

		// non-variadic — symbol<> (moved from T4.cpp extern "C")
		WEAK symbol<void(int channel, int arg)> Com_DvarDump{ "Com_DvarDump" };

		// sub_5F6D80 — the engine's va(): formats into a rotating static buffer.
		WEAK symbol<const char*(const char* fmt, ...)>Com_FormatMsg{ "Com_FormatMsg" };

		// sub_7AFF40 — CRT memset. Used by reconstructions that must reproduce
		// the vanilla call sequence rather than emit their own inlined clear.
		WEAK symbol<void*(void* dst, int c, size_t n)>Mem_Memset{ "Mem_Memset" };
	}
}