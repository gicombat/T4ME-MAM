#pragma once
#include <cstdint>

// ==========================================================
// T4M — per-version game-address resolver.
//
// Backed by the embedded "addr_ger.csv" (columns: name,default,ger). At runtime, addr(name)
// returns the game VA bound to `name` for the running exe:
//   - the DEFAULT column on the reference layout (LanFixed / Steam-ENG),
//   - the GER column on the German Steam exe.
// The running variant is decided once by T4::engine::environment::exeVariant().
//
// An unknown name (CSV incomplete) raises a MessageBox and returns 0 — so a missing mapping is
// caught loudly instead of silently corrupting a patch site.
// ==========================================================

namespace T4M
{
	uintptr_t addr(const char* name);
}
