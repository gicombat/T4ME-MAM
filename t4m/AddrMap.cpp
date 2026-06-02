// ==========================================================
// T4M — per-version (default / German) game-address resolver.
//
// Reads the embedded addr_ger.csv (name,default,ger) once, then addr(name) returns the column
// for the running exe (see T4::engine::environment::exeVariant()). A missing name or a malformed
// CSV raises a MessageBox (MessageBoxA is a Win32 API, safe during init/DllMain).
// ==========================================================

#include "StdInc.h"
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

namespace
{
	std::unordered_map<std::string, std::pair<uintptr_t, uintptr_t>> g_map; // name -> { default, ger }
	bool g_loaded = false;

	void LoadOnce()
	{
		if (g_loaded)
			return;
		g_loaded = true;

		std::string csv = GetBinaryResource(IDR_ADDR_CSV);
		if (csv.empty())
		{
			MessageBoxA(nullptr, "T4M: ressource addr_ger.csv introuvable ou vide.",
				"T4M - AddrMap", MB_OK | MB_ICONERROR);
			return;
		}

		size_t pos = 0;
		bool first = true;
		while (pos < csv.size())
		{
			size_t eol = csv.find('\n', pos);
			std::string line = csv.substr(pos, (eol == std::string::npos ? csv.size() : eol) - pos);
			pos = (eol == std::string::npos) ? csv.size() : eol + 1;

			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty())
				continue;
			if (first)
			{
				first = false;
				if (line.rfind("name", 0) == 0) // skip the header row
					continue;
			}

			size_t c1 = line.find(',');
			size_t c2 = (c1 == std::string::npos) ? std::string::npos : line.find(',', c1 + 1);
			if (c1 == std::string::npos || c2 == std::string::npos)
			{
				char msg[512];
				_snprintf(msg, sizeof(msg), "T4M: ligne addr_ger.csv malformee:\n%s", line.c_str());
				MessageBoxA(nullptr, msg, "T4M - AddrMap", MB_OK | MB_ICONERROR);
				continue;
			}

			std::string name = line.substr(0, c1);
			uintptr_t def = (uintptr_t)strtoul(line.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr, 16);
			uintptr_t ger = (uintptr_t)strtoul(line.substr(c2 + 1).c_str(), nullptr, 16);
			g_map[name] = std::make_pair(def, ger);
		}
	}
}

uintptr_t T4M::addr(const char* name)
{
	LoadOnce();

	auto it = g_map.find(name);
	if (it == g_map.end())
	{
		char msg[512];
		_snprintf(msg, sizeof(msg),
			"T4M: l'adresse \"%s\" est absente de addr_ger.csv.\n"
			"Le mapping est incomplet - ajoute-la au CSV.", name);
		MessageBoxA(nullptr, msg, "T4M - addr() : nom inconnu", MB_OK | MB_ICONERROR);
		return 0;
	}

	return (T4::engine::environment::exeVariant() == T4::engine::environment::EXE_GER)
		? it->second.second
		: it->second.first;
}
