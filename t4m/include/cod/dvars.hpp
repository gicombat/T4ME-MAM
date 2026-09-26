#pragma once

// dvar_s lives in T4::dvar (full def in structs.hpp). Forward-declare so the
namespace T4
{
	namespace dvar
	{
		struct dvar_s;

		// --- AI limit phase 8.2 (AI dvars) ------------------------------
		// "ai_foliageSeeThroughDist" — registered in sub_4FF700 (AI dvar init), stored at 0x18E86FC.
		// CoD4 calls it ai_foliageIngoreDist.
		WEAK engine::symbol<dvar_s*> ai_foliageSeeThroughDist{ "ai_foliageSeeThroughDist" };
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

		// --- AI limit phase 8.2 (AI dvars) ------------------------------
		WEAK symbol<T4::dvar::dvar_s*> ai_threatUpdateInterval{ "ai_threatUpdateInterval" };

		WEAK symbol<T4::dvar::dvar_s*> ai_showPotentialThreatDir{ "ai_showPotentialThreatDir" };
	}
} // namespace T4::engine
