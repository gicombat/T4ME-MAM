// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Runtime address mapping. Resolves a logical name to
//          the correct virtual address for the detected exe
//          variant. The data (name,default,defaultMP,ger,gerMP) is
//          embedded in the DLL as resource IDR_ADDR_CSV
//          (t4m.rc -> Resource\addr_mapping.csv); nothing is read from disk.
//
// Columns map to the variant detected at 0x401000:
//   default   = LanFixed / SteamDefault (eng SP)
//   defaultMP = SteamDefaultMP          (eng MP)
//   ger       = SteamGer                (ger SP)
//   gerMP     = SteamGerMP              (ger MP)
// Empty columns are filled incrementally; the 'symbol<>' name is the CSV key.
// ==========================================================
#pragma once

#include <cstdint>
#include <string>

namespace T4M
{
	// First dword of .text (read at 0x00401000) identifies the build.
	enum ExeVariant : unsigned int
	{
		Unknown = 0,
		SteamDefault = 0x9EF490B8,   // *(DWORD*)0x401000 == 0x9EF490B8
		LanFixed = 0x83EC8B55,   // *(DWORD*)0x401000 == 0x83EC8B55
		SteamGer = 0xFA90BF6E,   // *(DWORD*)0x401000 == 0xFA90BF6E
		// MP
		SteamDefaultMP = 0x7FE5ED21,
		SteamGerMP = 0x0B18AD1F
	};

	// Reads/classifies the signature dword. Result is cached after the first call.
	bool IsGermanVersion();
	bool IsSteamVersion();
	bool IsMpVersion();
	void SetExeVariant(unsigned int);
	ExeVariant CurrentExeVariant();
	const char* ExeVariantName(ExeVariant v);

	// True when the host process is a WaW mod tool (sp_tool.exe / mp_tool.exe), not the game.
	// T4M stays inert there (checked first in DllMain).
	bool IsModToolProcess();

	// Loads the embedded CSV resource IDR_ADDR_CSV.
	// Idempotent: once loaded it is a no-op unless force == true.
	// Returns the number of rows parsed (0 on failure). Safe at DLL init time
	// (does not touch the game engine; diagnostics go to OutputDebugString).
	size_t AddrMap_Load(bool force = false);

	// Resolve a logical name to a VA for the current exe variant:
	//   LanFixed / SteamDefault -> 'default'
	//   SteamGer                -> 'ger'   (else 'default' fallback)
	//   SteamDefaultMP          -> 'defaultMP'
	//   SteamGerMP              -> 'gerMP' (else 'defaultMP' fallback; never SP)
	// Returns 0 if the name is unknown. Triggers a lazy load on first use.
	uintptr_t GetAddress(const char* name);
	inline uintptr_t GetAddress(const std::string& name) { return GetAddress(name.c_str()); }

	// Typed convenience:  auto Com_Printf = T4M::addr_as<Com_Printf_t>("Com_Printf");
	template <class T>
	inline T addr_as(const char* name) { return reinterpret_cast<T>(GetAddress(name)); }

	// Diagnostics / introspection.
	bool        AddrMap_Has(const char* name);
	size_t      AddrMap_Count();
	size_t      AddrMap_UnresolvedGerCount(); // rows with an empty 'ger' (matters on ger)
	const char* AddrMap_LoadedPath();         // source actually loaded ("" if none / not yet loaded)
}
