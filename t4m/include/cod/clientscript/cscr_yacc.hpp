#pragma once

namespace T4
{
	namespace engine
	{
		WEAK symbol<int()>yyparse{ "yyparse" };
		WEAK symbol<int()>yylex{ "yylex" };
		WEAK symbol<int()>yy_get_next_buffer{ "yy_get_next_buffer" };
		WEAK symbol<int()>yy_get_previous_state{ "yy_get_previous_state" };
		WEAK symbol<void()>yyrestart{ "yyrestart" };
		WEAK symbol<yy_buffer_state* ()>yy_create_buffer{ "yy_create_buffer" };

		// WaW sub_69AEA0 — usercall(strVal@ecx) -> uint@eax ; no stack args
		inline unsigned int LowerCase(unsigned int strVal)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("LowerCase"));
			unsigned int result;
			__asm
			{
				mov   ecx, strVal
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_69BFF0 — usercall(len@ecx, str_@edx) -> int@eax ; no stack args
		inline int StringValue(int len, const char* str_)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("StringValue"));
			int result;
			__asm
			{
				mov   ecx, len
				mov   edx, str_
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_69D520 — usercall(yy_current_state@eax) -> int@eax ; no stack args
		inline int yy_try_NUL_trans(int yy_current_state)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("yy_try_NUL_trans"));
			int result;
			__asm
			{
				mov   eax, yy_current_state
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_69D690 — usercall(result@eax) -> void ; no stack args
		inline void yy_flush_buffer(yy_buffer_state* result)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("yy_flush_buffer"));
			__asm
			{
				mov   eax, result
				call  fn
			}
		}
		// WaW sub_69D710 — usercall(a1@eax, parseData@stack0) -> void ; caller-cleans
		inline void ScriptParse(scriptInstance_t a1, sval_u* parseData)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ScriptParse"));
			__asm
			{
				push  parseData
				mov   eax, a1
				call  fn
				add   esp, 4
			}
		}

		// Warning Adress unknow for now
		WEAK symbol<FILE* ()>yy_load_buffer_state{ "yy_load_buffer_state" };
		WEAK symbol<void(const char* err)>yy_fatal_error{ "yy_fatal_error" };
		WEAK symbol<void* (void* ptr, unsigned int size)>yy_flex_realloc{ "yy_flex_realloc" };
		WEAK symbol<void(yy_buffer_state* b, FILE* file)>yy_init_buffer{ "yy_init_buffer" };
	}
} // namespace T4::engine