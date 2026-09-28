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

		// --- AI limit phase 8.2 (math / CRT) ------------------------------
		// WaW sub_7E137E — CRT __libm_sse2_atan2: usercall(y@xmm0, x@xmm1) -> double@xmm0.
		inline double libm_sse2_atan2(double y, double x)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("libm_sse2_atan2"));
			double result;
			__asm
			{
				movsd xmm0, y
				movsd xmm1, x
				call  fn
				movsd result, xmm0
			}
			return result;
		}

		// WaW sub_401010 — inlined-floorf helper: usercall(x@xmm3) -> float@xmm0 ; touches xmm0-2, xmm4 only.
		inline float floorf_sse(float x)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("floorf_sse"));
			float result;
			__asm
			{
				movss xmm3, x
				call  fn
				movss result, xmm0
			}
			return result;
		}

		// CRT strtok (sub_7ABF70) — vanilla's own copy: its static cursor must be the one vanilla uses.
		WEAK symbol<char*(char* str, const char* delim)> crt_strtok{ "crt_strtok" };

		// sub_401060 — cdecl(float) -> st0 ; fsqrt.
		WEAK symbol<float(float x)> I_sqrt{ "I_sqrt" };

		// WaW sub_5E11C0 — usercall(angle@xmm3) -> xmm0.
		inline float AngleNormalize360(float angle)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AngleNormalize360"));
			float result;
			__asm
			{
				movss xmm3, angle
				call  fn
				movss result, xmm0
			}
			return result;
		}

		// WaW sub_5DF960 — usercall(vec@esi, angles@edi) ; no stack args. CoD4 vectoangles.
		// Not named vectoangles: T4E_items.ixx already defines a C++ approximation under that name.
		inline void vectoangles_vanilla(const float* vec, float* angles)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("vectoangles"));
			__asm
			{
				mov  esi, vec
				mov  edi, angles
				call fn
			}
		}

		// WaW sub_4037C0 — usercall(v@esi) -> length@xmm0 ; retn. Normalizes v in place;
		// leaves the x87 stack empty. CoD4 Vec3Normalize.
		inline float Vec3Normalize(float* v)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Vec3Normalize"));
			float len;
			__asm
			{
				mov    esi, v
				call   fn
				movss  len, xmm0
			}
			return len;
		}

		// WaW sub_5DEF90 — usercall(v@esi) -> void ; in-place, 2 components.
		inline void Vec2Normalize(float* v)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Vec2Normalize"));
			__asm
			{
				mov  esi, v
				call fn
			}
		}

		// sub_5FE8C0 — CoD4 Sys_Error: fatal popup, does not return.
		WEAK symbol<void(const char* fmt, ...)> Com_Fatal{ "Com_Fatal" };
	}
}