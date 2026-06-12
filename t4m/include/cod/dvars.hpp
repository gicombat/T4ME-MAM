#pragma once

// dvar_s lives in T4::dvar (full def in structs.hpp). Forward-declare so the
namespace T4
{
	namespace dvar
	{
		struct dvar_s;
	}
}

// dvar_t is a DISTINCT global-scope struct (defined in T4.h), not T4::dvar::dvar_s.
// Forward-declare it so symbol<dvar_t*> below compiles before T4.h is seen.
struct __declspec(align(4)) dvar_t;

namespace T4
{
	namespace engine
	{
		WEAK symbol<T4::dvar::dvar_s*>monkeytoy{ "monkeytoy" };

		WEAK symbol<dvar_t*> developer_script{ "developer_script" };
		WEAK symbol<dvar_t*> logfile{ "logfile" };

		// moved from T4.cpp extern "C"
		WEAK symbol<void(DWORD)> NET_RegisterDvars{ "NET_RegisterDvars" };
		WEAK symbol<dvar_t*(const char*)> Dvar_FindMalleableVar{ "Dvar_FindMalleableVar" };
	}
} // namespace T4::engine
