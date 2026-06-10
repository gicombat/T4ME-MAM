#pragma once

// dvar_s lives in T4::dvar (full def in structs.hpp). Forward-declare so the
namespace T4
{
	namespace dvar
	{
		struct dvar_s;
	}
}

namespace T4
{
	namespace engine
	{
		WEAK symbol<::T4::dvar::dvar_s*>monkeytoy{ "monkeytoy" };
	}
} // namespace T4::engine
