#pragma once

// SV_* engine functions. T4::engine per the namespace bucketing rule (vanilla prefix SV_).

namespace T4
{
	namespace engine
	{
		// Batch setter: pairs[count] of {index, string}. sub_631590 is a thin wrapper over it.
		WEAK symbol<void(const configstringBundle* pairs, int count)>SV_SetConfigstrings{ "SV_SetConfigstrings" };

		WEAK symbol<void(int clientNum, const char* fmt, ...)>SV_SendServerCommand{ "SV_SendServerCommand" };

		// WaW sub_631590 — usercall(index@eax, string@ecx) -> void ; no stack args
		inline void SV_SetConfigstring(int index, const char* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SV_SetConfigstring"));
			__asm
			{
				mov  eax, index
				mov  ecx, value
				call fn
			}
		}
	}
}
