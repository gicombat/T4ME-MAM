#pragma once

namespace T4
{
	namespace engine
	{
		// sub_6D6AD0 — renderer init. Called from Com_PostInit_Video (sub_644BE0+0x7D).
		WEAK symbol<void()>R_Init{ "R_Init" };
		WEAK symbol<void()> R_BeginRegistration{ "R_BeginRegistration" };
		WEAK symbol<void()> R_ClearScene{ "R_ClearScene" };
		WEAK symbol<void()> CL_BeginRegistration{ "CL_BeginRegistration" };
		WEAK symbol<void()> CL_ClearState{ "CL_ClearState" };
	}
} // namespace T4::engine
