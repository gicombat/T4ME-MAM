#pragma once

namespace T4
{
	namespace engine
	{
		// Function
		WEAK symbol<void(const char* text, vec4_t* color)>ConDrawInput_Text{ "ConDrawInput_Text" };
		WEAK symbol<void(const char* text, int numChars, vec4_t* color)> ConDrawInput_TextLimitChars{"ConDrawInput_TextLimitChars"};


		// Variable
		WEAK symbol<ConDrawInputGlob> conDraw{ "conDraw" };
	}
} // namespace T4::engine
