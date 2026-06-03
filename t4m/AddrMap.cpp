// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Runtime address mapping reader (see AddrMap.h).
//          The CSV is embedded in the DLL as resource IDR_ADDR_CSV
//          (t4m.rc -> Resource\addr_mapping.csv); nothing is read
//          from disk at runtime.
// ==========================================================
#include "StdInc.h"
#include "AddrMap.h"

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace T4M
{
	// ---- exe signatures (first dword of .text @ 0x00401000) --------------
	// Only meaningful BEFORE PatchT4_SteamDRM decrypts .text; afterwards ENG
	// and GER both read kSigSteamEng. The GER decision is therefore taken from
	// environment::exeVariant() (primed early in DllMain, pre-decrypt), never
	// from a lazy read here.
	static constexpr uintptr_t kSigAddr      = 0x00401000;
	static constexpr DWORD     kSigLanFixed  = 0x9EF490B8;
	static constexpr DWORD     kSigSteamEng  = 0x83EC8B55;
	static constexpr DWORD     kSigSteamGer  = 0xFA90BF6E;

	struct AddrEntry
	{
		uintptr_t def = 0;   // 'default' column (eng / LanFixed VA)
		uintptr_t ger = 0;   // 'ger' column (0 when not yet filled)
		bool      hasGer = false;
	};

	static std::unordered_map<std::string, AddrEntry> g_map;
	static bool        g_loaded   = false;
	static ExeVariant  g_variant  = ExeVariant::Unknown;
	static bool        g_variantResolved = false;
	static size_t      g_unresolvedGer = 0;

	static void Dbg(const char* msg)
	{
		// init-safe logging only (no engine calls during Sys_RunInit / PatchT4_*).
		OutputDebugStringA(msg);
	}

	void SetExeVariant(unsigned int newVariant)
	{
		g_variant = static_cast<T4M::ExeVariant>(newVariant);
		g_variantResolved = true;
	}

	ExeVariant CurrentExeVariant()
	{
		if (!g_variantResolved)
		{
			MessageBoxA(NULL, "ERROR WHEN RETRIEVENING EXE VARIANT, IT WAS NOT INITIALIZED", "ERROR", 0);
			return Unknown;
		}
		return g_variant;
	}

	bool IsGermanVersion()
	{
		return (CurrentExeVariant() == SteamGer ||
				CurrentExeVariant() == SteamGerMP);
	}

	bool IsSteamVersion()
	{
		return (CurrentExeVariant() == SteamGer		  || 
				CurrentExeVariant() == SteamDefault	  || 
				CurrentExeVariant() == SteamDefaultMP || 
				CurrentExeVariant() == SteamGerMP);
	}

	bool IsMpVersion()
	{
		if (T4M::CurrentExeVariant() == T4M::ExeVariant::LanFixed)
		{
			if (*(DWORD*)0x881CAC == 0x62616E55) // MP identifier "Unab" character string
			{
				return true;
			}
		}
		else if (T4M::CurrentExeVariant() == SteamDefaultMP || T4M::CurrentExeVariant() == SteamGerMP)
		{
			return true;
		}

		return false;
	}

	const char* ExeVariantName(ExeVariant v)
	{
		switch (v)
		{
			case ExeVariant::LanFixed: 
				return "LanFixed";
			case ExeVariant::SteamDefault: 
				return "SteamDefault";
			case ExeVariant::SteamGer: 
				return "SteamGer";
			case ExeVariant::SteamDefaultMP:
				return "SteamDefaultMP";
			case ExeVariant::SteamGerMP:
				return "SteamGerMP";
			default:                   
				return "Unknown";
		}
	}

	// ---- helpers ---------------------------------------------------------
	static inline void Trim(std::string& s)
	{
		const char* ws = " \t\r\n";
		const size_t a = s.find_first_not_of(ws);
		if (a == std::string::npos) { s.clear(); return; }
		const size_t b = s.find_last_not_of(ws);
		s = s.substr(a, b - a + 1);
	}

	static uintptr_t ParseHex(const std::string& s)
	{
		if (s.empty())
			return 0;
		return static_cast<uintptr_t>(std::strtoull(s.c_str(), nullptr, 16)); // tolerates "0x" prefix
	}

	// Parse the CSV text (embedded resource contents) into g_map.
	static bool ParseBuffer(const std::string& text)
	{
		g_map.clear();
		g_unresolvedGer = 0;

		std::istringstream f(text);
		std::string line;
		while (std::getline(f, line))
		{
			Trim(line);
			if (line.empty() || line[0] == '#')
				continue;

			// split into at most 3 comma fields
			std::string fields[3];
			size_t fi = 0, start = 0;
			for (size_t i = 0; i <= line.size() && fi < 3; ++i)
			{
				if (i == line.size() || line[i] == ',')
				{
					fields[fi++] = line.substr(start, i - start);
					start = i + 1;
				}
			}
			std::string name = fields[0]; Trim(name);
			std::string def  = fields[1]; Trim(def);
			std::string ger  = fields[2]; Trim(ger);

			// skip header row
			if (name == "name" && def == "default")
				continue;
			if (name.empty())
				continue;

			AddrEntry e;
			e.def = ParseHex(def);
			e.hasGer = !ger.empty();
			e.ger = e.hasGer ? ParseHex(ger) : 0;
			if (!e.hasGer)
				++g_unresolvedGer;

			g_map[name] = e;
		}

		return !g_map.empty();
	}

	size_t AddrMap_Load(bool force)
	{
		if (g_loaded && !force)
			return g_map.size();

		g_loaded = true; // mark attempted so we don't re-hit the resource on every GetAddress()

		const std::string csv = GetBinaryResource(IDR_ADDR_CSV);
		const bool ok = !csv.empty() && ParseBuffer(csv);
		if (!ok)
		{
			Dbg("[T4M AddrMap] FAILED to load embedded addr_mapping.csv (IDR_ADDR_CSV)\n");
			return 0;
		}

		char msg[256];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE,
		            "[T4M AddrMap] loaded %zu rows from embedded IDR_ADDR_CSV (variant=%s, unresolved-ger=%zu)\n",
		            g_map.size(), ExeVariantName(CurrentExeVariant()), g_unresolvedGer);
		Dbg(msg);
		return g_map.size();
	}

	// Signal (once per name+variant, to avoid flooding) that a requested address
	// is not present for the running exe — unknown name, empty CSV column for the
	// current variant, or a resolved value of 0. MessageBox so it is visible even
	// without a debugger attached; OutputDebugString as well.
	static void WarnMissingAddress(const char* name, const char* detail)
	{
		static std::unordered_set<std::string> warned;
		const std::string key = std::string(name ? name : "(null)") + "@" + ExeVariantName(CurrentExeVariant());
		if (!warned.insert(key).second)
			return; // already warned for this name on this variant

		char msg[512];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE,
		            "[T4M AddrMap] address not present\n  name    = %s\n  variant = %s\n  %s",
		            name ? name : "(null)", ExeVariantName(CurrentExeVariant()), detail);
		Dbg(msg);
		MessageBoxA(NULL, msg, "T4M AddrMap - missing address", MB_OK | MB_ICONWARNING);
	}

	uintptr_t GetAddress(const char* name)
	{
		if (!g_loaded)
			AddrMap_Load();

		auto it = g_map.find(name);
		if (it == g_map.end())
		{
			WarnMissingAddress(name, "name is not in addr_mapping.csv");
			return 0;
		}

		const AddrEntry& e = it->second;

		uintptr_t va;
		if (CurrentExeVariant() == ExeVariant::SteamGer)
		{
			if (!e.hasGer)
			{
				WarnMissingAddress(name, "no 'ger' value in CSV (falling back to default eng VA)");
				va = e.def; // fall back to eng until ger is filled
			}
			else
				va = e.ger;
		}
		else
		{
			va = e.def;
		}

		if (va == 0)
			WarnMissingAddress(name, "resolved address is 0 (CSV column empty for this variant)");

		return va;
	}

	bool AddrMap_Has(const char* name)
	{
		if (!g_loaded)
			AddrMap_Load();

		return g_map.find(name) != g_map.end();
	}

	size_t AddrMap_Count()              
	{ 
		if (!g_loaded) 
			AddrMap_Load(); 

		return g_map.size(); 
	}

	size_t AddrMap_UnresolvedGerCount() 
	{ 
		if (!g_loaded)
			AddrMap_Load(); 

		return g_unresolvedGer; 
	}

	const char* AddrMap_LoadedPath()         
	{
		return g_loaded ? "embedded:IDR_ADDR_CSV" : ""; 
	}
}
