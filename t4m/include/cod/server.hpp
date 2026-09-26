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

		// --- AI limit phase 8.2 (SV_) ------------------------------
		// WaW sub_5AC670 — usercall(nprims@eax, prims@ecx, startPos@edi, endPos@esi,
		//   hitNum@stack, entNum@stack, passEntNum@stack, contentmask@stack) ; retn, caller cleans 10h.
		// CoD4 SV_SightTrace; WaW adds the proximity prim list (eax/ecx) and reads *hitNum on
		// entry as a hint (first entity to test) before overwriting it with the result.
		inline void SV_SightTrace(int* hitNum, const float* startPos, const float* endPos, int entNum,
		                          int passEntNum, int contentmask, int nprims, const void* prims)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SV_SightTrace"));
			__asm
			{
				push contentmask
				push passEntNum
				push entNum
				push hitNum
				mov  eax, nprims
				mov  ecx, prims
				mov  edi, startPos
				mov  esi, endPos
				call fn
				add  esp, 10h
			}
		}

		// WaW sub_4AF130 — usercall(endPos@ecx, startPos@stack) -> float@xmm0 ; retn, caller cleans 4.
		// CoD4 SV_FX_GetVisibility.
		inline float SV_FX_GetVisibility(const float* startPos, const float* endPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SV_FX_GetVisibility"));
			float vis;
			__asm
			{
				push   startPos
				mov    ecx, endPos
				call   fn
				add    esp, 4
				movss  vis, xmm0
			}
			return vis;
		}
	}
}
